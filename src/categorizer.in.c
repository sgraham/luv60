  TokenKind token;
  do {

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

  "alignof"   { token = TOK_ALIGNOF; break; }
  "alloc"     { token = TOK_ALLOC; break; }
  "and"       { token = TOK_AND; break; }
  "as"        { token = TOK_AS; break; }
  "bool"      { token = TOK_BOOL; break; }
  "break"     { token = TOK_BREAK; break; }
  "byte"      { token = TOK_BYTE; break; }
  "cast"      { token = TOK_CAST; break; }
  "check"     { token = TOK_CHECK; break; }
  "codept"    { token = TOK_CODEPOINT; break; }
  "const"     { token = TOK_CONST; break; }
  "c_char"    { token = TOK_CONST_CHAR; break; }
  "c_opaque"  { token = TOK_CONST_OPAQUE; break; }
  "continue"  { token = TOK_CONTINUE; break; }
  "def"       { token = TOK_DEF; break; }
  "del"       { token = TOK_DEL; break; }
  "double"    { token = TOK_DOUBLE; break; }
  "elif"      { token = TOK_ELIF; break; }
  "else"      { token = TOK_ELSE; break; }
  "f16"       { token = TOK_F16; break; }
  "f32"       { token = TOK_F32; break; }
  "f64"       { token = TOK_F64; break; }
  "false"     { token = TOK_FALSE; break; }
  "float"     { token = TOK_FLOAT; break; }
  "for"       { token = TOK_FOR; break; }
  "foreign"   { token = TOK_FOREIGN; break; }
  "global"    { token = TOK_GLOBAL; break; }
  "i16"       { token = TOK_I16; break; }
  "i32"       { token = TOK_I32; break; }
  "i64"       { token = TOK_I64; break; }
  "i8"        { token = TOK_I8; break; }
  "if"        { token = TOK_IF; break; }
  "import"    { token = TOK_IMPORT; break; }
  "in"        { token = TOK_IN; break; }
  "int"       { token = TOK_INT; break; }
  "len"       { token = TOK_LEN; break; }
  "nonlocal"  { token = TOK_NONLOCAL; break; }
  "not"       { token = TOK_NOT; break; }
  "null"      { token = TOK_NULL; break; }
  "offsetof"  { token = TOK_OFFSETOF; break; }
  "on"        { token = TOK_ON; break; }
  "opaque"    { token = TOK_OPAQUE; break; }
  "or"        { token = TOK_OR; break; }
  "pass"      { token = TOK_PASS; break; }
  "print"     { token = TOK_PRINT; break; }
  "range"     { token = TOK_RANGE; break; }
  "relocate"  { token = TOK_RELOCATE; break; }
  "return"    { token = TOK_RETURN; break; }
  "size_t"    { token = TOK_SIZE_T; break; }
  "sizeof"    { token = TOK_SIZEOF; break; }
  "str"       { token = TOK_STR; break; }
  "struct"    { token = TOK_STRUCT; break; }
  "true"      { token = TOK_TRUE; break; }
  "typedef"   { token = TOK_TYPEDEF; break; }
  "typeid"    { token = TOK_TYPEID; break; }
  "typeof"    { token = TOK_TYPEOF; break; }
  "u16"       { token = TOK_U16; break; }
  "u32"       { token = TOK_U32; break; }
  "u64"       { token = TOK_U64; break; }
  "u8"        { token = TOK_U8; break; }
  "uint"      { token = TOK_UINT; break; }
  "with"      { token = TOK_WITH; break; }
  "{"         { token = TOK_LBRACE; break; }
  "}"         { token = TOK_RBRACE; break; }
  "("         {
                token = TOK_LPAREN;
                break;
              }
  ")"         {
                token = TOK_RPAREN;
                break;
              }
  "["         { token = TOK_LSQUARE; break; }
  "]"         { token = TOK_RSQUARE; break; }
  "=="        { token = TOK_EQEQ; SKIP(); break; }
  "="         { token = TOK_EQ; break; }
  ":"         { token = TOK_COLON; break; }
  "."         { token = TOK_DOT; break; }
  ","         { token = TOK_COMMA; break; }
  "!="        { token = TOK_BANGEQ; SKIP(); break; }
  "<<="       { token = TOK_LSHIFTEQ; SKIP(); SKIP(); break; }
  "<<"        { token = TOK_LSHIFT; SKIP(); break; }
  ">>="       { token = TOK_RSHIFTEQ; SKIP(); SKIP(); break; }
  ">>"        { token = TOK_RSHIFT; SKIP(); break; }
  "<="        { token = TOK_LEQ; SKIP(); break; }
  ">="        { token = TOK_GEQ; SKIP(); break; }
  "<"         { token = TOK_LT; break; }
  ">"         { token = TOK_GT; break; }
  "+="        { token = TOK_PLUSEQ; SKIP(); break; }
  "-="        { token = TOK_MINUSEQ; SKIP(); break; }
  "*="        { token = TOK_STAREQ; SKIP(); break; }
  "/="        { token = TOK_SLASHEQ; SKIP(); break; }
  "%="        { token = TOK_PERCENTEQ; SKIP(); break; }
  "|="        { token = TOK_PIPEEQ; SKIP(); break; }
  "^="        { token = TOK_CARETEQ; SKIP(); break; }
  "&="        { token = TOK_AMPERSANDEQ; SKIP(); break; }
  "+"         { token = TOK_PLUS; break; }
  "-"         { token = TOK_MINUS; break; }
  "*"         { token = TOK_STAR; break; }
  "/"         { token = TOK_SLASH; break; }
  "%"         { token = TOK_PERCENT; break; }
  "|"         { token = TOK_PIPE; break; }
  "^"         { token = TOK_CARET; break; }
  "&"         { token = TOK_AMPERSAND; break; }

  // This makes sure the following indent adjusters don't match. The indexing
  // phase will point at the second newline on the next pass even though it
  // looks like it's being consumed here.
  "\n"[ ]*"\n" { token = TOK_NEWLINE; break; }
  "\n                                            "    { token = TOK_ERROR; break; }
  "\n                                        "    { NEWLINE_INDENT_ADJUST_AND_BREAK(40); break; }
  "\n                                    "    { NEWLINE_INDENT_ADJUST_AND_BREAK(36); break; }
  "\n                                "    { NEWLINE_INDENT_ADJUST_AND_BREAK(32); break; }
  "\n                            "    { NEWLINE_INDENT_ADJUST_AND_BREAK(28); break; }
  "\n                        "    { NEWLINE_INDENT_ADJUST_AND_BREAK(24); break; }
  "\n                    "    { NEWLINE_INDENT_ADJUST_AND_BREAK(20); break; }
  "\n                "    { NEWLINE_INDENT_ADJUST_AND_BREAK(16); break; }
  "\n            "    { NEWLINE_INDENT_ADJUST_AND_BREAK(12); break; }
  "\n        "    { NEWLINE_INDENT_ADJUST_AND_BREAK(8); break; }
  "\n    "    { NEWLINE_INDENT_ADJUST_AND_BREAK(4); break; }
  "\n"        { NEWLINE_INDENT_ADJUST_AND_BREAK(0); break; }

  "\""        { token = TOK_STRING_QUOTED; break; }
  decorator   { token = TOK_IDENT_DECORATOR; break; }
  dec         { token = TOK_INT_LITERAL; break; }
  hex         { token = TOK_INT_LITERAL; break; }
  oct         { token = TOK_INT_LITERAL; break; }
  bin         { token = TOK_INT_LITERAL; break; }
  varname     { token = TOK_IDENT_VAR; break; }
  typename    { token = TOK_IDENT_TYPE; break; }
  constname   { token = TOK_IDENT_CONST; break; }
  nul         {
                token = TOK_EOF;
                break;
              }
  [^]         { token = TOK_INVALID; break; }
  *           { token = TOK_INVALID; break; }
  */
  } while(0);

  EMIT(token);
  start_rel_offset = end_rel_offset;
