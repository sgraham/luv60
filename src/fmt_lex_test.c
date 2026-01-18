#include "luv60.h"
#include "test.h"

static const char* fmtlex_token_kind_name(FmtTokenKind kind) {
  switch (kind) {
    case FMTTOK_LITERAL:
      return "LITERAL";
    case FMTTOK_LBRACE:
      return "LBRACE";
    case FMTTOK_RBRACE:
      return "RBRACE";
    case FMTTOK_FIELD_NAME:
      return "FIELD_NAME";
    case FMTTOK_CONVERSION:
      return "CONVERSION";
    case FMTTOK_COLON:
      return "COLON";
    case FMTTOK_FORMAT_SPEC:
      return "FORMAT_SPEC";
    case FMTTOK_DOT:
      return "DOT";
    case FMTTOK_LBRACKET:
      return "LBRACKET";
    case FMTTOK_RBRACKET:
      return "RBRACKET";
    case FMTTOK_ESCAPED_BRACE:
      return "ESCAPED_BRACE";
    case FMTTOK_EOF:
      return "EOF";
    case FMTTOK_ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

static bool fmtlex_test(const char* input, FmtTokenKind* expected, size_t num_expected) {
  fmtlex_start(input);

  for (size_t i = 0; i < num_expected; ++i) {
    FmtToken got = fmtlex_next();
    if (got.kind != expected[i]) {
      base_writef_stderr("\nindex %zd: got %s, wanted %s\n", i, fmtlex_token_kind_name(got.kind),
                         fmtlex_token_kind_name(expected[i]));
      return false;
    }
  }
  return true;
}

TEST(FmtLex, Basic) {
  FmtTokenKind expected[] = {
      FMTTOK_LITERAL,     //
      FMTTOK_LBRACE,      //
      FMTTOK_FIELD_NAME,  //
      FMTTOK_RBRACE,      //
      FMTTOK_LITERAL,     //
      FMTTOK_EOF,         //
  };
  EXPECT_TRUE(fmtlex_test("Hello {name}!", expected, COUNTOF(expected)));
}

TEST(FmtLex, FloatSpec) {
  FmtTokenKind expected[] = {
      FMTTOK_LITERAL,      //
      FMTTOK_LBRACE,       //
      FMTTOK_FIELD_NAME,   //
      FMTTOK_COLON,        //
      FMTTOK_FORMAT_SPEC,  //
      FMTTOK_RBRACE,       //
      FMTTOK_EOF,          //
  };
  EXPECT_TRUE(fmtlex_test("Value: {0:.2f}", expected, COUNTOF(expected)));
}

TEST(FmtLex, EscapedBraces) {
  FmtTokenKind expected[] = {
      FMTTOK_ESCAPED_BRACE,  //
      FMTTOK_LITERAL,        //
      FMTTOK_ESCAPED_BRACE,  //
      FMTTOK_LITERAL,        //
      FMTTOK_EOF,            //
  };
  EXPECT_TRUE(fmtlex_test("{{escaped}} braces", expected, COUNTOF(expected)));
}

TEST(FmtLex, AttrAndFormats) {
  FmtTokenKind expected[] = {
      FMTTOK_LBRACE,       //
      FMTTOK_FIELD_NAME,   //
      FMTTOK_DOT,          //
      FMTTOK_FIELD_NAME,   //
      FMTTOK_LBRACKET,     //
      FMTTOK_FIELD_NAME,   //
      FMTTOK_RBRACKET,     //
      FMTTOK_CONVERSION,   //
      FMTTOK_COLON,        //
      FMTTOK_FORMAT_SPEC,  //
      FMTTOK_RBRACE,       //
      FMTTOK_EOF,          //

  };
  EXPECT_TRUE(fmtlex_test("{obj.attr[key]!r:>10}", expected, COUNTOF(expected)));
}

TEST(FmtLex, DifferentSubs) {
  FmtTokenKind expected[] = {
      FMTTOK_LITERAL,      //
      FMTTOK_LBRACE,       //
      FMTTOK_FIELD_NAME,   //
      FMTTOK_RBRACE,       //
      FMTTOK_LITERAL,      //
      FMTTOK_LBRACE,       //
      FMTTOK_FIELD_NAME,   //
      FMTTOK_COLON,        //
      FMTTOK_FORMAT_SPEC,  //
      FMTTOK_RBRACE,       //
      FMTTOK_LITERAL,      //
      FMTTOK_EOF,          //

  };
  EXPECT_TRUE(fmtlex_test("Mix {0} and {name:^20s} together", expected, COUNTOF(expected)));
}
