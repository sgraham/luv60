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
  Arena* arena = arena_create(KiB(128), KiB(128));

  size_t len = strlen(buf) + 1;
  size_t alloc_size = ALIGN_UP(len + 64, base_page_size());
  unsigned char* padded_copy = arena_push(arena, alloc_size, base_page_size());
  memcpy(padded_copy, buf, len);

  uint32_t* token_offsets = (uint32_t*)arena_push(arena, alloc_size * sizeof(uint32_t), 8);
  lex_indexer(padded_copy, alloc_size, token_offsets);
  token_init((const unsigned char*)padded_copy);

  bool ok = true;
  for (size_t i = 0; i < num_exp; ++i) {
    TokenKind kind = token_categorize(token_offsets[i]);

    if (kind != exp[i].kind) {
      base_writef_stderr("\nindex %zd: got %s, wanted %s\n", i, token_names[kind],
                         token_names[exp[i].kind]);
      ok = false;
      goto done;
    }
    if (token_offsets[i] != exp[i].offset) {
      base_writef_stderr("\nindex %zd was %s, but got off %d, wanted %d\n", i, token_names[kind],
                         token_offsets[i], exp[i].offset);
      ok = false;
      goto done;
    }
  }

done:
  arena_destroy(arena);
  return ok;
}

TEST(Lex, Basic) {
  KindAndOffset expected[] = {
      {TOK_CONST, 0},              //
      {TOK_IDENT_VAR, 6},          //
      {TOK_EQ, 13},                //
      {TOK_IDENT_VAR, 15},         //
      {TOK_NEWLINE_INDENT_0, 19},  //
      {TOK_EOF, 20},               //
  };
  EXPECT_TRUE(lex_test("const kStuff = blah\n", expected, COUNTOF(expected)));
}

TEST(Lex, Punctuation) {
  KindAndOffset expected[] = {
      {TOK_LBRACE, 0},   //
      {TOK_RBRACE, 2},   //
      {TOK_LSQUARE, 4},  //
      {TOK_RSQUARE, 6},  //
      {TOK_DOT, 8},      //
      {TOK_LEQ, 10},     //
      {TOK_GEQ, 13},     //
      {TOK_LT, 16},      //
      {TOK_GT, 18},      //
      {TOK_PLUS, 20},    //
      {TOK_MINUS, 22},   //
      {TOK_STAR, 24},    //
      {TOK_SLASH, 26},   //
      {TOK_LSHIFT, 28},  //
      {TOK_RSHIFT, 31},  //
      {TOK_EOF, 33},     //
  };
  EXPECT_TRUE(lex_test("{ } [ ] . <= >= < > + - * / << >>", expected, COUNTOF(expected)));
}

TEST(Lex, Keywords) {
  KindAndOffset expected[] = {
      {TOK_CONST, 0},              //
      {TOK_DEF, 6},                //
      {TOK_ELIF, 10},              //
      {TOK_ELSE, 15},              //
      {TOK_FALSE, 20},             //
      {TOK_FOR, 26},               //
      {TOK_IF, 30},                //
      {TOK_STRUCT, 33},            //
      {TOK_NEWLINE_INDENT_0, 39},  //
      {TOK_TRUE, 40},              //
      {TOK_EOF, 44},               //
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
      {TOK_NEWLINE_INDENT_0, 8},    //
      {TOK_EOF, 9},        //
  };
  const char input[] = "def a():\n";
  EXPECT_TRUE(lex_test(input, expected, COUNTOF(expected)));
}

TEST(Lex, IndentDedent) {
  KindAndOffset expected[] = {
      {TOK_DEF, 0},                 //
      {TOK_IDENT_VAR, 4},           //
      {TOK_LPAREN, 5},              //
      {TOK_RPAREN, 6},              //
      {TOK_COLON, 7},               //
      {TOK_NEWLINE_INDENT_4, 8},    //
      {TOK_IF, 13},                 //
      {TOK_IDENT_VAR, 16},          //
      {TOK_COLON, 17},              //
      {TOK_NEWLINE_INDENT_8, 18},   //
      {TOK_PRINT, 27},              //
      {TOK_IDENT_VAR, 33},          //
      {TOK_NEWLINE_INDENT_4, 35},   //
      {TOK_ELIF, 40},               //
      {TOK_COLON, 44},              //
      {TOK_NEWLINE_INDENT_8, 45},   //
      {TOK_IDENT_VAR, 54},          //
      {TOK_NEWLINE_INDENT_8, 59},   //
      {TOK_IF, 68},                 //
      {TOK_IDENT_VAR, 71},          //
      {TOK_COLON, 72},              //
      {TOK_NEWLINE_INDENT_12, 73},  //
      {TOK_PASS, 86},               //
      {TOK_NEWLINE_INDENT_0, 90},   //
      {TOK_EOF, 91},                //
  };
  const char input[] =
      "def a():\n"
      "    if x:\n"
      "        print hi\n"
      "    elif:\n"
      "        stuff\n"
      "        if y:\n"
      "            pass\n";
  EXPECT_TRUE(lex_test(input, expected, COUNTOF(expected)));
}

TEST(Lex, SuccessiveBlocksDedent) {
  KindAndOffset expected[] = {
      {TOK_STRUCT, 0},             //
      {TOK_IDENT_TYPE, 7},         //
      {TOK_COLON, 8},              //
      {TOK_NEWLINE_INDENT_4, 9},   //
      {TOK_INT, 14},               //
      {TOK_IDENT_VAR, 18},         //
      {TOK_NEWLINE_BLANK, 19},     //
      {TOK_NEWLINE_INDENT_0, 20},  //
      {TOK_STRUCT, 21},            //
      {TOK_IDENT_TYPE, 28},        //
      {TOK_COLON, 29},             //
      {TOK_NEWLINE_INDENT_4, 30},  //
      {TOK_INT, 35},               //
      {TOK_IDENT_VAR, 39},         //
      {TOK_NEWLINE_INDENT_0, 40},  //
      {TOK_EOF, 41},               //
  };
  const char input[] =
      "struct A:\n"
      "    int a\n"
      "\n"
      "struct B:\n"
      "    int b\n";
  EXPECT_TRUE(lex_test(input, expected, COUNTOF(expected)));
}

TEST(Lex, SuccessiveBlocksStillIndented) {
  KindAndOffset expected[] = {
      {TOK_DEF, 0},                //
      {TOK_IDENT_VAR, 4},          //
      {TOK_LPAREN, 5},             //
      {TOK_RPAREN, 6},             //
      {TOK_COLON, 7},              //
      {TOK_NEWLINE_INDENT_4, 8},   //
      {TOK_INT, 13},               //
      {TOK_IDENT_VAR, 17},         //
      {TOK_NEWLINE_BLANK, 18},     //
      {TOK_NEWLINE_INDENT_4, 19},  //
      {TOK_DEF, 24},               //
      {TOK_IDENT_VAR, 28},         //
      {TOK_LPAREN, 29},            //
      {TOK_RPAREN, 30},            //
      {TOK_COLON, 31},             //
      {TOK_NEWLINE_INDENT_8, 32},  //
      {TOK_RETURN, 41},            //
      {TOK_INT_LITERAL, 48},       //
      {TOK_NEWLINE_INDENT_0, 49},  //
      {TOK_EOF, 50},               //
  };
  const char input[] =
      "def a():\n"
      "    int x\n"
      "\n"
      "    def b():\n"
      "        return 2\n";
  EXPECT_TRUE(lex_test(input, expected, COUNTOF(expected)));
}

#if 0  // New lexer doesn't do raw strings yet
TEST(Lex, RawString) {
  KindAndOffset expected[] = {
      {TOK_CONST, LOC(1, 1), SV_CONST("const")},                                //
      {TOK_IDENT_CONST, LOC(1, 7), SV_CONST("STUFF")},                          //
      {TOK_EQ, LOC(1, 13), SV_CONST("=")},                                      //
      {TOK_STRING_RAW, LOC(1, 15),                                              //
       SV_CONST("r'''\n  this is 'some' ''stuff''\n  and another line\n'''")},  //
      {TOK_HASH, LOC(4, 5), SV_CONST("#")},                                     //
      {TOK_NEWLINE, LOC(4, 6), SV_CONST("\n")},                                 //
      {TOK_IDENT_VAR, LOC(5, 1), SV_CONST("x")},                                //
      {TOK_IDENT_VAR, LOC(5, 3), SV_CONST("y")},                                //
      {TOK_NEWLINE, LOC(5, 4), SV_CONST("\n")},                                 //
      {TOK_EOF, LOC(6, 1), {"", 1}},                                            //
  };
  const char input[] =
      "const STUFF = r'''\n"
      "  this is 'some' ''stuff''\n"
      "  and another line\n"
      "''' #\n"
      "x y\n";
  EXPECT_TRUE(lex_test(input, expected, COUNTOF(expected)));
}
#endif

TEST(Lex, InterpString) {
#if 0
  KindAndOffset expected[] = {
      {TOK_STRING_INTERP, 0},   //
      {TOK_IDENT_VAR, 14},      //
      {TOK_STRING_INTERP, 16},  //
      {TOK_IDENT_VAR, 23},      //
      {TOK_STRING_QUOTED, 27},  //
      {TOK_EOF, 34},            //
  };
#else
  KindAndOffset expected[] = {
      {TOK_STRING_QUOTED, 0},   //
      {TOK_EOF, 35},            //
  };
#endif
  // Old lexer did this at lex-time, but now deferred to parser to interpret.
  // TODO: maybe move back to lexer I think.
  const char input[] = "\"string with \\(a) sub \\(var) value\"";
  EXPECT_TRUE(lex_test(input, expected, COUNTOF(expected)));
}

TEST(Lex, NestedIndent) {
  KindAndOffset expected[] = {
      {TOK_DEF, 0},                 //
      {TOK_IDENT_VAR, 4},           //
      {TOK_LPAREN, 8},              //
      {TOK_RPAREN, 9},              //
      {TOK_COLON, 10},              //
      {TOK_NEWLINE_INDENT_4, 11},   //
      {TOK_IF, 16},                 //
      {TOK_TRUE, 19},               //
      {TOK_COLON, 23},              //
      {TOK_NEWLINE_INDENT_8, 24},   //
      {TOK_IF, 33},                 //
      {TOK_TRUE, 36},               //
      {TOK_COLON, 40},              //
      {TOK_NEWLINE_INDENT_12, 41},  //
      {TOK_RETURN, 54},             //
      {TOK_NEWLINE_BLANK, 60},      //
      {TOK_NEWLINE_INDENT_4, 61},   //
      {TOK_IF, 66},                 //
      {TOK_FALSE, 69},              //
      {TOK_COLON, 74},              //
      {TOK_NEWLINE_INDENT_8, 75},   //
      {TOK_PRINT, 84},              //
      {TOK_INT_LITERAL, 90},        //
      {TOK_NEWLINE_BLANK, 91},      //
      {TOK_NEWLINE_INDENT_4, 92},   //
      {TOK_PRINT, 97},              //
      {TOK_INT_LITERAL, 103},       //
      {TOK_NEWLINE_INDENT_0, 104},  //
      {TOK_EOF, 105},               //
  };
  const char input[] =
      "def func():\n"
      "    if true:\n"
      "        if true:\n"
      "            return\n"
      "\n"
      "    if false:\n"
      "        print 1\n"
      "\n"
      "    print 2\n";
  EXPECT_TRUE(lex_test(input, expected, COUNTOF(expected)));
}

TEST(Lex, Decorator) {
  KindAndOffset expected[] = {
      {TOK_IDENT_DECORATOR, 0},    //
      {TOK_NEWLINE_INDENT_0, 6},   //
      {TOK_DEF, 7},                //
      {TOK_IDENT_VAR, 11},         //
      {TOK_LPAREN, 15},            //
      {TOK_RPAREN, 16},            //
      {TOK_COLON, 17},             //
      {TOK_NEWLINE_INDENT_4, 18},  //
      {TOK_PASS, 23},              //
      {TOK_NEWLINE_INDENT_0, 27},  //
      {TOK_EOF, 28},               //
  };
  const char input[] =
      "@stuff\n"
      "def func():\n"
      "    pass\n";
  EXPECT_TRUE(lex_test(input, expected, COUNTOF(expected)));
}

TEST(Lex, Continuation) {
  KindAndOffset expected[] = {
      {TOK_DEF, 0},                //
      {TOK_IDENT_VAR, 4},          //
      {TOK_LPAREN, 8},             //
      {TOK_RPAREN, 9},             //
      {TOK_COLON, 10},             //
      {TOK_NEWLINE_INDENT_4, 11},  //
      {TOK_IDENT_VAR, 16},         //
      {TOK_EQ, 18},                //
      {TOK_LSQUARE, 20},           //
      {TOK_NL, 21},                //
      {TOK_NL, 22},                //
      {TOK_INT_LITERAL, 25},       //
      {TOK_NL, 26},                //
      {TOK_NL, 27},                //
      {TOK_RSQUARE, 28},           //
      {TOK_NEWLINE_INDENT_0, 29},  //
      {TOK_EOF, 30},               //
  };
  const char input[] =
      "def func():\n"
      "    x = [\n"
      "\n"
      "  3\n"
      "\n"
      "]\n";
  EXPECT_TRUE(lex_test(input, expected, COUNTOF(expected)));
}

TEST(Lex, ChunkCrossDoubleTokensLeftShift) {
  KindAndOffset expected[] = {
      {TOK_LSHIFT, 63},  //
      {TOK_EOF, 65},     //
  };
  const char input[] =
      "                                                               <"
      "<";
  EXPECT_TRUE(lex_test(input, expected, COUNTOF(expected)));
}

TEST(Lex, ChunkCrossDoubleTokensRightShift) {
  KindAndOffset expected[] = {
      {TOK_RSHIFT, 63},  //
      {TOK_EOF, 65},     //
  };
  const char input[] =
      "                                                               >"
      ">";
  EXPECT_TRUE(lex_test(input, expected, COUNTOF(expected)));
}

TEST(Lex, ChunkCrossDoubleTokensEquals) {
  KindAndOffset expected[] = {
      {TOK_EQEQ, 63},  //
      {TOK_EOF, 65},   //
  };
  const char input[] =
      "                                                               ="
      "=";
  EXPECT_TRUE(lex_test(input, expected, COUNTOF(expected)));
}

TEST(Lex, ChunkCrossDoubleTokensNotEquals) {
  KindAndOffset expected[] = {
      {TOK_BANGEQ, 63},  //
      {TOK_EOF, 65},     //
  };
  const char input[] =
      "                                                               !"
      "=";
  EXPECT_TRUE(lex_test(input, expected, COUNTOF(expected)));
}

TEST(Lex, ChunkCrossDoubleTokensSomethingElseAfter) {
  KindAndOffset expected[] = {
      {TOK_EQ, 63},         //
      {TOK_IDENT_VAR, 64},  //
      {TOK_EOF, 65},        //
  };
  const char input[] =
      "                                                               ="
      "a";
  EXPECT_TRUE(lex_test(input, expected, COUNTOF(expected)));
}
