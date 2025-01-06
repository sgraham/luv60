#include "luv60.h"

#define DO_PRINTS 0

// This is based on https://arxiv.org/pdf/1902.08318.pdf which is the
// paper about simdjson.org.
//
// This code assumes AVX2 (circa 2011) which only We only have one
// kernel here that assumes AVX2 (~2011) and has 256 bit registers. All
// input is assumed to be padded to 64 bytes with spaces if necessary,
// and ASCII (0x00-0x7f), and aligned to a 64 byte address. TODO: bail
// to fallback for utf-8.
//
// Lexing is is done in 64 byte chunks (512 bits) of two registers.
//
// There are two phases:
// 1. Use SIMD instructions to build an index of "interesting locations"
//    in the input. The output of this step is an array of uint32_ts
//    each of which are the index into source buffer of the beginning of
//    what will be tokens.
// 2. The second step actually determines the type of the token at all
//    the interesting locations and or's in the classification, which
//    turns the indexes into |Token|s.
typedef struct Simd64 {
  __m256i chunks[2];
} Simd64;

typedef struct Classes {
  uint64_t space;
  uint64_t punct;
} Classes;

#define EQ_CHAR(name, ch)                                                                       \
  __attribute__((nonnull)) static FORCEINLINE uint64_t eq_##name(const Simd64* __restrict in) { \
    const __m256i splat = _mm256_set1_epi8(ch);                                                 \
    const Simd64 cmp = {_mm256_cmpeq_epi8(in->chunks[0], splat),                                \
                        _mm256_cmpeq_epi8(in->chunks[1], splat)};                               \
    const uint64_t bitmask_lo = ((uint32_t)_mm256_movemask_epi8(cmp.chunks[0]));                \
    const uint64_t bitmask_hi = ((uint32_t)_mm256_movemask_epi8(cmp.chunks[1]));                \
    const uint64_t bitmask = (bitmask_hi << 32) | bitmask_lo;                                   \
    return bitmask;                                                                             \
  }

EQ_CHAR(backslash, '\\')
EQ_CHAR(double_quote, '"')
EQ_CHAR(zero, 0)
EQ_CHAR(hash, '#')
EQ_CHAR(newline, '\n')

#define HAS_BIT(n)                                                               \
  static FORCEINLINE uint64_t has_bit_##n(const Simd64* __restrict in) {         \
    const __m256i splat = _mm256_set1_epi8(1 << n);                              \
    const Simd64 and = {_mm256_and_si256(in->chunks[0], splat),                  \
                        _mm256_and_si256(in->chunks[1], splat)};                 \
    const Simd64 cmp = {_mm256_cmpeq_epi8(and.chunks[0], splat),                 \
                        _mm256_cmpeq_epi8(and.chunks[1], splat)};                \
    const uint64_t bitmask_lo = ((uint32_t)_mm256_movemask_epi8(cmp.chunks[0])); \
    const uint64_t bitmask_hi = ((uint32_t)_mm256_movemask_epi8(cmp.chunks[1])); \
    const uint64_t backslash_bitmask = (bitmask_hi << 32) | bitmask_lo;          \
    return backslash_bitmask;                                                    \
  }

HAS_BIT(1)

// "cumulative bitwise xor," flipping bits each time a 1 is encountered.
//
// e.g. prefix_xor(00100100) == 00011100
static FORCEINLINE uint64_t prefix_xor(const uint64_t bitmask) {
  const __m128i all_ones = _mm_set1_epi8('\xFF');
  const __m128i result = _mm_clmulepi64_si128(_mm_set_epi64x(0ULL, bitmask), all_ones, 0);
  return _mm_cvtsi128_si64(result);
}

// Using vpshufb, we can use the least 4 significant bytes to index into
// 16 byte tables. So by doing two lookups, first one on the bottom 4 bits,
// and then shifting and a second lookup in a different table, we can
// separate characters into categories.
//
// For our purposes, we want to split out identifers, numbers, and
// whitespace, while assuming that the rest of viable inputs are
// possible punctuation. Phase 2 classification will error out if the
// potential punctuation turns out to be unused in the language, and on
// other unused below-0x20 control characters.
//
// The lookup tables we use are set up as follows, so e.g. in the final
// lookup if bit 1 is set, the input was <SPACE>, if bit 5 is set the
// character is in the range 'P'..'Z' or 'p'..'z'. By combining these,
// we can tell if the character is a number or identifier starting
// location.
//
//          hi    lo
// bit 0:  not used yet
// bit 1:    2     0    <SPACE>
// bit 2:    5     F    '_'
// bit 3:    3   0-9    '0'..'9'
// bit 4:  4,6   1-F    'A'..'O', 'a'..'o',
// bit 5:  5,7   0-A    'P'..'Z', 'p'..'z'
// bit 6:    4     0    '@'
// bit 7:  not used yet
//
// Expanded out into a full table to get the LUTs for the vpshufb instruction,
// they look like this:
//
//     low    |
//     nibble |  0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f
// high       |
// nibble     | 46  56  56  56  56  56  56  56  56  56  48  16  16  16  16  20
// ---------------------------------------------------------------------------
//   0      0 |
//   1      0 |
//   2      2 |  2
//   3      8 |  8   8   8   8   8   8   8   8   8   8
//   4     20 |  4  16  16  16  16  16  16  16  16  16  16  16  16  16  16  16
//   5     36 | 32  32  32  32  32  32  32  32  32  32  32                   4
//   6     16 |     16  16  16  16  16  16  16  16  16  16  16  16  16  16  16
//   7     32 |     32  32  32  32  32  32  32  32  32  32
//   8      0 |   
//   9      0 |
//   a      0 |
//   b      0 |
//   c      0 |
//   d      0 |
//   e      0 |
//   f      0 |

static FORCEINLINE Classes classify(const Simd64* __restrict in) {
  const __m256i low_lut =
      _mm256_setr_epi8(46, 56, 56, 56, 56, 56, 56, 56, 56, 56, 48, 16, 16, 16, 16, 20,  //
                       46, 56, 56, 56, 56, 56, 56, 56, 56, 56, 48, 16, 16, 16, 16, 20   //
      );
  const __m256i high_lut = _mm256_setr_epi8(0, 0, 2, 8, 20, 36, 16, 32, 0, 0, 0, 0, 0, 0, 0, 0,  //
                                            0, 0, 2, 8, 20, 36, 16, 32, 0, 0, 0, 0, 0, 0, 0, 0   //
  );

  const Simd64 low_mask = {_mm256_shuffle_epi8(low_lut, in->chunks[0]),
                           _mm256_shuffle_epi8(low_lut, in->chunks[1])};

  const Simd64 in_high = {
      _mm256_and_si256(_mm256_srli_epi32(in->chunks[0], 4), _mm256_set1_epi8(0x0f)),
      _mm256_and_si256(_mm256_srli_epi32(in->chunks[1], 4), _mm256_set1_epi8(0x0f))};

  const Simd64 high_mask = {_mm256_shuffle_epi8(high_lut, in_high.chunks[0]),
                            _mm256_shuffle_epi8(high_lut, in_high.chunks[1])};

  const Simd64 mask = {_mm256_and_si256(low_mask.chunks[0], high_mask.chunks[0]),
                       _mm256_and_si256(low_mask.chunks[1], high_mask.chunks[1])};

  return (Classes){.space = has_bit_1(&mask), .punct = eq_zero(&mask)};
}

static const uint64_t ODD_BITS = 0xAAAAAAAAAAAAAAAAull;

/**
 * Returns a mask of the next escape characters (masking out escaped backslashes), along with
 * any non-backslash escape codes.
 *
 * \n \\n \\\n \\\\n returns:
 * \n \   \ \n \ \
 * 11 100 1011 10100
 *
 * You are expected to mask out the first bit yourself if the previous block had a trailing
 * escape.
 *
 * & the result with potential_escape to get just the escape characters.
 * ^ the result with (potential_escape | first_is_escaped) to get escaped characters.
 */
static FORCEINLINE uint64_t next_escape_and_terminal_code(uint64_t potential_escape) {
  // Escaped characters are characters following an escape.
  const uint64_t maybe_escaped = potential_escape << 1;

  // To distinguish odd from even escape sequences, therefore, we turn on any *starting*
  // escapes that are on an odd byte. (We actually bring in all odd bits, for speed.)
  // - Odd runs of backslashes are 0000, and the code at the end ("n" in \n or \\n) is 1.
  // - Odd runs of backslashes are 1111, and the code at the end ("n" in \n or \\n) is 0.
  // - All other odd bytes are 1, and even bytes are 0.
  const uint64_t maybe_escaped_and_odd_bits = maybe_escaped | ODD_BITS;
  const uint64_t even_series_codes_and_odd_bits = maybe_escaped_and_odd_bits - potential_escape;

  // Now we flip all odd bytes back with xor. This:
  // - Makes odd runs of backslashes go from 0000 to 1010
  // - Makes even runs of backslashes go from 1111 to 1010
  // - Sets actually-escaped codes to 1 (the n in \n and \\n: \n = 11, \\n = 100)
  // - Resets all other bytes to 0
  return even_series_codes_and_odd_bits ^ ODD_BITS;
}

// |backslash| is mask of all escape characters, i.e. eq_backslash(block)
// |first_is_escaped| is the carry from block to block
// return is the bitmask of the characters that are escaped.
// This is taken straight from simdjson.
__attribute__((nonnull)) static FORCEINLINE uint64_t
escapes_next_block(uint64_t backslash, uint64_t* first_is_escaped) {
  if (!backslash) {
    const uint64_t escaped = *first_is_escaped;
    *first_is_escaped = 0;
    return escaped;
  }

  const uint64_t escape_and_terminal_code =
      next_escape_and_terminal_code(backslash & ~*first_is_escaped);
  const uint64_t escaped = escape_and_terminal_code ^ (backslash | *first_is_escaped);
  *first_is_escaped = (escape_and_terminal_code & backslash) >> 63;
  return escaped;
}

static FORCEINLINE uint64_t set_all_bits_if_high_bit_set(uint64_t input) {
  return (uint64_t)((int64_t)input >> 63);
}

static FORCEINLINE void find_delimiters(uint64_t quotes,
                                        uint64_t hashes,
                                        uint64_t newlines,
                                        uint64_t state_in_quoted,
                                        uint64_t state_in_comment,
                                        uint64_t* quotes_mask,
                                        uint64_t* comments_mask) {
  uint64_t starts = quotes | hashes;
  uint64_t end = (newlines & state_in_comment) | (quotes & state_in_quoted);
  end &= -end;

  uint64_t delimiters = end;
  starts &= ~((state_in_comment | state_in_quoted) ^ (-end - end));

  while (starts) {
    const uint64_t start = -starts & starts;
    ASSERT(start);

    const uint64_t quote = quotes & start;

    const uint64_t hash = hashes & start;

    end = (newlines & -hash) | (quotes & (-quote - quote));
    end &= -end;

    delimiters |= end | start;
    starts &= -end - end;
  }

  *quotes_mask = delimiters & quotes;
  *comments_mask = delimiters & ~quotes;
}

static FORCEINLINE uint32_t trailing_zeros(uint64_t input) {
  return __builtin_ctzll(input);
}

static FORCEINLINE uint64_t clear_lowest_bit(uint64_t input) {
  return input & (input - 1);
}

#if DO_PRINTS
static void print_with_visible_unprintable(char ch) {
  if (ch == '\n') {
    printf("↵");
  } else if (ch == ' ') {
    printf("⚬");
  } else if (ch < ' ' || ch >= 0x7f) {
    printf("·");
  } else {
    printf("%c", ch);
  }
}
static char* print_buf_ptr;
static void print_with_coloured_bits(char* label, uint64_t bitmask, char* comment) {
  printf("%20s: ", label);
  for (int i = 0; i < 64; ++i) {
    if (bitmask & 1) {
      printf("\033[31m");  // red
    } else {
      printf("\033[0m");  // default
    }
    print_with_visible_unprintable(print_buf_ptr[i]);
    bitmask >>= 1;
  }
  printf("  \033[32m// %s", comment);
  printf("\033[0m");
  printf("\n");
}
#endif

// These are for carries across blocks.
static uint64_t state_in_quoted_;
static uint64_t state_in_comment_;
static uint64_t state_structural_start_;
static uint32_t offset_;
static const uint8_t* buf_;
static size_t buf_size_;
static int64_t state_start_rel_offset_;
static int indents_[256];
static int num_indents_ = 1;
static int continuation_paren_level_;

void lex_start(const uint8_t* buf, size_t byte_count_rounded_up) {
  state_in_quoted_ = 0;
  state_in_comment_ = 0;
  state_structural_start_ = 1;
  buf_ = buf;
  offset_ = 0;
  buf_size_ = byte_count_rounded_up;
  state_start_rel_offset_ = 0;
  num_indents_ = 1;
  continuation_paren_level_ = 0;
}

void lex_next_block(uint8_t token_kinds[128], uint32_t token_offsets[128]) {
  // The first attempt at this lexer followed simdjson's lexing. Their
  // double quote mask finding is very clever. But, in the presence of
  // comments I wasn't able to find a way to create a mask that handled
  // the possibility of double quotes appearing in comments. Due to
  // this, we have to use some branches/loops to be able to exclude #
  // from within quotes, and quotes in comments.

  Simd64 data = {_mm256_loadu_si256((const __m256i*)&buf_[offset_]),
                 _mm256_loadu_si256((const __m256i*)&buf_[offset_ + 32])};
#if DO_PRINTS
  print_buf_ptr = (char*)&buf_[offset_];
#endif
  uint64_t first_is_escaped = 0;
  const uint64_t backslash = eq_backslash(&data);

  const uint64_t escaped = escapes_next_block(backslash, &first_is_escaped);

  const uint64_t newlines = eq_newline(&data);

  uint64_t quotes = eq_double_quote(&data) & ~escaped;
  const uint64_t hashes = eq_hash(&data) & ~escaped;

  uint64_t quotes_mask = state_in_quoted_;
  uint64_t comments_mask = state_in_comment_;

  if (state_in_comment_ || hashes) {
    uint64_t comments = 0;
    find_delimiters(quotes, hashes, newlines, quotes_mask, comments_mask, &quotes, &comments);

    quotes_mask ^= prefix_xor(quotes);
    state_in_quoted_ = set_all_bits_if_high_bit_set(quotes_mask);
    comments_mask ^= prefix_xor(comments);
    state_in_comment_ = set_all_bits_if_high_bit_set(comments_mask);
  } else {
    quotes_mask ^= prefix_xor(quotes);
    state_in_quoted_ = set_all_bits_if_high_bit_set(quotes_mask);
  }

  Classes classes = classify(&data);

  uint64_t S = classes.punct & ~(quotes_mask | comments_mask);

  S = S | quotes;

  uint64_t P = S | classes.space;

  const uint64_t structural_start_shoved = P >> 63;
  P = (P << 1) | state_structural_start_;
  state_structural_start_ = structural_start_shoved;

  P &= ~classes.space & ~(quotes_mask | comments_mask);

  S = S | P;

  uint64_t indexes = S & ~(quotes & ~quotes_mask);

#if DO_PRINTS
  print_with_coloured_bits("final", indexes, "");
#endif

  uint8_t* tk = token_kinds;
  uint32_t* to = token_offsets;

  // Now actually do the branchy categorization of this batch of tokens. TBD:
  // maybe faster to extract bits from |indexes| first and then categorize in a second
  // pass that's going back to the buffer?
  //
  // The unaligned loads are probably bad too, and could probably be turned into
  // overlapped successive shifts, etc.

  uint64_t start_rel_offset;
  if (state_start_rel_offset_ == 0) {
    start_rel_offset = trailing_zeros(indexes);
    indexes = clear_lowest_bit(indexes);
  } else {
    start_rel_offset = state_start_rel_offset_;
  }

  while (indexes) {
    uint64_t start_abs_offset = offset_ + start_rel_offset;
    uint64_t end_rel_offset = trailing_zeros(indexes);
    indexes = clear_lowest_bit(indexes);
#if DO_PRINTS
    printf("[%3lld, %3lld)\n", start_rel_offset, end_rel_offset);
#endif

    const unsigned char* p = (const unsigned char*)&buf_[start_abs_offset];

    const unsigned char* q;

#define SKIP()                                \
  do {                                        \
    start_rel_offset = end_rel_offset;        \
    end_rel_offset = trailing_zeros(indexes); \
    indexes = clear_lowest_bit(indexes);      \
  } while (0)

#define EMIT(tok)             \
  do {                        \
    *tk++ = tok;              \
    *to++ = start_abs_offset; \
  } while (0)

#define NEWLINE_INDENT_ADJUST_AND_BREAK(n)                         \
  if (continuation_paren_level_) {                                 \
    token = TOK_NL;                                                \
  } else {                                                         \
    if (indents_[num_indents_ - 1] < n) {                          \
      indents_[num_indents_++] = n;                                \
      EMIT(TOK_NEWLINE);                                           \
      ++start_abs_offset;                                          \
      token = TOK_INDENT;                                          \
      break;                                                       \
    } else if (indents_[num_indents_ - 1] > n) {                   \
      EMIT(TOK_NEWLINE);                                           \
      ++start_abs_offset;                                          \
      for (;;) {                                                   \
        if (num_indents_ > 1 && indents_[num_indents_ - 2] == n) { \
          token = TOK_DEDENT;                                      \
          --num_indents_;                                          \
          break;                                                   \
        }                                                          \
        EMIT(TOK_DEDENT);                                          \
        --num_indents_;                                            \
      }                                                            \
      break;                                                       \
    }                                                              \
    token = TOK_NEWLINE;                                           \
  }                                                                \
  break;

#include "categorizer.c"
  }

  *tk++ = TOK_INVALID;

  state_start_rel_offset_ = start_rel_offset - 64;
  offset_ += 64;
}

StrView lex_get_strview(uint32_t from, uint32_t to) {
  return (StrView){(const char*)&buf_[from], to - from};
}

void lex_get_location_and_line_slow(uint32_t offset,
                                    uint32_t* loc_line,
                                    uint32_t* loc_column,
                                    StrView* contents) {
  const char* line_start = (const char*)&buf_[0];
  uint32_t line = 1;
  uint32_t col = 1;
  const char* find = (const char*)&buf_[offset];
  for (const char* p = (const char*)&buf_[0];; ++p) {
    ASSERT(*p != 0);
    if (p == find) {
      const char* line_end = strchr(p, '\n');  // TODO: error on file w/o newline
      *loc_line = line;
      *loc_column = col;
      *contents = (StrView){line_start, line_end - line_start};
      return;
    }
    if (*p == '\n') {
      line += 1;
      col = 1;
      line_start = p + 1;
    } else {
      col += 1;
    }
  }
}
