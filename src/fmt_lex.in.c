#include "luv60.h"

typedef enum { STATE_LITERAL, STATE_IN_BRACES, STATE_IN_FORMAT_SPEC } ScannerState;

typedef struct {
  const char* input;
  const char* cursor;
  const char* marker;
  const char* ctxmarker;
  ScannerState state;
  int brace_depth;
} Scanner;

static Scanner scanner_;

void fmtlex_start(const char* input) {
  scanner_.input = input;
  scanner_.cursor = input;
  scanner_.marker = input;
  scanner_.ctxmarker = input;
  scanner_.state = STATE_LITERAL;
  scanner_.brace_depth = 0;
}

static FmtToken scan_literal(void) {
  const char* YYCURSOR = scanner_.cursor;
  //const char* YYMARKER = scanner_.marker;
  const char* start = YYCURSOR;

  /*!re2c
      re2c:define:YYCTYPE = char;
      re2c:yyfill:enable = 0;

      end = "\x00";

      "{{"  {
          scanner_.cursor = YYCURSOR;
          return (FmtToken){FMTTOK_ESCAPED_BRACE, (StrView){start, YYCURSOR - start}};
      }
      "}}"  {
          scanner_.cursor = YYCURSOR;
          return (FmtToken){FMTTOK_ESCAPED_BRACE, (StrView){start, YYCURSOR - start}};
      }
      "{"   {
          scanner_.cursor = YYCURSOR;
          scanner_.state = STATE_IN_BRACES;
          scanner_.brace_depth = 1;
          return (FmtToken){FMTTOK_LBRACE, (StrView){start, 1}};
      }
      [^\x00{}]+ {
          scanner_.cursor = YYCURSOR;
          return (FmtToken){FMTTOK_LITERAL, (StrView){start, YYCURSOR - start}};
      }
      end   {
          scanner_.cursor = YYCURSOR;
          return (FmtToken){FMTTOK_EOF, (StrView){start, 0}};
      }
      *     {
          scanner_.cursor = YYCURSOR;
          return (FmtToken){FMTTOK_ERROR, (StrView){start, 1}};
      }
  */
}

FmtToken scan_in_braces(void) {
  const char* YYCURSOR = scanner_.cursor;
  //const char* YYMARKER = scanner_.marker;
  const char* start = YYCURSOR;

  /*!re2c
      digit = [0-9];
      alpha = [a-zA-Z_];
      alnum = [a-zA-Z0-9_];

      "}"   {
          scanner_.cursor = YYCURSOR;
          scanner_.state = STATE_LITERAL;
          scanner_.brace_depth = 0;
          return (FmtToken){FMTTOK_RBRACE, (StrView){start, 1}};
      }
      ":"   {
          scanner_.cursor = YYCURSOR;
          scanner_.state = STATE_IN_FORMAT_SPEC;
          return (FmtToken){FMTTOK_COLON, (StrView){start, 1}};
      }
      "!"[rsa] {
          scanner_.cursor = YYCURSOR;
          return (FmtToken){FMTTOK_CONVERSION, (StrView){start, YYCURSOR - start}};
      }
      "."   {
          scanner_.cursor = YYCURSOR;
          return (FmtToken){FMTTOK_DOT, (StrView){start, 1}};
      }
      "["   {
          scanner_.cursor = YYCURSOR;
          return (FmtToken){FMTTOK_LBRACKET, (StrView){start, 1}};
      }
      "]"   {
          scanner_.cursor = YYCURSOR;
          return (FmtToken){FMTTOK_RBRACKET, (StrView){start, 1}};
      }
      digit+ | alpha alnum* {
          scanner_.cursor = YYCURSOR;
          return (FmtToken){FMTTOK_FIELD_NAME, (StrView){start, YYCURSOR - start}};
      }
      end   {
          scanner_.cursor = YYCURSOR;
          return (FmtToken){FMTTOK_ERROR, (StrView){start, 0}};
      }
      *     {
          scanner_.cursor = YYCURSOR;
          return (FmtToken){FMTTOK_ERROR, (StrView){start, 1}};
      }
  */
}

FmtToken scan_format_spec(void) {
  const char* YYCURSOR = scanner_.cursor;
  //const char* YYMARKER = scanner_.marker;
  const char* start = YYCURSOR;

  /*!re2c
      "}"   {
          scanner_.cursor = YYCURSOR;
          scanner_.state = STATE_LITERAL;
          scanner_.brace_depth = 0;
          return (FmtToken){FMTTOK_RBRACE, (StrView){start, 1}};
      }
      "{"   {
          scanner_.cursor = YYCURSOR;
          scanner_.state = STATE_IN_BRACES;
          scanner_.brace_depth++;
          return (FmtToken){FMTTOK_LBRACE, (StrView){start, 1}};
      }
      [^\x00{}]+ {
          scanner_.cursor = YYCURSOR;
          return (FmtToken){FMTTOK_FORMAT_SPEC, (StrView){start, YYCURSOR - start}};
      }
      end   {
          scanner_.cursor = YYCURSOR;
          return (FmtToken){FMTTOK_ERROR, (StrView){start, 0}};
      }
      *     {
          scanner_.cursor = YYCURSOR;
          return (FmtToken){FMTTOK_ERROR, (StrView){start, 1}};
      }
  */
}

FmtToken fmtlex_next(void) {
  switch (scanner_.state) {
    case STATE_LITERAL:
      return scan_literal();
    case STATE_IN_BRACES:
      return scan_in_braces();
    case STATE_IN_FORMAT_SPEC:
      return scan_format_spec();
    default:
      return (FmtToken){FMTTOK_ERROR, (StrView){scanner_.cursor, 0}};
  }
}
