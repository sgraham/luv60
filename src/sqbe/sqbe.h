#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum SqTypeKind {
  STK_VOID = 'v',
  STK_BYTE = 'b',
  STK_HALF = 'h',
  STK_WORD = 'w',
  STK_LONG = 'l',
  STK_SINGLE = 's',
  STK_DOUBLE = 'd',
  STK_AGGREGATE = 'a',
  STK_UNION = 'u',
} SqTypeKind;

typedef struct SqType {
  SqTypeKind kind;
  bool sign;  // only used for byte and half
  size_t size;

  // For aggregates only:
  const char* name;
  SqField fields;
} SqType;

typedef struct SqField {
  SqType* type;
  size_t count;
  SqField* next;
} SqField;

extern SqType sq_sbyte;
extern SqType sq_ubyte;
extern SqType sq_shalf;
extern SqType sq_uhalf;
extern SqType sq_word;
extern SqType sq_long;
extern SqType sq_single;
extern SqType sq_double;
extern SqType sq_void;

typedef enum SqValueKind {
  SVK_CONST,
  SVK_GLOBAL,
  SVK_LABEL,
  SVK_TEMPORARY,
  SVK_VARIADIC,
} SqValueKind;

typedef struct SqValue {
  SqValueKind kind;
  bool threadlocal;
  SqType* type;
  union {
    const char* name;
    uint32_t wval;
    uint64_t lval;
    float sval;
    float dval;
  };
} SqValue;

typedef enum SqInstr {
  I_ADD,
  I_ALLOC16,
  I_ALLOC4,
  I_ALLOC8,
  I_AND,
  I_BLIT,
  I_CALL,
  I_CAST,
  I_CEQD,
  I_CEQL,
  I_CEQS,
  I_CEQW,
  I_CGED,
  I_CGES,
  I_CGTD,
  I_CGTS,
  I_CLED,
  I_CLES,
  I_CLTD,
  I_CLTS,
  I_CNED,
  I_CNEL,
  I_CNES,
  I_CNEW,
  I_COD,
  I_COPY,
  I_COS,
  I_CSGEL,
  I_CSGEW,
  I_CSGTL,
  I_CSGTW,
  I_CSLEL,
  I_CSLEW,
  I_CSLTL,
  I_CSLTW,
  I_CUGEL,
  I_CUGEW,
  I_CUGTL,
  I_CUGTW,
  I_CULEL,
  I_CULEW,
  I_CULTL,
  I_CULTW,
  I_CUOD,
  I_CUOS,
  I_DBGLOC,
  I_DIV,
  I_DTOSI,
  I_DTOUI,
  I_EXTS,
  I_EXTSB,
  I_EXTSH,
  I_EXTSW,
  I_EXTUB,
  I_EXTUH,
  I_EXTUW,
  I_HLT,
  I_JMP,
  I_JNZ,
  I_LOADD,
  I_LOADL,
  I_LOADS,
  I_LOADSB,
  I_LOADSH,
  I_LOADSW,
  I_LOADUB,
  I_LOADUH,
  I_LOADUW,
  I_MUL,
  I_OR,
  I_REM,
  I_RET,
  I_NEG,
  I_SAR,
  I_SHL,
  I_SHR,
  I_SLTOF,
  I_STOREB,
  I_STORED,
  I_STOREH,
  I_STOREL,
  I_STORES,
  I_STOREW,
  I_STOSI,
  I_STOUI,
  I_SUB,
  I_SWTOF,
  I_TRUNCD,
  I_UDIV,
  I_ULTOF,
  I_UREM,
  I_UWTOF,
  I_VAARG,
  I_VASTART,
  I_XOR,

  SqInstrCount
} SqInstr;

extern const char* ir_instr_names[SqInstrCount];

typedef enum SqStatementKind {
  SSK_COMMENT,
  SSK_INSTR,
  SSK_LABEL,
} SqStatementType;

typedef struct SqArguments {
  SqValue value;
  SqArguments* next;
} SqArguments;

typedef struct SqStatement {
  SqStatementKind kind;
  union {
    char* comment;
    struct {
      SqInstr instr;
      SqValue* out;
      SqArguments* args;
    };
    char* label;
  };
} SqStatement;

typedef struct SqFuncParam {
  const char* name;
  SqTyp* type;
  SqFuncParam* next;
} SqFuncParam;

typedef struct SqStatements {
  size_t line;
  size_t size;
  SqStatement* stmts;
} SqStatements;

typedef struct SqFunc {
  const SqType* returns;
  SqFuncParam* params;
  bool variadic;
  SqStatements prelude;
  SqStatements body;
} SqFunc;

typedef enum SqDataType {
  IDT_ZEROED,
  IDT_VALUE,
  IDT_STRING,
  IDT_SYMOFFS,
} SqDataType;

typedef struct SqDataItem {
  SqDataType type;
  union {
    SqValue value;
    size_t zeroed;
    struct {
      char* str;
      size_t size;
    };
    struct {
      const char* sym;
      int64_t offset;
    };
  };
  SqDataItem* next;
} SqDataItem;

typedef struct SqData {
  size_t align;
  char* section;
  char* secflags;
  bool threadlocal;
  SqDataItem items;
} SqData;

typedef enum SqDefKind {
  SDK_TYPE,
  SDK_FUNC,
  SDK_DATA,
} SqDefKind;

typedef struct SqDef {
  const char* name;
  int file;
  SqDefKind kind;
  bool exported;
  union {
    SqFunc func;
    SqType type;
    SqData data;
  };
} SqDef;

typedef struct SqProgram {
  SqDef* defs;
  SqDef** next;
} SqProgram;

void sq_append_def(struct SqProgram *prog, struct SqDef *def);

void sq_push_stmt(SqStatements *stmts, SqStatement* stmt);

// Appends to the body of func via push_stmt.
void sq_push_instr(SqFunc* func, const SqValue* out, SqInstr instr, ...);
void sq_push_comment(SqFunc* func, const char* fmt, ...);

SqValue sq_constl(uint64_t l);
SqValue sq_constw(uint32_t w);
SqValue sq_consts(float s);
SqValue sq_constd(double d);

