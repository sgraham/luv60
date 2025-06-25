#include "sqbe.h"

// Intermediate state so caller can be written against an object model, but
// currently just emits textual .ssa files. Eventually, hook into qbe's parse.c
// directly generating internal state.

const SqType sq_sbyte = {.kind = STK_BYTE, .sign = true, .size = 1};
const SqType sq_ubyte = {.kind = STK_BYTE, .sign = false, .size = 1};
const SqType sq_shalf = {.kind = STK_HALF, .sign = true, .size = 2};
const SqType sq_uhalf = {.kind = STK_HALF, .sign = false, .size = 2};
const SqType sq_word = {.kind = STK_WORD, .size = 4};
const SqType sq_long = {.kind = STK_LONG, .size = 8};
const SqType sq_single = {.kind = STK_SINGLE, .size = 4};
const SqType sq_double = {.kind = STK_DOUBLE, .size = 8};
const SqType sq_void = {.kind = STK_VOID};

void sq_append_def(struct SqProgram *prog, struct SqDef *def) {
  abort();
}

void sq_push_stmt(SqStatements *stmts, SqStatement* stmt) {
  abort();
}

void sq_push_instr(SqFunc* func, const SqValue* out, SqInstr instr, ...) {
  abort();
}

void sq_push_comment(SqFunc* func, const char* fmt, ...) {
  abort();
}

SqValue sq_constl(uint64_t l) {
  return (SqValue){
      .kind = SVK_CONST,
      .type = &sq_long,
      .lval = l,
  };
}

SqValue sq_constw(uint32_t w) {
  return (SqValue){
      .kind = SVK_CONST,
      .type = &sq_word,
      .wval = w,
  };
}

SqValue sq_consts(float s) {
  return (SqValue){
      .kind = SVK_CONST,
      .type = &sq_single,
      .sval = s,
  };
}

SqValue sq_constd(double d) {
  return (SqValue){
      .kind = SVK_CONST,
      .type = &sq_double,
      .dval = d,
  };
}
