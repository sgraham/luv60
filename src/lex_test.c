#include "luv60.h"
#include "test.h"

static const char* token_names[NUM_TOKEN_KINDS] = {
#define TOKEN(n) #n,
#include "tokens.inc"
#undef TOKEN
};

typedef struct KindAndOffset {
  TokenKind kind;
  uint32_t offset;
} KindAndOffset;

static bool lex_test(const char* buf, KindAndOffset* exp, size_t num_exp) {
  size_t len = strlen(buf) + 1;
  size_t alloc_size = ALIGN_UP(len + 64, PAGE_SIZE);
  unsigned char* padded_copy = base_large_alloc_rw(alloc_size);
  memcpy(padded_copy, buf, len);

  lex_start(padded_copy, alloc_size);

  uint8_t token_kinds[128] = {0};
  uint32_t token_offsets[128] = {0};
  size_t cur_tok_index = 0;
  for (size_t i = 0; i < num_exp; ++i) {
    while (!token_kinds[cur_tok_index]) {
      cur_tok_index = 0;
      lex_next_block(token_kinds, token_offsets);
    }

    if (token_kinds[cur_tok_index] != exp[i].kind) {
      base_writef_stderr("\nindex %zd: got %s, wanted %s\n", i,
                         token_names[token_kinds[cur_tok_index]], token_names[exp[i].kind]);
      base_large_alloc_free(padded_copy);
      return false;
    }
    if (token_offsets[cur_tok_index] != exp[i].offset) {
      base_writef_stderr("\nindex %zd was %s, but got off %d, wanted %d\n", i,
                         token_names[token_kinds[cur_tok_index]], token_offsets[cur_tok_index],
                         exp[i].offset);
      base_large_alloc_free(padded_copy);
      return false;
    }

    ++cur_tok_index;
  }

  base_large_alloc_free(padded_copy);
  return true;
}

TEST(Lex, Basic) {
  KindAndOffset expected[] = {
      {TOK_CONST, 0},       //
      {TOK_IDENT_VAR, 6},   //
      {TOK_EQ, 13},         //
      {TOK_IDENT_VAR, 15},  //
      {TOK_NEWLINE, 19},    //
      {TOK_EOF, 20},        //
  };
  EXPECT_TRUE(lex_test("const kStuff = blah\n", expected, COUNTOF(expected)));
}

TEST(Lex, Punctuation) {
  KindAndOffset expected[] = {
      {TOK_LBRACE, 0},    //
      {TOK_RBRACE, 2},    //
      {TOK_LSQUARE, 4},   //
      {TOK_RSQUARE, 6},   //
      {TOK_DOT, 8},       //
      {TOK_LEQ, 10},      //
      {TOK_GEQ, 13},      //
      {TOK_LT, 16},       //
      {TOK_GT, 18},       //
      {TOK_PLUS, 20},     //
      {TOK_MINUS, 22},    //
      {TOK_STAR, 24},     //
      {TOK_SLASH, 26},    //
      {TOK_PLUSEQ, 28},   //
      {TOK_MINUSEQ, 31},  //
      {TOK_STAREQ, 34},   //
      {TOK_SLASHEQ, 37},  //
      {TOK_EOF, 39},      //
  };
  EXPECT_TRUE(lex_test("{ } [ ] . <= >= < > + - * / += -= *= /=", expected, COUNTOF(expected)));
}

TEST(Lex, Keywords) {
  KindAndOffset expected[] = {
      {TOK_CONST, 0},     //
      {TOK_DEF, 6},       //
      {TOK_ELIF, 10},     //
      {TOK_ELSE, 15},     //
      {TOK_FALSE, 20},    //
      {TOK_FOR, 26},      //
      {TOK_IF, 30},       //
      {TOK_STRUCT, 33},   //
      {TOK_NEWLINE, 39},  //
      {TOK_TRUE, 40},     //
      {TOK_EOF, 44},      //
  };
  EXPECT_TRUE(
      lex_test("const def elif else false for if struct\ntrue", expected, COUNTOF(expected)));
}

TEST(Lex, DecimalInt) {
  KindAndOffset expected[] = {
      {TOK_INT_LITERAL, 0},   //
      {TOK_INT_LITERAL, 2},   //
      {TOK_INT_LITERAL, 6},   //
      {TOK_INT_LITERAL, 12},  //
      {TOK_INT_LITERAL, 33},  //
      {TOK_INT_LITERAL, 37},  //
      {TOK_INT_LITERAL, 41},  //
      {TOK_INT_LITERAL, 46},  //
      {TOK_INT_LITERAL, 51},  //
      {TOK_INT_LITERAL, 56},  //
      {TOK_INT_LITERAL, 61},  //
      {TOK_INT_LITERAL, 66},  //
      {TOK_INT_LITERAL, 71},  //
      {TOK_EOF, 84},          //
  };
  EXPECT_TRUE(lex_test(
      "0 5   100   18446744073709551615 1i8 2u8 3i16 4u16 5i32 6u32 7i64 8u64 123_456_7_u32",
      expected, COUNTOF(expected)));
}

TEST(Lex, HexInt) {
  KindAndOffset expected[] = {
      {TOK_INT_LITERAL, 0},   //
      {TOK_INT_LITERAL, 4},   //
      {TOK_INT_LITERAL, 10},  //
      {TOK_INT_LITERAL, 23},  //
      {TOK_INT_LITERAL, 30},  //
      {TOK_EOF, 36},          //
  };
  EXPECT_TRUE(lex_test("0x0 0x5   0x12abcdef34 0x1i64 0x2u64", expected, COUNTOF(expected)));
}

TEST(Lex, Newlines) {
  KindAndOffset expected[] = {
      {TOK_DEF, 0},        //
      {TOK_IDENT_VAR, 4},  //
      {TOK_LPAREN, 5},     //
      {TOK_RPAREN, 6},     //
      {TOK_COLON, 7},      //
      {TOK_NEWLINE, 8},    //
      {TOK_EOF, 9},        //
  };
  const char input[] = "def a():\n";
  EXPECT_TRUE(lex_test(input, expected, COUNTOF(expected)));
}
