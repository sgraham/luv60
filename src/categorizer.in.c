  /*!re2c
re2c:define:YYCTYPE = "unsigned char";
re2c:define:YYCURSOR = p;
re2c:define:YYMARKER = q;
re2c:yyfill:enable = 0;

nul = "\000";
varname = ( [_]*[a-z_][a-zA-Z0-9_]* );
typename = ( [_]*[A-Z] | [_]*[A-Z][A-Za-z0-9_]*[a-z][A-Za-z0-9_]* );
constname = [_]*[A-Z][A-Z0-9_]+;
slcomment = "#"[^\000\r\n]*;
intsuffix = ("u8" | "i8" | "u16" | "i16" | "u32" | "i32" | "u64" | "i64");
bin = '0b' [01_]+ intsuffix?;
oct = '0o' [0-7_]+ intsuffix?;
dec = [0-9_]+ intsuffix?;
hex = '0x' [0-9a-fA-F_]+ intsuffix?;
decorator = '@' [a-z_][a-zA-Z0-9_]*;

  "alignof"   { return TOK_ALIGNOF; }
  "alloc"     { return TOK_ALLOC; }
  "and"       { return TOK_AND; }
  "as"        { return TOK_AS; }
  "bool"      { return TOK_BOOL; }
  "break"     { return TOK_BREAK; }
  "byte"      { return TOK_BYTE; }
  "cast"      { return TOK_CAST; }
  "check"     { return TOK_CHECK; }
  "codept"    { return TOK_CODEPOINT; }
  "const"     { return TOK_CONST; }
  "c_char"    { return TOK_CONST_CHAR; }
  "c_opaque"  { return TOK_CONST_OPAQUE; }
  "continue"  { return TOK_CONTINUE; }
  "def"       { return TOK_DEF; }
  "del"       { return TOK_DEL; }
  "double"    { return TOK_DOUBLE; }
  "elif"      { return TOK_ELIF; }
  "else"      { return TOK_ELSE; }
  "f16"       { return TOK_F16; }
  "f32"       { return TOK_F32; }
  "f64"       { return TOK_F64; }
  "false"     { return TOK_FALSE; }
  "float"     { return TOK_FLOAT; }
  "for"       { return TOK_FOR; }
  "foreign"   { return TOK_FOREIGN; }
  "global"    { return TOK_GLOBAL; }
  "i16"       { return TOK_I16; }
  "i32"       { return TOK_I32; }
  "i64"       { return TOK_I64; }
  "i8"        { return TOK_I8; }
  "if"        { return TOK_IF; }
  "import"    { return TOK_IMPORT; }
  "in"        { return TOK_IN; }
  "int"       { return TOK_INT; }
  "len"       { return TOK_LEN; }
  "nonlocal"  { return TOK_NONLOCAL; }
  "not"       { return TOK_NOT; }
  "null"      { return TOK_NULL; }
  "offsetof"  { return TOK_OFFSETOF; }
  "on"        { return TOK_ON; }
  "opaque"    { return TOK_OPAQUE; }
  "or"        { return TOK_OR; }
  "pass"      { return TOK_PASS; }
  "print"     { return TOK_PRINT; }
  "range"     { return TOK_RANGE; }
  "relocate"  { return TOK_RELOCATE; }
  "return"    { return TOK_RETURN; }
  "size_t"    { return TOK_SIZE_T; }
  "sizeof"    { return TOK_SIZEOF; }
  "str"       { return TOK_STR; }
  "struct"    { return TOK_STRUCT; }
  "true"      { return TOK_TRUE; }
  "typedef"   { return TOK_TYPEDEF; }
  "typeid"    { return TOK_TYPEID; }
  "typeof"    { return TOK_TYPEOF; }
  "u16"       { return TOK_U16; }
  "u32"       { return TOK_U32; }
  "u64"       { return TOK_U64; }
  "u8"        { return TOK_U8; }
  "uint"      { return TOK_UINT; }
  "with"      { return TOK_WITH; }
  "{"         { ++token_continuation_paren_level; return TOK_LBRACE; }
  "}"         { --token_continuation_paren_level; return TOK_RBRACE; }
  "("         { ++token_continuation_paren_level; return TOK_LPAREN; }
  ")"         { --token_continuation_paren_level; return TOK_RPAREN; }
  "["         { ++token_continuation_paren_level; return TOK_LSQUARE; }
  "]"         { --token_continuation_paren_level; return TOK_RSQUARE; }
  "=="        { return TOK_EQEQ; }
  "!="        { return TOK_BANGEQ; }
  "<<"        { return TOK_LSHIFT; }
  ">>"        { return TOK_RSHIFT; }
  "<="        { return TOK_LEQ; }
  ">="        { return TOK_GEQ; }
  "="         { return TOK_EQ; }
  ":"         { return TOK_COLON; }
  "."         { return TOK_DOT; }
  ","         { return TOK_COMMA; }
  "<"         { return TOK_LT; }
  ">"         { return TOK_GT; }
  "+"         { return TOK_PLUS; }
  "-"         { return TOK_MINUS; }
  "*"         { return TOK_STAR; }
  "/"         { return TOK_SLASH; }
  "%"         { return TOK_PERCENT; }
  "|"         { return TOK_PIPE; }
  "^"         { return TOK_CARET; }
  "&"         { return TOK_AMPERSAND; }

  // These makes sure the following indent adjusters don't match on blank lines.
  // Because the indexer determines where we look next, the next token will
  // start at the ending newline, even though it looks like it's being
  // "consumed" here.
  "\n"[ ]*"#.*\n" { return token_continuation_paren_level ? TOK_NL : TOK_NEWLINE_BLANK; }
  "\n"[ ]*"\n" { return token_continuation_paren_level ? TOK_NL : TOK_NEWLINE_BLANK; }

  "\n                                            "    { return TOK_ERROR; }
  "\n                                        "    { NEWLINE_INDENT_ADJUST(40); }
  "\n                                    "    { NEWLINE_INDENT_ADJUST(36); }
  "\n                                "    { NEWLINE_INDENT_ADJUST(32); }
  "\n                            "    { NEWLINE_INDENT_ADJUST(28); }
  "\n                        "    { NEWLINE_INDENT_ADJUST(24); }
  "\n                    "    { NEWLINE_INDENT_ADJUST(20); }
  "\n                "    { NEWLINE_INDENT_ADJUST(16); }
  "\n            "    { NEWLINE_INDENT_ADJUST(12); }
  "\n        "    { NEWLINE_INDENT_ADJUST(8); }
  "\n    "    { NEWLINE_INDENT_ADJUST(4); }
  "\n"        { NEWLINE_INDENT_ADJUST(0); }

  "\""        { return TOK_STRING_QUOTED; }
  decorator   { return TOK_IDENT_DECORATOR; }
  dec         { return TOK_INT_LITERAL; }
  hex         { return TOK_INT_LITERAL; }
  oct         { return TOK_INT_LITERAL; }
  bin         { return TOK_INT_LITERAL; }
  varname     { return TOK_IDENT_VAR; }
  typename    { return TOK_IDENT_TYPE; }
  constname   { return TOK_IDENT_CONST; }
  nul         { return TOK_EOF; }
  [^]         { return TOK_INVALID; }
  *           { return TOK_INVALID; }
  */

#if 0
  "<<="       { token = TOK_LSHIFTEQ; SKIP(); SKIP(); break; }
  ">>="       { token = TOK_RSHIFTEQ; SKIP(); SKIP(); break; }
  "+="        { token = TOK_PLUSEQ; SKIP(); break; }
  "-="        { token = TOK_MINUSEQ; SKIP(); break; }
  "*="        { token = TOK_STAREQ; SKIP(); break; }
  "/="        { token = TOK_SLASHEQ; SKIP(); break; }
  "%="        { token = TOK_PERCENTEQ; SKIP(); break; }
  "|="        { token = TOK_PIPEEQ; SKIP(); break; }
  "^="        { token = TOK_CARETEQ; SKIP(); break; }
  "&="        { token = TOK_AMPERSANDEQ; SKIP(); break; }
#endif
