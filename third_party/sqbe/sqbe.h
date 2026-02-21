// License information at the end of the file.
//
// Do this:
//     #define SQBE_IMPLEMENTATION
// before you include this file in *one* C/C++ file to create the implementation.

#ifndef SQBE_H_INCLUDED_
#define SQBE_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define SQ_OPAQUE_STRUCT_DEF(x) \
  typedef struct x {            \
    uint32_t u;                 \
  } x

SQ_OPAQUE_STRUCT_DEF(SqLinkage);
SQ_OPAQUE_STRUCT_DEF(SqType);
SQ_OPAQUE_STRUCT_DEF(SqBlock);
SQ_OPAQUE_STRUCT_DEF(SqRef);
SQ_OPAQUE_STRUCT_DEF(SqSymbol);
SQ_OPAQUE_STRUCT_DEF(SqItemCtx);

#undef SQ_OPAQUE_STRUCT_DEF

typedef enum SqTarget {
  SQ_TARGET_DEFAULT,      //
  SQ_TARGET_AMD64_APPLE,  //
  SQ_TARGET_AMD64_SYSV,   //
  SQ_TARGET_AMD64_WIN,    //
  SQ_TARGET_ARM64,        //
  SQ_TARGET_ARM64_APPLE,  //
  SQ_TARGET_RV64,         //
} SqTarget;

typedef enum SqFormat {
  SQ_FORMAT_TEXT_S,     // Default textual .s to be assembled by system as
  SQ_FORMAT_OBJ_MACHO,  // macOS Mach-O .o
} SqFormat;

typedef int (*SqOutputFn)(const char* fmt, va_list ap);

typedef struct SqConfiguration {
  // SQ_TARGET_DEFAULT for compiling for the current (host) platform, otherwise
  // specify the target to cross-compile to.
  SqTarget target;

  SqFormat format;

  // Where the final assembler is written to.
  FILE* output;

  // debug_flags string can contain the following characters to cause QBE to output
  // information to stderr while compiling. NOTE: no final assembly will be emitted
  // if string other than "" is specified.
  // - P: parsing
  // - M: memory optimization
  // - N: ssa construction
  // - C: copy elimination
  // - F: constant folding
  // - K: if-conversion
  // - A: abi lowering
  // - I: instruction selection
  // - L: liveness
  // - S: spilling
  // - R: reg. allocation
  // - T: types
  const char* debug_flags;

  // TODO: document, mostly the defaults should be fine though.
  unsigned int max_blocks_per_function;
  unsigned int max_linkage_definitions;
  unsigned int max_compiler_function_reserve;
  unsigned int function_commit_chunk_size;
  unsigned int max_compiler_global_reserve;
  unsigned int global_commit_chunk_size;

  // Customizable output, all error messages, etc. will be vectored through this
  // function. If NULL, goes to stdout.
  SqOutputFn output_function;
} SqConfiguration;

#define SQ_CONFIGURATION_DEFAULT                                \
  ((SqConfiguration){.target = SQ_TARGET_DEFAULT,               \
                     .format = SQ_FORMAT_TEXT_S,                \
                     .output = stdout,                          \
                     .debug_flags = "",                         \
                     .max_blocks_per_function = 2048,           \
                     .max_linkage_definitions = 256,            \
                     .max_compiler_function_reserve = 64 << 20, \
                     .function_commit_chunk_size = 64 << 10,    \
                     .max_compiler_global_reserve = 64 << 20,   \
                     .global_commit_chunk_size = 64 << 10,      \
                     .output_function = NULL})

void sq_init(SqConfiguration* config);
bool sq_shutdown(void);

SqLinkage sq_linkage_create(int alignment,
                            bool exported,
                            bool tls,
                            bool common,
                            const char* section_name,
                            const char* section_flags);
#define sq_linkage_default ((SqLinkage){0})
#define sq_linkage_export ((SqLinkage){1})

// These values must match internal Kw, Kl, etc.
typedef enum SqTypeKind {
  SQ_TYPE_W = 0,
  SQ_TYPE_L = 1,
  SQ_TYPE_S = 2,
  SQ_TYPE_D = 3,
  SQ_TYPE_SB = 4,
  SQ_TYPE_UB = 5,
  SQ_TYPE_SH = 6,
  SQ_TYPE_UH = 7,
  SQ_TYPE_C = 8,          // User-defined class
  SQ_TYPE_0 = 9,          // void
  SQ_TYPE_E = -2,         // error
  SQ_TYPE_M = SQ_TYPE_L,  // memory
  SQ_TYPE_VARARGS = -3,   // for sq_varargs_begin (not in qbe)
  SQ_TYPE_ENV = -4,       // for sq_type_env (not in qbe)
} SqTypeKind;

#define sq_type_void ((SqType){SQ_TYPE_0})
#define sq_type_word ((SqType){SQ_TYPE_W})
#define sq_type_long ((SqType){SQ_TYPE_L})
#define sq_type_single ((SqType){SQ_TYPE_S})
#define sq_type_double ((SqType){SQ_TYPE_D})
#define sq_type_byte ((SqType){SQ_TYPE_UB})
#define sq_type_half ((SqType){SQ_TYPE_UH})
#define sq_type_sbyte ((SqType){SQ_TYPE_SB})
#define sq_type_shalf ((SqType){SQ_TYPE_SH})
#define sq_type_ubyte ((SqType){SQ_TYPE_UB})
#define sq_type_uhalf ((SqType){SQ_TYPE_UH})
#define sq_type_env ((SqType){SQ_TYPE_ENV})

void sq_type_struct_start(const char* name, int align /*=0 for natural*/);
void sq_type_add_field(SqType field);
void sq_type_add_field_with_count(SqType field, uint32_t count);
SqType sq_type_struct_end(void);

void sq_itemctx_activate(SqItemCtx ctx);

// The returned SqItemCtx is already sq_itemctx_activate()d. If generating
// multiple functions or data declarations at the same time, the correct one
// needs to be (re)activated to set the context for subsequent calls to
// sq_data_* functions.
SqItemCtx sq_data_start(SqLinkage linkage, const char* name);
void sq_data_byte(uint8_t val);
void sq_data_half(uint16_t val);
void sq_data_word(uint32_t val);
void sq_data_long(uint64_t val);
void sq_data_string(const char* str);
void sq_data_single(float f);
void sq_data_double(double d);
void sq_data_ref(SqSymbol ref, int64_t offset);
SqSymbol sq_data_end(void);

// The returned SqItemCtx is already sq_itemctx_activate()d. If generating
// multiple functions or data declarations at the same time, the correct one
// needs to be (re)activated to set the context for subsequent calls to
// sq_func_*, sq_ref_*, sq_block_*, sq_i_*, or sq_const_* functions.
SqItemCtx sq_func_start(SqLinkage linkage, SqType return_type, const char* name);
SqSymbol sq_func_end(void);

SqBlock sq_func_get_entry_block(void);

SqRef sq_const_int(int64_t i);
SqRef sq_const_single(float f);
SqRef sq_const_double(double d);

// SqRef are function local, so the return value from cannot be cached across
// functions.
SqRef sq_ref_for_symbol(SqSymbol sym);

// Forward declare a reference for the `_into` versions of instructions.
// Normally this is unnecessary, typically only when using a phi instruction do
// you need the SqRef before the instruction that generates it.
SqRef sq_ref_declare(void);

SqRef sq_ref_extern(const char* name);

#define sq_func_param(type) sq_func_param_named(type, NULL)
SqRef sq_func_param_named(SqType type, const char* name);

#define sq_block_declare() sq_block_declare_named(NULL)
SqBlock sq_block_declare_named(const char* name);

void sq_block_start(SqBlock block);

#define sq_block_declare_and_start() sq_block_declare_and_start_named(NULL)
SqBlock sq_block_declare_and_start_named(const char* name);

void sq_i_ret_void(void);
void sq_i_ret(SqRef val);
void sq_i_jmp(SqBlock block);
void sq_i_jnz(SqRef cond, SqBlock if_true, SqBlock if_false);

// TODO: only 2-branch phi supported currently
SqRef sq_i_phi(SqType size_class, SqBlock block0, SqRef val0, SqBlock block1, SqRef val1);

void sq_i_blit(SqRef from, SqRef to, int num_bytes);

typedef struct SqCallArg {
  SqType type;
  SqRef value;
} SqCallArg;

#define sq_varargs_begin (SqCallArg){(SqType){SQ_TYPE_VARARGS},(SqRef){0}}

SqRef sq_i_calla(SqType result,
                 SqRef func,
                 int num_args,
                 SqCallArg* cas);

SqRef sq_i_call0(SqType result, SqRef func);
SqRef sq_i_call1(SqType result, SqRef func, SqCallArg ca0);
SqRef sq_i_call2(SqType result, SqRef func, SqCallArg ca0, SqCallArg ca1);
SqRef sq_i_call3(SqType result, SqRef func, SqCallArg ca0, SqCallArg ca1, SqCallArg ca2);
SqRef sq_i_call4(SqType result, SqRef func, SqCallArg ca0, SqCallArg ca1, SqCallArg ca2, SqCallArg ca3);
SqRef sq_i_call5(SqType result, SqRef func, SqCallArg ca0, SqCallArg ca1, SqCallArg ca2, SqCallArg ca3, SqCallArg ca4);
SqRef sq_i_call6(SqType result, SqRef func, SqCallArg ca0, SqCallArg ca1, SqCallArg ca2, SqCallArg ca3, SqCallArg ca4, SqCallArg ca5);
SqRef sq_i_call7(SqType result, SqRef func, SqCallArg ca0, SqCallArg ca1, SqCallArg ca2, SqCallArg ca3, SqCallArg ca4, SqCallArg ca5, SqCallArg ca6);
SqRef sq_i_call8(SqType result, SqRef func, SqCallArg ca0, SqCallArg ca1, SqCallArg ca2, SqCallArg ca3, SqCallArg ca4, SqCallArg ca5, SqCallArg ca6, SqCallArg ca7);

SqRef sq_i_add(SqType size_class, SqRef arg0 /*wlsd*/, SqRef arg1 /*wlsd*/);
void sq_i_add_into(SqRef into, SqType size_class, SqRef arg0 /*wlsd*/, SqRef arg1 /*wlsd*/);
SqRef sq_i_sub(SqType size_class, SqRef arg0 /*wlsd*/, SqRef arg1 /*wlsd*/);
void sq_i_sub_into(SqRef into, SqType size_class, SqRef arg0 /*wlsd*/, SqRef arg1 /*wlsd*/);
SqRef sq_i_neg(SqType size_class, SqRef arg0 /*wlsd*/);
void sq_i_neg_into(SqRef into, SqType size_class, SqRef arg0 /*wlsd*/);
SqRef sq_i_div(SqType size_class, SqRef arg0 /*wlsd*/, SqRef arg1 /*wlsd*/);
void sq_i_div_into(SqRef into, SqType size_class, SqRef arg0 /*wlsd*/, SqRef arg1 /*wlsd*/);
SqRef sq_i_rem(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/);
void sq_i_rem_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/);
SqRef sq_i_udiv(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/);
void sq_i_udiv_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/);
SqRef sq_i_urem(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/);
void sq_i_urem_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/);
SqRef sq_i_mul(SqType size_class, SqRef arg0 /*wlsd*/, SqRef arg1 /*wlsd*/);
void sq_i_mul_into(SqRef into, SqType size_class, SqRef arg0 /*wlsd*/, SqRef arg1 /*wlsd*/);
SqRef sq_i_and(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/);
void sq_i_and_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/);
SqRef sq_i_or(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/);
void sq_i_or_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/);
SqRef sq_i_xor(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/);
void sq_i_xor_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/);
SqRef sq_i_sar(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wwee*/);
void sq_i_sar_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wwee*/);
SqRef sq_i_shr(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wwee*/);
void sq_i_shr_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wwee*/);
SqRef sq_i_shl(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wwee*/);
void sq_i_shl_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wwee*/);
SqRef sq_i_ceqw(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
void sq_i_ceqw_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
SqRef sq_i_cnew(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
void sq_i_cnew_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
SqRef sq_i_csgew(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
void sq_i_csgew_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
SqRef sq_i_csgtw(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
void sq_i_csgtw_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
SqRef sq_i_cslew(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
void sq_i_cslew_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
SqRef sq_i_csltw(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
void sq_i_csltw_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
SqRef sq_i_cugew(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
void sq_i_cugew_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
SqRef sq_i_cugtw(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
void sq_i_cugtw_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
SqRef sq_i_culew(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
void sq_i_culew_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
SqRef sq_i_cultw(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
void sq_i_cultw_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/);
SqRef sq_i_ceql(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
void sq_i_ceql_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
SqRef sq_i_cnel(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
void sq_i_cnel_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
SqRef sq_i_csgel(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
void sq_i_csgel_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
SqRef sq_i_csgtl(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
void sq_i_csgtl_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
SqRef sq_i_cslel(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
void sq_i_cslel_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
SqRef sq_i_csltl(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
void sq_i_csltl_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
SqRef sq_i_cugel(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
void sq_i_cugel_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
SqRef sq_i_cugtl(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
void sq_i_cugtl_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
SqRef sq_i_culel(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
void sq_i_culel_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
SqRef sq_i_cultl(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
void sq_i_cultl_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/);
SqRef sq_i_ceqs(SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/);
void sq_i_ceqs_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/);
SqRef sq_i_cges(SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/);
void sq_i_cges_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/);
SqRef sq_i_cgts(SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/);
void sq_i_cgts_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/);
SqRef sq_i_cles(SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/);
void sq_i_cles_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/);
SqRef sq_i_clts(SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/);
void sq_i_clts_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/);
SqRef sq_i_cnes(SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/);
void sq_i_cnes_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/);
SqRef sq_i_cos(SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/);
void sq_i_cos_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/);
SqRef sq_i_cuos(SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/);
void sq_i_cuos_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/);
SqRef sq_i_ceqd(SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/);
void sq_i_ceqd_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/);
SqRef sq_i_cged(SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/);
void sq_i_cged_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/);
SqRef sq_i_cgtd(SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/);
void sq_i_cgtd_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/);
SqRef sq_i_cled(SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/);
void sq_i_cled_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/);
SqRef sq_i_cltd(SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/);
void sq_i_cltd_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/);
SqRef sq_i_cned(SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/);
void sq_i_cned_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/);
SqRef sq_i_cod(SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/);
void sq_i_cod_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/);
SqRef sq_i_cuod(SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/);
void sq_i_cuod_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/);
void sq_i_storeb(SqRef arg0 /*weee*/, SqRef arg1 /*meee*/);
void sq_i_storeh(SqRef arg0 /*weee*/, SqRef arg1 /*meee*/);
void sq_i_storew(SqRef arg0 /*weee*/, SqRef arg1 /*meee*/);
void sq_i_storel(SqRef arg0 /*leee*/, SqRef arg1 /*meee*/);
void sq_i_stores(SqRef arg0 /*seee*/, SqRef arg1 /*meee*/);
void sq_i_stored(SqRef arg0 /*deee*/, SqRef arg1 /*meee*/);
SqRef sq_i_loadsb(SqType size_class, SqRef arg0 /*mmee*/);
void sq_i_loadsb_into(SqRef into, SqType size_class, SqRef arg0 /*mmee*/);
SqRef sq_i_loadub(SqType size_class, SqRef arg0 /*mmee*/);
void sq_i_loadub_into(SqRef into, SqType size_class, SqRef arg0 /*mmee*/);
SqRef sq_i_loadsh(SqType size_class, SqRef arg0 /*mmee*/);
void sq_i_loadsh_into(SqRef into, SqType size_class, SqRef arg0 /*mmee*/);
SqRef sq_i_loaduh(SqType size_class, SqRef arg0 /*mmee*/);
void sq_i_loaduh_into(SqRef into, SqType size_class, SqRef arg0 /*mmee*/);
SqRef sq_i_loadsw(SqType size_class, SqRef arg0 /*mmee*/);
void sq_i_loadsw_into(SqRef into, SqType size_class, SqRef arg0 /*mmee*/);
SqRef sq_i_loaduw(SqType size_class, SqRef arg0 /*mmee*/);
void sq_i_loaduw_into(SqRef into, SqType size_class, SqRef arg0 /*mmee*/);
SqRef sq_i_load(SqType size_class, SqRef arg0 /*mmmm*/);
void sq_i_load_into(SqRef into, SqType size_class, SqRef arg0 /*mmmm*/);
SqRef sq_i_extsb(SqType size_class, SqRef arg0 /*wwee*/);
void sq_i_extsb_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/);
SqRef sq_i_extub(SqType size_class, SqRef arg0 /*wwee*/);
void sq_i_extub_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/);
SqRef sq_i_extsh(SqType size_class, SqRef arg0 /*wwee*/);
void sq_i_extsh_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/);
SqRef sq_i_extuh(SqType size_class, SqRef arg0 /*wwee*/);
void sq_i_extuh_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/);
SqRef sq_i_extsw(SqRef arg0 /*ewee*/);
void sq_i_extsw_into(SqRef into, SqRef arg0 /*ewee*/);
SqRef sq_i_extuw(SqRef arg0 /*ewee*/);
void sq_i_extuw_into(SqRef into, SqRef arg0 /*ewee*/);
SqRef sq_i_exts(SqRef arg0 /*eees*/);
void sq_i_exts_into(SqRef into, SqRef arg0 /*eees*/);
SqRef sq_i_truncd(SqRef arg0 /*eede*/);
void sq_i_truncd_into(SqRef into, SqRef arg0 /*eede*/);
SqRef sq_i_stosi(SqType size_class, SqRef arg0 /*ssee*/);
void sq_i_stosi_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/);
SqRef sq_i_stoui(SqType size_class, SqRef arg0 /*ssee*/);
void sq_i_stoui_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/);
SqRef sq_i_dtosi(SqType size_class, SqRef arg0 /*ddee*/);
void sq_i_dtosi_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/);
SqRef sq_i_dtoui(SqType size_class, SqRef arg0 /*ddee*/);
void sq_i_dtoui_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/);
SqRef sq_i_swtof(SqType size_class, SqRef arg0 /*eeww*/);
void sq_i_swtof_into(SqRef into, SqType size_class, SqRef arg0 /*eeww*/);
SqRef sq_i_uwtof(SqType size_class, SqRef arg0 /*eeww*/);
void sq_i_uwtof_into(SqRef into, SqType size_class, SqRef arg0 /*eeww*/);
SqRef sq_i_sltof(SqType size_class, SqRef arg0 /*eell*/);
void sq_i_sltof_into(SqRef into, SqType size_class, SqRef arg0 /*eell*/);
SqRef sq_i_ultof(SqType size_class, SqRef arg0 /*eell*/);
void sq_i_ultof_into(SqRef into, SqType size_class, SqRef arg0 /*eell*/);
SqRef sq_i_cast(SqType size_class, SqRef arg0 /*sdwl*/);
void sq_i_cast_into(SqRef into, SqType size_class, SqRef arg0 /*sdwl*/);
SqRef sq_i_alloc4(SqRef arg0 /*elee*/);
void sq_i_alloc4_into(SqRef into, SqRef arg0 /*elee*/);
SqRef sq_i_alloc8(SqRef arg0 /*elee*/);
void sq_i_alloc8_into(SqRef into, SqRef arg0 /*elee*/);
SqRef sq_i_alloc16(SqRef arg0 /*elee*/);
void sq_i_alloc16_into(SqRef into, SqRef arg0 /*elee*/);
SqRef sq_i_vaarg(SqType size_class, SqRef arg0 /*mmmm*/);
void sq_i_vaarg_into(SqRef into, SqType size_class, SqRef arg0 /*mmmm*/);
SqRef sq_i_vastart(SqRef arg0 /*meee*/);
void sq_i_vastart_into(SqRef into, SqRef arg0 /*meee*/);
SqRef sq_i_copy(SqType size_class, SqRef arg0 /*wlsd*/);
void sq_i_copy_into(SqRef into, SqType size_class, SqRef arg0 /*wlsd*/);
void sq_i_dbgloc(SqRef arg0 /*weee*/, SqRef arg1 /*weee*/);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* SQBE_H_INCLUDED_ */

// --------------------
//    IMPLEMENTATION
// --------------------

#ifdef SQBE_IMPLEMENTATION
#undef SQBE_IMPLEMENTATION
#ifdef _MSC_VER
#define SQ_NO_RETURN __declspec(noreturn)
#pragma warning(push)
#pragma warning(disable: 4146)
#pragma warning(disable: 4200)
#pragma warning(disable: 4244)
#pragma warning(disable: 4245)
#pragma warning(disable: 4389)
#pragma warning(disable: 4459)
#pragma warning(disable: 4701)
#else
#define SQ_NO_RETURN __attribute__((noreturn))
#endif

#ifndef SQ_TRAP
#include <stdlib.h>
#define SQ_TRAP() abort()
#endif

#ifndef SQ_ASSERT
#include <assert.h>
#define SQ_ASSERT(x) assert(x)
#endif
/*** START FILE: all.h ***/
/* skipping assert.h */
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAKESURE(what, x) typedef char make_sure_##what[(x)?1:-1]
#define die(...) die_(__FILE__, __VA_ARGS__)
#define err(...) do { err_(__VA_ARGS__); return; } while(0);
#define err_i(...) do { err_(__VA_ARGS__); return 0; } while(0);
#define err_null(...) do { err_(__VA_ARGS__); return NULL; } while(0);
#define ret_on_err() do { if (GC(in_error)) return; } while(0);
#define ret_on_err_i() do { if (GC(in_error)) return 0; } while(0);
#define ret_on_err_nullr() do { if (GC(in_error)) return NULL_R; } while(0);

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long bits;

typedef struct BSet BSet;
typedef struct Ref Ref;
typedef struct Op Op;
typedef struct Ins Ins;
typedef struct Phi Phi;
typedef struct Blk Blk;
typedef struct Use Use;
typedef struct Sym Sym;
typedef struct Num Num;
typedef struct Alias Alias;
typedef struct Tmp Tmp;
typedef struct Con Con;
typedef struct Addr Mem;
typedef struct Fn Fn;
typedef struct Typ Typ;
typedef struct Field Field;
typedef struct Dat Dat;
typedef struct Lnk Lnk;
typedef struct Target Target;
typedef struct MachoCtx MachoCtx;

typedef struct Asmbits Asmbits;
typedef struct Edge Edge;
typedef struct Insert Insert;
typedef struct Name Name;


enum {
	NString = 80,
	NIns    = 1 << 20,
	NAlign  = 3,
	NField  = 32,
	NBit    = CHAR_BIT * sizeof(bits),
};

struct Asmbits {
	bits n;
	int size;
	Asmbits *link;
};

struct Target {
	char name[16];
	char apple;
	char windows;
	int gpr0;   /* first general purpose reg */
	int ngpr;
	int fpr0;   /* first floating point reg */
	int nfpr;
	bits rglob; /* globally live regs (e.g., sp, fp) */
	int nrglob;
	int *rsave; /* caller-save */
	int nrsave[2];
	bits (*retregs)(Ref, int[2]);
	bits (*argregs)(Ref, int[2]);
	int (*memargs)(int);
	void (*abi0)(Fn *);
	void (*abi1)(Fn *);
	void (*isel)(Fn *);
	void (*emitfn)(Fn *, FILE *);
	void (*emitfin)(FILE *);
	char asloc[4];
	char assym[4];
	uint cansel:1;
};

#define BIT(n) ((bits)1 << (n))

enum {
	RXX = 0,
	Tmp0 = NBit, /* first non-reg temporary */
};

struct BSet {
	uint nt;
	bits *t;
};

struct Ref {
	uint type:3;
	uint val:29;
};

enum {
	RTmp,
	RCon,
	RInt,
	RType, /* last kind to come out of the parser */
	RSlot,
	RCall,
	RMem,
};

#define NULL_R   (Ref){RTmp, 0}
#define UNDEF    (Ref){RCon, 0}  /* represents uninitialized data */
#define CON_Z    (Ref){RCon, 1}
#define TMP(x)   (Ref){RTmp, x}
#define CON(x)   (Ref){RCon, x}
#define SLOT(x)  (Ref){RSlot, (x)&0x1fffffff}
#define TYPE(x)  (Ref){RType, x}
#define CALL(x)  (Ref){RCall, x}
#define MEM(x)   (Ref){RMem, x}
#define INT(x)   (Ref){RInt, (x)&0x1fffffff}

static inline int req(Ref a, Ref b)
{
	return a.type == b.type && a.val == b.val;
}

static inline int rtype(Ref r)
{
	if (req(r, NULL_R))
		return -1;
	return r.type;
}

static inline int rsval(Ref r)
{
	return ((int)r.val ^ 0x10000000) - 0x10000000;
}

enum CmpI {
	Cieq,
	Cine,
	Cisge,
	Cisgt,
	Cisle,
	Cislt,
	Ciuge,
	Ciugt,
	Ciule,
	Ciult,
	NCmpI,
};

enum CmpF {
	Cfeq,
	Cfge,
	Cfgt,
	Cfle,
	Cflt,
	Cfne,
	Cfo,
	Cfuo,
	NCmpF,
	NCmp = NCmpI + NCmpF,
};

enum O {
	Oxxx,
#define O(op, x, y) O##op,
/* ------------------------------------------------------------including ops.h */
#ifndef X /* amd64 */
	#define X(NMemArgs, SetsZeroFlag, LeavesFlags)
#endif

#ifndef V /* riscv64 */
	#define V(Imm)
#endif

#ifndef F
#define F(a,b,c,d,e,f,g,h,i,j)
#endif

#define T(a,b,c,d,e,f,g,h) {                          \
	{[Kw]=K##a, [Kl]=K##b, [Ks]=K##c, [Kd]=K##d}, \
	{[Kw]=K##e, [Kl]=K##f, [Ks]=K##g, [Kd]=K##h}  \
}

/*********************/
/* PUBLIC OPERATIONS */
/*********************/

/*                                can fold                        */
/*                                | has identity                  */
/*                                | | identity value for arg[1]   */
/*                                | | | commutative               */
/*                                | | | | associative             */
/*                                | | | | | idempotent            */
/*                                | | | | | | c{eq,ne}[wl]        */
/*                                | | | | | | | c[us][gl][et][wl] */
/*                                | | | | | | | | value if = args */
/*                                | | | | | | | | | pinned        */
/* Arithmetic and Bits            v v v v v v v v v v             */
O(add,     T(w,l,s,d, w,l,s,d), F(1,1,0,1,1,0,0,0,0,0)) X(2,1,0) V(1)
O(sub,     T(w,l,s,d, w,l,s,d), F(1,1,0,0,0,0,0,0,0,0)) X(2,1,0) V(0)
O(neg,     T(w,l,s,d, x,x,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(1,1,0) V(0)
O(div,     T(w,l,s,d, w,l,s,d), F(1,1,1,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(rem,     T(w,l,e,e, w,l,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(udiv,    T(w,l,e,e, w,l,e,e), F(1,1,1,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(urem,    T(w,l,e,e, w,l,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(mul,     T(w,l,s,d, w,l,s,d), F(1,1,1,1,0,0,0,0,0,0)) X(2,0,0) V(0)
O(and,     T(w,l,e,e, w,l,e,e), F(1,0,0,1,1,1,0,0,0,0)) X(2,1,0) V(1)
O(or,      T(w,l,e,e, w,l,e,e), F(1,1,0,1,1,1,0,0,0,0)) X(2,1,0) V(1)
O(xor,     T(w,l,e,e, w,l,e,e), F(1,1,0,1,1,0,0,0,0,0)) X(2,1,0) V(1)
O(sar,     T(w,l,e,e, w,w,e,e), F(1,1,0,0,0,0,0,0,0,0)) X(1,1,0) V(1)
O(shr,     T(w,l,e,e, w,w,e,e), F(1,1,0,0,0,0,0,0,0,0)) X(1,1,0) V(1)
O(shl,     T(w,l,e,e, w,w,e,e), F(1,1,0,0,0,0,0,0,0,0)) X(1,1,0) V(1)

/* Comparisons */
O(ceqw,    T(w,w,e,e, w,w,e,e), F(1,1,1,1,0,0,1,0,1,0)) X(0,1,0) V(0)
O(cnew,    T(w,w,e,e, w,w,e,e), F(1,1,0,1,0,0,1,0,0,0)) X(0,1,0) V(0)
O(csgew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csgtw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(cslew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csltw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)
O(cugew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cugtw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(culew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cultw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)

O(ceql,    T(l,l,e,e, l,l,e,e), F(1,0,0,1,0,0,1,0,1,0)) X(0,1,0) V(0)
O(cnel,    T(l,l,e,e, l,l,e,e), F(1,0,0,1,0,0,1,0,0,0)) X(0,1,0) V(0)
O(csgel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csgtl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(cslel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csltl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)
O(cugel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cugtl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(culel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cultl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)

O(ceqs,    T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cges,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cgts,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cles,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(clts,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cnes,    T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cos,     T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cuos,    T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)

O(ceqd,    T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cged,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cgtd,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cled,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cltd,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cned,    T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cod,     T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cuod,    T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)

/* Memory */
O(storeb,  T(w,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(storeh,  T(w,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(storew,  T(w,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(storel,  T(l,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(stores,  T(s,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(stored,  T(d,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

O(loadsb,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loadub,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loadsh,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loaduh,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loadsw,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loaduw,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(load,    T(m,m,m,m, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

/* Extensions and Truncations */
O(extsb,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extub,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extsh,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extuh,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extsw,   T(e,w,e,e, e,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extuw,   T(e,w,e,e, e,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

O(exts,    T(e,e,e,s, e,e,e,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(truncd,  T(e,e,d,e, e,e,x,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(stosi,   T(s,s,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(stoui,   T(s,s,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(dtosi,   T(d,d,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(dtoui,   T(d,d,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(swtof,   T(e,e,w,w, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(uwtof,   T(e,e,w,w, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(sltof,   T(e,e,l,l, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(ultof,   T(e,e,l,l, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(cast,    T(s,d,w,l, x,x,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

/* Stack Allocation */
O(alloc4,  T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(alloc8,  T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(alloc16, T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)

/* Variadic Function Helpers */
O(vaarg,   T(m,m,m,m, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(vastart, T(m,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)

O(copy,    T(w,l,s,d, x,x,x,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

/* Debug */
O(dbgloc,  T(w,e,e,e, w,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

/****************************************/
/* INTERNAL OPERATIONS (keep nop first) */
/****************************************/

/* Miscellaneous and Architecture-Specific Operations */
O(nop,     T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(addr,    T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(blit0,   T(m,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,1,0) V(0)
O(blit1,   T(w,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,1,0) V(0)
O(sel0,    T(w,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(sel1,    T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(swap,    T(w,l,s,d, w,l,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(1,0,0) V(0)
O(sign,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(salloc,  T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xidiv,   T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(1,0,0) V(0)
O(xdiv,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(1,0,0) V(0)
O(xcmp,    T(w,l,s,d, w,l,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(1,1,0) V(0)
O(xtest,   T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(1,1,0) V(0)
O(acmp,    T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(acmn,    T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(afcmp,   T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(reqz,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(rnez,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)

/* Arguments, Parameters, and Calls */
O(par,     T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parsb,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parub,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parsh,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(paruh,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parc,    T(e,x,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(pare,    T(e,x,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(arg,     T(w,l,s,d, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argsb,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argub,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argsh,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(arguh,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argc,    T(e,x,e,e, e,l,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(arge,    T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argv,    T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(call,    T(m,m,m,m, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)

/* Flags Setting */
O(flagieq,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagine,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagisge, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagisgt, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagisle, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagislt, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiuge, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiugt, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiule, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiult, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfeq,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfge,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfgt,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfle,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagflt,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfne,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfo,   T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfuo,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

/* Backend Flag Select (Condition Move) */
O(xselieq,  T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseline,  T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselisge, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselisgt, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselisle, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselislt, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliuge, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliugt, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliule, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliult, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfeq,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfge,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfgt,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfle,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselflt,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfne,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfo,   T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfuo,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)

#undef T
#undef X
#undef V
#undef O

/*
| column -t -o ' '
*/
/* ------------------------------------------------------------end of ops.h */
	NOp,
};

enum J {
	Jxxx,
#define JMPS(X)                                 \
	X(retw)   X(retl)   X(rets)   X(retd)   \
	X(retsb)  X(retub)  X(retsh)  X(retuh)  \
	X(retc)   X(ret0)   X(jmp)    X(jnz)    \
	X(jfieq)  X(jfine)  X(jfisge) X(jfisgt) \
	X(jfisle) X(jfislt) X(jfiuge) X(jfiugt) \
	X(jfiule) X(jfiult) X(jffeq)  X(jffge)  \
	X(jffgt)  X(jffle)  X(jfflt)  X(jffne)  \
	X(jffo)   X(jffuo)  X(hlt)
#define X(j) J##j,
	JMPS(X)
#undef X
	NJmp
};

enum {
	Ocmpw = Oceqw,
	Ocmpw1 = Ocultw,
	Ocmpl = Oceql,
	Ocmpl1 = Ocultl,
	Ocmps = Oceqs,
	Ocmps1 = Ocuos,
	Ocmpd = Oceqd,
	Ocmpd1 = Ocuod,
	Oalloc = Oalloc4,
	Oalloc1 = Oalloc16,
	Oflag = Oflagieq,
	Oflag1 = Oflagfuo,
	Oxsel = Oxselieq,
	Oxsel1 = Oxselfuo,
	NPubOp = Onop,
	Jjf = Jjfieq,
	Jjf1 = Jjffuo,
};

#define INRANGE(x, l, u) ((unsigned)(x) - l <= u - l) /* linear in x */
#define isstore(o) INRANGE(o, Ostoreb, Ostored)
#define isload(o) INRANGE(o, Oloadsb, Oload)
#define isalloc(o) INRANGE(o, Oalloc4, Oalloc16)
#define isext(o) INRANGE(o, Oextsb, Oextuw)
#define ispar(o) INRANGE(o, Opar, Opare)
#define isarg(o) INRANGE(o, Oarg, Oargv)
#define isret(j) INRANGE(j, Jretw, Jret0)
#define isparbh(o) INRANGE(o, Oparsb, Oparuh)
#define isargbh(o) INRANGE(o, Oargsb, Oarguh)
#define isretbh(j) INRANGE(j, Jretsb, Jretuh)
#define isxsel(o) INRANGE(o, Oxsel, Oxsel1)

enum {
	Kx = -1, /* "top" class (see usecheck() and clsmerge()) */
	Kw,
	Kl,
	Ks,
	Kd
};

#define KWIDE(k) ((k)&1)
#define KBASE(k) ((k)>>1)

struct Op {
	char *name;
	short argcls[2][4];
	uint canfold:1;
	uint hasid:1;     /* op identity value? */
	uint idval:1;     /* identity value 0/1 */
	uint commutes:1;  /* commutative op? */
	uint assoc:1;     /* associative op? */
	uint idemp:1;     /* idempotent op? */
	uint cmpeqwl:1;   /* Kl/Kw cmp eq/ne? */
	uint cmplgtewl:1; /* Kl/Kw cmp lt/gt/le/ge? */
	uint eqval:1;     /* 1 for eq; 0 for ne */
	uint pinned:1;    /* GCM pinned op? */
};

struct Ins {
	uint op:30;
	uint cls:2;
	Ref to;
	Ref arg[2];
};

struct Phi {
	Ref to;
	Ref *arg;
	Blk **blk;
	uint narg;
	short cls;
	uint visit:1;
	Phi *link;
};

struct Blk {
	Phi *phi;
	Ins *ins;
	uint nins;
	struct {
		short type;
		Ref arg;
	} jmp;
	Blk *s1;
	Blk *s2;
	Blk *link;

	uint id;
	uint visit;

	Blk *idom;
	Blk *dom, *dlink;
	Blk **fron;
	uint nfron;
	int depth;

	Blk **pred;
	uint npred;
	BSet in[1], out[1], gen[1];
	int nlive[2];
	int loop;
	char name[NString];
};

struct Use {
	enum {
		UXXX,
		UPhi,
		UIns,
		UJmp,
	} type;
	uint bid;
	union {
		Ins *ins;
		Phi *phi;
	} u;
};

struct Sym {
	enum {
		SGlo,
		SThr,
	} type;
	uint32_t id;
};

struct Num {
	uchar n;
	uchar nl, nr;
	Ref l, r;
};

enum {
	NoAlias,
	MayAlias,
	MustAlias
};

struct Alias {
	enum {
		ABot = 0,
		ALoc = 1, /* stack local */
		ACon = 2,
		AEsc = 3, /* stack escaping */
		ASym = 4,
		AUnk = 6,
	#define astack(t) ((t) & 1)
	} type;
	int base;
	int64_t offset;
	union {
		Sym sym;
		struct {
			int sz; /* -1 if > NBit */
			bits m;
		} loc;
	} u;
	Alias *slot;
};

struct Tmp {
	char name[NString];
	Ins *def;
	Use *use;
	uint ndef, nuse;
	uint bid; /* id of a defining block */
	uint cost;
	int slot; /* -1 for unset */
	short cls;
	struct {
		int r;  /* register or -1 */
		int w;  /* weight */
		bits m; /* avoid these registers */
	} hint;
	int phi;
	Alias alias;
	enum {
		WFull,
		Wsb, /* must match Oload/Oext order */
		Wub,
		Wsh,
		Wuh,
		Wsw,
		Wuw
	} width;
	int visit;
	uint gcmbid;
};

struct Con {
	enum {
		CUndef,
		CBits,
		CAddr,
	} type;
	Sym sym;
	union {
		int64_t i;
		double d;
		float s;
	} bits;
	char flt; /* 1 to print as s, 2 to print as d */
};

typedef struct Addr Addr;

struct Addr { /* amd64 addressing */
	Con offset;
	Ref base;
	Ref index;
	int scale;
};

struct Lnk {
	char export;
	char thread;
	char common;
	char align;
	char *sec;
	char *secf;
};

struct Fn {
	Blk *start;
	Tmp *tmp;
	Con *con;
	Mem *mem;
	int ntmp;
	int ncon;
	int nmem;
	uint nblk;
	int retty; /* index in typ[], -1 if no aggregate return */
	Ref retr;
	Blk **rpo;
	bits reg;
	int slot;
	int salign;
	char vararg;
	char dynalloc;
	char leaf;
	char name[NString];
	Lnk lnk;
};

struct Typ {
	char name[NString];
	char isdark;
	char isunion;
	int align;
	uint64_t size;
	uint nunion;
	struct Field {
		enum {
			FEnd,
			Fb,
			Fh,
			Fw,
			Fl,
			Fs,
			Fd,
			FPad,
			FTyp,
		} type;
		uint len; /* or index in typ[] for FTyp */
	} (*fields)[NField+1];
};

struct Dat {
	enum {
		DStart,
		DEnd,
		DB,
		DH,
		DW,
		DL,
		DZ
	} type;
	char *name;
	Lnk *lnk;
	union {
		int64_t num;
		double fltd;
		float flts;
		char *str;
		struct {
			char *name;
			int64_t off;
		} ref;
	} u;
	char isref;
	char isstr;
};

enum {
	util__NPtr = 256,
	util__IBits = 12,
	util__IMask = (1<<util__IBits) - 1,

	parse__M = 23,
	parse__BMask = 8191, /* for blocks hash */
};

typedef struct Bucket Bucket;
struct Bucket {
	uint nstr;
	char **str;
};

struct Edge {
	int dest;
	int dead;
	Edge *work;
};

typedef struct GlobalContext {
	/* cfg.c */
	Blk cfg__newblk_z;

	/* emit.c */
	Asmbits *emit__stash;
	int64_t emit__zero;
	uint32_t *emit__file;
	uint emit__nfile;
	uint emit__curfile;

	/* fold.c */
	int *fold__val;
	Edge *fold__flowrk;
	Edge (*fold__edge)[2];
	Use **fold__usewrk;
	uint fold__nuse;

	/* load.c */
	Fn *load__curf;
	uint load__inum;     /* current insertion number */
	Insert *load__ilog;  /* global insertion log */
	uint load__nlog;     /* number of entries in the log */

	/* main.c */
	FILE* main__outf;
	int main__dbg;
	int main__objmode;
	MachoCtx *main__macho_ctx;

	/* parse.c */
	int parse__lexinit_done;
	char parse__lex_tok[NString];
	uchar parse__lexh[1 << (32-parse__M)];
	int parse__lnum;
	FILE *parse__inf;
	char *parse__inpath;
	int parse__thead;
	struct {
		char chr;
		double fltd;
		float flts;
		int64_t num;
		char *str;
	} parse__tokval;
	Fn *parse__curf;
	int *parse__tmph;
	int parse__tmphcap;
	Phi **parse__plink;
	Blk *parse__curb;
	Blk **parse__blink;
	Blk *parse__blkh[parse__BMask+1];
	int parse__nblk;
	int parse__rcls;
	uint parse__ntyp;

	/* rega.c */
	bits rega__regu;      /* registers used */
	Tmp *rega__tmp;       /* function temporaries */
	Mem *rega__mem;       /* function mem references */
	struct {
		Ref src, dst;
		int cls;
	} rega__pm[Tmp0];     /* parallel move constructed */
	int rega__npm;        /* size of pm */
	int rega__loop;       /* current loop level */
	uint rega__stmov;     /* stats: added moves */
	uint rega__stblk;     /* stats: added blocks */

  /* spill.c */
	BSet *spill__fst; /* temps to prioritize in registers (for tcmp1) */
	Tmp *spill__tmp;  /* current temporaries (for tcmpX) */
	int spill__ntmp;  /* current # of temps (for limit) */
	int spill__locs;  /* stack size used by locals */
	int spill__slot4; /* next slot of 4 bytes */
	int spill__slot8; /* ditto, 8 bytes */
	BSet spill__mask[2][1]; /* class masks */
	int *spill__limit_tarr;
	int spill__limit_maxt;

	/* ssa.c */
	Name *ssa__namel;

	/* util.c */
	void *util__ptr[util__NPtr];
	void **util__pool;
	int util__nptr;
	Bucket util__itbl[util__IMask+1]; /* string interning table */
  int util__newtmp_n;

	int amd64_emit__amd64_sysv_emitfn_id0;
	int amd64_emit__amd64_winabi_emitfn_id0;
	int arm64_emit__arm64_emitfn_id0;
	int rv64_emit__rv64_emitfn_id0;

	/* nominally owned by util.c, but used everywhere */
	Typ *typ;
	Ins* insb;  // Ins insb[NIns];

	/* nominally owned by gvn.c, but used in gvn.c and copy.c */
	Ref con01[2];

	/* nominally owned by main.c, but used everywhere */
	Target T;
	char debug['Z' + 1];
	// ['P'] = 0, /* parsing */
	// ['M'] = 0, /* memory optimization */
	// ['N'] = 0, /* ssa construction */
	// ['C'] = 0, /* copy elimination */
	// ['F'] = 0, /* constant folding */
	// ['K'] = 0, /* if-conversion */
	// ['A'] = 0, /* abi lowering */
	// ['I'] = 0, /* instruction selection */
	// ['L'] = 0, /* liveness */
	// ['S'] = 0, /* spilling */
	// ['R'] = 0, /* reg. allocation */
	// ['T'] = 0, /* types */
	Ins *curi;

  int in_error;

} GlobalContext;

static GlobalContext global_context;

// This should be used sparingly for things that are "really" global. Normally,
// prefer a file-local G(x) that prefixes with the filename. Currently, just
// `typ`, `insb`, and `curi`, `debug`, `T`, and `in_error`.
#define GC(x) global_context.x


/* util.c */
typedef enum {
	PHeap, /* free() necessary */
	PFn, /* discarded after processing the function */
} Pool;

static uint32_t hash(char *);
SQ_NO_RETURN static void die_(char *, char *, ...) ;
static void *emalloc(size_t);
static void *alloc(size_t);
static void qbe_free(void *);
static void freeall(void);
static void *vnew(ulong, size_t, Pool);
static void vfree(void *);
static void vgrow(void *, ulong);
static void addins(Ins **, uint *, Ins *);
static void addbins(Ins **, uint *, Blk *);
static void strf(char[NString], char *, ...);
static uint32_t intern(char *);
static char *str(uint32_t);
static int argcls(Ins *, int);
static int isreg(Ref);
static int iscmp(int, int *, int *);
static void igroup(Blk *, Ins *, Ins **, Ins **);
static void emit(int, int, Ref, Ref, Ref);
static void emiti(Ins);
static void idup(Blk *, Ins *, ulong);
static Ins *icpy(Ins *, Ins *, ulong);
static int cmpop(int);
static int cmpneg(int);
static int clsmerge(short *, short);
static int phicls(int, Tmp *);
static uint phiargn(Phi *, Blk *);
static Ref phiarg(Phi *, Blk *);
static Ref newtmp(char *, int, Fn *);
static void chuse(Ref, int, Fn *);
static int symeq(Sym, Sym);
static Ref newcon(Con *, Fn *);
static Ref getcon(int64_t, Fn *);
static int addcon(Con *, Con *, int);
static int isconbits(Fn *fn, Ref r, int64_t *v);
static void salloc(Ref, Ref, Fn *);
static void dumpts(BSet *, Tmp *, FILE *);
static void runmatch(uchar *, Num *, Ref, Ref *);
static void bsinit(BSet *, uint);
static void bszero(BSet *);
static uint bscount(BSet *);
static void bsset(BSet *, uint);
static void bsclr(BSet *, uint);
static void bscopy(BSet *, BSet *);
static void bsunion(BSet *, BSet *);
static void bsinter(BSet *, BSet *);
static void bsdiff(BSet *, BSet *);
static int bsequal(BSet *, BSet *);
static int bsiter(BSet *, int *);

static inline int
bshas(BSet *bs, uint elt)
{
	SQ_ASSERT(elt < bs->nt * NBit);
	return (bs->t[elt/NBit] & BIT(elt%NBit)) != 0;
}

/* parse.c */
static Op optab[NOp];
static void printfn(Fn *, FILE *);
static void printtyp(Typ*, FILE *);
static void printref(Ref, Fn *, FILE *);
static void err_(char *, ...);

/* abi.c */
static void elimsb(Fn *);

/* cfg.c */
static Blk *newblk(void);
static void fillpreds(Fn *);
static void fillcfg(Fn *);
static void filldom(Fn *);
static int sdom(Blk *, Blk *);
static int dom(Blk *, Blk *);
static void fillfron(Fn *);
static void loopiter(Fn *, void (*)(Blk *, Blk *));
static void filldepth(Fn *);
static Blk *lca(Blk *, Blk *);
static void fillloop(Fn *);
static void simpljmp(Fn *);
static int reaches(Fn *, Blk *, Blk *);
static int reachesnotvia(Fn *, Blk *, Blk *, Blk *);
static int ifgraph(Blk *, Blk **, Blk **, Blk **);
static void simplcfg(Fn *);

/* mem.c */
static void promote(Fn *);
static void coalesce(Fn *);

/* alias.c */
static void fillalias(Fn *);
static void getalias(Alias *, Ref, Fn *);
static int alias(Ref, int, int, Ref, int, int *, Fn *);
static int escapes(Ref, Fn *);

/* load.c */
static int loadsz(Ins *);
static int storesz(Ins *);
static void loadopt(Fn *);

/* ssa.c */
static void adduse(Tmp *, int, Blk *, ...);
static void filluse(Fn *);
static void ssa(Fn *);
static void ssacheck(Fn *);

/* copy.c */
static void narrowpars(Fn *fn);
static Ref copyref(Fn *, Blk *, Ins *);
static Ref phicopyref(Fn *, Blk *, Phi *);

/* fold.c */
static int foldint(Con *, int, int, Con *, Con *);
static Ref foldref(Fn *, Ins *);

/* gvn.c */
extern Ref con01[2];  /* 0 and 1 */
static int zeroval(Fn *, Blk *, Ref, int, int *);
static void gvn(Fn *);

/* gcm.c */
static int pinned(Ins *);
static void gcm(Fn *);

/* ifopt.c */
static void ifconvert(Fn *fn);

/* simpl.c */
static void simpl(Fn *);

/* live.c */
static void liveon(BSet *, Blk *, Blk *);
static void filllive(Fn *);

/* spill.c */
static void fillcost(Fn *);
static void spill(Fn *);

/* rega.c */
static void rega(Fn *);

/* emit.c */
static void emitfnlnk(char *, Lnk *, FILE *);
static void emitdat(Dat *, FILE *);
static void emitdbgfile(char *, FILE *);
static void emitdbgloc(uint, uint, FILE *);
static int stashbits(bits, int);
static void elf_emitfnfin(char *, FILE *);
static void elf_emitfin(FILE *);
static void macho_emitfin(FILE *);
static void pe_emitfin(FILE *);
#undef G
/*** END FILE: all.h ***/
/*** START FILE: amd64/all.h ***/
/* skipping ../all.h */

typedef struct Amd64Op Amd64Op;

enum Amd64Reg {
	QBE_AMD64_RAX = RXX+1, /* caller-save */
	QBE_AMD64_RCX,  /* caller-save */
	QBE_AMD64_RDX,  /* caller-save */
	QBE_AMD64_RSI,  /* caller-save on sysv, callee-save on win */
	QBE_AMD64_RDI,  /* caller-save on sysv, callee-save on win */
	QBE_AMD64_R8,  /* caller-save */
	QBE_AMD64_R9,  /* caller-save */
	QBE_AMD64_R10,  /* caller-save */
	QBE_AMD64_R11,  /* caller-save */

	QBE_AMD64_RBX, /* callee-save */
	QBE_AMD64_R12,
	QBE_AMD64_R13,
	QBE_AMD64_R14,
	QBE_AMD64_R15,

	QBE_AMD64_RBP, /* globally live */
	QBE_AMD64_RSP,

	QBE_AMD64_XMM0, /* sse */
	QBE_AMD64_XMM1,
	QBE_AMD64_XMM2,
	QBE_AMD64_XMM3,
	QBE_AMD64_XMM4,
	QBE_AMD64_XMM5,
	QBE_AMD64_XMM6,
	QBE_AMD64_XMM7,
	QBE_AMD64_XMM8,
	QBE_AMD64_XMM9,
	QBE_AMD64_XMM10,
	QBE_AMD64_XMM11,
	QBE_AMD64_XMM12,
	QBE_AMD64_XMM13,
	QBE_AMD64_XMM14,
	QBE_AMD64_XMM15,

	QBE_AMD64_NFPR = QBE_AMD64_XMM14 - QBE_AMD64_XMM0 + 1, /* reserve QBE_AMD64_XMM15 */
	QBE_AMD64_NGPR = QBE_AMD64_RSP - QBE_AMD64_RAX + 1,
	QBE_AMD64_NFPS = QBE_AMD64_NFPR,

	QBE_AMD64_NGPS_SYSV = QBE_AMD64_R11 - QBE_AMD64_RAX + 1,
	QBE_AMD64_NCLR_SYSV = QBE_AMD64_R15 - QBE_AMD64_RBX + 1,

	QBE_AMD64_NGPS_WIN = QBE_AMD64_R11 - QBE_AMD64_RAX + 1 - 2,  /* -2 for QBE_AMD64_RDI/QBE_AMD64_RDI */
	QBE_AMD64_NCLR_WIN = QBE_AMD64_R15 - QBE_AMD64_RBX + 1 + 2,  /* +2 for QBE_AMD64_RDI/QBE_AMD64_RDI */
};
MAKESURE(reg_not_tmp, QBE_AMD64_XMM15 < (int)Tmp0);

struct Amd64Op {
	char nmem;
	char zflag;
	char lflag;
};

/* targ.c */
static Amd64Op amd64_op[158];

/* sysv.c (abi) */
static int amd64_sysv_rsave[25];
static int amd64_sysv_rclob[6];
static bits amd64_sysv_retregs(Ref, int[2]);
static bits amd64_sysv_argregs(Ref, int[2]);
static void amd64_sysv_abi(Fn *);

/* winabi.c */
static int amd64_winabi_rsave[23];
static int amd64_winabi_rclob[8];
static bits amd64_winabi_retregs(Ref, int[2]);
static bits amd64_winabi_argregs(Ref, int[2]);
static void amd64_winabi_abi(Fn *);

/* isel.c */
static void amd64_isel(Fn *);

/* emit.c */
static void amd64_sysv_emitfn(Fn *, FILE *);
static void amd64_winabi_emitfn(Fn *, FILE *);
#undef G
/*** END FILE: amd64/all.h ***/
/*** START FILE: arm64/all.h ***/
/* skipping ../all.h */

enum Arm64Reg {
	QBE_ARM64_R0 = RXX + 1,
	     QBE_ARM64_R1,  QBE_ARM64_R2,  QBE_ARM64_R3,  QBE_ARM64_R4,  QBE_ARM64_R5,  QBE_ARM64_R6,  QBE_ARM64_R7,
	QBE_ARM64_R8,  QBE_ARM64_R9,  QBE_ARM64_R10, QBE_ARM64_R11, QBE_ARM64_R12, QBE_ARM64_R13, QBE_ARM64_R14, QBE_ARM64_R15,
	QBE_ARM64_IP0, QBE_ARM64_IP1, QBE_ARM64_R18, QBE_ARM64_R19, QBE_ARM64_R20, QBE_ARM64_R21, QBE_ARM64_R22, QBE_ARM64_R23,
	QBE_ARM64_R24, QBE_ARM64_R25, QBE_ARM64_R26, QBE_ARM64_R27, QBE_ARM64_R28, QBE_ARM64_FP,  QBE_ARM64_LR,  QBE_ARM64_SP,

	QBE_ARM64_V0,  QBE_ARM64_V1,  QBE_ARM64_V2,  QBE_ARM64_V3,  QBE_ARM64_V4,  QBE_ARM64_V5,  QBE_ARM64_V6,  QBE_ARM64_V7,
	QBE_ARM64_V8,  QBE_ARM64_V9,  QBE_ARM64_V10, QBE_ARM64_V11, QBE_ARM64_V12, QBE_ARM64_V13, QBE_ARM64_V14, QBE_ARM64_V15,
	QBE_ARM64_V16, QBE_ARM64_V17, QBE_ARM64_V18, QBE_ARM64_V19, QBE_ARM64_V20, QBE_ARM64_V21, QBE_ARM64_V22, QBE_ARM64_V23,
	QBE_ARM64_V24, QBE_ARM64_V25, QBE_ARM64_V26, QBE_ARM64_V27, QBE_ARM64_V28, QBE_ARM64_V29, QBE_ARM64_V30, /* V31, */

	QBE_ARM64_NFPR = QBE_ARM64_V30 - QBE_ARM64_V0 + 1,
	QBE_ARM64_NGPR = QBE_ARM64_SP - QBE_ARM64_R0 + 1,
	QBE_ARM64_NGPS = QBE_ARM64_R18 - QBE_ARM64_R0 + 1 /* QBE_ARM64_LR */ + 1,
	QBE_ARM64_NFPS = (QBE_ARM64_V7 - QBE_ARM64_V0 + 1) + (QBE_ARM64_V30 - QBE_ARM64_V16 + 1),
	QBE_ARM64_NCLR = (QBE_ARM64_R28 - QBE_ARM64_R19 + 1) + (QBE_ARM64_V15 - QBE_ARM64_V8 + 1),
};
MAKESURE(reg_not_tmp, QBE_ARM64_V30 < (int)Tmp0);

/* targ.c */
static int arm64_rsave[44];
static int arm64_rclob[19];

/* abi.c */
static bits arm64_retregs(Ref, int[2]);
static bits arm64_argregs(Ref, int[2]);
static void arm64_abi(Fn *);
static void apple_extsb(Fn *);

/* isel.c */
static int arm64_logimm(uint64_t, int);
static void arm64_isel(Fn *);

/* emit.c */
static void arm64_emitfn(Fn *, FILE *);
#undef G
/*** END FILE: arm64/all.h ***/
/*** START FILE: arm64/emitmacho.h ***/
static MachoCtx* macho_new(void);
static void macho_free(MachoCtx*);
static void macho_emitdat(Dat*, MachoCtx*);
static void macho_emitfn(Fn*, MachoCtx*);
static void macho_emitfin_obj(MachoCtx*);
static void macho_write(MachoCtx*, FILE*);
#undef G
/*** END FILE: arm64/emitmacho.h ***/
/*** START FILE: rv64/all.h ***/
/* skipping ../all.h */

typedef struct Rv64Op Rv64Op;

enum Rv64Reg {
	/* caller-save */
	QBE_RV64_T0 = RXX + 1, QBE_RV64_T1, QBE_RV64_T2, QBE_RV64_T3, QBE_RV64_T4, QBE_RV64_T5,
	QBE_RV64_A0, QBE_RV64_A1, QBE_RV64_A2, QBE_RV64_A3, QBE_RV64_A4, QBE_RV64_A5, QBE_RV64_A6, QBE_RV64_A7,

	/* callee-save */
	QBE_RV64_S1, QBE_RV64_S2, QBE_RV64_S3, QBE_RV64_S4, QBE_RV64_S5, QBE_RV64_S6, QBE_RV64_S7, QBE_RV64_S8, QBE_RV64_S9, QBE_RV64_S10, QBE_RV64_S11,

	/* globally live */
	QBE_RV64_FP, QBE_RV64_SP, QBE_RV64_GP, QBE_RV64_TP, QBE_RV64_RA,

	/* QBE_RV64_FP caller-save */
	QBE_RV64_FT0, QBE_RV64_FT1, QBE_RV64_FT2, QBE_RV64_FT3, QBE_RV64_FT4, QBE_RV64_FT5, QBE_RV64_FT6, QBE_RV64_FT7, QBE_RV64_FT8, QBE_RV64_FT9, QBE_RV64_FT10,
	QBE_RV64_FA0, QBE_RV64_FA1, QBE_RV64_FA2, QBE_RV64_FA3, QBE_RV64_FA4, QBE_RV64_FA5, QBE_RV64_FA6, QBE_RV64_FA7,

	/* QBE_RV64_FP callee-save */
	QBE_RV64_FS0, QBE_RV64_FS1, QBE_RV64_FS2, QBE_RV64_FS3, QBE_RV64_FS4, QBE_RV64_FS5, QBE_RV64_FS6, QBE_RV64_FS7, QBE_RV64_FS8, QBE_RV64_FS9, QBE_RV64_FS10, QBE_RV64_FS11,

	/* reserved (see rv64/emit.c) */
	QBE_RV64_T6, QBE_RV64_FT11,

	QBE_RV64_NFPR = QBE_RV64_FS11 - QBE_RV64_FT0 + 1,
	QBE_RV64_NGPR = QBE_RV64_RA - QBE_RV64_T0 + 1,
	QBE_RV64_NGPS = QBE_RV64_A7 - QBE_RV64_T0 + 1,
	QBE_RV64_NFPS = QBE_RV64_FA7 - QBE_RV64_FT0 + 1,
	QBE_RV64_NCLR = (QBE_RV64_S11 - QBE_RV64_S1 + 1) + (QBE_RV64_FS11 - QBE_RV64_FS0 + 1),
};
MAKESURE(reg_not_tmp, QBE_RV64_FT11 < (int)Tmp0);

struct Rv64Op {
	char imm;
};

/* targ.c */
static int rv64_rsave[34];
static int rv64_rclob[24];
static Rv64Op rv64_op[158];

/* abi.c */
static bits rv64_retregs(Ref, int[2]);
static bits rv64_argregs(Ref, int[2]);
static void rv64_abi(Fn *);

/* isel.c */
static void rv64_isel(Fn *);

/* emit.c */
static void rv64_emitfn(Fn *, FILE *);
#undef G
/*** END FILE: rv64/all.h ***/
/*** START FILE: abi.c ***/
/* skipping all.h */

/* eliminate sub-word abi op
 * variants for targets that
 * treat char/short/... as
 * words with arbitrary high
 * bits
 */
void
elimsb(Fn *fn)
{
	Blk *b;
	Ins *i;

	for (b=fn->start; b; b=b->link) {
		for (i=b->ins; i<&b->ins[b->nins]; i++) {
			if (isargbh(i->op))
				i->op = Oarg;
			if (isparbh(i->op))
				i->op = Opar;
		}
		if (isretbh(b->jmp.type))
			b->jmp.type = Jretw;
	}
}
#undef G
/*** END FILE: abi.c ***/
/*** START FILE: alias.c ***/
/* skipping all.h */

void
getalias(Alias *a, Ref r, Fn *fn)
{
	Con *c;

	switch (rtype(r)) {
	default:
		die("unreachable");
	case RTmp:
		*a = fn->tmp[r.val].alias;
		if (astack(a->type))
			a->type = a->slot->type;
		SQ_ASSERT(a->type != ABot);
		break;
	case RCon:
		c = &fn->con[r.val];
		if (c->type == CAddr) {
			a->type = ASym;
			a->u.sym = c->sym;
		} else
			a->type = ACon;
		a->offset = c->bits.i;
		a->slot = 0;
		break;
	}
}

int
alias(Ref p, int op, int sp, Ref q, int sq, int *delta, Fn *fn)
{
	Alias ap, aq;
	int ovlap;

	getalias(&ap, p, fn);
	getalias(&aq, q, fn);
	ap.offset += op;
	/* when delta is meaningful (ovlap == 1),
	 * we do not overflow int because sp and
	 * sq are bounded by 2^28 */
	*delta = ap.offset - aq.offset;
	ovlap = ap.offset < aq.offset + sq && aq.offset < ap.offset + sp;

	if (astack(ap.type) && astack(aq.type)) {
		/* if both are offsets of the same
		 * stack slot, they alias iif they
		 * overlap */
		if (ap.base == aq.base && ovlap)
			return MustAlias;
		return NoAlias;
	}

	if (ap.type == ASym && aq.type == ASym) {
		/* they conservatively alias if the
		 * symbols are different, or they
		 * alias for sure if they overlap */
		if (!symeq(ap.u.sym, aq.u.sym))
			return MayAlias;
		if (ovlap)
			return MustAlias;
		return NoAlias;
	}

	if ((ap.type == ACon && aq.type == ACon)
	|| (ap.type == aq.type && ap.base == aq.base)) {
		SQ_ASSERT(ap.type == ACon || ap.type == AUnk);
		/* if they have the same base, we
		 * can rely on the offsets only */
		if (ovlap)
			return MustAlias;
		return NoAlias;
	}

	/* if one of the two is unknown
	 * there may be aliasing unless
	 * the other is provably local */
	if (ap.type == AUnk && aq.type != ALoc)
		return MayAlias;
	if (aq.type == AUnk && ap.type != ALoc)
		return MayAlias;

	return NoAlias;
}

int
escapes(Ref r, Fn *fn)
{
	Alias *a;

	if (rtype(r) != RTmp)
		return 1;
	a = &fn->tmp[r.val].alias;
	return !astack(a->type) || a->slot->type == AEsc;
}

static void
qbe_alias_esc(Ref r, Fn *fn)
{
	Alias *a;

	SQ_ASSERT(rtype(r) <= RType);
	if (rtype(r) == RTmp) {
		a = &fn->tmp[r.val].alias;
		if (astack(a->type))
			a->slot->type = AEsc;
	}
}

static void
qbe_alias_store(Ref r, int sz, Fn *fn)
{
	Alias *a;
	int64_t off;
	bits m;

	if (rtype(r) == RTmp) {
		a = &fn->tmp[r.val].alias;
		if (a->slot) {
			SQ_ASSERT(astack(a->type));
			off = a->offset;
			if (sz >= NBit
			|| (off < 0 || off >= NBit))
				m = -1;
			else
				m = (BIT(sz) - 1) << off;
			a->slot->u.loc.m |= m;
		}
	}
}

void
fillalias(Fn *fn)
{
	uint n;
	int t, sz;
	int64_t x;
	Blk *b;
	Phi *p;
	Ins *i;
	Con *c;
	Alias *a, a0, a1;

	for (t=0; t<fn->ntmp; t++)
		fn->tmp[t].alias.type = ABot;
	for (n=0; n<fn->nblk; ++n) {
		b = fn->rpo[n];
		for (p=b->phi; p; p=p->link) {
			SQ_ASSERT(rtype(p->to) == RTmp);
			a = &fn->tmp[p->to.val].alias;
			SQ_ASSERT(a->type == ABot);
			a->type = AUnk;
			a->base = p->to.val;
			a->offset = 0;
			a->slot = 0;
		}
		for (i=b->ins; i<&b->ins[b->nins]; ++i) {
			a = 0;
			if (!req(i->to, NULL_R)) {
				SQ_ASSERT(rtype(i->to) == RTmp);
				a = &fn->tmp[i->to.val].alias;
				SQ_ASSERT(a->type == ABot);
				if (Oalloc <= i->op && i->op <= Oalloc1) {
					a->type = ALoc;
					a->slot = a;
					a->u.loc.sz = -1;
					if (rtype(i->arg[0]) == RCon) {
						c = &fn->con[i->arg[0].val];
						x = c->bits.i;
						if (c->type == CBits)
						if (0 <= x && x <= NBit)
							a->u.loc.sz = x;
					}
				} else {
					a->type = AUnk;
					a->slot = 0;
				}
				a->base = i->to.val;
				a->offset = 0;
			}
			if (i->op == Ocopy) {
				SQ_ASSERT(a);
				getalias(a, i->arg[0], fn);
			}
			if (i->op == Oadd) {
				getalias(&a0, i->arg[0], fn);
				getalias(&a1, i->arg[1], fn);
				if (a0.type == ACon) {
					*a = a1;
					a->offset += a0.offset;
				}
				else if (a1.type == ACon) {
					*a = a0;
					a->offset += a1.offset;
				}
			}
			if (req(i->to, NULL_R) || a->type == AUnk)
			if (i->op != Oblit0) {
				if (!isload(i->op))
					qbe_alias_esc(i->arg[0], fn);
				if (!isstore(i->op))
				if (i->op != Oargc)
					qbe_alias_esc(i->arg[1], fn);
			}
			if (i->op == Oblit0) {
				++i;
				SQ_ASSERT(i->op == Oblit1);
				SQ_ASSERT(rtype(i->arg[0]) == RInt);
				sz = abs(rsval(i->arg[0]));
				qbe_alias_store((i-1)->arg[1], sz, fn);
			}
			if (isstore(i->op))
				qbe_alias_store(i->arg[1], storesz(i), fn);
		}
		if (b->jmp.type != Jretc)
			qbe_alias_esc(b->jmp.arg, fn);
	}
	for (b=fn->start; b; b=b->link)
		for (p=b->phi; p; p=p->link)
			for (n=0; n<p->narg; n++)
				qbe_alias_esc(p->arg[n], fn);
}
#undef G
/*** END FILE: alias.c ***/
/*** START FILE: cfg.c ***/
/* skipping all.h */

#define G(x) global_context.cfg__##x

Blk *
newblk(void)
{
	Blk *b;

	b = alloc(sizeof *b);
	*b = G(newblk_z);
	b->ins = vnew(0, sizeof b->ins[0], PFn);
	b->pred = vnew(0, sizeof b->pred[0], PFn);
	return b;
}

static void
qbe_cfg_fixphis(Fn *f)
{
	Blk *b, *bp;
	Phi *p;
	uint n, n0;

	for (b=f->start; b; b=b->link) {
		SQ_ASSERT(b->id < f->nblk);
		for (p=b->phi; p; p=p->link) {
			for (n=n0=0; n<p->narg; n++) {
				bp = p->blk[n];
				if (bp->id != -1u)
				if (bp->s1 == b || bp->s2 == b) {
					p->blk[n0] = bp;
					p->arg[n0] = p->arg[n];
					n0++;
				}
			}
			SQ_ASSERT(n0 > 0);
			p->narg = n0;
		}
	}
}

static void
qbe_cfg_addpred(Blk *bp, Blk *b)
{
	vgrow(&b->pred, ++b->npred);
	b->pred[b->npred-1] = bp;
}

void
fillpreds(Fn *f)
{
	Blk *b;

	for (b=f->start; b; b=b->link)
		b->npred = 0;
	for (b=f->start; b; b=b->link) {
		if (b->s1)
			qbe_cfg_addpred(b, b->s1);
		if (b->s2 && b->s2 != b->s1)
			qbe_cfg_addpred(b, b->s2);
	}
}

static void
qbe_cfg_porec(Blk *b, uint *npo)
{
	Blk *s1, *s2;

	if (!b || b->id != -1u)
		return;
	b->id = 0; /* marker */
	s1 = b->s1;
	s2 = b->s2;
	if (s1 && s2 && s1->loop > s2->loop) {
		s1 = b->s2;
		s2 = b->s1;
	}
	qbe_cfg_porec(s1, npo);
	qbe_cfg_porec(s2, npo);
	b->id = (*npo)++;
}

static void
qbe_cfg_fillrpo(Fn *f)
{
	Blk *b, **p;

	for (b=f->start; b; b=b->link)
		b->id = -1u;
	f->nblk = 0;
	qbe_cfg_porec(f->start, &f->nblk);
	vgrow(&f->rpo, f->nblk);
	for (p=&f->start; (b=*p);) {
		if (b->id == -1u) {
			*p = b->link;
		} else {
			b->id = f->nblk-b->id-1;
			f->rpo[b->id] = b;
			p = &b->link;
		}
	}
}

/* fill rpo, preds; prune dead blks */
void
fillcfg(Fn *f)
{
	qbe_cfg_fillrpo(f);
	fillpreds(f);
	qbe_cfg_fixphis(f);
}

/* for dominators computation, read
 * "A Simple, Fast Dominance Algorithm"
 * by K. Cooper, T. Harvey, and K. Kennedy.
 */

static Blk *
qbe_cfg_inter(Blk *b1, Blk *b2)
{
	Blk *bt;

	if (b1 == 0)
		return b2;
	while (b1 != b2) {
		if (b1->id < b2->id) {
			bt = b1;
			b1 = b2;
			b2 = bt;
		}
		while (b1->id > b2->id) {
			b1 = b1->idom;
			SQ_ASSERT(b1);
		}
	}
	return b1;
}

void
filldom(Fn *fn)
{
	Blk *b, *d;
	int ch;
	uint n, p;

	for (b=fn->start; b; b=b->link) {
		b->idom = 0;
		b->dom = 0;
		b->dlink = 0;
	}
	do {
		ch = 0;
		for (n=1; n<fn->nblk; n++) {
			b = fn->rpo[n];
			d = 0;
			for (p=0; p<b->npred; p++)
				if (b->pred[p]->idom
				||  b->pred[p] == fn->start)
					d = qbe_cfg_inter(d, b->pred[p]);
			if (d != b->idom) {
				ch++;
				b->idom = d;
			}
		}
	} while (ch);
	for (b=fn->start; b; b=b->link)
		if ((d=b->idom)) {
			SQ_ASSERT(d != b);
			b->dlink = d->dom;
			d->dom = b;
		}
}

int
sdom(Blk *b1, Blk *b2)
{
	SQ_ASSERT(b1 && b2);
	if (b1 == b2)
		return 0;
	while (b2->id > b1->id)
		b2 = b2->idom;
	return b1 == b2;
}

int
dom(Blk *b1, Blk *b2)
{
	return b1 == b2 || sdom(b1, b2);
}

static void
qbe_cfg_addfron(Blk *a, Blk *b)
{
	uint n;

	for (n=0; n<a->nfron; n++)
		if (a->fron[n] == b)
			return;
	if (!a->nfron)
		a->fron = vnew(++a->nfron, sizeof a->fron[0], PFn);
	else
		vgrow(&a->fron, ++a->nfron);
	a->fron[a->nfron-1] = b;
}

/* fill the dominance frontier */
void
fillfron(Fn *fn)
{
	Blk *a, *b;

	for (b=fn->start; b; b=b->link)
		b->nfron = 0;
	for (b=fn->start; b; b=b->link) {
		if (b->s1)
			for (a=b; !sdom(a, b->s1); a=a->idom)
				qbe_cfg_addfron(a, b->s1);
		if (b->s2)
			for (a=b; !sdom(a, b->s2); a=a->idom)
				qbe_cfg_addfron(a, b->s2);
	}
}

static void
qbe_cfg_loopmark(Blk *hd, Blk *b, void f(Blk *, Blk *))
{
	uint p;

	if (b->id < hd->id || b->visit == hd->id)
		return;
	b->visit = hd->id;
	f(hd, b);
	for (p=0; p<b->npred; ++p)
		qbe_cfg_loopmark(hd, b->pred[p], f);
}

void
loopiter(Fn *fn, void f(Blk *, Blk *))
{
	uint n, p;
	Blk *b;

	for (b=fn->start; b; b=b->link)
		b->visit = -1u;
	for (n=0; n<fn->nblk; ++n) {
		b = fn->rpo[n];
		for (p=0; p<b->npred; ++p)
			if (b->pred[p]->id >= n)
				qbe_cfg_loopmark(b, b->pred[p], f);
	}
}

/* dominator tree depth */
void
filldepth(Fn *fn)
{
	Blk *b, *d;
	int depth;

	for (b=fn->start; b; b=b->link)
		b->depth = -1;

	fn->start->depth = 0;

	for (b=fn->start; b; b=b->link) {
		if (b->depth != -1)
			continue;
		depth = 1;
		for (d=b->idom; d->depth==-1; d=d->idom)
			depth++;
		depth += d->depth;
		b->depth = depth;
		for (d=b->idom; d->depth==-1; d=d->idom)
			d->depth = --depth;
	}
}

/* least common ancestor in dom tree */
Blk *
lca(Blk *b1, Blk *b2)
{
	if (!b1)
		return b2;
	if (!b2)
		return b1;
	while (b1->depth > b2->depth)
		b1 = b1->idom;
	while (b2->depth > b1->depth)
		b2 = b2->idom;
	while (b1 != b2) {
		b1 = b1->idom;
		b2 = b2->idom;
	}
	return b1;
}

static void
multloop(Blk *hd, Blk *b)
{
	(void)hd;
	b->loop *= 10;
}

void
fillloop(Fn *fn)
{
	Blk *b;

	for (b=fn->start; b; b=b->link)
		b->loop = 1;
	loopiter(fn, multloop);
}

static void
qbe_cfg_uffind(Blk **pb, Blk **uf)
{
	Blk **pb1;

	pb1 = &uf[(*pb)->id];
	if (*pb1) {
		qbe_cfg_uffind(pb1, uf);
		*pb = *pb1;
	}
}

/* requires rpo and no phis, breaks cfg */
void
simpljmp(Fn *fn)
{

	Blk **uf; /* union-find */
	Blk **p, *b, *ret;

	ret = newblk();
	ret->id = fn->nblk++;
	ret->jmp.type = Jret0;
	uf = emalloc(fn->nblk * sizeof uf[0]);
	for (b=fn->start; b; b=b->link) {
		SQ_ASSERT(!b->phi);
		if (b->jmp.type == Jret0) {
			b->jmp.type = Jjmp;
			b->s1 = ret;
		}
		if (b->nins == 0)
		if (b->jmp.type == Jjmp) {
			qbe_cfg_uffind(&b->s1, uf);
			if (b->s1 != b)
				uf[b->id] = b->s1;
		}
	}
	for (p=&fn->start; (b=*p); p=&b->link) {
		if (b->s1)
			qbe_cfg_uffind(&b->s1, uf);
		if (b->s2)
			qbe_cfg_uffind(&b->s2, uf);
		if (b->s1 && b->s1 == b->s2) {
			b->jmp.type = Jjmp;
			b->s2 = 0;
		}
	}
	*p = ret;
	qbe_free(uf);
}

static int
qbe_cfg_reachrec(Blk *b, Blk *to)
{
	if (b == to)
		return 1;
	if (!b || b->visit)
		return 0;

	b->visit = 1;
	if (qbe_cfg_reachrec(b->s1, to))
		return 1;
	if (qbe_cfg_reachrec(b->s2, to))
		return 1;

	return 0;
}

/* Blk.visit needs to be clear at entry */
int
reaches(Fn *fn, Blk *b, Blk *to)
{
	int r;

	SQ_ASSERT(to);
	r = qbe_cfg_reachrec(b, to);
	for (b=fn->start; b; b=b->link)
		b->visit = 0;
	return r;
}

/* can b reach 'to' not through excl
 * Blk.visit needs to be clear at entry */
int
reachesnotvia(Fn *fn, Blk *b, Blk *to, Blk *excl)
{
	excl->visit = 1;
	return reaches(fn, b, to);
}

int
ifgraph(Blk *ifb, Blk **pthenb, Blk **pelseb, Blk **pjoinb)
{
	Blk *s1, *s2, **t;

	if (ifb->jmp.type != Jjnz)
		return 0;

	s1 = ifb->s1;
	s2 = ifb->s2;
	if (s1->id > s2->id) {
		s1 = ifb->s2;
		s2 = ifb->s1;
		t = pthenb;
		pthenb = pelseb;
		pelseb = t;
	}
	if (s1 == s2)
		return 0;

	if (s1->jmp.type != Jjmp || s1->npred != 1)
		return 0;

	if (s1->s1 == s2) {
		/* if-then / if-else */
		if (s2->npred != 2)
			return 0;
		*pthenb = s1;
		*pelseb = ifb;
		*pjoinb = s2;
		return 1;
	}

	if (s2->jmp.type != Jjmp || s2->npred != 1)
		return 0;
	if (s1->s1 != s2->s1 || s1->s1->npred != 2)
		return 0;

	SQ_ASSERT(s1->s1 != ifb);
	*pthenb = s1;
	*pelseb = s2;
	*pjoinb = s1->s1;
	return 1;
}

typedef struct Jmp Jmp;

struct Jmp {
	int type;
	Ref arg;
	Blk *s1, *s2;
};

static int
qbe_cfg_jmpeq(Jmp *a, Jmp *b)
{
	return a->type == b->type && req(a->arg, b->arg)
		&& a->s1 == b->s1 && a->s2 == b->s2;
}

static int
qbe_cfg_jmpnophi(Jmp *j)
{
	if (j->s1 && j->s1->phi)
		return 0;
	if (j->s2 && j->s2->phi)
		return 0;
	return 1;
}

/* require cfg rpo, breaks use */
void
simplcfg(Fn *fn)
{
	Ins cpy, *i;
	Blk *b, *bb, **pb;
	Jmp *jmp, *j, *jj;
	Phi *p;
	int *empty, done;
	uint n;

	if (GC(debug)['C']) {
		fprintf(stderr, "\n> Before CFG simplification:\n");
		printfn(fn, stderr);
	}

	cpy = (Ins){.op = Ocopy};
	for (b=fn->start; b; b=b->link)
		if (b->npred == 1) {
			bb = b->pred[0];
			for (p=b->phi; p; p=p->link) {
				cpy.cls = p->cls;
				cpy.to = p->to;
				cpy.arg[0] = phiarg(p, bb);
				addins(&bb->ins, &bb->nins, &cpy);
			}
			b->phi = 0;
		}

	jmp = emalloc(fn->nblk * sizeof jmp[0]);
	empty = emalloc(fn->nblk * sizeof empty[0]);
	for (b=fn->start; b; b=b->link) {
		jmp[b->id].type = b->jmp.type;
		jmp[b->id].arg = b->jmp.arg;
		jmp[b->id].s1 = b->s1;
		jmp[b->id].s2 = b->s2;
		empty[b->id] = !b->phi;
		for (i=b->ins; i<&b->ins[b->nins]; i++)
			if (i->op != Onop && i->op != Odbgloc) {
				empty[b->id] = 0;
				break;
			}
	}

	do {
		done = 1;
		for (b=fn->start; b; b=b->link) {
			if (b->id == -1u)
				continue;
			j = &jmp[b->id];
			if (j->type == Jjmp && j->s1->npred == 1) {
				SQ_ASSERT(!j->s1->phi);
				addbins(&b->ins, &b->nins, j->s1);
				empty[b->id] &= empty[j->s1->id];
				jj = &jmp[j->s1->id];
				pb = (Blk*[]){jj->s1, jj->s2, 0};
				for (; (bb=*pb); pb++)
					for (p=bb->phi; p; p=p->link) {
						n = phiargn(p, j->s1);
						p->blk[n] = b;
					}
				j->s1->id = -1u;
				*j = *jj;
				done = 0;
			}
			else if (j->type == Jjnz
			&& empty[j->s1->id] && empty[j->s2->id]
			&& qbe_cfg_jmpeq(&jmp[j->s1->id], &jmp[j->s2->id])
			&& qbe_cfg_jmpnophi(&jmp[j->s1->id])) {
				*j = jmp[j->s1->id];
				done = 0;
			}
		}
	} while (!done);

	for (b=fn->start; b; b=b->link)
		if (b->id != -1u) {
			j = &jmp[b->id];
			b->jmp.type = j->type;
			b->jmp.arg = j->arg;
			b->s1 = j->s1;
			b->s2 = j->s2;
			SQ_ASSERT(!j->s1 || j->s1->id != -1u);
			SQ_ASSERT(!j->s2 || j->s2->id != -1u);
		}

	fillcfg(fn);
	qbe_free(empty);
	qbe_free(jmp);

	if (GC(debug)['C']) {
		fprintf(stderr, "\n> After CFG simplification:\n");
		printfn(fn, stderr);
	}
}
#undef G
/*** END FILE: cfg.c ***/
/*** START FILE: copy.c ***/
/* skipping all.h */

typedef struct Ext Ext;

struct Ext {
	char zext;
	char nopw; /* is a no-op if arg width is <= nopw */
	char usew; /* uses only the low usew bits of arg */
};

#define G(x) global_context.copy__##x

static int
qbe_copy_ext(Ins *i, Ext *e)
{
	static Ext tbl[] = {
		/*extsb*/ {0,  7,  8},
		/*extub*/ {1,  8,  8},
		/*extsh*/ {0, 15, 16},
		/*extuh*/ {1, 16, 16},
		/*extsw*/ {0, 31, 32},
		/*extuw*/ {1, 32, 32},
	};

	if (!isext(i->op))
		return 0;
	*e = tbl[i->op - Oextsb];
	return 1;
}

static int
qbe_copy_bitwidth(uint64_t v)
{
	int n;

	n = 0;
	if (v >> 32) { n += 32; v >>= 32; }
	if (v >> 16) { n += 16; v >>= 16; }
	if (v >>  8) { n +=  8; v >>=  8; }
	if (v >>  4) { n +=  4; v >>=  4; }
	if (v >>  2) { n +=  2; v >>=  2; }
	if (v >>  1) { n +=  1; v >>=  1; }
	return n+v;
}

/* no more than w bits are used */
static int
qbe_copy_usewidthle(Fn *fn, Ref r, int w)
{
	Ext e;
	Tmp *t;
	Use *u;
	Phi *p;
	Ins *i;
	Ref rc;
	int64_t v;
	int b;

	SQ_ASSERT(rtype(r) == RTmp);
	t = &fn->tmp[r.val];
	for (u=t->use; u<&t->use[t->nuse]; u++) {
		switch (u->type) {
		case UPhi:
			p = u->u.phi;
			/* during gvn, phi nodes may be
			 * replaced by other temps; in
			 * this case, the replaced phi
			 * uses are added to the
			 * replacement temp uses and
			 * Phi.to is set to NULL_R */
			if (p->visit || req(p->to, NULL_R))
				continue;
			p->visit = 1;
			b = qbe_copy_usewidthle(fn, p->to, w);
			p->visit = 0;
			if (b)
				continue;
			break;
		case UIns:
			i = u->u.ins;
			SQ_ASSERT(i != 0);
			if (i->op == Ocopy)
				if (qbe_copy_usewidthle(fn, i->to, w))
					continue;
			if (qbe_copy_ext(i, &e)) {
				if (e.usew <= w)
					continue;
				if (qbe_copy_usewidthle(fn, i->to, w))
					continue;
			}
			if (i->op == Oand) {
				if (req(r, i->arg[0]))
					rc = i->arg[1];
				else {
					SQ_ASSERT(req(r, i->arg[1]));
					rc = i->arg[0];
				}
				if (isconbits(fn, rc, &v)
				&& qbe_copy_bitwidth(v) <= w)
					continue;
				break;
			}
			break;
		default:
			break;
		}
		return 0;
	}
	return 1;
}

static int
qbe_copy_min_(int v1, int v2)
{
	return v1 < v2 ? v1 : v2;
}

/* is the ref narrower than w bits */
static int
qbe_copy_defwidthle(Fn *fn, Ref r, int w)
{
	Ext e;
	Tmp *t;
	Phi *p;
	Ins *i;
	uint n;
	int64_t v;
	int x;

	if (isconbits(fn, r, &v)
	&& qbe_copy_bitwidth(v) <= w)
		return 1;
	if (rtype(r) != RTmp)
		return 0;
	t = &fn->tmp[r.val];
	if (t->cls != Kw)
		return 0;

	if (!t->def) {
		/* phi def */
		for (p=fn->rpo[t->bid]->phi; p; p=p->link)
			if (req(p->to, r))
				break;
		SQ_ASSERT(p);
		if (p->visit)
			return 1;
		p->visit = 1;
		for (n=0; n<p->narg; n++)
			if (!qbe_copy_defwidthle(fn, p->arg[n], w)) {
				p->visit = 0;
				return 0;
			}
		p->visit = 0;
		return 1;
	}

	i = t->def;
	if (i->op == Ocopy)
                return qbe_copy_defwidthle(fn, i->arg[0], w);
	if (i->op == Oshr || i->op == Osar) {
		if (isconbits(fn, i->arg[1], &v))
		if (0 < v && v <= 32) {
			if (i->op == Oshr && w+v >= 32)
				return 1;
			if (w < 32) {
				if (i->op == Osar)
					w = qbe_copy_min_(31, w+v);
				else
					w = qbe_copy_min_(32, w+v);
			}
		}
		return qbe_copy_defwidthle(fn, i->arg[0], w);
	}
	if (iscmp(i->op, &x, &x))
		return w >= 1;
	if (i->op == Oand) {
		if (qbe_copy_defwidthle(fn, i->arg[0], w)
		|| qbe_copy_defwidthle(fn, i->arg[1], w))
			return 1;
		return 0;
	}
	if (i->op == Oor || i->op == Oxor) {
		if (qbe_copy_defwidthle(fn, i->arg[0], w)
		&& qbe_copy_defwidthle(fn, i->arg[1], w))
			return 1;
		return 0;
	}
	if (qbe_copy_ext(i, &e)) {
		if (e.zext && e.usew <= w)
			return 1;
		w = qbe_copy_min_(w, e.nopw);
		return qbe_copy_defwidthle(fn, i->arg[0], w);
	}

	return 0;
}

static int
qbe_copy_isw1(Fn *fn, Ref r)
{
	return qbe_copy_defwidthle(fn, r, 1);
}

/* insert early extub/extuh instructions
 * for pars used only narrowly; this
 * helps factoring extensions out of
 * loops
 *
 * needs use; breaks use
 */
void
narrowpars(Fn *fn)
{
	Blk *b;
	int loop;
	Ins ext, *i, *ins;
	uint npar, nins;
	Ref r;

	/* only useful for functions with loops */
	loop = 0;
	for (b=fn->start; b; b=b->link)
		if (b->loop > 1) {
			loop = 1;
			break;
		}
	if (!loop)
		return;

	b = fn->start;

	npar = 0;
	for (i=b->ins; i<&b->ins[b->nins]; i++) {
		if (!ispar(i->op))
			break;
		npar++;
	}
	if (npar == 0)
		return;

	nins = b->nins + npar;
	ins = vnew(nins, sizeof ins[0], PFn);
	icpy(ins, b->ins, npar);
	icpy(ins + 2*npar, b->ins+npar, b->nins-npar);
	b->ins = ins;
	b->nins = nins;

	for (i=b->ins; i<&b->ins[b->nins]; i++) {
		if (!ispar(i->op))
			break;
		ext = (Ins){.op = Onop};
		if (i->cls == Kw)
		if (qbe_copy_usewidthle(fn, i->to, 16)) {
			ext.op = Oextuh;
			if (qbe_copy_usewidthle(fn, i->to, 8))
				ext.op = Oextub;
			r = newtmp("vw", i->cls, fn);
			ext.cls = i->cls;
			ext.to = i->to;
			ext.arg[0] = r;
			i->to = r;
		}
		*(i+npar) = ext;
	}
}

Ref
copyref(Fn *fn, Blk *b, Ins *i)
{
	/* which extensions are copies for a given
	 * argument width */
	static bits extcpy[] = {
		[WFull] = 0,
		[Wsb] = BIT(Wsb) | BIT(Wsh) | BIT(Wsw),
		[Wub] = BIT(Wub) | BIT(Wuh) | BIT(Wuw),
		[Wsh] = BIT(Wsh) | BIT(Wsw),
		[Wuh] = BIT(Wuh) | BIT(Wuw),
		[Wsw] = BIT(Wsw),
		[Wuw] = BIT(Wuw),
	};
	Ext e;
	Tmp *t;
	int64_t v;
	int w, z;

	if (i->op == Ocopy)
		return i->arg[0];

	/* op identity value */
	if (optab[i->op].hasid
	&& KBASE(i->cls) == 0 /* integer only - fp NaN! */
	&& req(i->arg[1], GC(con01)[optab[i->op].idval])
	&& (!optab[i->op].cmpeqwl || qbe_copy_isw1(fn, i->arg[0])))
		return i->arg[0];

	/* idempotent op with identical args */
	if (optab[i->op].idemp
	&& req(i->arg[0], i->arg[1]))
		return i->arg[0];

	/* integer cmp with identical args */
	if ((optab[i->op].cmpeqwl || optab[i->op].cmplgtewl)
	&& req(i->arg[0], i->arg[1]))
		return GC(con01)[optab[i->op].eqval];

	/* cmpeq/ne 0 with 0/non-0 inference */
	if (optab[i->op].cmpeqwl
	&& req(i->arg[1], CON_Z)
	&& zeroval(fn, b, i->arg[0], argcls(i, 0), &z))
		return GC(con01)[optab[i->op].eqval^z^1];

	/* redundant and mask */
	if (i->op == Oand
	&& isconbits(fn, i->arg[1], &v)
	&& (v > 0 && ((v+1) & v) == 0)
	&& qbe_copy_defwidthle(fn, i->arg[0], qbe_copy_bitwidth(v)))
		return i->arg[0];

	if (i->cls == Kw
	&& (i->op == Oextsw || i->op == Oextuw))
		return i->arg[0];

	if (qbe_copy_ext(i, &e) && rtype(i->arg[0]) == RTmp) {
		t = &fn->tmp[i->arg[0].val];
		SQ_ASSERT(KBASE(t->cls) == 0);

		/* do not break typing by returning
		 * a narrower temp */
		if ((int)KWIDE(i->cls) > KWIDE(t->cls))
			return NULL_R;

		w  = Wsb + (i->op - Oextsb);
		if (BIT(w) & extcpy[t->width])
			return i->arg[0];

		/* avoid eliding extensions of params
		 * inserted in the start block; their
		 * point is to make further extensions
		 * redundant */
		if ((!t->def || !ispar(t->def->op))
		&& qbe_copy_usewidthle(fn, i->to, e.usew))
			return i->arg[0];

		if (qbe_copy_defwidthle(fn, i->arg[0], e.nopw))
			return i->arg[0];
	}

	return NULL_R;
}

static int
qbe_copy_phieq(Phi *pa, Phi *pb)
{
	Ref r;
	uint n;

	SQ_ASSERT(pa->narg == pb->narg);
	for (n=0; n<pa->narg; n++) {
		r = phiarg(pb, pa->blk[n]);
		if (!req(pa->arg[n], r))
			return 0;
	}
	return 1;
}

Ref
phicopyref(Fn *fn, Blk *b, Phi *p)
{
	Blk *d, **s;
	Phi *p1;
	uint n, c;

	/* identical args */
	for (n=0; n<p->narg-1; n++)
		if (!req(p->arg[n], p->arg[n+1]))
			break;
	if (n == p->narg-1)
		return p->arg[n];

	/* same as a previous phi */
	for (p1=b->phi; p1!=p; p1=p1->link) {
		SQ_ASSERT(p1);
		if (qbe_copy_phieq(p1, p))
			return p1->to;
	}

	/* can be replaced by a
	 * dominating jnz arg */
	d = b->idom;
	if (p->narg != 2
	|| d->jmp.type != Jjnz
	|| !qbe_copy_isw1(fn, d->jmp.arg))
		return NULL_R;

	s = (Blk*[]){0, 0};
	for (n=0; n<2; n++)
		for (c=0; c<2; c++)
			if (req(p->arg[n], GC(con01)[c]))
				s[c] = p->blk[n];

	/* if s1 ends with a jnz on either b
	 * or s2; the inference below is wrong
	 * without the jump type checks */
	if (d->s1 == s[1] && d->s2 == s[0]
	&& d->s1->jmp.type == Jjmp
	&& d->s2->jmp.type == Jjmp)
		return d->jmp.arg;

	return NULL_R;
}
#undef G
/*** END FILE: copy.c ***/
/*** START FILE: emit.c ***/
/* skipping all.h */

#define G(x) global_context.emit__##x

enum {
	SecText,
	SecData,
	SecBss,
};

static void
emitlnk(char *n, Lnk *l, int s, FILE *f)
{
	static char *sec[2][3] = {
		[0][SecText] = ".text",
		[0][SecData] = ".data",
		[0][SecBss] = ".bss",
		[1][SecText] = ".abort \"unreachable\"",
		[1][SecData] = ".section .tdata,\"awT\"",
		[1][SecBss] = ".section .tbss,\"awT\"",
	};
	char *pfx, *sfx;

	pfx = n[0] == '"' ? "" : GC(T).assym;
	sfx = "";
	if (GC(T).apple && l->thread) {
		l->sec = "__DATA";
		l->secf = "__thread_data,thread_local_regular";
		sfx = "$tlv$init";
		fputs(
			".section __DATA,__thread_vars,"
			"thread_local_variables\n",
			f
		);
		fprintf(f, "%s%s:\n", pfx, n);
		fprintf(f,
			"\t.quad __tlv_bootstrap\n"
			"\t.quad 0\n"
			"\t.quad %s%s%s\n\n",
			pfx, n, sfx
		);
	}
	if (l->sec) {
		fprintf(f, ".section %s", l->sec);
		if (l->secf)
			fprintf(f, ",%s", l->secf);
	} else
		fputs(sec[l->thread != 0][s], f);
	fputc('\n', f);
	if (l->align)
		fprintf(f, ".balign %d\n", l->align);
	if (l->export)
		fprintf(f, ".globl %s%s\n", pfx, n);
	fprintf(f, "%s%s%s:\n", pfx, n, sfx);
}

void
emitfnlnk(char *n, Lnk *l, FILE *f)
{
	emitlnk(n, l, SecText, f);
}

void
emitdat(Dat *d, FILE *f)
{
	static struct {
		char decl[8];
		int64_t mask;
	} di[] = {
		[DB] = {"\t.byte", 0xffL},
		[DH] = {"\t.short", 0xffffL},
		[DW] = {"\t.int", 0xffffffffL},
		[DL] = {"\t.quad", -1L},
	};
	char *p;

	switch (d->type) {
	case DStart:
		G(zero) = 0;
		break;
	case DEnd:
		if (d->lnk->common) {
			if (G(zero) == -1)
				die("invalid common data definition");
			p = d->name[0] == '"' ? "" : GC(T).assym;
			fprintf(f, ".comm %s%s,%"PRId64,
				p, d->name, G(zero));
			if (d->lnk->align)
				fprintf(f, ",%d", d->lnk->align);
			fputc('\n', f);
		}
		else if (G(zero) != -1) {
			emitlnk(d->name, d->lnk, SecBss, f);
			fprintf(f, "\t.fill %"PRId64",1,0\n", G(zero));
		}
		break;
	case DZ:
		if (G(zero) != -1)
			G(zero) += d->u.num;
		else
			fprintf(f, "\t.fill %"PRId64",1,0\n", d->u.num);
		break;
	default:
		if (G(zero) != -1) {
			emitlnk(d->name, d->lnk, SecData, f);
			if (G(zero) > 0)
				fprintf(f, "\t.fill %"PRId64",1,0\n", G(zero));
			G(zero) = -1;
		}
		if (d->isstr) {
			if (d->type != DB)
				err("strings only supported for 'b' currently");
			fprintf(f, "\t.ascii %s\n", d->u.str);
		}
		else if (d->isref) {
			p = d->u.ref.name[0] == '"' ? "" : GC(T).assym;
			fprintf(f, "%s %s%s%+"PRId64"\n",
				di[d->type].decl, p, d->u.ref.name,
				d->u.ref.off);
		}
		else {
			fprintf(f, "%s %"PRId64"\n",
				di[d->type].decl,
				d->u.num & di[d->type].mask);
		}
		break;
	}
}

int
stashbits(bits n, int size)
{
	Asmbits **pb, *b;
	int i;

	SQ_ASSERT(size == 4 || size == 8 || size == 16);
	for (pb=&G(stash), i=0; (b=*pb); pb=&b->link, i++)
		if (size <= b->size && b->n == n)
			return i;
	b = emalloc(sizeof *b);
	b->n = n;
	b->size = size;
	b->link = 0;
	*pb = b;
	return i;
}

static void
qbe_emit_emitfin(FILE *f, char *sec[3])
{
	Asmbits *b;
	int lg, i;
	union { int32_t i; float f; } uf;
	union { int64_t i; double d; } ud;

	if (!G(stash))
		return;
	fprintf(f, "/* floating point constants */\n");
	for (lg=4; lg>=2; lg--)
		for (b=G(stash), i=0; b; b=b->link, i++) {
			if (b->size == (1<<lg)) {
				fprintf(f,
					".section %s\n"
					".p2align %d\n"
					"%sfp%d:",
					sec[lg-2], lg, GC(T).asloc, i
				);
				if (lg == 4)
					fprintf(f,
						"\n\t.quad %"PRId64
						"\n\t.quad 0\n\n",
						(int64_t)b->n);
				else if (lg == 3) {
					ud.i = b->n;
					fprintf(f,
						"\n\t.quad %"PRId64
						" /* %f */\n\n",
						ud.i, ud.d);
				} else if (lg == 2) {
					uf.i = b->n;
					fprintf(f,
						"\n\t.int %"PRId32
						" /* %f */\n\n",
						uf.i, (double)uf.f);
				}
			}
		}
	while ((b=G(stash))) {
		G(stash) = b->link;
		qbe_free(b);
	}
}

void
elf_emitfin(FILE *f)
{
	static char *sec[3] = { ".rodata", ".rodata", ".rodata" };

	qbe_emit_emitfin(f ,sec);
	fprintf(f, ".section .note.GNU-stack,\"\",@progbits\n");
}

void
elf_emitfnfin(char *fn, FILE *f)
{
	fprintf(f, ".type %s, @function\n", fn);
	fprintf(f, ".size %s, .-%s\n", fn, fn);
}

void
macho_emitfin(FILE *f)
{
	static char *sec[3] = {
		"__TEXT,__literal4,4byte_literals",
		"__TEXT,__literal8,8byte_literals",
		".abort \"unreachable\"",
	};

	qbe_emit_emitfin(f, sec);
}

void
pe_emitfin(FILE *f)
{
	static char *sec[3] = { ".rodata", ".rodata", ".rodata" };

	qbe_emit_emitfin(f, sec);
}

void
emitdbgfile(char *fn, FILE *f)
{
	uint32_t id;
	uint n;

	id = intern(fn);
	for (n=0; n<G(nfile); n++)
		if (G(file)[n] == id) {
			/* gas requires positive
			 * file numbers */
			G(curfile) = n + 1;
			return;
		}
	if (!G(file))
		G(file) = vnew(0, sizeof *G(file), PHeap);
	vgrow(&G(file), ++G(nfile));
	G(file)[G(nfile)-1] = id;
	G(curfile) = G(nfile);
	fprintf(f, ".file %u %s\n", G(curfile), fn);
}

void
emitdbgloc(uint line, uint col, FILE *f)
{
	if (col != 0)
		fprintf(f, "\t.loc %u %u %u\n", G(curfile), line, col);
	else
		fprintf(f, "\t.loc %u %u\n", G(curfile), line);
}
#undef G
/*** END FILE: emit.c ***/
/*** START FILE: fold.c ***/
/* skipping all.h */

/* boring folding code */
#define G(x) global_context.fold__##x

static int
qbe_fold_iscon(Con *c, int w, uint64_t k)
{
	if (c->type != CBits)
		return 0;
	if (w)
		return (uint64_t)c->bits.i == k;
	else
		return (uint32_t)c->bits.i == (uint32_t)k;
}

int
foldint(Con *res, int op, int w, Con *cl, Con *cr)
{
	union {
		int64_t s;
		uint64_t u;
		float fs;
		double fd;
	} l, r;
	uint64_t x;
	Sym sym;
	int typ;

	memset(&sym, 0, sizeof sym);
	typ = CBits;
	l.s = cl->bits.i;
	r.s = cr->bits.i;
	if (op == Oadd) {
		if (cl->type == CAddr) {
			if (cr->type == CAddr)
				return 1;
			typ = CAddr;
			sym = cl->sym;
		}
		else if (cr->type == CAddr) {
			typ = CAddr;
			sym = cr->sym;
		}
	}
	else if (op == Osub) {
		if (cl->type == CAddr) {
			if (cr->type != CAddr) {
				typ = CAddr;
				sym = cl->sym;
			} else if (!symeq(cl->sym, cr->sym))
				return 1;
		}
		else if (cr->type == CAddr)
			return 1;
	}
	else if (cl->type == CAddr || cr->type == CAddr)
		return 1;
	if (op == Odiv || op == Orem || op == Oudiv || op == Ourem) {
		if (qbe_fold_iscon(cr, w, 0))
			return 1;
		if (op == Odiv || op == Orem) {
			x = w ? INT64_MIN : INT32_MIN;
			if (qbe_fold_iscon(cr, w, -1))
			if (qbe_fold_iscon(cl, w, x))
				return 1;
		}
	}
	switch (op) {
	case Oadd:  x = l.u + r.u; break;
	case Osub:  x = l.u - r.u; break;
	case Oneg:  x = -l.u; break;
	case Odiv:  x = w ? l.s / r.s : (int32_t)l.s / (int32_t)r.s; break;
	case Orem:  x = w ? l.s % r.s : (int32_t)l.s % (int32_t)r.s; break;
	case Oudiv: x = w ? l.u / r.u : (uint32_t)l.u / (uint32_t)r.u; break;
	case Ourem: x = w ? l.u % r.u : (uint32_t)l.u % (uint32_t)r.u; break;
	case Omul:  x = l.u * r.u; break;
	case Oand:  x = l.u & r.u; break;
	case Oor:   x = l.u | r.u; break;
	case Oxor:  x = l.u ^ r.u; break;
	case Osar:  x = (w ? l.s : (int32_t)l.s) >> (r.u & (31|w<<5)); break;
	case Oshr:  x = (w ? l.u : (uint32_t)l.u) >> (r.u & (31|w<<5)); break;
	case Oshl:  x = l.u << (r.u & (31|w<<5)); break;
	case Oextsb: x = (int8_t)l.u;   break;
	case Oextub: x = (uint8_t)l.u;  break;
	case Oextsh: x = (int16_t)l.u;  break;
	case Oextuh: x = (uint16_t)l.u; break;
	case Oextsw: x = (int32_t)l.u;  break;
	case Oextuw: x = (uint32_t)l.u; break;
	case Ostosi: x = w ? (int64_t)cl->bits.s : (int32_t)cl->bits.s; break;
	case Ostoui: x = w ? (uint64_t)cl->bits.s : (uint32_t)cl->bits.s; break;
	case Odtosi: x = w ? (int64_t)cl->bits.d : (int32_t)cl->bits.d; break;
	case Odtoui: x = w ? (uint64_t)cl->bits.d : (uint32_t)cl->bits.d; break;
	case Ocast:
		x = l.u;
		if (cl->type == CAddr) {
			typ = CAddr;
			sym = cl->sym;
		}
		break;
	default:
		if (Ocmpw <= op && op <= Ocmpl1) {
			if (op <= Ocmpw1) {
				l.u = (int32_t)l.u;
				r.u = (int32_t)r.u;
			} else
				op -= Ocmpl - Ocmpw;
			switch (op - Ocmpw) {
			case Ciule: x = l.u <= r.u; break;
			case Ciult: x = l.u < r.u;  break;
			case Cisle: x = l.s <= r.s; break;
			case Cislt: x = l.s < r.s;  break;
			case Cisgt: x = l.s > r.s;  break;
			case Cisge: x = l.s >= r.s; break;
			case Ciugt: x = l.u > r.u;  break;
			case Ciuge: x = l.u >= r.u; break;
			case Cieq:  x = l.u == r.u; break;
			case Cine:  x = l.u != r.u; break;
			default: die("unreachable");
			}
		}
		else if (Ocmps <= op && op <= Ocmps1) {
			switch (op - Ocmps) {
			case Cfle: x = l.fs <= r.fs; break;
			case Cflt: x = l.fs < r.fs;  break;
			case Cfgt: x = l.fs > r.fs;  break;
			case Cfge: x = l.fs >= r.fs; break;
			case Cfne: x = l.fs != r.fs; break;
			case Cfeq: x = l.fs == r.fs; break;
			case Cfo: x = l.fs < r.fs || l.fs >= r.fs; break;
			case Cfuo: x = !(l.fs < r.fs || l.fs >= r.fs); break;
			default: die("unreachable");
			}
		}
		else if (Ocmpd <= op && op <= Ocmpd1) {
			switch (op - Ocmpd) {
			case Cfle: x = l.fd <= r.fd; break;
			case Cflt: x = l.fd < r.fd;  break;
			case Cfgt: x = l.fd > r.fd;  break;
			case Cfge: x = l.fd >= r.fd; break;
			case Cfne: x = l.fd != r.fd; break;
			case Cfeq: x = l.fd == r.fd; break;
			case Cfo: x = l.fd < r.fd || l.fd >= r.fd; break;
			case Cfuo: x = !(l.fd < r.fd || l.fd >= r.fd); break;
			default: die("unreachable");
			}
		}
		else
			die("unreachable");
	}
	*res = (Con){.type=typ, .sym=sym, .bits={.i=x}};
	return 0;
}

static void
qbe_fold_foldflt(Con *res, int op, int w, Con *cl, Con *cr)
{
	float xs, ls, rs;
	double xd, ld, rd;

	if (cl->type != CBits || cr->type != CBits)
		err("invalid address operand for '%s'", optab[op].name);
	*res = (Con){.type = CBits};
	memset(&res->bits, 0, sizeof(res->bits));
	if (w)  {
		ld = cl->bits.d;
		rd = cr->bits.d;
		switch (op) {
		case Oadd: xd = ld + rd; break;
		case Osub: xd = ld - rd; break;
		case Oneg: xd = -ld; break;
		case Odiv: xd = ld / rd; break;
		case Omul: xd = ld * rd; break;
		case Oswtof: xd = (int32_t)cl->bits.i; break;
		case Ouwtof: xd = (uint32_t)cl->bits.i; break;
		case Osltof: xd = (int64_t)cl->bits.i; break;
		case Oultof: xd = (uint64_t)cl->bits.i; break;
		case Oexts: xd = cl->bits.s; break;
		case Ocast: xd = ld; break;
		default: die("unreachable");
		}
		res->bits.d = xd;
		res->flt = 2;
	} else {
		ls = cl->bits.s;
		rs = cr->bits.s;
		switch (op) {
		case Oadd: xs = ls + rs; break;
		case Osub: xs = ls - rs; break;
		case Oneg: xs = -ls; break;
		case Odiv: xs = ls / rs; break;
		case Omul: xs = ls * rs; break;
		case Oswtof: xs = (int32_t)cl->bits.i; break;
		case Ouwtof: xs = (uint32_t)cl->bits.i; break;
		case Osltof: xs = (int64_t)cl->bits.i; break;
		case Oultof: xs = (uint64_t)cl->bits.i; break;
		case Otruncd: xs = cl->bits.d; break;
		case Ocast: xs = ls; break;
		default: die("unreachable");
		}
		res->bits.s = xs;
		res->flt = 1;
	}
}

static Ref
qbe_fold_opfold(int op, int cls, Con *cl, Con *cr, Fn *fn)
{
	Ref r;
	Con c = {0};

	if (cls == Kw || cls == Kl) {
		if (foldint(&c, op, cls == Kl, cl, cr))
			return NULL_R;
	} else {
		qbe_fold_foldflt(&c, op, cls == Kd, cl, cr);
		ret_on_err_nullr();
	}
	if (!KWIDE(cls))
		c.bits.i &= 0xffffffff;
	r = newcon(&c, fn);
	SQ_ASSERT(!(cls == Ks || cls == Kd) || c.flt);
	return r;
}

/* used by GVN */
Ref
foldref(Fn *fn, Ins *i)
{
	Ref rr;
	Con *cl, *cr;

	if (rtype(i->to) != RTmp)
		return NULL_R;
	if (optab[i->op].canfold) {
		if (rtype(i->arg[0]) != RCon)
			return NULL_R;
		cl = &fn->con[i->arg[0].val];
		rr = i->arg[1];
		if (req(rr, NULL_R))
		    rr = CON_Z;
		if (rtype(rr) != RCon)
			return NULL_R;
		cr = &fn->con[rr.val];

		return qbe_fold_opfold(i->op, i->cls, cl, cr, fn);
	}
	return NULL_R;
}
#undef G
/*** END FILE: fold.c ***/
/*** START FILE: gcm.c ***/
/* skipping all.h */

#define NOBID (-1u)

static int
qbe_gcm_isdivwl(Ins *i)
{
	switch (i->op) {
	case Odiv:
	case Orem:
	case Oudiv:
	case Ourem:
		return KBASE(i->cls) == 0;
	default:
		return 0;
	}
}

int
pinned(Ins *i)
{
	return optab[i->op].pinned || qbe_gcm_isdivwl(i);
}

/* pinned ins that can be eliminated if unused */
static int
qbe_gcm_canelim(Ins *i)
{
	return isload(i->op) || isalloc(i->op) || qbe_gcm_isdivwl(i);
}

static uint qbe_gcm_earlyins(Fn *, Blk *, Ins *);

static uint
qbe_gcm_schedearly(Fn *fn, Ref r)
{
	Tmp *t;
	Blk *b;

	if (rtype(r) != RTmp)
		return 0;

	t = &fn->tmp[r.val];
	if (t->gcmbid != NOBID)
		return t->gcmbid;

	b = fn->rpo[t->bid];
	if (t->def) {
		SQ_ASSERT(b->ins <= t->def && t->def < &b->ins[b->nins]);
		t->gcmbid = 0;  /* mark as visiting */
		t->gcmbid = qbe_gcm_earlyins(fn, b, t->def);
	} else {
		/* phis do not move */
		t->gcmbid = t->bid;
	}

	return t->gcmbid;
}

static uint
qbe_gcm_earlyins(Fn *fn, Blk *b, Ins *i)
{
	uint b0, b1;

	b0 = qbe_gcm_schedearly(fn, i->arg[0]);
	SQ_ASSERT(b0 != NOBID);
	b1 = qbe_gcm_schedearly(fn, i->arg[1]);
	SQ_ASSERT(b1 != NOBID);
	if (fn->rpo[b0]->depth < fn->rpo[b1]->depth) {
		SQ_ASSERT(dom(fn->rpo[b0], fn->rpo[b1]));
		b0 = b1;
	}
	return pinned(i) ? b->id : b0;
}

static void
qbe_gcm_earlyblk(Fn *fn, uint bid)
{
	Blk *b;
	Phi *p;
	Ins *i;
	uint n;

	b = fn->rpo[bid];
	for (p=b->phi; p; p=p->link)
		for (n=0; n<p->narg; n++)
			qbe_gcm_schedearly(fn, p->arg[n]);
	for (i=b->ins; i<&b->ins[b->nins]; i++)
		if (pinned(i)) {
			qbe_gcm_schedearly(fn, i->arg[0]);
			qbe_gcm_schedearly(fn, i->arg[1]);
		}
	qbe_gcm_schedearly(fn, b->jmp.arg);
}

/* least common ancestor in dom tree */
static uint
qbe_gcm_lcabid(Fn *fn, uint bid1, uint bid2)
{
	Blk *b;

	if (bid1 == NOBID)
		return bid2;
	if (bid2 == NOBID)
		return bid1;

	b = lca(fn->rpo[bid1], fn->rpo[bid2]);
	SQ_ASSERT(b);
	return b->id;
}

static uint
qbe_gcm_bestbid(Fn *fn, uint earlybid, uint latebid)
{
	Blk *curb, *earlyb, *bestb;

	if (latebid == NOBID)
		return NOBID; /* unused */

	SQ_ASSERT(earlybid != NOBID);

	earlyb = fn->rpo[earlybid];
	bestb = curb = fn->rpo[latebid];
	SQ_ASSERT(dom(earlyb, curb));

	while (curb != earlyb) {
		curb = curb->idom;
		if (curb->loop < bestb->loop)
			bestb = curb;
	}
	return bestb->id;
}

static uint qbe_gcm_lateins(Fn *, Blk *, Ins *, Ref r);
static uint qbe_gcm_latephi(Fn *, Phi *, Ref r);
static uint qbe_gcm_latejmp(Blk *, Ref r);

/* return lca bid of ref uses */
static uint
qbe_gcm_schedlate(Fn *fn, Ref r)
{
	Tmp *t;
	Blk *b;
	Use *u;
	uint earlybid;
	uint latebid;
	uint uselatebid = 0;

	if (rtype(r) != RTmp)
		return NOBID;

	t = &fn->tmp[r.val];
	if (t->visit)
		return t->gcmbid;

	t->visit = 1;
	earlybid = t->gcmbid;
	if (earlybid == NOBID)
		return NOBID; /* not used */

	/* reuse gcmbid for late bid */
	t->gcmbid = t->bid;
	latebid = NOBID;
	for (u=t->use; u<&t->use[t->nuse]; u++) {
		SQ_ASSERT(u->bid < fn->nblk);
		b = fn->rpo[u->bid];
		switch (u->type) {
		case UXXX:
			die("unreachable");
			break;
		case UPhi:
			uselatebid = qbe_gcm_latephi(fn, u->u.phi, r);
			break;
		case UIns:
			uselatebid = qbe_gcm_lateins(fn, b, u->u.ins, r);
			break;
		case UJmp:
			uselatebid = qbe_gcm_latejmp(b, r);
			break;
		}
		latebid = qbe_gcm_lcabid(fn, latebid, uselatebid);
	}
	/* latebid may be NOBID if the temp is used
	 * in fixed instructions that may be eliminated
	 * and are themselves unused transitively */

	if (t->def && !pinned(t->def))
		t->gcmbid = qbe_gcm_bestbid(fn, earlybid, latebid);
	/* else, keep the early one */

	/* now, gcmbid is the best bid */
	return t->gcmbid;
}

/* returns lca bid of uses or NOBID if
 * the definition can be eliminated */
static uint
qbe_gcm_lateins(Fn *fn, Blk *b, Ins *i, Ref r)
{
	uint latebid;

	SQ_ASSERT(b->ins <= i && i < &b->ins[b->nins]);
	SQ_ASSERT(req(i->arg[0], r) || req(i->arg[1], r));

	latebid = qbe_gcm_schedlate(fn, i->to);
	if (pinned(i)) {
		if (latebid == NOBID)
		if (qbe_gcm_canelim(i))
			return NOBID;
		return b->id;
	}

	return latebid;
}

static uint
qbe_gcm_latephi(Fn *fn, Phi *p, Ref r)
{
	uint n;
	uint latebid;

	if (!p->narg)
		return NOBID; /* marked as unused */

	latebid = NOBID;
	for (n = 0; n < p->narg; n++)
		if (req(p->arg[n], r))
			latebid = qbe_gcm_lcabid(fn, latebid, p->blk[n]->id);

	SQ_ASSERT(latebid != NOBID);
	return latebid;
}

static uint
qbe_gcm_latejmp(Blk *b, Ref r)
{
	if (req(b->jmp.arg, NULL_R))
		return NOBID;
	else {
		SQ_ASSERT(req(b->jmp.arg, r));
		return b->id;
	}
}

static void
qbe_gcm_lateblk(Fn *fn, uint bid)
{
	Blk *b;
	Phi **pp;
	Ins *i;

	b = fn->rpo[bid];
	for (pp=&b->phi; *(pp);)
		if (qbe_gcm_schedlate(fn, (*pp)->to) == NOBID) {
			(*pp)->narg = 0; /* mark unused */
			*pp = (*pp)->link; /* remove phi */
		} else
			pp = &(*pp)->link;

	for (i=b->ins; i<&b->ins[b->nins]; i++)
		if (pinned(i))
			qbe_gcm_schedlate(fn, i->to);
}

static void
qbe_gcm_addgcmins(Fn *fn, Ins *vins, uint nins)
{
	Ins *i;
	Tmp *t;
	Blk *b;

	for (i=vins; i<&vins[nins]; i++) {
		SQ_ASSERT(rtype(i->to) == RTmp);
		t = &fn->tmp[i->to.val];
		b = fn->rpo[t->gcmbid];
		addins(&b->ins, &b->nins, i);
	}
}

/* move live instructions to the
 * end of their target block; use-
 * before-def errors are fixed by
 * schedblk */
static void
qbe_gcm_gcmmove(Fn *fn)
{
	Tmp *t;
	Ins *vins, *i;
	uint nins;

	nins = 0;
	vins = vnew(nins, sizeof vins[0], PFn);

	for (t=fn->tmp; t<&fn->tmp[fn->ntmp]; t++) {
		if (t->def == 0)
			continue;
		if (t->bid == t->gcmbid)
			continue;
		i = t->def;
		if (pinned(i) && !qbe_gcm_canelim(i))
			continue;
		SQ_ASSERT(rtype(i->to) == RTmp);
		SQ_ASSERT(t == &fn->tmp[i->to.val]);
		if (t->gcmbid != NOBID)
			addins(&vins, &nins, i);
		*i = (Ins){.op = Onop};
	}
	qbe_gcm_addgcmins(fn, vins, nins);
}

/* dfs ordering */
static Ins *
qbe_gcm_schedins(Fn *fn, Blk *b, Ins *i, Ins **pvins, uint *pnins)
{
	Ins *i0, *i1;
	Tmp *t;
	uint n;

	igroup(b, i, &i0, &i1);
	for (i=i0; i<i1; i++)
		for (n=0; n<2; n++) {
			if (rtype(i->arg[n]) != RTmp)
				continue;
			t = &fn->tmp[i->arg[n].val];
			if (t->bid != b->id || !t->def)
				continue;
			qbe_gcm_schedins(fn, b, t->def, pvins, pnins);
		}
	for (i=i0; i<i1; i++) {
		addins(pvins, pnins, i);
		*i = (Ins){.op = Onop};
	}
	return i1;
}

/* order ins within a block */
static void
qbe_gcm_schedblk(Fn *fn)
{
	Blk *b;
	Ins *i, *vins;
	uint nins;

	vins = vnew(0, sizeof vins[0], PHeap);
	for (b=fn->start; b; b=b->link) {
		nins = 0;
		for (i=b->ins; i<&b->ins[b->nins];)
			i = qbe_gcm_schedins(fn, b, i, &vins, &nins);
		idup(b, vins, nins);
	}
	vfree(vins);
}

static int
qbe_gcm_cheap(Ins *i)
{
	int x;

	if (KBASE(i->cls) != 0)
		return 0;
	switch (i->op) {
	case Oneg:
	case Oadd:
	case Osub:
	case Omul:
	case Oand:
	case Oor:
	case Oxor:
	case Osar:
	case Oshr:
	case Oshl:
		return 1;
	default:
		return iscmp(i->op, &x, &x);
	}
}

static void
qbe_gcm_sinkref(Fn *fn, Blk *b, Ref *pr)
{
	Ins i;
	Tmp *t;
	Ref r;

	if (rtype(*pr) != RTmp)
		return;
	t = &fn->tmp[pr->val];
	if (!t->def
	|| t->bid == b->id
	|| pinned(t->def)
	|| !qbe_gcm_cheap(t->def))
		return;

	/* sink t->def to b */
	i = *t->def;
	r = newtmp("snk", t->cls, fn);
	t = 0;  /* invalidated */
	*pr = r;
	i.to = r;
	fn->tmp[r.val].gcmbid = b->id;
	emiti(i);
	qbe_gcm_sinkref(fn, b, &i.arg[0]);
	qbe_gcm_sinkref(fn, b, &i.arg[1]);
}

/* redistribute trivial ops to point of
 * use to reduce register pressure
 * requires rpo, use; breaks use
 */
static void
qbe_gcm_sink(Fn *fn)
{
	Blk *b;
	Ins *i;

	for (b=fn->start; b; b=b->link) {
		for (i=b->ins; i<&b->ins[b->nins]; i++)
			if (isload(i->op))
				qbe_gcm_sinkref(fn, b, &i->arg[0]);
			else if (isstore(i->op))
				qbe_gcm_sinkref(fn, b, &i->arg[1]);
		qbe_gcm_sinkref(fn, b, &b->jmp.arg);
	}
	qbe_gcm_addgcmins(fn, GC(curi), &GC(insb)[NIns] - GC(curi));
}

/* requires use dom
 * maintains rpo pred dom
 * breaks use
 */
void
gcm(Fn *fn)
{
	Tmp *t;
	uint bid;

	filldepth(fn);
	fillloop(fn);

	for (t=fn->tmp; t<&fn->tmp[fn->ntmp]; t++) {
		t->visit = 0;
		t->gcmbid = NOBID;
	}
	for (bid=0; bid<fn->nblk; bid++)
		qbe_gcm_earlyblk(fn, bid);
	for (bid=0; bid<fn->nblk; bid++)
		qbe_gcm_lateblk(fn, bid);

	qbe_gcm_gcmmove(fn);
	filluse(fn);
	GC(curi) = &GC(insb)[NIns];
	qbe_gcm_sink(fn);
	filluse(fn);
	qbe_gcm_schedblk(fn);
	
	if (GC(debug)['G']) {
		fprintf(stderr, "\n> After GCM:\n");
		printfn(fn, stderr);
	}
}
#undef G
/*** END FILE: gcm.c ***/
/*** START FILE: gvn.c ***/
/* skipping all.h */

static inline uint
qbe_gvn_mix(uint x0, uint x1)
{
	return x0 + 17*x1;
}

static inline uint
qbe_gvn_rhash(Ref r)
{
	return qbe_gvn_mix(r.type, r.val);
}

static uint
qbe_gvn_ihash(Ins *i)
{
	uint h;

	h = qbe_gvn_mix(i->op, i->cls);
	h = qbe_gvn_mix(h, qbe_gvn_rhash(i->arg[0]));
	h = qbe_gvn_mix(h, qbe_gvn_rhash(i->arg[1]));

	return h;
}

static int
qbe_gvn_ieq(Ins *ia, Ins *ib)
{
	if (ia->op == ib->op)
	if (ia->cls == ib->cls)
	if (req(ia->arg[0], ib->arg[0]))
	if (req(ia->arg[1], ib->arg[1]))
		return 1;
	return 0;
}

static Ins **gvntbl;
static uint gvntbln;

static Ins *
qbe_gvn_gvndup(Ins *i, int insert)
{
	uint idx;
	Ins *ii;

	idx = qbe_gvn_ihash(i) % gvntbln;
	for (;;) {
		ii = gvntbl[idx];
		if (!ii)
			break;
		if (qbe_gvn_ieq(i, ii))
			return ii;

		idx++;
		if (gvntbln <= idx)
			idx = 0;
	}
	if (insert)
		gvntbl[idx] = i;
	return 0;
}

static void
qbe_gvn_replaceuse(Fn *fn, Use *u, Ref r1, Ref r2)
{
	Blk *b;
	Ins *i;
	Phi *p;
	Ref *pr;
	Tmp *t2;
	int n;

	t2 = 0;
	if (rtype(r2) == RTmp)
		t2 = &fn->tmp[r2.val];
	b = fn->rpo[u->bid];
	switch (u->type) {
	case UPhi:
		p = u->u.phi;
		for (pr=p->arg; pr<&p->arg[p->narg]; pr++)
			if (req(*pr, r1))
				*pr = r2;
		if (t2)
			adduse(t2, UPhi, b, p);
		break;
	case UIns:
		i = u->u.ins;
		for (n=0; n<2; n++)
			if (req(i->arg[n], r1))
				i->arg[n] = r2;
		if (t2)
			adduse(t2, UIns, b, i);
		break;
	case UJmp:
		if (req(b->jmp.arg, r1))
			b->jmp.arg = r2;
		if (t2)
			adduse(t2, UJmp, b);
		break;
	case UXXX:
		die("unreachable");
	}
}

static void
qbe_gvn_replaceuses(Fn *fn, Ref r1, Ref r2)
{
	Tmp *t1;
	Use *u;

	SQ_ASSERT(rtype(r1) == RTmp);
	t1 = &fn->tmp[r1.val];
	for (u=t1->use; u<&t1->use[t1->nuse]; u++)
		qbe_gvn_replaceuse(fn, u, r1, r2);
	t1->nuse = 0;
}

static void
qbe_gvn_dedupphi(Fn *fn, Blk *b)
{
	Phi *p, **pp;
	Ref r;

	for (pp=&b->phi; (p=*pp);) {
		r = phicopyref(fn, b, p);
		if (!req(r, NULL_R)) {
			qbe_gvn_replaceuses(fn, p->to, r);
			p->to = NULL_R;
			*pp = p->link;
		} else
			pp = &p->link;
	}
}

static int
qbe_gvn_rcmp(Ref a, Ref b)
{
	if (rtype(a) != rtype(b))
		return rtype(a) - rtype(b);
	return a.val - b.val;
}

static void
qbe_gvn_normins(Fn *fn, Ins *i)
{
	uint n;
	int64_t v;
	Ref r;

	/* truncate constant bits to
	 * 32 bits for s/w uses */
	for (n=0; n<2; n++) {
		if (!KWIDE(argcls(i, n)))
		if (isconbits(fn, i->arg[n], &v))
		if ((v & 0xffffffff) != v)
			i->arg[n] = getcon(v & 0xffffffff, fn);
	}
	/* order arg[0] <= arg[1] for
	 * commutative ops, preferring
	 * RTmp in arg[0] */
	if (optab[i->op].commutes)
	if (qbe_gvn_rcmp(i->arg[0], i->arg[1]) > 0) {
		r = i->arg[1];
		i->arg[1] = i->arg[0];
		i->arg[0] = r;
	}
}

static int
qbe_gvn_negcon(int cls, Con *c)
{
	static Con z = {.type = CBits, .bits.i = 0};

	return foldint(c, Osub, cls, &z, c);
}

static void
qbe_gvn_assoccon(Fn *fn, Blk *b, Ins *i1)
{
	Tmp *t2;
	Ins *i2;
	int op, fail;
	Con c, c1, c2;

	op = i1->op;
	if (op == Osub)
		op = Oadd;

	if (!optab[op].assoc
	|| KBASE(i1->cls) != 0
	|| rtype(i1->arg[0]) != RTmp
	|| rtype(i1->arg[1]) != RCon)
		return;
	c1 = fn->con[i1->arg[1].val];

	t2 = &fn->tmp[i1->arg[0].val];
	if (t2->def == 0)
		return;
	i2 = t2->def;

	if (op != (i2->op == Osub ? Oadd : i2->op)
	|| rtype(i2->arg[1]) != RCon)
		return;
	c2 = fn->con[i2->arg[1].val];

	SQ_ASSERT(KBASE(i2->cls) == 0);
	SQ_ASSERT(KWIDE(i2->cls) >= KWIDE(i1->cls));

	if (i1->op == Osub && qbe_gvn_negcon(i1->cls, &c1))
		return;
	if (i2->op == Osub && qbe_gvn_negcon(i2->cls, &c2))
		return;
	if (foldint(&c, op, i1->cls, &c1, &c2))
		return;

	if (op == Oadd && c.type == CBits)
	if ((i1->cls == Kl  && c.bits.i < 0)
	|| (i1->cls == Kw && (int32_t)c.bits.i < 0)) {
		fail = qbe_gvn_negcon(i1->cls, &c);
		SQ_ASSERT(fail == 0);
		(void)fail;
		op = Osub;
	}

	i1->op = op;
	i1->arg[0] = i2->arg[0];
	i1->arg[1] = newcon(&c, fn);
	adduse(&fn->tmp[i1->arg[0].val], UIns, b, i1);
}

static void
qbe_gvn_killins(Fn *fn, Ins *i, Ref r)
{
	qbe_gvn_replaceuses(fn, i->to, r);
	*i = (Ins){.op = Onop};
}

static void
qbe_gvn_dedupins(Fn *fn, Blk *b, Ins *i)
{
	Ref r;
	Ins *i1;

	qbe_gvn_normins(fn, i);
	if (i->op == Onop || pinned(i))
		return;

	/* when sel instructions are inserted
	 * before gvn, we may want to optimize
	 * them here */
	SQ_ASSERT(i->op != Osel0);
	SQ_ASSERT(!req(i->to, NULL_R));
	qbe_gvn_assoccon(fn, b, i);

	r = copyref(fn, b, i);
	if (!req(r, NULL_R)) {
		qbe_gvn_killins(fn, i, r);
		return;
	}
	r = foldref(fn, i);
	if (!req(r, NULL_R)) {
		qbe_gvn_killins(fn, i, r);
		return;
	}
	i1 = qbe_gvn_gvndup(i, 1);
	if (i1) {
		qbe_gvn_killins(fn, i, i1->to);
		return;
	}
}

static int
cmpeqz(Fn *fn, Ref r, Ref *arg, int *cls, int *eqval)
{
	Ins *i;

	if (rtype(r) != RTmp)
		return 0;
	i = fn->tmp[r.val].def;
	if (i)
	if (optab[i->op].cmpeqwl)
	if (req(i->arg[1], CON_Z)) {
		*arg = i->arg[0];
		*cls = argcls(i, 0);
		*eqval = optab[i->op].eqval;
		return 1;
	}
	return 0;
}

static int
qbe_gvn_branchdom(Fn *fn, Blk *bif, Blk *bbr1, Blk *bbr2, Blk *b)
{
	SQ_ASSERT(bif->jmp.type == Jjnz);

	if (b != bif
	&& dom(bbr1, b)
	&& !reachesnotvia(fn, bbr2, b, bif))
		return 1;

	return 0;
}

static int
qbe_gvn_domzero(Fn *fn, Blk *d, Blk *b, int *z)
{
	if (qbe_gvn_branchdom(fn, d, d->s1, d->s2, b)) {
		*z = 0;
		return 1;
	}
	if (qbe_gvn_branchdom(fn, d, d->s2, d->s1, b)) {
		*z = 1;
		return 1;
	}
	return 0;
}

/* infer 0/non-0 value from dominating jnz */
int
zeroval(Fn *fn, Blk *b, Ref r, int cls, int *z)
{
	Blk *d;
	Ref arg;
	int cls1, eqval;

	for (d=b->idom; d; d=d->idom) {
		if (d->jmp.type != Jjnz)
			continue;
		if (req(r, d->jmp.arg)
		&& cls == Kw
		&& qbe_gvn_domzero(fn, d, b, z)) {
			return 1;
		}
		if (cmpeqz(fn, d->jmp.arg, &arg, &cls1, &eqval)
		&& req(r, arg)
		&& cls == cls1
		&& qbe_gvn_domzero(fn, d, b, z)) {
			*z ^= eqval;
			return 1;
		}
	}
	return 0;
}

static int
qbe_gvn_usecls(Use *u, Ref r, int cls)
{
	int k;

	switch (u->type) {
	case UIns:
		k = Kx;  /* widest use */
		if (req(u->u.ins->arg[0], r))
			k = argcls(u->u.ins, 0);
		if (req(u->u.ins->arg[1], r))
		if (k == Kx || !KWIDE(k))
			k = argcls(u->u.ins, 1);
		return k == Kx ? cls : k;
	case UPhi:
		if (req(u->u.phi->to, NULL_R))
			return cls; /* eliminated */
		return u->u.phi->cls;
	case UJmp:
		return Kw;
	default:
		break;
	}
	die("unreachable");
}

static void
qbe_gvn_propjnz0(Fn *fn, Blk *bif, Blk *s0, Blk *snon0, Ref r, int cls)
{
	Blk *b;
	Tmp *t;
	Use *u;

	if (s0->npred != 1 || rtype(r) != RTmp)
		return;
	t = &fn->tmp[r.val];
	for (u=t->use; u<&t->use[t->nuse]; u++) {
		b = fn->rpo[u->bid];
		/* we may compare an l temp with a w
		 * comparison; so check that the use
		 * does not involve high bits */
		if (qbe_gvn_usecls(u, r, cls) == cls)
		if (qbe_gvn_branchdom(fn, bif, s0, snon0, b))
			qbe_gvn_replaceuse(fn, u, r, CON_Z);
	}
}

static void
qbe_gvn_dedupjmp(Fn *fn, Blk *b)
{
	Blk **ps;
	int64_t v;
	Ref arg;
	int cls, eqval, z;

	if (b->jmp.type != Jjnz)
		return;

	/* propagate jmp arg as 0 through s2 */
	qbe_gvn_propjnz0(fn, b, b->s2, b->s1, b->jmp.arg, Kw);
	/* propagate cmp eq/ne 0 def of jmp arg as 0 */
	if (cmpeqz(fn, b->jmp.arg, &arg, &cls, &eqval)) {
		ps = (Blk*[]){b->s1, b->s2};
		qbe_gvn_propjnz0(fn, b, ps[eqval^1], ps[eqval], arg, cls);
	}

	/* collapse trivial/constant jnz to jmp */
	v = 1;
	z = 0;
	if (b->s1 == b->s2
	|| isconbits(fn, b->jmp.arg, &v)
	|| zeroval(fn, b, b->jmp.arg, Kw, &z)) {
		if (v == 0 || z)
			b->s1 = b->s2;
		/* we later move active ins out of dead blks */
		b->s2 = 0;
		b->jmp.type = Jjmp;
		b->jmp.arg = NULL_R;
	}
}

static void
qbe_gvn_rebuildcfg(Fn *fn)
{
	uint n, nblk;
	Blk *b, *s, **rpo;
	Ins *i;

	nblk = fn->nblk;
	rpo = emalloc(nblk * sizeof rpo[0]);
	memcpy(rpo, fn->rpo, nblk * sizeof rpo[0]);

	fillcfg(fn);

	/* move instructions that were in
	 * killed blocks and may be active
	 * in the computation in the start
	 * block */
	s = fn->start;
	for (n=0; n<nblk; n++) {
		b = rpo[n];
		if (b->id != -1u)
			continue;
		/* blk unreachable after GVN */
		SQ_ASSERT(b != s);
		for (i=b->ins; i<&b->ins[b->nins]; i++)
			if (!optab[i->op].pinned)
			if (qbe_gvn_gvndup(i, 0) == i)
				addins(&s->ins, &s->nins, i);
	}
	qbe_free(rpo);
}

/* requires rpo pred ssa use
 * recreates rpo preds
 * breaks pred use dom ssa (GCM fixes ssa)
 */
void
gvn(Fn *fn)
{
	Blk *b;
	Phi *p;
	Ins *i;
	uint n, nins;

	GC(con01)[0] = getcon(0, fn);
	GC(con01)[1] = getcon(1, fn);

	/* copy.c uses the visit bit */
	for (b=fn->start; b; b=b->link)
		for (p=b->phi; p; p=p->link)
			p->visit = 0;

	fillloop(fn);
	narrowpars(fn);
	filluse(fn);
	ssacheck(fn);

	nins = 0;
	for (b=fn->start; b; b=b->link) {
		b->visit = 0;
		nins += b->nins;
	}

	gvntbln = nins + nins/2;
	gvntbl = emalloc(gvntbln * sizeof gvntbl[0]);
	for (n=0; n<fn->nblk; n++) {
		b = fn->rpo[n];
		qbe_gvn_dedupphi(fn, b);
		for (i=b->ins; i<&b->ins[b->nins]; i++)
			qbe_gvn_dedupins(fn, b, i);
		qbe_gvn_dedupjmp(fn, b);
	}
	qbe_gvn_rebuildcfg(fn);
	qbe_free(gvntbl);
	gvntbl = 0;

	if (GC(debug)['G']) {
		fprintf(stderr, "\n> After GVN:\n");
		printfn(fn, stderr);
	}
}
#undef G
/*** END FILE: gvn.c ***/
/*** START FILE: ifopt.c ***/
/* skipping all.h */

enum {
	MaxIns = 2,
	MaxPhis = 2,
};

static int
qbe_ifopt_okbranch(Blk *b)
{
	Ins *i;
	int n;

	n = 0;
	for (i=b->ins; i<&b->ins[b->nins]; i++)
		if (i->op != Odbgloc) {
			if (pinned(i))
				return 0;
			if (i->op != Onop)
				n++;
		}
	return n <= MaxIns;
}

static int
qbe_ifopt_okjoin(Blk *b)
{
	Phi *p;
	int n;

	n = 0;
	for (p=b->phi; p; p=p->link) {
		if (KBASE(p->cls) != 0)
			return 0;
		n++;
	}
	return n <= MaxPhis;
}

static int
qbe_ifopt_okgraph(Blk *ifb, Blk *thenb, Blk *elseb, Blk *joinb)
{
	if (joinb->npred != 2 || !qbe_ifopt_okjoin(joinb))
		return 0;
	SQ_ASSERT(thenb != elseb);
	if (thenb != ifb && !qbe_ifopt_okbranch(thenb))
		return 0;
	if (elseb != ifb && !qbe_ifopt_okbranch(elseb))
		return 0;
	return 1;
}

static void
qbe_ifopt_convert(Blk *ifb, Blk *thenb, Blk *elseb, Blk *joinb)
{
	Ins *ins, sel;
	Phi *p;
	uint nins;

	ins = vnew(0, sizeof ins[0], PHeap);
	nins = 0;
	addbins(&ins, &nins, ifb);
	if (thenb != ifb)
		addbins(&ins, &nins, thenb);
	if (elseb != ifb)
		addbins(&ins, &nins, elseb);
	SQ_ASSERT(joinb->npred == 2);
	if (joinb->phi) {
		sel = (Ins){
			.op = Osel0, .cls = Kw,
			.arg = {ifb->jmp.arg},
		};
		addins(&ins, &nins, &sel);
	}
	sel = (Ins){.op = Osel1};
	for (p=joinb->phi; p; p=p->link) {
		sel.to = p->to;
		sel.cls = p->cls;
		sel.arg[0] = phiarg(p, thenb);
		sel.arg[1] = phiarg(p, elseb);
		addins(&ins, &nins, &sel);
	}
	idup(ifb, ins, nins);
	ifb->jmp.type = Jjmp;
	ifb->jmp.arg = NULL_R;
	ifb->s1 = joinb;
	ifb->s2 = 0;
	joinb->npred = 1;
	joinb->pred[0] = ifb;
	joinb->phi = 0;
	vfree(ins);
}

/* eliminate if-then[-else] graphlets
 * using sel instructions
 * needs rpo pred use; breaks cfg use
 */
void
ifconvert(Fn *fn)
{
	Blk *ifb, *thenb, *elseb, *joinb;

	if (GC(debug)['K'])
		fputs("\n> If-conversion:\n", stderr);

	for (ifb=fn->start; ifb; ifb=ifb->link)
		if (ifgraph(ifb, &thenb, &elseb, &joinb))
		if (qbe_ifopt_okgraph(ifb, thenb, elseb, joinb)) {
			if (GC(debug)['K'])
				fprintf(stderr,
					"    @%s -> @%s, @%s -> @%s\n",
					ifb->name, thenb->name, elseb->name,
					joinb->name);
			qbe_ifopt_convert(ifb, thenb, elseb, joinb);
		}

	if (GC(debug)['K']) {
		fprintf(stderr, "\n> After if-conversion:\n");
		printfn(fn, stderr);
	}
}
#undef G
/*** END FILE: ifopt.c ***/
/*** START FILE: live.c ***/
/* skipping all.h */

void
liveon(BSet *v, Blk *b, Blk *s)
{
	Phi *p;
	uint a;

	bscopy(v, s->in);
	for (p=s->phi; p; p=p->link)
		if (rtype(p->to) == RTmp)
			bsclr(v, p->to.val);
	for (p=s->phi; p; p=p->link)
		for (a=0; a<p->narg; a++)
			if (p->blk[a] == b)
			if (rtype(p->arg[a]) == RTmp) {
				bsset(v, p->arg[a].val);
				bsset(b->gen, p->arg[a].val);
			}
}

static void
qbe_live_bset(Ref r, Blk *b, int *nlv, Tmp *tmp)
{

	if (rtype(r) != RTmp)
		return;
	bsset(b->gen, r.val);
	if (!bshas(b->in, r.val)) {
		nlv[KBASE(tmp[r.val].cls)]++;
		bsset(b->in, r.val);
	}
}

/* liveness analysis
 * requires rpo computation
 */
void
filllive(Fn *f)
{
	Blk *b;
	Ins *i;
	int k, t, m[2], n, chg, nlv[2];
	BSet u[1], v[1];
	Mem *ma;

	bsinit(u, f->ntmp);
	bsinit(v, f->ntmp);
	for (b=f->start; b; b=b->link) {
		bsinit(b->in, f->ntmp);
		bsinit(b->out, f->ntmp);
		bsinit(b->gen, f->ntmp);
	}
	chg = 1;
Again:
	for (n=f->nblk-1; n>=0; n--) {
		b = f->rpo[n];

		bscopy(u, b->out);
		if (b->s1) {
			liveon(v, b, b->s1);
			bsunion(b->out, v);
		}
		if (b->s2) {
			liveon(v, b, b->s2);
			bsunion(b->out, v);
		}
		chg |= !bsequal(b->out, u);

		memset(nlv, 0, sizeof nlv);
		b->out->t[0] |= GC(T).rglob;
		bscopy(b->in, b->out);
		for (t=0; bsiter(b->in, &t); t++)
			nlv[KBASE(f->tmp[t].cls)]++;
		if (rtype(b->jmp.arg) == RCall) {
			SQ_ASSERT((int)bscount(b->in) == GC(T).nrglob &&
				b->in->t[0] == GC(T).rglob);
			b->in->t[0] |= GC(T).retregs(b->jmp.arg, nlv);
		} else
			qbe_live_bset(b->jmp.arg, b, nlv, f->tmp);
		for (k=0; k<2; k++)
			b->nlive[k] = nlv[k];
		for (i=&b->ins[b->nins]; i!=b->ins;) {
			if ((--i)->op == Ocall && rtype(i->arg[1]) == RCall) {
				b->in->t[0] &= ~GC(T).retregs(i->arg[1], m);
				for (k=0; k<2; k++) {
					nlv[k] -= m[k];
					/* caller-save registers are used
					 * by the callee, in that sense,
					 * right in the middle of the call,
					 * they are live: */
					nlv[k] += GC(T).nrsave[k];
					if (nlv[k] > b->nlive[k])
						b->nlive[k] = nlv[k];
				}
				b->in->t[0] |= GC(T).argregs(i->arg[1], m);
				for (k=0; k<2; k++) {
					nlv[k] -= GC(T).nrsave[k];
					nlv[k] += m[k];
				}
			}
			if (!req(i->to, NULL_R)) {
				SQ_ASSERT(rtype(i->to) == RTmp);
				t = i->to.val;
				if (bshas(b->in, t))
					nlv[KBASE(f->tmp[t].cls)]--;
				bsset(b->gen, t);
				bsclr(b->in, t);
			}
			for (k=0; k<2; k++)
				switch (rtype(i->arg[k])) {
				case RMem:
					ma = &f->mem[i->arg[k].val];
					qbe_live_bset(ma->base, b, nlv, f->tmp);
					qbe_live_bset(ma->index, b, nlv, f->tmp);
					break;
				default:
					qbe_live_bset(i->arg[k], b, nlv, f->tmp);
					break;
				}
			for (k=0; k<2; k++)
				if (nlv[k] > b->nlive[k])
					b->nlive[k] = nlv[k];
		}
	}
	if (chg) {
		chg = 0;
		goto Again;
	}

	if (GC(debug)['L']) {
		fprintf(stderr, "\n> Liveness analysis:\n");
		for (b=f->start; b; b=b->link) {
			fprintf(stderr, "\t%-10sin:   ", b->name);
			dumpts(b->in, f->tmp, stderr);
			fprintf(stderr, "\t          out:  ");
			dumpts(b->out, f->tmp, stderr);
			fprintf(stderr, "\t          gen:  ");
			dumpts(b->gen, f->tmp, stderr);
			fprintf(stderr, "\t          live: ");
			fprintf(stderr, "%d %d\n", b->nlive[0], b->nlive[1]);
		}
	}
}
#undef G
/*** END FILE: live.c ***/
/*** START FILE: load.c ***/
/* skipping all.h */

#define G(x) global_context.load__##x

#define MASK(w) (BIT(8*(w)-1)*2-1) /* must work when w==8 */

typedef struct Loc Loc;
typedef struct Slice Slice;

struct Loc {
	enum {
		LRoot,   /* right above the original load */
		LLoad,   /* inserting a load is allowed */
		LNoLoad, /* only scalar operations allowed */
	} type;
	uint off;
	Blk *blk;
};

struct Slice {
	Ref ref;
	int off;
	short sz;
	short cls; /* load class */
};

struct Insert {
	uint isphi:1;
	uint num:31;
	uint bid;
	uint off;
	union {
		Ins ins;
		struct {
			Slice m;
			Phi *p;
		} phi;
	} new;
};

int
loadsz(Ins *l)
{
	switch (l->op) {
	case Oloadsb: case Oloadub: return 1;
	case Oloadsh: case Oloaduh: return 2;
	case Oloadsw: case Oloaduw: return 4;
	case Oload: return KWIDE(l->cls) ? 8 : 4;
	}
	die("unreachable");
}

int
storesz(Ins *s)
{
	switch (s->op) {
	case Ostoreb: return 1;
	case Ostoreh: return 2;
	case Ostorew: case Ostores: return 4;
	case Ostorel: case Ostored: return 8;
	}
	die("unreachable");
}

static Ref
qbe_load_iins(int cls, int op, Ref a0, Ref a1, Loc *l)
{
	Insert *ist;

	vgrow(&G(ilog), ++G(nlog));
	ist = &G(ilog)[G(nlog)-1];
	ist->isphi = 0;
	ist->num = G(inum)++;
	ist->bid = l->blk->id;
	ist->off = l->off;
	ist->new.ins = (Ins){op, cls, NULL_R, {a0, a1}};
	return ist->new.ins.to = newtmp("ld", cls, G(curf));
}

static void
qbe_load_cast(Ref *r, int cls, Loc *l)
{
	int cls0;

	if (rtype(*r) == RCon)
		return;
	SQ_ASSERT(rtype(*r) == RTmp);
	cls0 = G(curf)->tmp[r->val].cls;
	if (cls0 == cls || (cls == Kw && cls0 == Kl))
		return;
	if (KWIDE(cls0) < KWIDE(cls)) {
		if (cls0 == Ks)
			*r = qbe_load_iins(Kw, Ocast, *r, NULL_R, l);
		*r = qbe_load_iins(Kl, Oextuw, *r, NULL_R, l);
		if (cls == Kd)
			*r = qbe_load_iins(Kd, Ocast, *r, NULL_R, l);
	} else {
		if (cls0 == Kd && cls != Kl)
			*r = qbe_load_iins(Kl, Ocast, *r, NULL_R, l);
		if (cls0 != Kd || cls != Kw)
			*r = qbe_load_iins(cls, Ocast, *r, NULL_R, l);
	}
}

static inline void
qbe_load_mask(int cls, Ref *r, bits msk, Loc *l)
{
	qbe_load_cast(r, cls, l);
	*r = qbe_load_iins(cls, Oand, *r, getcon(msk, G(curf)), l);
}

static Ref
qbe_load_load(Slice sl, bits msk, Loc *l)
{
	Alias *a;
	Ref r, r1;
	int ld, cls, all;
	Con c;

	ld = (int[]){
		[1] = Oloadub,
		[2] = Oloaduh,
		[4] = Oloaduw,
		[8] = Oload
	}[sl.sz];
	all = msk == MASK(sl.sz);
	if (all)
		cls = sl.cls;
	else
		cls = sl.sz > 4 ? Kl : Kw;
	r = sl.ref;
	/* sl.ref might not be live here,
	 * but its alias base ref will be
	 * (see qbe_load_killsl() below) */
	if (rtype(r) == RTmp) {
		a = &G(curf)->tmp[r.val].alias;
		switch (a->type) {
		default:
			die("unreachable");
		case ALoc:
		case AEsc:
		case AUnk:
			r = TMP(a->base);
			if (!a->offset)
				break;
			r1 = getcon(a->offset, G(curf));
			r = qbe_load_iins(Kl, Oadd, r, r1, l);
			break;
		case ACon:
		case ASym:
			memset(&c, 0, sizeof c);
			c.type = CAddr;
			c.sym = a->u.sym;
			c.bits.i = a->offset;
			r = newcon(&c, G(curf));
			break;
		}
	}
	r = qbe_load_iins(cls, ld, r, NULL_R, l);
	if (!all)
		qbe_load_mask(cls, &r, msk, l);
	return r;
}

static int
qbe_load_killsl(Ref r, Slice sl)
{
	Alias *a;

	if (rtype(sl.ref) != RTmp)
		return 0;
	a = &G(curf)->tmp[sl.ref.val].alias;
	switch (a->type) {
	default:   die("unreachable");
	case ALoc:
	case AEsc:
	case AUnk: return req(TMP(a->base), r);
	case ACon:
	case ASym: return 0;
	}
}

/* returns a ref containing the contents of the slice
 * passed as argument, all the bits set to 0 in the
 * mask argument are zeroed in the result;
 * the returned ref has an integer class when the
 * mask does not cover all the bits of the slice,
 * otherwise, it has class sl.cls
 * the procedure returns R when it fails */
static Ref
qbe_load_def(Slice sl, bits msk, Blk *b, Ins *i, Loc *il)
{
	Slice sl1;
	Blk *bp;
	bits msk1, msks;
	int off, cls, cls1, op, sz, ld;
	uint np, oldl, oldt;
	Ref r, r1;
	Phi *p;
	Insert *ist;
	Loc l;

	/* invariants:
	 * -1- b dominates il->blk; so we can use
	 *     temporaries of b in il->blk
	 * -2- if il->type != LNoLoad, then il->blk
	 *     postdominates the original load; so it
	 *     is safe to load in il->blk
	 * -3- if il->type != LNoLoad, then b
	 *     postdominates il->blk (and by 2, the
	 *     original load)
	 */
	SQ_ASSERT(dom(b, il->blk));
	oldl = G(nlog);
	oldt = G(curf)->ntmp;
	if (0) {
	Load:
		G(curf)->ntmp = oldt;
		G(nlog) = oldl;
		if (il->type != LLoad)
			return NULL_R;
		return qbe_load_load(sl, msk, il);
	}

	if (!i)
		i = &b->ins[b->nins];
	cls = sl.sz > 4 ? Kl : Kw;
	msks = MASK(sl.sz);

	while (i > b->ins) {
		--i;
		if (qbe_load_killsl(i->to, sl)
		|| (i->op == Ocall && escapes(sl.ref, G(curf))))
			goto Load;
		ld = isload(i->op);
		if (ld) {
			sz = loadsz(i);
			r1 = i->arg[0];
			r = i->to;
		} else if (isstore(i->op)) {
			sz = storesz(i);
			r1 = i->arg[1];
			r = i->arg[0];
		} else if (i->op == Oblit1) {
			SQ_ASSERT(rtype(i->arg[0]) == RInt);
			sz = abs(rsval(i->arg[0]));
			SQ_ASSERT(i > b->ins);
			--i;
			SQ_ASSERT(i->op == Oblit0);
			r1 = i->arg[1];
		} else
			continue;
		switch (alias(sl.ref, sl.off, sl.sz, r1, sz, &off, G(curf))) {
		case MustAlias:
			if (i->op == Oblit0) {
				sl1 = sl;
				sl1.ref = i->arg[0];
				if (off >= 0) {
					SQ_ASSERT(off < sz);
					sl1.off = off;
					sz -= off;
					off = 0;
				} else {
					sl1.off = 0;
					sl1.sz += off;
				}
				if (sz > sl1.sz)
					sz = sl1.sz;
				SQ_ASSERT(sz <= 8);
				sl1.sz = sz;
			}
			if (off < 0) {
				off = -off;
				msk1 = (MASK(sz) << 8*off) & msks;
				op = Oshl;
			} else {
				msk1 = (MASK(sz) >> 8*off) & msks;
				op = Oshr;
			}
			if ((msk1 & msk) == 0)
				continue;
			if (i->op == Oblit0) {
				r = qbe_load_def(sl1, MASK(sz), b, i, il);
				if (req(r, NULL_R))
					goto Load;
			}
			if (off) {
				cls1 = cls;
				if (op == Oshr && off + sl.sz > 4)
					cls1 = Kl;
				qbe_load_cast(&r, cls1, il);
				r1 = getcon(8*off, G(curf));
				r = qbe_load_iins(cls1, op, r, r1, il);
			}
			if ((msk1 & msk) != msk1 || off + sz < sl.sz)
				qbe_load_mask(cls, &r, msk1 & msk, il);
			if ((msk & ~msk1) != 0) {
				r1 = qbe_load_def(sl, msk & ~msk1, b, i, il);
				if (req(r1, NULL_R))
					goto Load;
				r = qbe_load_iins(cls, Oor, r, r1, il);
			}
			if (msk == msks)
				qbe_load_cast(&r, sl.cls, il);
			return r;
		case MayAlias:
			if (ld)
				continue;
			else
				goto Load;
		case NoAlias:
			continue;
		default:
			die("unreachable");
		}
	}

	for (ist=G(ilog); ist<&G(ilog)[G(nlog)]; ++ist)
		if (ist->isphi && ist->bid == b->id)
		if (req(ist->new.phi.m.ref, sl.ref))
		if (ist->new.phi.m.off == sl.off)
		if (ist->new.phi.m.sz == sl.sz) {
			r = ist->new.phi.p->to;
			if (msk != msks)
				qbe_load_mask(cls, &r, msk, il);
			else
				qbe_load_cast(&r, sl.cls, il);
			return r;
		}

	for (p=b->phi; p; p=p->link)
		if (qbe_load_killsl(p->to, sl))
			/* scanning predecessors in that
			 * case would be unsafe */
			goto Load;

	if (b->npred == 0)
		goto Load;
	if (b->npred == 1) {
		bp = b->pred[0];
		SQ_ASSERT(bp->loop >= il->blk->loop);
		l = *il;
		if (bp->s2)
			l.type = LNoLoad;
		r1 = qbe_load_def(sl, msk, bp, 0, &l);
		if (req(r1, NULL_R))
			goto Load;
		return r1;
	}

	r = newtmp("ld", sl.cls, G(curf));
	p = alloc(sizeof *p);
	vgrow(&G(ilog), ++G(nlog));
	ist = &G(ilog)[G(nlog)-1];
	ist->isphi = 1;
	ist->bid = b->id;
	ist->new.phi.m = sl;
	ist->new.phi.p = p;
	p->to = r;
	p->cls = sl.cls;
	p->narg = b->npred;
	p->arg = vnew(p->narg, sizeof p->arg[0], PFn);
	p->blk = vnew(p->narg, sizeof p->blk[0], PFn);
	for (np=0; np<b->npred; ++np) {
		bp = b->pred[np];
		if (!bp->s2
		&& il->type != LNoLoad
		&& bp->loop < il->blk->loop)
			l.type = LLoad;
		else
			l.type = LNoLoad;
		l.blk = bp;
		l.off = bp->nins;
		r1 = qbe_load_def(sl, msks, bp, 0, &l);
		if (req(r1, NULL_R))
			goto Load;
		p->arg[np] = r1;
		p->blk[np] = bp;
		/* XXX - multiplicity in predecessors!!! */
	}
	if (msk != msks)
		qbe_load_mask(cls, &r, msk, il);
	return r;
}

static int
qbe_load_icmp(const void *pa, const void *pb)
{
	Insert *a, *b;
	int c;

	a = (Insert *)pa;
	b = (Insert *)pb;
	if ((c = a->bid - b->bid))
		return c;
	if (a->isphi && b->isphi)
		return 0;
	if (a->isphi)
		return -1;
	if (b->isphi)
		return +1;
	if ((c = a->off - b->off))
		return c;
	return a->num - b->num;
}

/* require rpo ssa alias */
void
loadopt(Fn *fn)
{
	Ins *i, *ib;
	Blk *b;
	int sz;
	uint n, ni, ext, nt;
	Insert *ist;
	Slice sl;
	Loc l;

	G(curf) = fn;
	G(ilog) = vnew(0, sizeof G(ilog)[0], PHeap);
	G(nlog) = 0;
	G(inum) = 0;
	for (b=fn->start; b; b=b->link)
		for (i=b->ins; i<&b->ins[b->nins]; ++i) {
			if (!isload(i->op))
				continue;
			sz = loadsz(i);
			sl = (Slice){i->arg[0], 0, sz, i->cls};
			l = (Loc){LRoot, i-b->ins, b};
			i->arg[1] = qbe_load_def(sl, MASK(sz), b, i, &l);
		}
	qsort(G(ilog), G(nlog), sizeof G(ilog)[0], qbe_load_icmp);
	vgrow(&G(ilog), G(nlog)+1);
	G(ilog)[G(nlog)].bid = fn->nblk; /* add a sentinel */
	ib = vnew(0, sizeof(Ins), PHeap);
	for (ist=G(ilog), n=0; n<fn->nblk; ++n) {
		b = fn->rpo[n];
		for (; ist->bid == n && ist->isphi; ++ist) {
			ist->new.phi.p->link = b->phi;
			b->phi = ist->new.phi.p;
		}
		ni = 0;
		nt = 0;
		for (;;) {
			if (ist->bid == n && ist->off == ni)
				i = &ist++->new.ins;
			else {
				if (ni == b->nins)
					break;
				i = &b->ins[ni++];
				if (isload(i->op)
				&& !req(i->arg[1], NULL_R)) {
					ext = Oextsb + i->op - Oloadsb;
					switch (i->op) {
					default:
						die("unreachable");
					case Oloadsb:
					case Oloadub:
					case Oloadsh:
					case Oloaduh:
						i->op = ext;
						break;
					case Oloadsw:
					case Oloaduw:
						if (i->cls == Kl) {
							i->op = ext;
							break;
						}
						/* fall through */
					case Oload:
						i->op = Ocopy;
						break;
					}
					i->arg[0] = i->arg[1];
					i->arg[1] = NULL_R;
				}
			}
			vgrow(&ib, ++nt);
			ib[nt-1] = *i;
		}
		idup(b, ib, nt);
	}
	vfree(ib);
	vfree(G(ilog));
	if (GC(debug)['M']) {
		fprintf(stderr, "\n> After load elimination:\n");
		printfn(fn, stderr);
	}
}
#undef G
/*** END FILE: load.c ***/
/*** START FILE: main.c ***/
/* skipping all.h */
/* skipping arm64/emitmacho.h */
/* skipping config.h */
#include <ctype.h>
/* skipping getopt.h */

#define G(x) global_context.main__##x

static GlobalContext global_context;


static Target T_amd64_sysv;
static Target T_amd64_apple;
static Target T_amd64_win;
static Target T_arm64;
static Target T_arm64_apple;
static Target T_rv64;


static void
qbe_main_data(Dat *d)
{
	if (G(dbg))
		return;
	if (G(objmode)) {
		macho_emitdat(d, G(macho_ctx));
		ret_on_err();
	} else {
		emitdat(d, G(outf));
		ret_on_err();
		if (d->type == DEnd) {
			fputs("/* end data */\n\n", G(outf));
		}
	}
	if (d->type == DEnd) {
		freeall();
	}
}

static void
qbe_main_func(Fn *fn)
{
	uint n;

	if (G(dbg))
		fprintf(stderr, "**** Function %s ****", fn->name);
	if (GC(debug)['P']) {
		fprintf(stderr, "\n> After parsing:\n");
		printfn(fn, stderr);
	}
	GC(T).abi0(fn);
	fillcfg(fn);
	filluse(fn);
	promote(fn);
	ret_on_err();
	filluse(fn);
	ssa(fn);
	filluse(fn);
	ssacheck(fn);
	ret_on_err();
	fillalias(fn);
	loadopt(fn);
	filluse(fn);
	fillalias(fn);
	coalesce(fn);
	filluse(fn);
	filldom(fn);
	ssacheck(fn);
	ret_on_err();
	gvn(fn);
	fillcfg(fn);
	simplcfg(fn);
	filluse(fn);
	filldom(fn);
	gcm(fn);
	filluse(fn);
	ssacheck(fn);
	if (GC(T).cansel) {
		ifconvert(fn);
		fillcfg(fn);
		filluse(fn);
		filldom(fn);
		ssacheck(fn);
	}
	GC(T).abi1(fn);
	simpl(fn);
	fillcfg(fn);
	filluse(fn);
	GC(T).isel(fn);
  ret_on_err();
	fillcfg(fn);
	filllive(fn);
	fillloop(fn);
	fillcost(fn);
	spill(fn);
	rega(fn);
	fillcfg(fn);
	simpljmp(fn);
	fillcfg(fn);
	SQ_ASSERT(fn->rpo[0] == fn->start);
	for (n=0;; n++) {
		if (n == fn->nblk-1) {
			fn->rpo[n]->link = 0;
			break;
		} else
			fn->rpo[n]->link = fn->rpo[n+1];
	}
	if (G(objmode)) {
		macho_emitfn(fn, G(macho_ctx));
	} else if (!G(dbg)) {
		GC(T).emitfn(fn, G(outf));
		fprintf(G(outf), "/* end function %s */\n\n", fn->name);
	} else {
		fprintf(stderr, "\n");
	}
	freeall();
}

static void
qbe_main_dbgfile(char *fn)
{
	if (!G(objmode))
		emitdbgfile(fn, G(outf));
}
#undef G
/*** END FILE: main.c ***/
/*** START FILE: mem.c ***/
/* skipping all.h */

typedef struct Range Range;
typedef struct Store Store;
typedef struct Slot Slot;

/* require use, maintains use counts */
void
promote(Fn *fn)
{
	Blk *b;
	Ins *i, *l;
	Tmp *t;
	Use *u, *ue;
	int s, k;

	/* promote uniform stack slots to temporaries */
	b = fn->start;
	for (i=b->ins; i<&b->ins[b->nins]; i++) {
		if (Oalloc > i->op || i->op > Oalloc1)
			continue;
		/* specific to NAlign == 3 */
		SQ_ASSERT(rtype(i->to) == RTmp);
		t = &fn->tmp[i->to.val];
		if (t->ndef != 1)
			goto Skip;
		k = -1;
		s = -1;
		for (u=t->use; u<&t->use[t->nuse]; u++) {
			if (u->type != UIns)
				goto Skip;
			l = u->u.ins;
			if (isload(l->op))
			if (s == -1 || s == loadsz(l)) {
				s = loadsz(l);
				continue;
			}
			if (isstore(l->op))
			if (req(i->to, l->arg[1]) && !req(i->to, l->arg[0]))
			if (s == -1 || s == storesz(l))
			if (k == -1 || k == optab[l->op].argcls[0][0]) {
				s = storesz(l);
				k = optab[l->op].argcls[0][0];
				continue;
			}
			goto Skip;
		}
		/* get rid of the alloc and replace uses */
		*i = (Ins){.op = Onop};
		t->ndef--;
		ue = &t->use[t->nuse];
		for (u=t->use; u!=ue; u++) {
			l = u->u.ins;
			if (isstore(l->op)) {
				l->cls = k;
				l->op = Ocopy;
				l->to = l->arg[1];
				l->arg[1] = NULL_R;
				t->nuse--;
				t->ndef++;
			} else {
				if (k == -1)
					err("slot %%%s is read but never stored to",
						fn->tmp[l->arg[0].val].name);
				/* try to turn loads into copies so we
				 * can eliminate them later */
				switch(l->op) {
				case Oloadsw:
				case Oloaduw:
					if (k == Kl)
						goto Extend;
					/* fall through */
				case Oload:
					if (KBASE(k) != KBASE(l->cls))
						l->op = Ocast;
					else
						l->op = Ocopy;
					break;
				default:
				Extend:
					l->op = Oextsb + (l->op - Oloadsb);
					break;
				}
			}
		}
	Skip:;
	}
	if (GC(debug)['M']) {
		fprintf(stderr, "\n> After slot promotion:\n");
		printfn(fn, stderr);
	}
}

/* [a, b) with 0 <= a */
struct Range {
	int a, b;
};

struct Store {
	int ip;
	Ins *i;
};

struct Slot {
	int t;
	int sz;
	bits m;
	bits l;
	Range r;
	Slot *s;
	Store *st;
	int nst;
};

static inline int
qbe_mem_rin(Range r, int n)
{
	return r.a <= n && n < r.b;
}

static inline int
qbe_mem_rovlap(Range r0, Range r1)
{
	return r0.b && r1.b && r0.a < r1.b && r1.a < r0.b;
}

static void
qbe_mem_radd(Range *r, int n)
{
	if (!r->b)
		*r = (Range){n, n+1};
	else if (n < r->a)
		r->a = n;
	else if (n >= r->b)
		r->b = n+1;
}

static int
qbe_mem_slot(Slot **ps, int64_t *off, Ref r, Fn *fn, Slot *sl)
{
	Alias a;
	Tmp *t;

	getalias(&a, r, fn);
	if (a.type != ALoc)
		return 0;
	t = &fn->tmp[a.base];
	if (t->visit < 0)
		return 0;
	*off = a.offset;
	*ps = &sl[t->visit];
	return 1;
}

static void
qbe_mem_load(Ref r, bits x, int ip, Fn *fn, Slot *sl)
{
	int64_t off;
	Slot *s;

	if (qbe_mem_slot(&s, &off, r, fn, sl)) {
		s->l |= x << off;
		s->l &= s->m;
		if (s->l)
			qbe_mem_radd(&s->r, ip);
	}
}

static void
qbe_mem_store(Ref r, bits x, int ip, Ins *i, Fn *fn, Slot *sl)
{
	int64_t off;
	Slot *s;

	if (qbe_mem_slot(&s, &off, r, fn, sl)) {
		if (s->l) {
			qbe_mem_radd(&s->r, ip);
			s->l &= ~(x << off);
		} else {
			vgrow(&s->st, ++s->nst);
			s->st[s->nst-1].ip = ip;
			s->st[s->nst-1].i = i;
		}
	}
}

static int
qbe_mem_scmp(const void *pa, const void *pb)
{
	Slot *a, *b;

	a = (Slot *)pa, b = (Slot *)pb;
	if (a->sz != b->sz)
		return b->sz - a->sz;
	return a->r.a - b->r.a;
}

static void
qbe_mem_maxrpo(Blk *hd, Blk *b)
{
	if (hd->loop < (int)b->id)
		hd->loop = b->id;
}

void
coalesce(Fn *fn)
{
	Range r, *br;
	Slot *s, *s0, *sl;
	Blk *b, **ps, *succ[3];
	Ins *i, **bl;
	Use *u;
	Tmp *t, *ts;
	Ref *arg;
	bits x;
	int64_t off0, off1;
	int n, m, ip, sz, nsl, nbl, *stk;
	uint total, freed, fused;

	/* minimize the stack usage
	 * by coalescing slots
	 */
	nsl = 0;
	sl = vnew(0, sizeof sl[0], PHeap);
	for (n=Tmp0; n<fn->ntmp; n++) {
		t = &fn->tmp[n];
		t->visit = -1;
		if (t->alias.type == ALoc)
		if (t->alias.slot == &t->alias)
		if (t->bid == fn->start->id)
		if (t->alias.u.loc.sz != -1) {
			t->visit = nsl;
			vgrow(&sl, ++nsl);
			s = &sl[nsl-1];
			s->t = n;
			s->sz = t->alias.u.loc.sz;
			s->m = t->alias.u.loc.m;
			s->s = 0;
			s->st = vnew(0, sizeof s->st[0], PHeap);
			s->nst = 0;
		}
	}

	/* one-pass liveness analysis */
	for (b=fn->start; b; b=b->link)
		b->loop = -1;
	loopiter(fn, qbe_mem_maxrpo);
	nbl = 0;
	bl = vnew(0, sizeof bl[0], PHeap);
	br = emalloc(fn->nblk * sizeof br[0]);
	ip = INT_MAX - 1;
	for (n=fn->nblk-1; n>=0; n--) {
		b = fn->rpo[n];
		succ[0] = b->s1;
		succ[1] = b->s2;
		succ[2] = 0;
		br[n].b = ip--;
		for (s=sl; s<&sl[nsl]; s++) {
			s->l = 0;
			for (ps=succ; *ps; ps++) {
				m = (*ps)->id;
				if (m > n && qbe_mem_rin(s->r, br[m].a)) {
					s->l = s->m;
					qbe_mem_radd(&s->r, ip);
				}
			}
		}
		if (b->jmp.type == Jretc)
			qbe_mem_load(b->jmp.arg, -1, --ip, fn, sl);
		for (i=&b->ins[b->nins]; i!=b->ins;) {
			--i;
			arg = i->arg;
			if (i->op == Oargc) {
				qbe_mem_load(arg[1], -1, --ip, fn, sl);
			}
			if (isload(i->op)) {
				x = BIT(loadsz(i)) - 1;
				qbe_mem_load(arg[0], x, --ip, fn, sl);
			}
			if (isstore(i->op)) {
				x = BIT(storesz(i)) - 1;
				qbe_mem_store(arg[1], x, ip--, i, fn, sl);
			}
			if (i->op == Oblit0) {
				SQ_ASSERT((i+1)->op == Oblit1);
				SQ_ASSERT(rtype((i+1)->arg[0]) == RInt);
				sz = abs(rsval((i+1)->arg[0]));
				x = sz >= NBit ? (bits)-1 : BIT(sz) - 1;
				qbe_mem_store(arg[1], x, ip--, i, fn, sl);
				qbe_mem_load(arg[0], x, ip, fn, sl);
				vgrow(&bl, ++nbl);
				bl[nbl-1] = i;
			}
		}
		for (s=sl; s<&sl[nsl]; s++)
			if (s->l) {
				qbe_mem_radd(&s->r, ip);
				if (b->loop != -1) {
					SQ_ASSERT(b->loop >= n);
					qbe_mem_radd(&s->r, br[b->loop].b - 1);
				}
			}
		br[n].a = ip;
	}
	qbe_free(br);

	/* kill dead stores */
	for (s=sl; s<&sl[nsl]; s++)
		for (n=0; n<s->nst; n++)
			if (!qbe_mem_rin(s->r, s->st[n].ip)) {
				i = s->st[n].i;
				if (i->op == Oblit0)
					*(i+1) = (Ins){.op = Onop};
				*i = (Ins){.op = Onop};
			}

	/* kill slots with an empty live range */
	total = 0;
	freed = 0;
	stk = vnew(0, sizeof stk[0], PHeap);
	n = 0;
	for (s=s0=sl; s<&sl[nsl]; s++) {
		total += s->sz;
		if (!s->r.b) {
			vfree(s->st);
			vgrow(&stk, ++n);
			stk[n-1] = s->t;
			freed += s->sz;
		} else
			*s0++ = *s;
	}
	nsl = s0-sl;
	if (GC(debug)['M']) {
		fputs("\n> Slot coalescing:\n", stderr);
		if (n) {
			fputs("\tkill [", stderr);
			for (m=0; m<n; m++)
				fprintf(stderr, " %%%s",
					fn->tmp[stk[m]].name);
			fputs(" ]\n", stderr);
		}
	}
	while (n--) {
		t = &fn->tmp[stk[n]];
		SQ_ASSERT(t->ndef == 1 && t->def);
		i = t->def;
		if (isload(i->op)) {
			i->op = Ocopy;
			i->arg[0] = UNDEF;
			continue;
		}
		*i = (Ins){.op = Onop};
		for (u=t->use; u<&t->use[t->nuse]; u++) {
			if (u->type == UJmp) {
				b = fn->rpo[u->bid];
				SQ_ASSERT(isret(b->jmp.type));
				b->jmp.type = Jret0;
				b->jmp.arg = NULL_R;
				continue;
			}
			SQ_ASSERT(u->type == UIns);
			i = u->u.ins;
			if (!req(i->to, NULL_R)) {
				SQ_ASSERT(rtype(i->to) == RTmp);
				vgrow(&stk, ++n);
				stk[n-1] = i->to.val;
			} else if (isarg(i->op)) {
				SQ_ASSERT(i->op == Oargc);
				i->arg[1] = CON_Z;  /* crash */
			} else {
				if (i->op == Oblit0)
					*(i+1) = (Ins){.op = Onop};
				*i = (Ins){.op = Onop};
			}
		}
	}
	vfree(stk);

	/* fuse slots by decreasing size */
	qsort(sl, nsl, sizeof *sl, qbe_mem_scmp);
	fused = 0;
	for (n=0; n<nsl; n++) {
		s0 = &sl[n];
		if (s0->s)
			continue;
		s0->s = s0;
		r = s0->r;
		for (s=s0+1; s<&sl[nsl]; s++) {
			if (s->s || !s->r.b)
				goto Skip;
			if (qbe_mem_rovlap(r, s->r))
				/* O(n); can be approximated
				 * by 'goto Skip;' if need be
				 */
				for (m=n; &sl[m]<s; m++)
					if (sl[m].s == s0)
					if (qbe_mem_rovlap(sl[m].r, s->r))
						goto Skip;
			qbe_mem_radd(&r, s->r.a);
			qbe_mem_radd(&r, s->r.b - 1);
			s->s = s0;
			fused += s->sz;
		Skip:;
		}
	}

	/* substitute fused slots */
	for (s=sl; s<&sl[nsl]; s++) {
		t = &fn->tmp[s->t];
		/* the visit link is stale,
		 * reset it before the qbe_mem_slot()
		 * calls below
		 */
		t->visit = s-sl;
		SQ_ASSERT(t->ndef == 1 && t->def);
		if (s->s == s)
			continue;
		*t->def = (Ins){.op = Onop};
		ts = &fn->tmp[s->s->t];
		SQ_ASSERT(t->bid == ts->bid);
		if (t->def < ts->def) {
			/* make sure the slot we
			 * selected has a def that
			 * dominates its new uses
			 */
			*t->def = *ts->def;
			*ts->def = (Ins){.op = Onop};
			ts->def = t->def;
		}
		for (u=t->use; u<&t->use[t->nuse]; u++) {
			if (u->type == UJmp) {
				b = fn->rpo[u->bid];
				b->jmp.arg = TMP(s->s->t);
				continue;
			}
			SQ_ASSERT(u->type == UIns);
			arg = u->u.ins->arg;
			for (n=0; n<2; n++)
				if (req(arg[n], TMP(s->t)))
					arg[n] = TMP(s->s->t);
		}
	}

	/* fix newly overlapping blits */
	for (n=0; n<nbl; n++) {
		i = bl[n];
		if (i->op == Oblit0)
		if (qbe_mem_slot(&s, &off0, i->arg[0], fn, sl))
		if (qbe_mem_slot(&s0, &off1, i->arg[1], fn, sl))
		if (s->s == s0->s) {
			if (off0 < off1) {
				sz = rsval((i+1)->arg[0]);
				SQ_ASSERT(sz >= 0);
				(i+1)->arg[0] = INT(-sz);
			} else if (off0 == off1) {
				*i = (Ins){.op = Onop};
				*(i+1) = (Ins){.op = Onop};
			}
		}
	}
	vfree(bl);

	if (GC(debug)['M']) {
		for (s0=sl; s0<&sl[nsl]; s0++) {
			if (s0->s != s0)
				continue;
			fprintf(stderr, "\tfuse (% 3db) [", s0->sz);
			for (s=s0; s<&sl[nsl]; s++) {
				if (s->s != s0)
					continue;
				fprintf(stderr, " %%%s", fn->tmp[s->t].name);
				if (s->r.b)
					fprintf(stderr, "[%d,%d)",
						s->r.a-ip, s->r.b-ip);
				else
					fputs("{}", stderr);
			}
			fputs(" ]\n", stderr);
		}
		fprintf(stderr, "\tsums %u/%u/%u (killed/fused/total)\n\n",
			freed, fused, total);
		printfn(fn, stderr);
	}

	for (s=sl; s<&sl[nsl]; s++)
		vfree(s->st);
	vfree(sl);
}
#undef G
/*** END FILE: mem.c ***/
/*** START FILE: parse.c ***/
/* skipping all.h */
#include <ctype.h>
#include <stdarg.h>

#define G(x) global_context.parse__##x

enum {
	Ksb = 4, /* matches Oarg/Opar/Jret */
	Kub,
	Ksh,
	Kuh,
	Kc,
	K0,

	Ke = -2, /* erroneous mode */
	Km = Kl, /* memory pointer */
};

static Op optab[NOp] = {
#undef F
#define F(cf, hi, id, co, as, im, ic, lg, cv, pn) \
	.canfold = cf, \
	.hasid = hi, .idval = id, \
	.commutes = co, .assoc = as, \
	.idemp = im, \
	.cmpeqwl = ic, .cmplgtewl = lg, .eqval = cv, \
	.pinned = pn
#define O(op, k, flags) [O##op]={.name = #op, .argcls = k, flags},
/* ------------------------------------------------------------including ops.h */
#ifndef X /* amd64 */
	#define X(NMemArgs, SetsZeroFlag, LeavesFlags)
#endif

#ifndef V /* riscv64 */
	#define V(Imm)
#endif

#ifndef F
#define F(a,b,c,d,e,f,g,h,i,j)
#endif

#define T(a,b,c,d,e,f,g,h) {                          \
	{[Kw]=K##a, [Kl]=K##b, [Ks]=K##c, [Kd]=K##d}, \
	{[Kw]=K##e, [Kl]=K##f, [Ks]=K##g, [Kd]=K##h}  \
}

/*********************/
/* PUBLIC OPERATIONS */
/*********************/

/*                                can fold                        */
/*                                | has identity                  */
/*                                | | identity value for arg[1]   */
/*                                | | | commutative               */
/*                                | | | | associative             */
/*                                | | | | | idempotent            */
/*                                | | | | | | c{eq,ne}[wl]        */
/*                                | | | | | | | c[us][gl][et][wl] */
/*                                | | | | | | | | value if = args */
/*                                | | | | | | | | | pinned        */
/* Arithmetic and Bits            v v v v v v v v v v             */
O(add,     T(w,l,s,d, w,l,s,d), F(1,1,0,1,1,0,0,0,0,0)) X(2,1,0) V(1)
O(sub,     T(w,l,s,d, w,l,s,d), F(1,1,0,0,0,0,0,0,0,0)) X(2,1,0) V(0)
O(neg,     T(w,l,s,d, x,x,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(1,1,0) V(0)
O(div,     T(w,l,s,d, w,l,s,d), F(1,1,1,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(rem,     T(w,l,e,e, w,l,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(udiv,    T(w,l,e,e, w,l,e,e), F(1,1,1,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(urem,    T(w,l,e,e, w,l,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(mul,     T(w,l,s,d, w,l,s,d), F(1,1,1,1,0,0,0,0,0,0)) X(2,0,0) V(0)
O(and,     T(w,l,e,e, w,l,e,e), F(1,0,0,1,1,1,0,0,0,0)) X(2,1,0) V(1)
O(or,      T(w,l,e,e, w,l,e,e), F(1,1,0,1,1,1,0,0,0,0)) X(2,1,0) V(1)
O(xor,     T(w,l,e,e, w,l,e,e), F(1,1,0,1,1,0,0,0,0,0)) X(2,1,0) V(1)
O(sar,     T(w,l,e,e, w,w,e,e), F(1,1,0,0,0,0,0,0,0,0)) X(1,1,0) V(1)
O(shr,     T(w,l,e,e, w,w,e,e), F(1,1,0,0,0,0,0,0,0,0)) X(1,1,0) V(1)
O(shl,     T(w,l,e,e, w,w,e,e), F(1,1,0,0,0,0,0,0,0,0)) X(1,1,0) V(1)

/* Comparisons */
O(ceqw,    T(w,w,e,e, w,w,e,e), F(1,1,1,1,0,0,1,0,1,0)) X(0,1,0) V(0)
O(cnew,    T(w,w,e,e, w,w,e,e), F(1,1,0,1,0,0,1,0,0,0)) X(0,1,0) V(0)
O(csgew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csgtw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(cslew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csltw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)
O(cugew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cugtw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(culew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cultw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)

O(ceql,    T(l,l,e,e, l,l,e,e), F(1,0,0,1,0,0,1,0,1,0)) X(0,1,0) V(0)
O(cnel,    T(l,l,e,e, l,l,e,e), F(1,0,0,1,0,0,1,0,0,0)) X(0,1,0) V(0)
O(csgel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csgtl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(cslel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csltl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)
O(cugel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cugtl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(culel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cultl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)

O(ceqs,    T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cges,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cgts,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cles,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(clts,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cnes,    T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cos,     T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cuos,    T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)

O(ceqd,    T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cged,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cgtd,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cled,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cltd,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cned,    T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cod,     T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cuod,    T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)

/* Memory */
O(storeb,  T(w,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(storeh,  T(w,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(storew,  T(w,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(storel,  T(l,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(stores,  T(s,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(stored,  T(d,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

O(loadsb,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loadub,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loadsh,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loaduh,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loadsw,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loaduw,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(load,    T(m,m,m,m, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

/* Extensions and Truncations */
O(extsb,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extub,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extsh,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extuh,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extsw,   T(e,w,e,e, e,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extuw,   T(e,w,e,e, e,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

O(exts,    T(e,e,e,s, e,e,e,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(truncd,  T(e,e,d,e, e,e,x,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(stosi,   T(s,s,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(stoui,   T(s,s,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(dtosi,   T(d,d,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(dtoui,   T(d,d,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(swtof,   T(e,e,w,w, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(uwtof,   T(e,e,w,w, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(sltof,   T(e,e,l,l, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(ultof,   T(e,e,l,l, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(cast,    T(s,d,w,l, x,x,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

/* Stack Allocation */
O(alloc4,  T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(alloc8,  T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(alloc16, T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)

/* Variadic Function Helpers */
O(vaarg,   T(m,m,m,m, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(vastart, T(m,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)

O(copy,    T(w,l,s,d, x,x,x,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

/* Debug */
O(dbgloc,  T(w,e,e,e, w,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

/****************************************/
/* INTERNAL OPERATIONS (keep nop first) */
/****************************************/

/* Miscellaneous and Architecture-Specific Operations */
O(nop,     T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(addr,    T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(blit0,   T(m,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,1,0) V(0)
O(blit1,   T(w,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,1,0) V(0)
O(sel0,    T(w,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(sel1,    T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(swap,    T(w,l,s,d, w,l,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(1,0,0) V(0)
O(sign,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(salloc,  T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xidiv,   T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(1,0,0) V(0)
O(xdiv,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(1,0,0) V(0)
O(xcmp,    T(w,l,s,d, w,l,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(1,1,0) V(0)
O(xtest,   T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(1,1,0) V(0)
O(acmp,    T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(acmn,    T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(afcmp,   T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(reqz,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(rnez,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)

/* Arguments, Parameters, and Calls */
O(par,     T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parsb,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parub,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parsh,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(paruh,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parc,    T(e,x,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(pare,    T(e,x,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(arg,     T(w,l,s,d, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argsb,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argub,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argsh,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(arguh,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argc,    T(e,x,e,e, e,l,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(arge,    T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argv,    T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(call,    T(m,m,m,m, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)

/* Flags Setting */
O(flagieq,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagine,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagisge, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagisgt, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagisle, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagislt, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiuge, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiugt, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiule, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiult, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfeq,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfge,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfgt,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfle,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagflt,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfne,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfo,   T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfuo,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

/* Backend Flag Select (Condition Move) */
O(xselieq,  T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseline,  T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselisge, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselisgt, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselisle, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselislt, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliuge, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliugt, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliule, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliult, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfeq,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfge,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfgt,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfle,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselflt,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfne,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfo,   T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfuo,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)

#undef T
#undef X
#undef V
#undef O

/*
| column -t -o ' '
*/
/* ------------------------------------------------------------end of ops.h */
#undef F
};

typedef enum {
	PXXX,
	PLbl,
	PPhi,
	PIns,
	PEnd,
} PState;



















static void
qbe_parse_closeblk(void)
{
	idup(G(curb), GC(insb), GC(curi)-GC(insb));
	G(blink) = &G(curb)->link;
	GC(curi) = GC(insb);
}

#define err_ps(...) do { err_(__VA_ARGS__); return (PState){0}; } while(0)


static int
qbe_parse_usecheck(Ref r, int k, Fn *fn)
{
	return rtype(r) != RTmp || fn->tmp[r.val].cls == k
		|| (fn->tmp[r.val].cls == Kl && k == Kw);
}

static void
qbe_parse_typecheck(Fn *fn)
{
	Blk *b;
	Phi *p;
	Ins *i;
	uint n;
	int k;
	Tmp *t;
	Ref r;
	BSet pb[1], ppb[1];

	fillpreds(fn);
	bsinit(pb, fn->nblk);
	bsinit(ppb, fn->nblk);
	for (b=fn->start; b; b=b->link) {
		for (p=b->phi; p; p=p->link)
			fn->tmp[p->to.val].cls = p->cls;
		for (i=b->ins; i<&b->ins[b->nins]; i++)
			if (rtype(i->to) == RTmp) {
				t = &fn->tmp[i->to.val];
				if (clsmerge(&t->cls, i->cls))
					err("temporary %%%s is assigned with"
						" multiple types", t->name);
			}
	}
	for (b=fn->start; b; b=b->link) {
		bszero(pb);
		for (n=0; n<b->npred; n++)
			bsset(pb, b->pred[n]->id);
		for (p=b->phi; p; p=p->link) {
			bszero(ppb);
			t = &fn->tmp[p->to.val];
			for (n=0; n<p->narg; n++) {
				k = t->cls;
				if (bshas(ppb, p->blk[n]->id))
					err("multiple entries for @%s in phi %%%s",
						p->blk[n]->name, t->name);
				if (!qbe_parse_usecheck(p->arg[n], k, fn))
					err("invalid type for operand %%%s in phi %%%s",
						fn->tmp[p->arg[n].val].name, t->name);
				bsset(ppb, p->blk[n]->id);
			}
			if (!bsequal(pb, ppb))
				err("predecessors not matched in phi %%%s", t->name);
		}
		for (i=b->ins; i<&b->ins[b->nins]; i++)
			for (n=0; n<2; n++) {
				k = optab[i->op].argcls[n][i->cls];
				r = i->arg[n];
				t = &fn->tmp[r.val];
				if (k == Ke)
					err("invalid instruction type in %s",
						optab[i->op].name);
				if (rtype(r) == RType)
					continue;
				if (rtype(r) != -1 && k == Kx)
					err("no %s operand expected in %s",
						n == 1 ? "second" : "first",
						optab[i->op].name);
				if (rtype(r) == -1 && k != Kx)
					err("missing %s operand in %s",
						n == 1 ? "second" : "first",
						optab[i->op].name);
				if (!qbe_parse_usecheck(r, k, fn))
					err("invalid type for %s operand %%%s in %s",
						n == 1 ? "second" : "first",
						t->name, optab[i->op].name);
			}
		r = b->jmp.arg;
		if (isret(b->jmp.type)) {
			if (b->jmp.type == Jretc)
				k = Kl;
			else if (b->jmp.type >= Jretsb)
				k = Kw;
			else
				k = b->jmp.type - Jretw;
			if (!qbe_parse_usecheck(r, k, fn))
				goto JErr;
		}
		if (b->jmp.type == Jjnz && !qbe_parse_usecheck(r, Kw, fn))
		JErr:
			err("invalid type for jump argument %%%s in block @%s",
				fn->tmp[r.val].name, b->name);
		if (b->s1 && b->s1->jmp.type == Jxxx)
			err("block @%s is used undefined", b->s1->name);
		if (b->s2 && b->s2->jmp.type == Jxxx)
			err("block @%s is used undefined", b->s2->name);
	}
}









static void
qbe_parse_printcon(Con *c, FILE *f)
{
	switch (c->type) {
	case CUndef:
		break;
	case CAddr:
		if (c->sym.type == SThr)
			fprintf(f, "thread ");
		fprintf(f, "$%s", str(c->sym.id));
		if (c->bits.i)
			fprintf(f, "%+"PRIi64, c->bits.i);
		break;
	case CBits:
		if (c->flt == 1)
			fprintf(f, "s_%f", c->bits.s);
		else if (c->flt == 2)
			fprintf(f, "d_%lf", c->bits.d);
		else
			fprintf(f, "%"PRIi64, c->bits.i);
		break;
	}
}

static char* field_type_name[] = {
    [FEnd] = "End",  //
    [Fb] = "b",      //
    [Fh] = "h",      //
    [Fw] = "w",      //
    [Fl] = "l",      //
    [Fs] = "s",      //
    [Fd] = "d",      //
    [FPad] = "Pad",  //
    [FTyp] = "Typ",  //
};

void printtyp(Typ* ty, FILE* f) {
  fprintf(f, "type :%s\n", ty->name);
  fprintf(f, "  isdark: %d\n", ty->isdark);
  fprintf(f, "  isunion: %d\n", ty->isunion);
  fprintf(f, "  align: %d\n", ty->align);
  fprintf(f, "  size: %u\n", (uint)ty->size);
  fprintf(f, "  nunion: %u\n", ty->nunion);
  for (uint i = 0; i < ty->nunion; ++i) {
    fprintf(f, "  union %d\n", i);
    Field* fld = ty->fields[i];
    for (int j = 0; j < NField+1; ++j) {
      if (fld[j].type == FTyp) {
        fprintf(f, "    field %d: %s => %s\n", j, field_type_name[fld[j].type],
                GC(typ)[fld[j].len].name);
      } else {
        fprintf(f, "    field %d: %s len %u\n", j, field_type_name[fld[j].type], fld[j].len);
      }
      if (fld[j].type == FEnd)
        break;
    }
  }
}

void
printref(Ref r, Fn *fn, FILE *f)
{
	int i;
	Mem *m;

	switch (rtype(r)) {
	case RTmp:
		if (r.val < Tmp0)
			fprintf(f, "R%d", r.val);
		else
			fprintf(f, "%%%s", fn->tmp[r.val].name);
		break;
	case RCon:
		if (req(r, UNDEF))
			fprintf(f, "UNDEF");
		else
			qbe_parse_printcon(&fn->con[r.val], f);
		break;
	case RSlot:
		fprintf(f, "S%d", rsval(r));
		break;
	case RCall:
		fprintf(f, "%04x", r.val);
		break;
	case RType:
		fprintf(f, ":%s", GC(typ)[r.val].name);
		break;
	case RMem:
		i = 0;
		m = &fn->mem[r.val];
		fputc('[', f);
		if (m->offset.type != CUndef) {
			qbe_parse_printcon(&m->offset, f);
			i = 1;
		}
		if (!req(m->base, NULL_R)) {
			if (i)
				fprintf(f, " + ");
			printref(m->base, fn, f);
			i = 1;
		}
		if (!req(m->index, NULL_R)) {
			if (i)
				fprintf(f, " + ");
			fprintf(f, "%d * ", m->scale);
			printref(m->index, fn, f);
		}
		fputc(']', f);
		break;
	case RInt:
		fprintf(f, "%d", rsval(r));
		break;
	case -1:
		fprintf(f, "R");
		break;
	}
}

void
printfn(Fn *fn, FILE *f)
{
	static char ktoc[] = "wlsd";
	static char *jtoa[NJmp] = {
	#define X(j) [J##j] = #j,
		JMPS(X)
	#undef X
	};
	Blk *b;
	Phi *p;
	Ins *i;
	uint n;

	fprintf(f, "function $%s() {\n", fn->name);
	for (b=fn->start; b; b=b->link) {
		fprintf(f, "@%s\n", b->name);
		for (p=b->phi; p; p=p->link) {
			fprintf(f, "\t");
			printref(p->to, fn, f);
			fprintf(f, " =%c phi ", ktoc[p->cls]);
			SQ_ASSERT(p->narg);
			for (n=0;; n++) {
				fprintf(f, "@%s ", p->blk[n]->name);
				printref(p->arg[n], fn, f);
				if (n == p->narg-1) {
					fprintf(f, "\n");
					break;
				} else
					fprintf(f, ", ");
			}
		}
		for (i=b->ins; i<&b->ins[b->nins]; i++) {
			fprintf(f, "\t");
			if (!req(i->to, NULL_R)) {
				printref(i->to, fn, f);
				fprintf(f, " =%c ", ktoc[i->cls]);
			}
			SQ_ASSERT(optab[i->op].name);
			fprintf(f, "%s", optab[i->op].name);
			if (req(i->to, NULL_R))
				switch (i->op) {
				case Oarg:
				case Oswap:
				case Oxcmp:
				case Oacmp:
				case Oacmn:
				case Oafcmp:
				case Oxtest:
				case Oxdiv:
				case Oxidiv:
					fputc(ktoc[i->cls], f);
				}
			if (!req(i->arg[0], NULL_R)) {
				fprintf(f, " ");
				printref(i->arg[0], fn, f);
			}
			if (!req(i->arg[1], NULL_R)) {
				fprintf(f, ", ");
				printref(i->arg[1], fn, f);
			}
			fprintf(f, "\n");
		}
		switch (b->jmp.type) {
		case Jret0:
		case Jretsb:
		case Jretub:
		case Jretsh:
		case Jretuh:
		case Jretw:
		case Jretl:
		case Jrets:
		case Jretd:
		case Jretc:
			fprintf(f, "\t%s", jtoa[b->jmp.type]);
			if (b->jmp.type != Jret0 || !req(b->jmp.arg, NULL_R)) {
				fprintf(f, " ");
				printref(b->jmp.arg, fn, f);
			}
			if (b->jmp.type == Jretc)
				fprintf(f, ", :%s", GC(typ)[fn->retty].name);
			fprintf(f, "\n");
			break;
		case Jhlt:
			fprintf(f, "\thlt\n");
			break;
		case Jjmp:
			if (b->s1 != b->link)
				fprintf(f, "\tjmp @%s\n", b->s1->name);
			break;
		default:
			fprintf(f, "\t%s ", jtoa[b->jmp.type]);
			if (b->jmp.type == Jjnz) {
				printref(b->jmp.arg, fn, f);
				fprintf(f, ", ");
			}
			SQ_ASSERT(b->s1 && b->s2);
			fprintf(f, "@%s, @%s\n", b->s1->name, b->s2->name);
			break;
		}
	}
	fprintf(f, "}\n");
}
#undef G
/*** END FILE: parse.c ***/
/*** START FILE: rega.c ***/
/* skipping all.h */

#define G(x) global_context.rega__##x

#ifdef TEST_PMOV
	#undef assert
	#define SQ_ASSERT(x) assert_test(#x, x)
#endif

typedef struct RMap RMap;

struct RMap {
	int t[Tmp0];
	int r[Tmp0];
	int w[Tmp0];   /* wait list, for unmatched hints */
	BSet b[1];
	int n;
};

static int *
qbe_rega_hint(int t)
{
	return &G(tmp)[phicls(t, G(tmp))].hint.r;
}

static void
qbe_rega_sethint(int t, int r)
{
	Tmp *p;

	p = &G(tmp)[phicls(t, G(tmp))];
	if (p->hint.r == -1 || p->hint.w > G(loop)) {
		p->hint.r = r;
		p->hint.w = G(loop);
		G(tmp)[t].visit = -1;
	}
}

static void
qbe_rega_rcopy(RMap *ma, RMap *mb)
{
	memcpy(ma->t, mb->t, sizeof ma->t);
	memcpy(ma->r, mb->r, sizeof ma->r);
	memcpy(ma->w, mb->w, sizeof ma->w);
	bscopy(ma->b, mb->b);
	ma->n = mb->n;
}

static int
qbe_rega_rfind(RMap *m, int t)
{
	int i;

	for (i=0; i<m->n; i++)
		if (m->t[i] == t)
			return m->r[i];
	return -1;
}

static Ref
qbe_rega_rref(RMap *m, int t)
{
	int r, s;

	r = qbe_rega_rfind(m, t);
	if (r == -1) {
		s = G(tmp)[t].slot;
		SQ_ASSERT(s != -1 && "should have spilled");
		return SLOT(s);
	} else
		return TMP(r);
}

static void
qbe_rega_radd(RMap *m, int t, int r)
{
	SQ_ASSERT((t >= Tmp0 || t == r) && "invalid temporary");
	SQ_ASSERT(((GC(T).gpr0 <= r && r < GC(T).gpr0 + GC(T).ngpr)
		|| (GC(T).fpr0 <= r && r < GC(T).fpr0 + GC(T).nfpr))
		&& "invalid register");
	SQ_ASSERT(!bshas(m->b, t) && "temporary has mapping");
	SQ_ASSERT(!bshas(m->b, r) && "register already allocated");
	SQ_ASSERT(m->n <= GC(T).ngpr+GC(T).nfpr && "too many mappings");
	bsset(m->b, t);
	bsset(m->b, r);
	m->t[m->n] = t;
	m->r[m->n] = r;
	m->n++;
	G(regu) |= BIT(r);
}

static Ref
qbe_rega_ralloctry(RMap *m, int t, int try)
{
	bits regs;
	int h, r, r0, r1;

	if (t < Tmp0) {
		SQ_ASSERT(bshas(m->b, t));
		return TMP(t);
	}
	if (bshas(m->b, t)) {
		r = qbe_rega_rfind(m, t);
		SQ_ASSERT(r != -1);
		return TMP(r);
	}
	r = G(tmp)[t].visit;
	if (r == -1 || bshas(m->b, r))
		r = *qbe_rega_hint(t);
	if (r == -1 || bshas(m->b, r)) {
		if (try)
			return NULL_R;
		regs = G(tmp)[phicls(t, G(tmp))].hint.m;
		regs |= m->b->t[0];
		if (KBASE(G(tmp)[t].cls) == 0) {
			r0 = GC(T).gpr0;
			r1 = r0 + GC(T).ngpr;
		} else {
			r0 = GC(T).fpr0;
			r1 = r0 + GC(T).nfpr;
		}
		for (r=r0; r<r1; r++)
			if (!(regs & BIT(r)))
				goto Found;
		for (r=r0; r<r1; r++)
			if (!bshas(m->b, r))
				goto Found;
		die("no more regs");
	}
Found:
	qbe_rega_radd(m, t, r);
	qbe_rega_sethint(t, r);
	G(tmp)[t].visit = r;
	h = *qbe_rega_hint(t);
	if (h != -1 && h != r)
		m->w[h] = t;
	return TMP(r);
}

static inline Ref
qbe_rega_ralloc(RMap *m, int t)
{
	return qbe_rega_ralloctry(m, t, 0);
}

static int
qbe_rega_rfree(RMap *m, int t)
{
	int i, r;

	SQ_ASSERT(t >= Tmp0 || !(BIT(t) & GC(T).rglob));
	if (!bshas(m->b, t))
		return -1;
	for (i=0; m->t[i] != t; i++)
		SQ_ASSERT(i+1 < m->n);
	r = m->r[i];
	bsclr(m->b, t);
	bsclr(m->b, r);
	m->n--;
	memmove(&m->t[i], &m->t[i+1], (m->n-i) * sizeof m->t[0]);
	memmove(&m->r[i], &m->r[i+1], (m->n-i) * sizeof m->r[0]);
	SQ_ASSERT(t >= Tmp0 || t == r);
	return r;
}

static void
qbe_rega_mdump(RMap *m)
{
	int i;

	for (i=0; i<m->n; i++)
		if (m->t[i] >= Tmp0)
			fprintf(stderr, " (%s, R%d)",
				G(tmp)[m->t[i]].name,
				m->r[i]);
	fprintf(stderr, "\n");
}

static void
qbe_rega_pmadd(Ref src, Ref dst, int k)
{
	if (G(npm) == Tmp0)
		die("cannot have more moves than registers");
	G(pm)[G(npm)].src = src;
	G(pm)[G(npm)].dst = dst;
	G(pm)[G(npm)].cls = k;
	G(npm)++;
}

enum PMStat { ToMove, Moving, Moved };

static int
qbe_rega_pmrec(enum PMStat *status, int i, int *k)
{
	int j, c;

	/* note, this routine might emit
	 * too many large instructions
	 */
	if (req(G(pm)[i].src, G(pm)[i].dst)) {
		status[i] = Moved;
		return -1;
	}
	SQ_ASSERT(KBASE(G(pm)[i].cls) == KBASE(*k));
	SQ_ASSERT((Kw|Kl) == Kl && (Ks|Kd) == Kd);
	*k |= G(pm)[i].cls;
	for (j=0; j<G(npm); j++)
		if (req(G(pm)[j].dst, G(pm)[i].src))
			break;
	switch (j == G(npm) ? Moved : status[j]) {
	case Moving:
		c = j; /* start of cycle */
		emit(Oswap, *k, NULL_R, G(pm)[i].src, G(pm)[i].dst);
		break;
	case ToMove:
		status[i] = Moving;
		c = qbe_rega_pmrec(status, j, k);
		if (c == i) {
			c = -1; /* end of cycle */
			break;
		}
		if (c != -1) {
			emit(Oswap, *k, NULL_R, G(pm)[i].src, G(pm)[i].dst);
			break;
		}
		/* fall through */
	case Moved:
		c = -1;
		emit(Ocopy, G(pm)[i].cls, G(pm)[i].dst, G(pm)[i].src, NULL_R);
		break;
	default:
		die("unreachable");
	}
	status[i] = Moved;
	return c;
}

static void
qbe_rega_pmgen(void)
{
	int i;
	enum PMStat *status;

	status = alloc(G(npm) * sizeof status[0]);
	SQ_ASSERT(!G(npm) || status[G(npm)-1] == ToMove);
	for (i=0; i<G(npm); i++)
		if (status[i] == ToMove)
			qbe_rega_pmrec(status, i, (int[]){G(pm)[i].cls});
}

static void
qbe_rega_move(int r, Ref to, RMap *m)
{
	int n, t, r1;

	r1 = req(to, NULL_R) ? -1 : qbe_rega_rfree(m, to.val);
	if (bshas(m->b, r)) {
		/* r is used and not by to */
		SQ_ASSERT(r1 != r);
		(void)r1;
		for (n=0; m->r[n] != r; n++)
			SQ_ASSERT(n+1 < m->n);
		t = m->t[n];
		qbe_rega_rfree(m, t);
		bsset(m->b, r);
		qbe_rega_ralloc(m, t);
		bsclr(m->b, r);
	}
	t = req(to, NULL_R) ? r : to.val;
	qbe_rega_radd(m, t, r);
}

static int
qbe_rega_regcpy(Ins *i)
{
	return i->op == Ocopy && isreg(i->arg[0]);
}

static Ins *
qbe_rega_dopm(Blk *b, Ins *i, RMap *m)
{
	RMap m0;
	int n, r, r1, t, s;
	Ins *i1, *ip;
	bits def;

	m0 = *m; /* okay since we don't use m0.b */
	m0.b->t = 0;
	i1 = ++i;
	do {
		i--;
		qbe_rega_move(i->arg[0].val, i->to, m);
	} while (i != b->ins && qbe_rega_regcpy(i-1));
	SQ_ASSERT(m0.n <= m->n);
	if (i != b->ins && (i-1)->op == Ocall) {
		def = GC(T).retregs((i-1)->arg[1], 0) | GC(T).rglob;
		for (r=0; GC(T).rsave[r]>=0; r++)
			if (!(BIT(GC(T).rsave[r]) & def))
				qbe_rega_move(GC(T).rsave[r], NULL_R, m);
	}
	for (G(npm)=0, n=0; n<m->n; n++) {
		t = m->t[n];
		s = G(tmp)[t].slot;
		r1 = m->r[n];
		r = qbe_rega_rfind(&m0, t);
		if (r != -1)
			qbe_rega_pmadd(TMP(r1), TMP(r), G(tmp)[t].cls);
		else if (s != -1)
			qbe_rega_pmadd(TMP(r1), SLOT(s), G(tmp)[t].cls);
	}
	for (ip=i; ip<i1; ip++) {
		if (!req(ip->to, NULL_R))
			qbe_rega_rfree(m, ip->to.val);
		r = ip->arg[0].val;
		if (qbe_rega_rfind(m, r) == -1)
			qbe_rega_radd(m, r, r);
	}
	qbe_rega_pmgen();
	return i;
}

static int
qbe_rega_prio1(Ref r1, Ref r2)
{
	/* trivial heuristic to begin with,
	 * later we can use the distance to
	 * the definition instruction
	 */
	(void) r2;
	return *qbe_rega_hint(r1.val) != -1;
}

static void
qbe_rega_insert(Ref *r, Ref **rs, int p)
{
	int i;

	rs[i = p] = r;
	while (i-- > 0 && qbe_rega_prio1(*r, *rs[i])) {
		rs[i+1] = rs[i];
		rs[i] = r;
	}
}

static void
qbe_rega_doblk(Blk *b, RMap *cur)
{
	int t, x, r, rf, rt, nr;
	bits rs;
	Ins *i, *i1;
	Mem *m;
	Ref *ra[4];

	if (rtype(b->jmp.arg) == RTmp)
		b->jmp.arg = qbe_rega_ralloc(cur, b->jmp.arg.val);
	GC(curi) = &GC(insb)[NIns];
	for (i1=&b->ins[b->nins]; i1!=b->ins;) {
		emiti(*--i1);
		i = GC(curi);
		rf = -1;
		switch (i->op) {
		case Ocall:
			rs = GC(T).argregs(i->arg[1], 0) | GC(T).rglob;
			for (r=0; GC(T).rsave[r]>=0; r++)
				if (!(BIT(GC(T).rsave[r]) & rs))
					qbe_rega_rfree(cur, GC(T).rsave[r]);
			break;
		case Ocopy:
			if (qbe_rega_regcpy(i)) {
				GC(curi)++;
				i1 = qbe_rega_dopm(b, i1, cur);
				G(stmov) += i+1 - GC(curi);
				continue;
			}
			if (isreg(i->to))
			if (rtype(i->arg[0]) == RTmp)
				qbe_rega_sethint(i->arg[0].val, i->to.val);
			/* fall through */
		default:
			if (!req(i->to, NULL_R)) {
				SQ_ASSERT(rtype(i->to) == RTmp);
				r = i->to.val;
				if (r < Tmp0 && (BIT(r) & GC(T).rglob))
					break;
				rf = qbe_rega_rfree(cur, r);
				if (rf == -1) {
					SQ_ASSERT(!isreg(i->to));
					GC(curi)++;
					continue;
				}
				i->to = TMP(rf);
			}
			break;
		}
		for (x=0, nr=0; x<2; x++)
			switch (rtype(i->arg[x])) {
			case RMem:
				m = &G(mem)[i->arg[x].val];
				if (rtype(m->base) == RTmp)
					qbe_rega_insert(&m->base, ra, nr++);
				if (rtype(m->index) == RTmp)
					qbe_rega_insert(&m->index, ra, nr++);
				break;
			case RTmp:
				qbe_rega_insert(&i->arg[x], ra, nr++);
				break;
			}
		for (r=0; r<nr; r++)
			*ra[r] = qbe_rega_ralloc(cur, ra[r]->val);
		if (i->op == Ocopy && req(i->to, i->arg[0]))
			GC(curi)++;

		/* try to change the register of a hinted
		 * temporary if rf is available */
		if (rf != -1 && (t = cur->w[rf]) != 0)
		if (!bshas(cur->b, rf) && *qbe_rega_hint(t) == rf
		&& (rt = qbe_rega_rfree(cur, t)) != -1) {
			G(tmp)[t].visit = -1;
			qbe_rega_ralloc(cur, t);
			SQ_ASSERT(bshas(cur->b, rf));
			emit(Ocopy, G(tmp)[t].cls, TMP(rt), TMP(rf), NULL_R);
			G(stmov) += 1;
			cur->w[rf] = 0;
			for (r=0; r<nr; r++)
				if (req(*ra[r], TMP(rt)))
					*ra[r] = TMP(rf);
			/* one could iterate this logic with
			 * the newly freed rt, but in this case
			 * the above loop must be changed */
		}
	}
	idup(b, GC(curi), &GC(insb)[NIns]-GC(curi));
}

/* qsort() comparison function to peel
 * loop nests from inside out */
static int
qbe_rega_carve(const void *a, const void *b)
{
	Blk *ba, *bb;

	/* todo, evaluate if this order is really
	 * better than the simple postorder */
	ba = *(Blk**)a;
	bb = *(Blk**)b;
	if (ba->loop == bb->loop)
		return ba->id > bb->id ? -1 : ba->id < bb->id;
	return ba->loop > bb->loop ? -1 : +1;
}

/* comparison function to order temporaries
 * for allocation at the end of blocks */
static int
qbe_rega_prio2(int t1, int t2)
{
	if ((G(tmp)[t1].visit ^ G(tmp)[t2].visit) < 0)  /* != signs */
		return G(tmp)[t1].visit != -1 ? +1 : -1;
	if ((*qbe_rega_hint(t1) ^ *qbe_rega_hint(t2)) < 0)
		return *qbe_rega_hint(t1) != -1 ? +1 : -1;
	return G(tmp)[t1].cost - G(tmp)[t2].cost;
}

/* register allocation
 * depends on rpo, phi, cost, (and obviously spill)
 */
void
rega(Fn *fn)
{
	int j, t, r, x, rl[Tmp0];
	Blk *b, *b1, *s, ***ps, *blist, **blk, **bp;
	RMap *end, *beg, cur, old, *m;
	Ins *i;
	Phi *p;
	uint u, n;
	Ref src, dst;

	/* 1. setup */
	G(stmov) = 0;
	G(stblk) = 0;
	G(regu) = 0;
	G(tmp) = fn->tmp;
	G(mem) = fn->mem;
	blk = alloc(fn->nblk * sizeof blk[0]);
	end = alloc(fn->nblk * sizeof end[0]);
	beg = alloc(fn->nblk * sizeof beg[0]);
	for (n=0; n<fn->nblk; n++) {
		bsinit(end[n].b, fn->ntmp);
		bsinit(beg[n].b, fn->ntmp);
	}
	bsinit(cur.b, fn->ntmp);
	bsinit(old.b, fn->ntmp);

	G(loop) = INT_MAX;
	for (t=0; t<fn->ntmp; t++) {
		G(tmp)[t].hint.r = t < Tmp0 ? t : -1;
		G(tmp)[t].hint.w = G(loop);
		G(tmp)[t].visit = -1;
	}
	for (bp=blk, b=fn->start; b; b=b->link)
		*bp++ = b;
	qsort(blk, fn->nblk, sizeof blk[0], qbe_rega_carve);
	for (b=fn->start, i=b->ins; i<&b->ins[b->nins]; i++)
		if (i->op != Ocopy || !isreg(i->arg[0]))
			break;
		else {
			SQ_ASSERT(rtype(i->to) == RTmp);
			qbe_rega_sethint(i->to.val, i->arg[0].val);
		}

	/* 2. assign registers */
	for (bp=blk; bp<&blk[fn->nblk]; bp++) {
		b = *bp;
		n = b->id;
		G(loop) = b->loop;
		cur.n = 0;
		bszero(cur.b);
		memset(cur.w, 0, sizeof cur.w);
		for (x=0, t=Tmp0; bsiter(b->out, &t); t++) {
			j = x++;
			rl[j] = t;
			while (j-- > 0 && qbe_rega_prio2(t, rl[j]) > 0) {
				rl[j+1] = rl[j];
				rl[j] = t;
			}
		}
		for (r=0; bsiter(b->out, &r) && r<Tmp0; r++)
			qbe_rega_radd(&cur, r, r);
		for (j=0; j<x; j++)
			qbe_rega_ralloctry(&cur, rl[j], 1);
		for (j=0; j<x; j++)
			qbe_rega_ralloc(&cur, rl[j]);
		qbe_rega_rcopy(&end[n], &cur);
		qbe_rega_doblk(b, &cur);
		bscopy(b->in, cur.b);
		for (p=b->phi; p; p=p->link)
			if (rtype(p->to) == RTmp)
				bsclr(b->in, p->to.val);
		qbe_rega_rcopy(&beg[n], &cur);
	}

	/* 3. emit copies shared by multiple edges
	 * to the same block */
	for (s=fn->start; s; s=s->link) {
		if (s->npred <= 1)
			continue;
		m = &beg[s->id];

		/* rl maps a register that is live at the
		 * beginning of s to the one used in all
		 * predecessors (if any, -1 otherwise) */
		memset(rl, 0, sizeof rl);

		/* to find the register of a phi in a
		 * predecessor, we have to find the
		 * corresponding argument */
		for (p=s->phi; p; p=p->link) {
			if (rtype(p->to) != RTmp
			|| (r=qbe_rega_rfind(m, p->to.val)) == -1)
				continue;
			for (u=0; u<p->narg; u++) {
				b = p->blk[u];
				src = p->arg[u];
				if (rtype(src) != RTmp)
					continue;
				x = qbe_rega_rfind(&end[b->id], src.val);
				if (x == -1) /* spilled */
					continue;
				rl[r] = (!rl[r] || rl[r] == x) ? x : -1;
			}
			if (rl[r] == 0)
				rl[r] = -1;
		}

		/* process non-phis temporaries */
		for (j=0; j<m->n; j++) {
			t = m->t[j];
			r = m->r[j];
			if (rl[r] || t < Tmp0 /* todo, remove this */)
				continue;
			for (bp=s->pred; bp<&s->pred[s->npred]; bp++) {
				x = qbe_rega_rfind(&end[(*bp)->id], t);
				if (x == -1) /* spilled */
					continue;
				rl[r] = (!rl[r] || rl[r] == x) ? x : -1;
			}
			if (rl[r] == 0)
				rl[r] = -1;
		}

		G(npm) = 0;
		for (j=0; j<m->n; j++) {
			t = m->t[j];
			r = m->r[j];
			x = rl[r];
			SQ_ASSERT(x != 0 || t < Tmp0 /* todo, ditto */);
			if (x > 0 && !bshas(m->b, x)) {
				qbe_rega_pmadd(TMP(x), TMP(r), G(tmp)[t].cls);
				m->r[j] = x;
				bsset(m->b, x);
			}
		}
		GC(curi) = &GC(insb)[NIns];
		qbe_rega_pmgen();
		j = &GC(insb)[NIns] - GC(curi);
		if (j == 0)
			continue;
		G(stmov) += j;
		s->nins += j;
		i = alloc(s->nins * sizeof(Ins));
		icpy(icpy(i, GC(curi), j), s->ins, s->nins-j);
		s->ins = i;
	}

	if (GC(debug)['R'])  {
		fprintf(stderr, "\n> Register mappings:\n");
		for (n=0; n<fn->nblk; n++) {
			b = fn->rpo[n];
			fprintf(stderr, "\t%-10s beg", b->name);
			qbe_rega_mdump(&beg[n]);
			fprintf(stderr, "\t           end");
			qbe_rega_mdump(&end[n]);
		}
		fprintf(stderr, "\n");
	}

	/* 4. emit remaining copies in new blocks */
	blist = 0;
	for (b=fn->start;; b=b->link) {
		ps = (Blk**[3]){&b->s1, &b->s2, (Blk*[1]){0}};
		for (; (s=**ps); ps++) {
			G(npm) = 0;
			for (p=s->phi; p; p=p->link) {
				dst = p->to;
				SQ_ASSERT(rtype(dst)==RSlot || rtype(dst)==RTmp);
				if (rtype(dst) == RTmp) {
					r = qbe_rega_rfind(&beg[s->id], dst.val);
					if (r == -1)
						continue;
					dst = TMP(r);
				}
				for (u=0; p->blk[u]!=b; u++)
					SQ_ASSERT(u+1 < p->narg);
				src = p->arg[u];
				if (rtype(src) == RTmp)
					src = qbe_rega_rref(&end[b->id], src.val);
				qbe_rega_pmadd(src, dst, p->cls);
			}
			for (t=Tmp0; bsiter(s->in, &t); t++) {
				src = qbe_rega_rref(&end[b->id], t);
				dst = qbe_rega_rref(&beg[s->id], t);
				qbe_rega_pmadd(src, dst, G(tmp)[t].cls);
			}
			GC(curi) = &GC(insb)[NIns];
			qbe_rega_pmgen();
			if (GC(curi) == &GC(insb)[NIns])
				continue;
			b1 = newblk();
			b1->loop = (b->loop+s->loop) / 2;
			b1->link = blist;
			blist = b1;
			fn->nblk++;
			strf(b1->name, "%s_%s", b->name, s->name);
			G(stmov) += &GC(insb)[NIns]-GC(curi);
			G(stblk) += 1;
			idup(b1, GC(curi), &GC(insb)[NIns]-GC(curi));
			b1->jmp.type = Jjmp;
			b1->s1 = s;
			**ps = b1;
		}
		if (!b->link) {
			b->link = blist;
			break;
		}
	}
	for (b=fn->start; b; b=b->link)
		b->phi = 0;
	fn->reg = G(regu);

	if (GC(debug)['R']) {
		fprintf(stderr, "\n> Register allocation statistics:\n");
		fprintf(stderr, "\tnew moves:  %d\n", G(stmov));
		fprintf(stderr, "\tnew blocks: %d\n", G(stblk));
		fprintf(stderr, "\n> After register allocation:\n");
		printfn(fn, stderr);
	}
}
#undef G
/*** END FILE: rega.c ***/
/*** START FILE: simpl.c ***/
/* skipping all.h */

#define G(x) global_context.simpl__##x

static void
qbe_simpl_blit(Ref sd[2], int sz, Fn *fn)
{
	struct { int st, ld, cls, size; } *p, tbl[] = {
		{ Ostorel, Oload,   Kl, 8 },
		{ Ostorew, Oload,   Kw, 4 },
		{ Ostoreh, Oloaduh, Kw, 2 },
		{ Ostoreb, Oloadub, Kw, 1 }
	};
	Ref r, r1, ro;
	int off, fwd, n;

	fwd = sz >= 0;
	sz = abs(sz);
	off = fwd ? sz : 0;
	for (p=tbl; sz; p++)
		for (n=p->size; sz>=n; sz-=n) {
			off -= fwd ? n : 0;
			r = newtmp("blt", Kl, fn);
			r1 = newtmp("blt", Kl, fn);
			ro = getcon(off, fn);
			emit(p->st, 0, NULL_R, r, r1);
			emit(Oadd, Kl, r1, sd[1], ro);
			r1 = newtmp("blt", Kl, fn);
			emit(p->ld, p->cls, r, r1, NULL_R);
			emit(Oadd, Kl, r1, sd[0], ro);
			off += fwd ? 0 : n;
		}
}

static int
ulog2_tab64[64] = {
	63,  0,  1, 41, 37,  2, 16, 42,
	38, 29, 32,  3, 12, 17, 43, 55,
	39, 35, 30, 53, 33, 21,  4, 23,
	13,  9, 18,  6, 25, 44, 48, 56,
	62, 40, 36, 15, 28, 31, 11, 54,
	34, 52, 20, 22,  8,  5, 24, 47,
	61, 14, 27, 10, 51, 19,  7, 46,
	60, 26, 50, 45, 59, 49, 58, 57,
};

static int
qbe_simpl_ulog2(uint64_t pow2)
{
	return ulog2_tab64[(pow2 * 0x5b31ab928877a7e) >> 58];
}

static int
qbe_simpl_ispow2(uint64_t v)
{
	return v && (v & (v - 1)) == 0;
}

static void
qbe_simpl_ins(Ins **pi, int *new, Blk *b, Fn *fn)
{
	ulong ni;
	Con *c;
	Ins *i;
	Ref r;
	int n;

	i = *pi;
	/* simplify more instructions here;
	 * copy 0 into xor, bit rotations,
	 * etc. */
	switch (i->op) {
	case Oblit1:
		SQ_ASSERT(i > b->ins);
		SQ_ASSERT((i-1)->op == Oblit0);
		if (!*new) {
			GC(curi) = &GC(insb)[NIns];
			ni = &b->ins[b->nins] - (i+1);
			GC(curi) -= ni;
			icpy(GC(curi), i+1, ni);
			*new = 1;
		}
		qbe_simpl_blit((i-1)->arg, rsval(i->arg[0]), fn);
		*pi = i-1;
		return;
	case Oudiv:
	case Ourem:
		r = i->arg[1];
		if (KBASE(i->cls) == 0)
		if (rtype(r) == RCon) {
			c = &fn->con[r.val];
			if (c->type == CBits)
			if (qbe_simpl_ispow2(c->bits.i)) {
				n = qbe_simpl_ulog2(c->bits.i);
				if (i->op == Ourem) {
					i->op = Oand;
					i->arg[1] = getcon((1ull<<n) - 1, fn);
				} else {
					i->op = Oshr;
					i->arg[1] = getcon(n, fn);
				}
			}
		}
		break;
	}
	if (*new)
		emiti(*i);
}

void
simpl(Fn *fn)
{
	Blk *b;
	Ins *i;
	int new;

	for (b=fn->start; b; b=b->link) {
		new = 0;
		for (i=&b->ins[b->nins]; i!=b->ins;) {
			--i;
			qbe_simpl_ins(&i, &new, b, fn);
		}
		if (new)
			idup(b, GC(curi), &GC(insb)[NIns]-GC(curi));
	}
}
#undef G
/*** END FILE: simpl.c ***/
/*** START FILE: spill.c ***/
/* skipping all.h */

#define G(x) global_context.spill__##x

static void
qbe_spill_aggreg(Blk *hd, Blk *b)
{
	int k;

	/* aggregate looping information at
	 * loop headers */
	bsunion(hd->gen, b->gen);
	for (k=0; k<2; k++)
		if (b->nlive[k] > hd->nlive[k])
			hd->nlive[k] = b->nlive[k];
}

static void
qbe_spill_tmpuse(Ref r, int use, int loop, Fn *fn)
{
	Mem *m;
	Tmp *t;

	if (rtype(r) == RMem) {
		m = &fn->mem[r.val];
		qbe_spill_tmpuse(m->base, 1, loop, fn);
		qbe_spill_tmpuse(m->index, 1, loop, fn);
	}
	else if (rtype(r) == RTmp && r.val >= Tmp0) {
		t = &fn->tmp[r.val];
		t->nuse += use;
		t->ndef += !use;
		t->cost += loop;
	}
}

/* evaluate spill costs of temporaries,
 * this also fills usage information
 * requires rpo, preds
 */
void
fillcost(Fn *fn)
{
	int n;
	uint a;
	Blk *b;
	Ins *i;
	Tmp *t;
	Phi *p;

	loopiter(fn, qbe_spill_aggreg);
	if (GC(debug)['S']) {
		fprintf(stderr, "\n> Loop information:\n");
		for (b=fn->start; b; b=b->link) {
			for (a=0; a<b->npred; ++a)
				if (b->id <= b->pred[a]->id)
					break;
			if (a != b->npred) {
				fprintf(stderr, "\t%-10s", b->name);
				fprintf(stderr, " (% 3d ", b->nlive[0]);
				fprintf(stderr, "% 3d) ", b->nlive[1]);
				dumpts(b->gen, fn->tmp, stderr);
			}
		}
	}
	for (t=fn->tmp; t-fn->tmp < fn->ntmp; t++) {
		t->cost = t-fn->tmp < Tmp0 ? UINT_MAX : 0;
		t->nuse = 0;
		t->ndef = 0;
	}
	for (b=fn->start; b; b=b->link) {
		for (p=b->phi; p; p=p->link) {
			t = &fn->tmp[p->to.val];
			qbe_spill_tmpuse(p->to, 0, 0, fn);
			for (a=0; a<p->narg; a++) {
				n = p->blk[a]->loop;
				t->cost += n;
				qbe_spill_tmpuse(p->arg[a], 1, n, fn);
			}
		}
		n = b->loop;
		for (i=b->ins; i<&b->ins[b->nins]; i++) {
			qbe_spill_tmpuse(i->to, 0, n, fn);
			qbe_spill_tmpuse(i->arg[0], 1, n, fn);
			qbe_spill_tmpuse(i->arg[1], 1, n, fn);
		}
		qbe_spill_tmpuse(b->jmp.arg, 1, n, fn);
	}
	if (GC(debug)['S']) {
		fprintf(stderr, "\n> Spill costs:\n");
		for (n=Tmp0; n<fn->ntmp; n++)
			fprintf(stderr, "\t%-10s %d\n",
				fn->tmp[n].name,
				fn->tmp[n].cost);
		fprintf(stderr, "\n");
	}
}

static int
qbe_spill_tcmp0(const void *pa, const void *pb)
{
	uint ca, cb;

	ca = G(tmp)[*(int *)pa].cost;
	cb = G(tmp)[*(int *)pb].cost;
	return (cb < ca) ? -1 : (cb > ca);
}

static int
qbe_spill_tcmp1(const void *pa, const void *pb)
{
	int c;

	c = bshas(G(fst), *(int *)pb) - bshas(G(fst), *(int *)pa);
	return c ? c : qbe_spill_tcmp0(pa, pb);
}

static Ref
qbe_spill_slot(int t)
{
	int s;

	SQ_ASSERT(t >= Tmp0 && "cannot spill register");
	s = G(tmp)[t].slot;
	if (s == -1) {
		/* specific to NAlign == 3 */
		/* nice logic to pack stack slots
		 * on demand, there can be only
		 * one hole and slot4 points to it
		 *
		 * invariant: slot4 <= slot8
		 */
		if (KWIDE(G(tmp)[t].cls)) {
			s = G(slot8);
			if (G(slot4) == G(slot8))
				G(slot4) += 2;
			G(slot8) += 2;
		} else {
			s = G(slot4);
			if (G(slot4) == G(slot8)) {
				G(slot8) += 2;
				G(slot4) += 1;
			} else
				G(slot4) = G(slot8);
		}
		s += G(locs);
		G(tmp)[t].slot = s;
	}
	return SLOT(s);
}

/* restricts b to hold at most k
 * temporaries, preferring those
 * present in f (if given), then
 * those with the largest spill
 * cost
 */
static void
qbe_spill_limit(BSet *b, int k, BSet *f)
{
	int i, t, nt;

	nt = bscount(b);
	if (nt <= k)
		return;
	if (nt > G(limit_maxt)) {
		qbe_free(G(limit_tarr));
		G(limit_tarr) = emalloc(nt * sizeof G(limit_tarr)[0]);
		G(limit_maxt) = nt;
	}
	for (i=0, t=0; bsiter(b, &t); t++) {
		bsclr(b, t);
		G(limit_tarr)[i++] = t;
	}
	if (nt > 1) {
		if (!f)
			qsort(G(limit_tarr), nt, sizeof G(limit_tarr)[0], qbe_spill_tcmp0);
		else {
			G(fst) = f;
			qsort(G(limit_tarr), nt, sizeof G(limit_tarr)[0], qbe_spill_tcmp1);
		}
	}
	for (i=0; i<k && i<nt; i++)
		bsset(b, G(limit_tarr)[i]);
	for (; i<nt; i++)
		qbe_spill_slot(G(limit_tarr)[i]);
}

/* spills temporaries to fit the
 * target limits using the same
 * preferences as qbe_spill_limit(); assumes
 * that k1 gprs and k2 fprs are
 * currently in use
 */
static void
qbe_spill_limit2(BSet *b1, int k1, int k2, BSet *f)
{
	BSet b2[1];

	bsinit(b2, G(ntmp)); /* todo, free those */
	bscopy(b2, b1);
	bsinter(b1, G(mask)[0]);
	bsinter(b2, G(mask)[1]);
	qbe_spill_limit(b1, GC(T).ngpr - k1, f);
	qbe_spill_limit(b2, GC(T).nfpr - k2, f);
	bsunion(b1, b2);
}

static void
qbe_spill_sethint(BSet *u, bits r)
{
	int t;

	for (t=Tmp0; bsiter(u, &t); t++)
		G(tmp)[phicls(t, G(tmp))].hint.m |= r;
}

/* reloads temporaries in u that are
 * not in v from their slots
 */
static void
qbe_spill_reloads(BSet *u, BSet *v)
{
	int t;

	for (t=Tmp0; bsiter(u, &t); t++)
		if (!bshas(v, t))
			emit(Oload, G(tmp)[t].cls, TMP(t), qbe_spill_slot(t), NULL_R);
}

static void
qbe_spill_store(Ref r, int s)
{
	if (s != -1)
		emit(Ostorew + G(tmp)[r.val].cls, 0, NULL_R, r, SLOT(s));
}

static int
qbe_spill_regcpy(Ins *i)
{
	return i->op == Ocopy && isreg(i->arg[0]);
}

static Ins *
qbe_spill_dopm(Blk *b, Ins *i, BSet *v)
{
	int n, t;
	BSet u[1];
	Ins *i1;
	bits r;

	bsinit(u, G(ntmp)); /* todo, free those */
	/* consecutive copies from
	 * registers need to be handled
	 * as one large instruction
	 *
	 * fixme: there is an assumption
	 * that calls are always followed
	 * by copy instructions here, this
	 * might not be true if previous
	 * passes change
	 */
	i1 = ++i;
	do {
		i--;
		t = i->to.val;
		if (!req(i->to, NULL_R))
		if (bshas(v, t)) {
			bsclr(v, t);
			qbe_spill_store(i->to, G(tmp)[t].slot);
		}
		bsset(v, i->arg[0].val);
	} while (i != b->ins && qbe_spill_regcpy(i-1));
	bscopy(u, v);
	if (i != b->ins && (i-1)->op == Ocall) {
		v->t[0] &= ~GC(T).retregs((i-1)->arg[1], 0);
		qbe_spill_limit2(v, GC(T).nrsave[0], GC(T).nrsave[1], 0);
		for (n=0, r=0; GC(T).rsave[n]>=0; n++)
			r |= BIT(GC(T).rsave[n]);
		v->t[0] |= GC(T).argregs((i-1)->arg[1], 0);
	} else {
		qbe_spill_limit2(v, 0, 0, 0);
		r = v->t[0];
	}
	qbe_spill_sethint(v, r);
	qbe_spill_reloads(u, v);
	do
		emiti(*--i1);
	while (i1 != i);
	return i;
}

static void
qbe_spill_merge(BSet *u, Blk *bu, BSet *v, Blk *bv)
{
	int t;

	if (bu->loop <= bv->loop)
		bsunion(u, v);
	else
		for (t=0; bsiter(v, &t); t++)
			if (G(tmp)[t].slot == -1)
				bsset(u, t);
}

/* spill code insertion
 * requires spill costs, rpo, liveness
 *
 * Note: this will replace liveness
 * information (in, out) with temporaries
 * that must be in registers at block
 * borders
 *
 * Be careful with:
 * - Ocopy instructions to ensure register
 *   constraints
 */
void
spill(Fn *fn)
{
	Blk *b, *s1, *s2, *hd, **bp;
	int j, l, t, k, lvarg[2];
	uint n;
	BSet u[1], v[1], w[1];
	Ins *i;
	Phi *p;
	Mem *m;
	bits r;

	G(tmp) = fn->tmp;
	G(ntmp) = fn->ntmp;
	bsinit(u, G(ntmp));
	bsinit(v, G(ntmp));
	bsinit(w, G(ntmp));
	bsinit(G(mask)[0], G(ntmp));
	bsinit(G(mask)[1], G(ntmp));
	G(locs) = fn->slot;
	G(slot4) = 0;
	G(slot8) = 0;
	for (t=0; t<G(ntmp); t++) {
		k = 0;
		if (t >= GC(T).fpr0 && t < GC(T).fpr0 + GC(T).nfpr)
			k = 1;
		if (t >= Tmp0)
			k = KBASE(G(tmp)[t].cls);
		bsset(G(mask)[k], t);
	}

	for (bp=&fn->rpo[fn->nblk]; bp!=fn->rpo;) {
		b = *--bp;
		/* invariant: all blocks with bigger rpo got
		 * their in,out updated. */

		/* 1. find temporaries in registers at
		 * the end of the block (put them in v) */
		GC(curi) = 0;
		s1 = b->s1;
		s2 = b->s2;
		hd = 0;
		if (s1 && s1->id <= b->id)
			hd = s1;
		if (s2 && s2->id <= b->id)
		if (!hd || s2->id >= hd->id)
			hd = s2;
		if (hd) {
			/* back-edge */
			bszero(v);
			hd->gen->t[0] |= GC(T).rglob; /* don't spill registers */
			for (k=0; k<2; k++) {
				n = k == 0 ? GC(T).ngpr : GC(T).nfpr;
				bscopy(u, b->out);
				bsinter(u, G(mask)[k]);
				bscopy(w, u);
				bsinter(u, hd->gen);
				bsdiff(w, hd->gen);
				if (bscount(u) < n) {
					j = bscount(w); /* live through */
					l = hd->nlive[k];
					qbe_spill_limit(w, n - (l - j), 0);
					bsunion(u, w);
				} else
					qbe_spill_limit(u, n, 0);
				bsunion(v, u);
			}
		} else if (s1) {
			/* avoid reloading temporaries
			 * in the middle of loops */
			bszero(v);
			liveon(w, b, s1);
			qbe_spill_merge(v, b, w, s1);
			if (s2) {
				liveon(u, b, s2);
				qbe_spill_merge(v, b, u, s2);
				bsinter(w, u);
			}
			qbe_spill_limit2(v, 0, 0, w);
		} else {
			bscopy(v, b->out);
			if (rtype(b->jmp.arg) == RCall)
				v->t[0] |= GC(T).retregs(b->jmp.arg, 0);
		}
		if (rtype(b->jmp.arg) == RTmp) {
			t = b->jmp.arg.val;
			SQ_ASSERT(KBASE(G(tmp)[t].cls) == 0);
			bsset(v, t);
			qbe_spill_limit2(v, 0, 0, NULL);
			if (!bshas(v, t))
				b->jmp.arg = qbe_spill_slot(t);
		}
		for (t=Tmp0; bsiter(b->out, &t); t++)
			if (!bshas(v, t))
				qbe_spill_slot(t);
		bscopy(b->out, v);

		/* 2. process the block instructions */
		GC(curi) = &GC(insb)[NIns];
		for (i=&b->ins[b->nins]; i!=b->ins;) {
			i--;
			if (qbe_spill_regcpy(i)) {
				i = qbe_spill_dopm(b, i, v);
				continue;
			}
			bszero(w);
			if (!req(i->to, NULL_R)) {
				SQ_ASSERT(rtype(i->to) == RTmp);
				t = i->to.val;
				if (bshas(v, t))
					bsclr(v, t);
				else {
					/* make sure we have a reg
					 * for the result */
					SQ_ASSERT(t >= Tmp0 && "dead reg");
					bsset(v, t);
					bsset(w, t);
				}
			}
			j = GC(T).memargs(i->op);
			for (n=0; n<2; n++)
				if (rtype(i->arg[n]) == RMem)
					j--;
			for (n=0; n<2; n++)
				switch (rtype(i->arg[n])) {
				case RMem:
					t = i->arg[n].val;
					m = &fn->mem[t];
					if (rtype(m->base) == RTmp) {
						bsset(v, m->base.val);
						bsset(w, m->base.val);
					}
					if (rtype(m->index) == RTmp) {
						bsset(v, m->index.val);
						bsset(w, m->index.val);
					}
					break;
				case RTmp:
					t = i->arg[n].val;
					lvarg[n] = bshas(v, t);
					bsset(v, t);
					if (j-- <= 0)
						bsset(w, t);
					break;
				}
			bscopy(u, v);
			qbe_spill_limit2(v, 0, 0, w);
			for (n=0; n<2; n++)
				if (rtype(i->arg[n]) == RTmp) {
					t = i->arg[n].val;
					if (!bshas(v, t)) {
						/* do not reload if the
						 * argument is dead
						 */
						if (!lvarg[n])
							bsclr(u, t);
						i->arg[n] = qbe_spill_slot(t);
					}
				}
			qbe_spill_reloads(u, v);
			if (!req(i->to, NULL_R)) {
				t = i->to.val;
				qbe_spill_store(i->to, G(tmp)[t].slot);
				if (t >= Tmp0)
					/* in case i->to was a
					 * dead temporary */
					bsclr(v, t);
			}
			emiti(*i);
			r = v->t[0]; /* Tmp0 is NBit */
			if (r)
				qbe_spill_sethint(v, r);
		}
		if (b == fn->start)
			SQ_ASSERT(v->t[0] == (GC(T).rglob | fn->reg));
		else
			SQ_ASSERT(v->t[0] == GC(T).rglob);

		for (p=b->phi; p; p=p->link) {
			SQ_ASSERT(rtype(p->to) == RTmp);
			t = p->to.val;
			if (bshas(v, t)) {
				bsclr(v, t);
				qbe_spill_store(p->to, G(tmp)[t].slot);
			} else if (bshas(b->in, t))
				/* only if the phi is live */
				p->to = qbe_spill_slot(p->to.val);
		}
		bscopy(b->in, v);
		idup(b, GC(curi), &GC(insb)[NIns]-GC(curi));
	}

	/* align the locals to a 16 byte boundary */
	/* specific to NAlign == 3 */
	G(slot8) += G(slot8) & 3;
	fn->slot += G(slot8);

	if (GC(debug)['S']) {
		fprintf(stderr, "\n> Block information:\n");
		for (b=fn->start; b; b=b->link) {
			fprintf(stderr, "\t%-10s (% 5d) ", b->name, b->loop);
			dumpts(b->out, fn->tmp, stderr);
		}
		fprintf(stderr, "\n> After spilling:\n");
		printfn(fn, stderr);
	}
}
#undef G
/*** END FILE: spill.c ***/
/*** START FILE: ssa.c ***/
/* skipping all.h */
#include <stdarg.h>

#define G(x) global_context.ssa__##x

void
adduse(Tmp *tmp, int ty, Blk *b, ...)
{
	Use *u;
	int n;
	va_list ap;

	if (!tmp->use)
		return;
	va_start(ap, b);
	n = tmp->nuse;
	vgrow(&tmp->use, ++tmp->nuse);
	u = &tmp->use[n];
	u->type = ty;
	u->bid = b->id;
	switch (ty) {
	case UPhi:
		u->u.phi = va_arg(ap, Phi *);
		break;
	case UIns:
		u->u.ins = va_arg(ap, Ins *);
		break;
	case UJmp:
		break;
	default:
		die("unreachable");
	}
	va_end(ap);
}

/* fill usage, width, phi, and class information
 * must not change .visit fields
 */
void
filluse(Fn *fn)
{
	Blk *b;
	Phi *p;
	Ins *i;
	int m, t, tp, w, x;
	uint a;
	Tmp *tmp;

	tmp = fn->tmp;
	for (t=Tmp0; t<fn->ntmp; t++) {
		tmp[t].def = 0;
		tmp[t].bid = -1u;
		tmp[t].ndef = 0;
		tmp[t].nuse = 0;
		tmp[t].cls = 0;
		tmp[t].phi = 0;
		tmp[t].width = WFull;
		if (tmp[t].use == 0)
			tmp[t].use = vnew(0, sizeof(Use), PFn);
	}
	for (b=fn->start; b; b=b->link) {
		for (p=b->phi; p; p=p->link) {
			SQ_ASSERT(rtype(p->to) == RTmp);
			tp = p->to.val;
			tmp[tp].bid = b->id;
			tmp[tp].ndef++;
			tmp[tp].cls = p->cls;
			tp = phicls(tp, fn->tmp);
			for (a=0; a<p->narg; a++)
				if (rtype(p->arg[a]) == RTmp) {
					t = p->arg[a].val;
					adduse(&tmp[t], UPhi, b, p);
					t = phicls(t, fn->tmp);
					if (t != tp)
						tmp[t].phi = tp;
				}
		}
		for (i=b->ins; i<&b->ins[b->nins]; i++) {
			if (!req(i->to, NULL_R)) {
				SQ_ASSERT(rtype(i->to) == RTmp);
				w = WFull;
				if (isparbh(i->op))
					w = Wsb + (i->op - Oparsb);
				if (isload(i->op) && i->op != Oload)
					w = Wsb + (i->op - Oloadsb);
				if (isext(i->op))
					w = Wsb + (i->op - Oextsb);
				if (iscmp(i->op, &x, &x))
					w = Wub;
				if (w == Wsw || w == Wuw)
				if (i->cls == Kw)
					w = WFull;
				t = i->to.val;
				tmp[t].width = w;
				tmp[t].def = i;
				tmp[t].bid = b->id;
				tmp[t].ndef++;
				tmp[t].cls = i->cls;
			}
			for (m=0; m<2; m++)
				if (rtype(i->arg[m]) == RTmp) {
					t = i->arg[m].val;
					adduse(&tmp[t], UIns, b, i);
				}
		}
		if (rtype(b->jmp.arg) == RTmp)
			adduse(&tmp[b->jmp.arg.val], UJmp, b);
	}
}

static Ref
qbe_ssa_refindex(int t, Fn *fn)
{
	return newtmp(fn->tmp[t].name, fn->tmp[t].cls, fn);
}

static void
qbe_ssa_phiins(Fn *fn)
{
	BSet u[1], defs[1];
	Blk *a, *b, **blist, **be, **bp;
	Ins *i;
	Phi *p;
	Use *use;
	Ref r;
	int t, nt, ok;
	uint n, defb;
	short k;

	bsinit(u, fn->nblk);
	bsinit(defs, fn->nblk);
	blist = emalloc(fn->nblk * sizeof blist[0]);
	be = &blist[fn->nblk];
	nt = fn->ntmp;
	for (t=Tmp0; t<nt; t++) {
		fn->tmp[t].visit = 0;
		if (fn->tmp[t].phi != 0)
			continue;
		if (fn->tmp[t].ndef == 1) {
			ok = 1;
			defb = fn->tmp[t].bid;
			use = fn->tmp[t].use;
			for (n=fn->tmp[t].nuse; n--; use++)
				ok &= use->bid == defb;
			if (ok || defb == fn->start->id)
				continue;
		}
		bszero(u);
		k = Kx;
		bp = be;
		for (b=fn->start; b; b=b->link) {
			b->visit = 0;
			r = NULL_R;
			for (i=b->ins; i<&b->ins[b->nins]; i++) {
				if (!req(r, NULL_R)) {
					if (req(i->arg[0], TMP(t)))
						i->arg[0] = r;
					if (req(i->arg[1], TMP(t)))
						i->arg[1] = r;
				}
				if (req(i->to, TMP(t))) {
					if (!bshas(b->out, t)) {
						r = qbe_ssa_refindex(t, fn);
						i->to = r;
					} else {
						if (!bshas(u, b->id)) {
							bsset(u, b->id);
							*--bp = b;
						}
						if (clsmerge(&k, i->cls))
							die("invalid input");
					}
				}
			}
			if (!req(r, NULL_R) && req(b->jmp.arg, TMP(t)))
				b->jmp.arg = r;
		}
		bscopy(defs, u);
		while (bp != be) {
			fn->tmp[t].visit = t;
			b = *bp++;
			bsclr(u, b->id);
			for (n=0; n<b->nfron; n++) {
				a = b->fron[n];
				if (a->visit++ == 0)
				if (bshas(a->in, t)) {
					p = alloc(sizeof *p);
					p->cls = k;
					p->to = TMP(t);
					p->link = a->phi;
					p->arg = vnew(0, sizeof p->arg[0], PFn);
					p->blk = vnew(0, sizeof p->blk[0], PFn);
					a->phi = p;
					if (!bshas(defs, a->id))
					if (!bshas(u, a->id)) {
						bsset(u, a->id);
						*--bp = a;
					}
				}
			}
		}
	}
	qbe_free(blist);
}

struct Name {
	Ref r;
	Blk *b;
	Name *up;
};

static Name *
qbe_ssa_nnew(Ref r, Blk *b, Name *up)
{
	Name *n;

	if (G(namel)) {
		n = G(namel);
		G(namel) = n->up;
	} else
		/* could use alloc, here
		 * but namel should be reset
		 */
		n = emalloc(sizeof *n);
	n->r = r;
	n->b = b;
	n->up = up;
	return n;
}

static void
qbe_ssa_nfree(Name *n)
{
	n->up = G(namel);
	G(namel) = n;
}

static void
qbe_ssa_rendef(Ref *r, Blk *b, Name **stk, Fn *fn)
{
	Ref r1;
	int t;

	t = r->val;
	if (req(*r, NULL_R) || !fn->tmp[t].visit)
		return;
	r1 = qbe_ssa_refindex(t, fn);
	fn->tmp[r1.val].visit = t;
	stk[t] = qbe_ssa_nnew(r1, b, stk[t]);
	*r = r1;
}

static Ref
qbe_ssa_getstk(int t, Blk *b, Name **stk)
{
	Name *n, *n1;

	n = stk[t];
	while (n && !dom(n->b, b)) {
		n1 = n;
		n = n->up;
		qbe_ssa_nfree(n1);
	}
	stk[t] = n;
	if (!n) {
		/* uh, oh, warn */
		return UNDEF;
	} else
		return n->r;
}

static void
qbe_ssa_renblk(Blk *b, Name **stk, Fn *fn)
{
	Phi *p;
	Ins *i;
	Blk *s, **ps, *succ[3];
	int t, m;

	for (p=b->phi; p; p=p->link)
		qbe_ssa_rendef(&p->to, b, stk, fn);
	for (i=b->ins; i<&b->ins[b->nins]; i++) {
		for (m=0; m<2; m++) {
			t = i->arg[m].val;
			if (rtype(i->arg[m]) == RTmp)
			if (fn->tmp[t].visit)
				i->arg[m] = qbe_ssa_getstk(t, b, stk);
		}
		qbe_ssa_rendef(&i->to, b, stk, fn);
	}
	t = b->jmp.arg.val;
	if (rtype(b->jmp.arg) == RTmp)
	if (fn->tmp[t].visit)
		b->jmp.arg = qbe_ssa_getstk(t, b, stk);
	succ[0] = b->s1;
	succ[1] = b->s2 == b->s1 ? 0 : b->s2;
	succ[2] = 0;
	for (ps=succ; (s=*ps); ps++)
		for (p=s->phi; p; p=p->link) {
			t = p->to.val;
			if ((t=fn->tmp[t].visit)) {
				m = p->narg++;
				vgrow(&p->arg, p->narg);
				vgrow(&p->blk, p->narg);
				p->arg[m] = qbe_ssa_getstk(t, b, stk);
				p->blk[m] = b;
			}
		}
	for (s=b->dom; s; s=s->dlink)
		qbe_ssa_renblk(s, stk, fn);
}

/* require rpo and use */
void
ssa(Fn *fn)
{
	Name **stk, *n;
	int d, nt;
	Blk *b, *b1;

	nt = fn->ntmp;
	stk = emalloc(nt * sizeof stk[0]);
	d = GC(debug)['L'];
	GC(debug)['L'] = 0;
	filldom(fn);
	if (GC(debug)['N']) {
		fprintf(stderr, "\n> Dominators:\n");
		for (b1=fn->start; b1; b1=b1->link) {
			if (!b1->dom)
				continue;
			fprintf(stderr, "%10s:", b1->name);
			for (b=b1->dom; b; b=b->dlink)
				fprintf(stderr, " %s", b->name);
			fprintf(stderr, "\n");
		}
	}
	fillfron(fn);
	filllive(fn);
	qbe_ssa_phiins(fn);
	qbe_ssa_renblk(fn->start, stk, fn);
	while (nt--)
		while ((n=stk[nt])) {
			stk[nt] = n->up;
			qbe_ssa_nfree(n);
		}
	GC(debug)['L'] = d;
	qbe_free(stk);
	if (GC(debug)['N']) {
		fprintf(stderr, "\n> After SSA construction:\n");
		printfn(fn, stderr);
	}
}

static int
qbe_ssa_phicheck(Phi *p, Blk *b, Ref t)
{
	Blk *b1;
	uint n;

	for (n=0; n<p->narg; n++)
		if (req(p->arg[n], t)) {
			b1 = p->blk[n];
			if (b1 != b && !sdom(b, b1))
				return 1;
		}
	return 0;
}

/* require use and ssa */
void
ssacheck(Fn *fn)
{
	Tmp *t;
	Ins *i;
	Phi *p;
	Use *u;
	Blk *b, *bu;
	Ref r;

	for (t=&fn->tmp[Tmp0]; t-fn->tmp < fn->ntmp; t++) {
		if (t->ndef > 1)
			err("ssa temporary %%%s defined more than once",
				t->name);
		if (t->nuse > 0 && t->ndef == 0) {
			bu = fn->rpo[t->use[0].bid];
			goto Err;
		}
	}
	for (b=fn->start; b; b=b->link) {
		for (p=b->phi; p; p=p->link) {
			r = p->to;
			t = &fn->tmp[r.val];
			for (u=t->use; u<&t->use[t->nuse]; u++) {
				bu = fn->rpo[u->bid];
				if (u->type == UPhi) {
					if (qbe_ssa_phicheck(u->u.phi, b, r))
						goto Err;
				} else
					if (bu != b && !sdom(b, bu))
						goto Err;
			}
		}
		for (i=b->ins; i<&b->ins[b->nins]; i++) {
			if (rtype(i->to) != RTmp)
				continue;
			r = i->to;
			t = &fn->tmp[r.val];
			for (u=t->use; u<&t->use[t->nuse]; u++) {
				bu = fn->rpo[u->bid];
				if (u->type == UPhi) {
					if (qbe_ssa_phicheck(u->u.phi, b, r))
						goto Err;
				} else {
					if (bu == b) {
						if (u->type == UIns)
							if (u->u.ins <= i)
								goto Err;
					} else
						if (!sdom(b, bu))
							goto Err;
				}
			}
		}
	}
	return;
Err:
	if (t->visit)
		die("%%%s violates ssa invariant", t->name);
	else
		err("ssa temporary %%%s is used undefined in @%s",
			t->name, bu->name);
}
#undef G
/*** END FILE: ssa.c ***/
/*** START FILE: util.c ***/
/* skipping all.h */
#include <stdarg.h>

#define G(x) global_context.util__##x

typedef struct Bitset Bitset;
typedef struct Vec Vec;

struct Vec {
	ulong mag;
	Pool pool;
	size_t esz;
	ulong cap;
	union {
		long long ll;
		long double ld;
		void *ptr;
	} align[];
};

enum {
	VMin = 2,
	VMag = 0xcabba9e,
};

uint32_t
hash(char *s)
{
	uint32_t h;

	for (h=0; *s; ++s)
		h = *s + 17*h;
	return h;
}



// to allow revectoring in sqbe



void *
vnew(ulong len, size_t esz, Pool pool)
{
	void *(*f)(size_t);
	ulong cap;
	Vec *v;

	for (cap=VMin; cap<len; cap*=2)
		;
	f = pool == PHeap ? emalloc : alloc;
	v = f(cap * esz + sizeof(Vec));
	v->mag = VMag;
	v->cap = cap;
	v->esz = esz;
	v->pool = pool;
	return v + 1;
}

void
vfree(void *p)
{
	Vec *v;

	v = (Vec *)p - 1;
	SQ_ASSERT(v->mag == VMag);
	if (v->pool == PHeap) {
		v->mag = 0;
		qbe_free(v);
	}
}

void
vgrow(void *vp, ulong len)
{
	Vec *v;
	void *v1;

	v = *(Vec **)vp - 1;
	SQ_ASSERT(v+1 && v->mag == VMag);
	if (v->cap >= len)
		return;
	v1 = vnew(len, v->esz, v->pool);
	memcpy(v1, v+1, v->cap * v->esz);
	vfree(v+1);
	*(Vec **)vp = v1;
}

void
addins(Ins **pvins, uint *pnins, Ins *i)
{
	if (i->op == Onop)
		return;
	vgrow(pvins, ++(*pnins));
	(*pvins)[(*pnins)-1] = *i;
}

void
addbins(Ins **pvins, uint *pnins, Blk *b)
{
	Ins *i;

	for (i=b->ins; i<&b->ins[b->nins]; i++)
		addins(pvins, pnins, i);
}

void
strf(char str[NString], char *s, ...)
{
	va_list ap;

	va_start(ap, s);
	vsnprintf(str, NString, s, ap);
	va_end(ap);
}

uint32_t
intern(char *s)
{
	Bucket *b;
	uint32_t h;
	uint i, n;

	h = hash(s) & util__IMask;
	b = &G(itbl)[h];
	n = b->nstr;

	for (i=0; i<n; i++)
		if (strcmp(s, b->str[i]) == 0)
			return h + (i<<util__IBits);

	if (n == 1<<(32-util__IBits))
		die("interning table overflow");
	if (n == 0)
		b->str = vnew(1, sizeof b->str[0], PHeap);
	else if ((n & (n-1)) == 0)
		vgrow(&b->str, n+n);

	b->str[n] = emalloc(strlen(s)+1);
	b->nstr = n + 1;
	strcpy(b->str[n], s);
	return h + (n<<util__IBits);
}

char *
str(uint32_t id)
{
	SQ_ASSERT(id>>util__IBits < G(itbl)[id&util__IMask].nstr);
	return G(itbl)[id&util__IMask].str[id>>util__IBits];
}

int
isreg(Ref r)
{
	return rtype(r) == RTmp && r.val < Tmp0;
}

int
iscmp(int op, int *pk, int *pc)
{
	if (Ocmpw <= op && op <= Ocmpw1) {
		*pc = op - Ocmpw;
		*pk = Kw;
	}
	else if (Ocmpl <= op && op <= Ocmpl1) {
		*pc = op - Ocmpl;
		*pk = Kl;
	}
	else if (Ocmps <= op && op <= Ocmps1) {
		*pc = NCmpI + op - Ocmps;
		*pk = Ks;
	}
	else if (Ocmpd <= op && op <= Ocmpd1) {
		*pc = NCmpI + op - Ocmpd;
		*pk = Kd;
	}
	else
		return 0;
	return 1;
}

void
igroup(Blk *b, Ins *i, Ins **i0, Ins **i1)
{
	Ins *ib, *ie;

	ib = b->ins;
	ie = ib + b->nins;
	switch (i->op) {
	case Oblit0:
		*i0 = i;
		*i1 = i + 2;
		return;
	case Oblit1:
		*i0 = i - 1;
		*i1 = i + 1;
		return;
	case_Opar:
		for (; i>ib && ispar((i-1)->op); i--)
			;
		*i0 = i;
		for (; i<ie && ispar(i->op); i++)
			;
		*i1 = i;
		return;
	case Ocall:
	case_Oarg:
		for (; i>ib && isarg((i-1)->op); i--)
			;
		*i0 = i;
		for (; i<ie && i->op != Ocall; i++)
			;
		SQ_ASSERT(i < ie);
		*i1 = i + 1;
		return;
	case Osel1:
		for (; i>ib && (i-1)->op == Osel1; i--)
			;
		SQ_ASSERT(i->op == Osel0);
		/* fall through */
	case Osel0:
		*i0 = i++;
		for (; i<ie && i->op == Osel1; i++)
			;
		*i1 = i;
		return;
	default:
		if (ispar(i->op))
			goto case_Opar;
		if (isarg(i->op))
			goto case_Oarg;
		*i0 = i;
		*i1 = i + 1;
		return;
	}
}

int
argcls(Ins *i, int n)
{
	return optab[i->op].argcls[n][i->cls];
}

void
emit(int op, int k, Ref to, Ref arg0, Ref arg1)
{
	if (GC(curi) == GC(insb))
		die("emit, too many instructions");
	*--GC(curi) = (Ins){
		.op = op, .cls = k,
		.to = to, .arg = {arg0, arg1}
	};
}

void
emiti(Ins i)
{
	emit(i.op, i.cls, i.to, i.arg[0], i.arg[1]);
}

void
idup(Blk *b, Ins *s, ulong n)
{
	vgrow(&b->ins, n);
	icpy(b->ins, s, n);
	b->nins = n;
}

Ins *
icpy(Ins *d, Ins *s, ulong n)
{
	if (n)
		memmove(d, s, n * sizeof(Ins));
	return d + n;
}

static int cmptab[][2] ={
	             /* negation    swap */
	[Ciule]      = {Ciugt,      Ciuge},
	[Ciult]      = {Ciuge,      Ciugt},
	[Ciugt]      = {Ciule,      Ciult},
	[Ciuge]      = {Ciult,      Ciule},
	[Cisle]      = {Cisgt,      Cisge},
	[Cislt]      = {Cisge,      Cisgt},
	[Cisgt]      = {Cisle,      Cislt},
	[Cisge]      = {Cislt,      Cisle},
	[Cieq]       = {Cine,       Cieq},
	[Cine]       = {Cieq,       Cine},
	[NCmpI+Cfle] = {NCmpI+Cfgt, NCmpI+Cfge},
	[NCmpI+Cflt] = {NCmpI+Cfge, NCmpI+Cfgt},
	[NCmpI+Cfgt] = {NCmpI+Cfle, NCmpI+Cflt},
	[NCmpI+Cfge] = {NCmpI+Cflt, NCmpI+Cfle},
	[NCmpI+Cfeq] = {NCmpI+Cfne, NCmpI+Cfeq},
	[NCmpI+Cfne] = {NCmpI+Cfeq, NCmpI+Cfne},
	[NCmpI+Cfo]  = {NCmpI+Cfuo, NCmpI+Cfo},
	[NCmpI+Cfuo] = {NCmpI+Cfo,  NCmpI+Cfuo},
};

int
cmpneg(int c)
{
	SQ_ASSERT(0 <= c && c < NCmp);
	return cmptab[c][0];
}

int
cmpop(int c)
{
	SQ_ASSERT(0 <= c && c < NCmp);
	return cmptab[c][1];
}

int
clsmerge(short *pk, short k)
{
	short k1;

	k1 = *pk;
	if (k1 == Kx) {
		*pk = k;
		return 0;
	}
	if ((k1 == Kw && k == Kl) || (k1 == Kl && k == Kw)) {
		*pk = Kw;
		return 0;
	}
	return k1 != k;
}

int
phicls(int t, Tmp *tmp)
{
	int t1;

	t1 = tmp[t].phi;
	if (!t1)
		return t;
	t1 = phicls(t1, tmp);
	tmp[t].phi = t1;
	return t1;
}

uint
phiargn(Phi *p, Blk *b)
{
	uint n;

	if (p)
		for (n=0; n<p->narg; n++)
			if (p->blk[n] == b)
				return n;
	return -1;
}

Ref
phiarg(Phi *p, Blk *b)
{
	uint n;

	n = phiargn(p, b);
	SQ_ASSERT(n != -1u && "block not found");
	return p->arg[n];
}

Ref
newtmp(char *prfx, int k,  Fn *fn)
{
	int t;

	t = fn->ntmp++;
	vgrow(&fn->tmp, fn->ntmp);
	memset(&fn->tmp[t], 0, sizeof(Tmp));
	if (prfx)
		strf(fn->tmp[t].name, "%s.%d", prfx, ++G(newtmp_n));
	fn->tmp[t].cls = k;
	fn->tmp[t].slot = -1;
	fn->tmp[t].nuse = +1;
	fn->tmp[t].ndef = +1;
	return TMP(t);
}

void
chuse(Ref r, int du, Fn *fn)
{
	if (rtype(r) == RTmp)
		fn->tmp[r.val].nuse += du;
}

int
symeq(Sym s0, Sym s1)
{
	return s0.type == s1.type && s0.id == s1.id;
}

Ref
newcon(Con *c0, Fn *fn)
{
	Con *c1;
	int i;

	for (i=1; i<fn->ncon; i++) {
		c1 = &fn->con[i];
		if (c0->type == c1->type
		&& symeq(c0->sym, c1->sym)
		&& c0->bits.i == c1->bits.i)
			return CON(i);
	}
	vgrow(&fn->con, ++fn->ncon);
	fn->con[i] = *c0;
	return CON(i);
}

Ref
getcon(int64_t val, Fn *fn)
{
	int c;

	for (c=1; c<fn->ncon; c++)
		if (fn->con[c].type == CBits
		&& fn->con[c].bits.i == val)
			return CON(c);
	vgrow(&fn->con, ++fn->ncon);
	fn->con[c] = (Con){.type = CBits, .bits.i = val};
	return CON(c);
}

int
addcon(Con *c0, Con *c1, int m)
{
	if (m != 1 && c1->type == CAddr)
		return 0;
	if (c0->type == CUndef) {
		*c0 = *c1;
		c0->bits.i *= m;
	} else {
		if (c1->type == CAddr) {
			if (c0->type == CAddr)
				return 0;
			c0->type = CAddr;
			c0->sym = c1->sym;
		}
		c0->bits.i += c1->bits.i * m;
	}
	return 1;
}

int
isconbits(Fn *fn, Ref r, int64_t *v)
{
	Con *c;

	if (rtype(r) == RCon) {
		c = &fn->con[r.val];
		if (c->type == CBits) {
			*v = c->bits.i;
			return 1;
		}
	}
	return 0;
}

void
salloc(Ref rt, Ref rs, Fn *fn)
{
	Ref r0, r1;
	int64_t sz;

	/* we need to make sure
	 * the stack remains aligned
	 * (rsp = 0) mod 16
	 */
	fn->dynalloc = 1;
	if (rtype(rs) == RCon) {
		sz = fn->con[rs.val].bits.i;
		if (sz < 0 || sz >= INT_MAX-15)
			err("invalid alloc size %"PRId64, sz);
		sz = (sz + 15)  & -16;
		emit(Osalloc, Kl, rt, getcon(sz, fn), NULL_R);
	} else {
		/* r0 = (r + 15) & -16 */
		r0 = newtmp("isel", Kl, fn);
		r1 = newtmp("isel", Kl, fn);
		emit(Osalloc, Kl, rt, r0, NULL_R);
		emit(Oand, Kl, r0, r1, getcon(-16, fn));
		emit(Oadd, Kl, r1, rs, getcon(15, fn));
		if (fn->tmp[rs.val].slot != -1)
			err("unlikely alloc argument %%%s for %%%s",
				fn->tmp[rs.val].name, fn->tmp[rt.val].name);
	}
}

void
bsinit(BSet *bs, uint n)
{
	n = (n + NBit-1) / NBit;
	bs->nt = n;
	bs->t = alloc(n * sizeof bs->t[0]);
}

MAKESURE(NBit_is_64, NBit == 64);
inline static uint
popcnt(bits b)
{
	b = (b & 0x5555555555555555) + ((b>>1) & 0x5555555555555555);
	b = (b & 0x3333333333333333) + ((b>>2) & 0x3333333333333333);
	b = (b & 0x0f0f0f0f0f0f0f0f) + ((b>>4) & 0x0f0f0f0f0f0f0f0f);
	b += (b>>8);
	b += (b>>16);
	b += (b>>32);
	return b & 0xff;
}

inline static int
firstbit(bits b)
{
	int n;

	n = 0;
	if (!(b & 0xffffffff)) {
		n += 32;
		b >>= 32;
	}
	if (!(b & 0xffff)) {
		n += 16;
		b >>= 16;
	}
	if (!(b & 0xff)) {
		n += 8;
		b >>= 8;
	}
	if (!(b & 0xf)) {
		n += 4;
		b >>= 4;
	}
	n += (char[16]){4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0}[b & 0xf];
	return n;
}

uint
bscount(BSet *bs)
{
	uint i, n;

	n = 0;
	for (i=0; i<bs->nt; i++)
		n += popcnt(bs->t[i]);
	return n;
}

static inline uint
qbe_util_bsmax(BSet *bs)
{
	return bs->nt * NBit;
}

void
bsset(BSet *bs, uint elt)
{
	SQ_ASSERT(elt < qbe_util_bsmax(bs));
	bs->t[elt/NBit] |= BIT(elt%NBit);
}

void
bsclr(BSet *bs, uint elt)
{
	SQ_ASSERT(elt < qbe_util_bsmax(bs));
	bs->t[elt/NBit] &= ~BIT(elt%NBit);
}

#define BSOP(f, op)                           \
	void                                  \
	f(BSet *a, BSet *b)                   \
	{                                     \
		uint i;                       \
		                              \
		SQ_ASSERT(a->nt == b->nt);       \
		for (i=0; i<a->nt; i++)       \
			a->t[i] op b->t[i];   \
	}

BSOP(bscopy, =)
BSOP(bsunion, |=)
BSOP(bsinter, &=)
BSOP(bsdiff, &= ~)

int
bsequal(BSet *a, BSet *b)
{
	uint i;

	SQ_ASSERT(a->nt == b->nt);
	for (i=0; i<a->nt; i++)
		if (a->t[i] != b->t[i])
			return 0;
	return 1;
}

void
bszero(BSet *bs)
{
	memset(bs->t, 0, bs->nt * sizeof bs->t[0]);
}

/* iterates on a bitset, use as follows
 *
 * 	for (i=0; bsiter(set, &i); i++)
 * 		use(i);
 *
 */
int
bsiter(BSet *bs, int *elt)
{
	bits b;
	uint t, i;

	i = *elt;
	t = i/NBit;
	if (t >= bs->nt)
		return 0;
	b = bs->t[t];
	b &= ~(BIT(i%NBit) - 1);
	while (!b) {
		++t;
		if (t >= bs->nt)
			return 0;
		b = bs->t[t];
	}
	*elt = NBit*t + firstbit(b);
	return 1;
}

void
dumpts(BSet *bs, Tmp *tmp, FILE *f)
{
	int t;

	fprintf(f, "[");
	for (t=Tmp0; bsiter(bs, &t); t++)
		fprintf(f, " %s", tmp[t].name);
	fprintf(f, " ]\n");
}

void
runmatch(uchar *code, Num *tn, Ref ref, Ref *var)
{
	Ref stkbuf[20], *stk;
	uchar *s, *pc;
	int bc, i;
	int n, nl, nr;

	SQ_ASSERT(rtype(ref) == RTmp);
	stk = stkbuf;
	pc = code;
	while ((bc = *pc))
		switch (bc) {
		case 1: /* pushsym */
		case 2: /* push */
			SQ_ASSERT(stk < &stkbuf[20]);
			SQ_ASSERT(rtype(ref) == RTmp);
			nl = tn[ref.val].nl;
			nr = tn[ref.val].nr;
			if (bc == 1 && nl > nr) {
				*stk++ = tn[ref.val].l;
				ref = tn[ref.val].r;
			} else {
				*stk++ = tn[ref.val].r;
				ref = tn[ref.val].l;
			}
			pc++;
			break;
		case 3: /* set */
			var[*++pc] = ref;
			if (*(pc + 1) == 0)
				return;
			/* fall through */
		case 4: /* pop */
			SQ_ASSERT(stk > &stkbuf[0]);
			ref = *--stk;
			pc++;
			break;
		case 5: /* switch */
			SQ_ASSERT(rtype(ref) == RTmp);
			n = tn[ref.val].n;
			s = pc + 1;
			for (i=*s++; i>0; i--, s++)
				if (n == *s++)
					break;
			pc += *s;
			break;
		default: /* jump */
			SQ_ASSERT(bc >= 10);
			pc = code + (bc - 10);
			break;
		}
}
#undef G
/*** END FILE: util.c ***/
/*** START FILE: amd64/emit.c ***/
/* skipping all.h */

#define G(x) global_context.amd64_emit__##x

typedef struct QBE_AMD64_EMIT_E QBE_AMD64_EMIT_E;

struct QBE_AMD64_EMIT_E {
	FILE *f;
	Fn *fn;
	int fp;
	uint64_t fsz;
	int nclob;
};

#define CMP(X) \
	X(Ciule,      "be", "a") \
	X(Ciult,      "b", "ae") \
	X(Cisle,      "le", "g") \
	X(Cislt,      "l", "ge") \
	X(Cisgt,      "g", "le") \
	X(Cisge,      "ge", "l") \
	X(Ciugt,      "a", "be") \
	X(Ciuge,      "ae", "b") \
	X(Cieq,       "z", "nz") \
	X(Cine,       "nz", "z") \
	X(NCmpI+Cfle, "be", "a") \
	X(NCmpI+Cflt, "b", "ae") \
	X(NCmpI+Cfgt, "a", "be") \
	X(NCmpI+Cfge, "ae", "b") \
	X(NCmpI+Cfo,  "np", "p") \
	X(NCmpI+Cfuo, "p", "np")

enum {
	SLong = 0,
	SWord = 1,
	SShort = 2,
	SByte = 3,

	QBE_AMD64_EMIT_Ki = -1, /* matches Kw and Kl */
	QBE_AMD64_EMIT_Ka = -2, /* matches all classes */
};

/* Instruction format strings:
 *
 * if the format string starts with -, the instruction
 * is assumed to be 3-address and is put in 2-address
 * mode using an extra mov if necessary
 *
 * if the format string starts with +, the same as the
 * above applies, but commutativity is also assumed
 *
 * %k  is used to set the class of the instruction,
 *     it'll expand to "l", "q", "ss", "sd", depending
 *     on the instruction class
 * %0  designates the first argument
 * %1  designates the second argument
 * %=  designates the result
 *
 * if %k is not used, a prefix to 0, 1, or = must be
 * added, it can be:
 *   M - memory reference
 *   L - long  (64 bits)
 *   W - word  (32 bits)
 *   H - short (16 bits)
 *   B - byte  (8 bits)
 *   S - single precision float
 *   D - double precision float
 */
static struct {
	short op;
	short cls;
	char *fmt;
} qbe_amd64_emit_omap[] = {
	{ Oadd,     QBE_AMD64_EMIT_Ka, "+add%k %1, %=" },
	{ Osub,     QBE_AMD64_EMIT_Ka, "-sub%k %1, %=" },
	{ Oand,     QBE_AMD64_EMIT_Ki, "+and%k %1, %=" },
	{ Oor,      QBE_AMD64_EMIT_Ki, "+or%k %1, %=" },
	{ Oxor,     QBE_AMD64_EMIT_Ki, "+xor%k %1, %=" },
	{ Osar,     QBE_AMD64_EMIT_Ki, "-sar%k %B1, %=" },
	{ Oshr,     QBE_AMD64_EMIT_Ki, "-shr%k %B1, %=" },
	{ Oshl,     QBE_AMD64_EMIT_Ki, "-shl%k %B1, %=" },
	{ Omul,     QBE_AMD64_EMIT_Ki, "+imul%k %1, %=" },
	{ Omul,     Ks, "+mulss %1, %=" },
	{ Omul,     Kd, "+mulsd %1, %=" },
	{ Odiv,     QBE_AMD64_EMIT_Ka, "-div%k %1, %=" },
	{ Ostorel,  QBE_AMD64_EMIT_Ka, "movq %L0, %M1" },
	{ Ostorew,  QBE_AMD64_EMIT_Ka, "movl %W0, %M1" },
	{ Ostoreh,  QBE_AMD64_EMIT_Ka, "movw %H0, %M1" },
	{ Ostoreb,  QBE_AMD64_EMIT_Ka, "movb %B0, %M1" },
	{ Ostores,  QBE_AMD64_EMIT_Ka, "movss %S0, %M1" },
	{ Ostored,  QBE_AMD64_EMIT_Ka, "movsd %D0, %M1" },
	{ Oload,    QBE_AMD64_EMIT_Ka, "mov%k %M0, %=" },
	{ Oloadsw,  Kl, "movslq %M0, %L=" },
	{ Oloadsw,  Kw, "movl %M0, %W=" },
	{ Oloaduw,  QBE_AMD64_EMIT_Ki, "movl %M0, %W=" },
	{ Oloadsh,  QBE_AMD64_EMIT_Ki, "movsw%k %M0, %=" },
	{ Oloaduh,  QBE_AMD64_EMIT_Ki, "movzw%k %M0, %=" },
	{ Oloadsb,  QBE_AMD64_EMIT_Ki, "movsb%k %M0, %=" },
	{ Oloadub,  QBE_AMD64_EMIT_Ki, "movzb%k %M0, %=" },
	{ Oextsw,   Kl, "movslq %W0, %L=" },
	{ Oextuw,   Kl, "movl %W0, %W=" },
	{ Oextsh,   QBE_AMD64_EMIT_Ki, "movsw%k %H0, %=" },
	{ Oextuh,   QBE_AMD64_EMIT_Ki, "movzw%k %H0, %=" },
	{ Oextsb,   QBE_AMD64_EMIT_Ki, "movsb%k %B0, %=" },
	{ Oextub,   QBE_AMD64_EMIT_Ki, "movzb%k %B0, %=" },

	{ Oexts,    Kd, "cvtss2sd %0, %=" },
	{ Otruncd,  Ks, "cvtsd2ss %0, %=" },
	{ Ostosi,   QBE_AMD64_EMIT_Ki, "cvttss2si%k %0, %=" },
	{ Odtosi,   QBE_AMD64_EMIT_Ki, "cvttsd2si%k %0, %=" },
	{ Oswtof,   QBE_AMD64_EMIT_Ka, "cvtsi2%k %W0, %=" },
	{ Osltof,   QBE_AMD64_EMIT_Ka, "cvtsi2%k %L0, %=" },
	{ Ocast,    QBE_AMD64_EMIT_Ki, "movq %D0, %L=" },
	{ Ocast,    QBE_AMD64_EMIT_Ka, "movq %L0, %D=" },

	{ Oaddr,    QBE_AMD64_EMIT_Ki, "lea%k %M0, %=" },
	{ Oswap,    QBE_AMD64_EMIT_Ki, "xchg%k %0, %1" },
	{ Osign,    Kl, "cqto" },
	{ Osign,    Kw, "cltd" },
	{ Oxdiv,    QBE_AMD64_EMIT_Ki, "div%k %0" },
	{ Oxidiv,   QBE_AMD64_EMIT_Ki, "idiv%k %0" },
	{ Oxcmp,    Ks, "ucomiss %S0, %S1" },
	{ Oxcmp,    Kd, "ucomisd %D0, %D1" },
	{ Oxcmp,    QBE_AMD64_EMIT_Ki, "cmp%k %0, %1" },
	{ Oxtest,   QBE_AMD64_EMIT_Ki, "test%k %0, %1" },
#define X(c, s, _) \
	{ Oflag+c,  QBE_AMD64_EMIT_Ki, "set" s " %B=\n\tmovzb%k %B=, %=" },
	CMP(X)
#undef X
	{ Oflagfeq, QBE_AMD64_EMIT_Ki, "setz %B=\n\tmovzb%k %B=, %=" },
	{ Oflagfne, QBE_AMD64_EMIT_Ki, "setnz %B=\n\tmovzb%k %B=, %=" },
	{ NOp, 0, 0 }
};

static char cmov[][2][16] = {
#define X(c, s0, s1) \
	[c] = { \
		"cmov" s0 " %0, %=", \
		"cmov" s1 " %1, %=", \
	},
	CMP(X)
#undef X
};

static char *qbe_amd64_emit_rname[][4] = {
	[QBE_AMD64_RAX] = {"rax", "eax", "ax", "al"},
	[QBE_AMD64_RBX] = {"rbx", "ebx", "bx", "bl"},
	[QBE_AMD64_RCX] = {"rcx", "ecx", "cx", "cl"},
	[QBE_AMD64_RDX] = {"rdx", "edx", "dx", "dl"},
	[QBE_AMD64_RSI] = {"rsi", "esi", "si", "sil"},
	[QBE_AMD64_RDI] = {"rdi", "edi", "di", "dil"},
	[QBE_AMD64_RBP] = {"rbp", "ebp", "bp", "bpl"},
	[QBE_AMD64_RSP] = {"rsp", "esp", "sp", "spl"},
	[QBE_AMD64_R8 ] = {"r8" , "r8d", "r8w", "r8b"},
	[QBE_AMD64_R9 ] = {"r9" , "r9d", "r9w", "r9b"},
	[QBE_AMD64_R10] = {"r10", "r10d", "r10w", "r10b"},
	[QBE_AMD64_R11] = {"r11", "r11d", "r11w", "r11b"},
	[QBE_AMD64_R12] = {"r12", "r12d", "r12w", "r12b"},
	[QBE_AMD64_R13] = {"r13", "r13d", "r13w", "r13b"},
	[QBE_AMD64_R14] = {"r14", "r14d", "r14w", "r14b"},
	[QBE_AMD64_R15] = {"r15", "r15d", "r15w", "r15b"},
};


static int
qbe_amd64_emit_slot(Ref r, QBE_AMD64_EMIT_E *e)
{
	int s;

	s = rsval(r);
	SQ_ASSERT(s <= e->fn->slot);
	/* specific to NAlign == 3 */
	if (s < 0) {
		if (e->fp == QBE_AMD64_RSP)
			return 4*-s - 8 + e->fsz + e->nclob*8;
		else
			return 4*-s;
	}
	else if (e->fp == QBE_AMD64_RSP)
		return 4*s + e->nclob*8;
	else if (e->fn->vararg) {
		if (GC(T).windows)
			return -4 * (e->fn->slot - s);
		else
			return -176 + -4 * (e->fn->slot - s);
	} else
		return -4 * (e->fn->slot - s);
}

static void
qbe_amd64_emit_emitcon(Con *con, QBE_AMD64_EMIT_E *e)
{
	char *p, *l;

	switch (con->type) {
	case CAddr:
		l = str(con->sym.id);
		p = l[0] == '"' ? "" : GC(T).assym;
		if (con->sym.type == SThr) {
			if (GC(T).apple)
				fprintf(e->f, "%s%s@TLVP", p, l);
			else
				fprintf(e->f, "%%fs:%s%s@tpoff", p, l);
		} else
			fprintf(e->f, "%s%s", p, l);
		if (con->bits.i)
			fprintf(e->f, "%+"PRId64, con->bits.i);
		break;
	case CBits:
		fprintf(e->f, "%"PRId64, con->bits.i);
		break;
	default:
		die("unreachable");
	}
}

static char* xmm_regs[] = {
    "xmm0", "xmm1", "xmm2",  "xmm3",  "xmm4",  "xmm5",  "xmm6",  "xmm7",
    "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
};

static char *
qbe_amd64_emit_regtoa(int reg, int sz)
{
	SQ_ASSERT(reg <= QBE_AMD64_XMM15);
	if (reg >= QBE_AMD64_XMM0) {
    return xmm_regs[reg-QBE_AMD64_XMM0];
	} else
		return qbe_amd64_emit_rname[reg][sz];
}

static Ref
qbe_amd64_emit_getarg(char c, Ins *i)
{
	switch (c) {
	case '0':
		return i->arg[0];
	case '1':
		return i->arg[1];
	case '=':
		return i->to;
	default:
		die("invalid arg letter %c", c);
	}
}

static void qbe_amd64_emit_emitins(Ins, QBE_AMD64_EMIT_E *);

static void
qbe_amd64_emit_emitcopy(Ref r1, Ref r2, int k, QBE_AMD64_EMIT_E *e)
{
	Ins icp;

	icp.op = Ocopy;
	icp.arg[0] = r2;
	icp.to = r1;
	icp.cls = k;
	qbe_amd64_emit_emitins(icp, e);
}

static void
qbe_amd64_emit_emitf(char *s, Ins *i, QBE_AMD64_EMIT_E *e)
{
	static char clstoa[][3] = {"l", "q", "ss", "sd"};
	char c;
	int sz;
	Ref ref;
	Mem *m;
	Con off;

	switch (*s) {
	case '+':
		if (req(i->arg[1], i->to)) {
			ref = i->arg[0];
			i->arg[0] = i->arg[1];
			i->arg[1] = ref;
		}
		/* fall through */
	case '-':
		SQ_ASSERT((!req(i->arg[1], i->to) || req(i->arg[0], i->to)) &&
			"cannot convert to 2-address");
		qbe_amd64_emit_emitcopy(i->to, i->arg[0], i->cls, e);
		s++;
		break;
	}

	fputc('\t', e->f);
Next:
	while ((c = *s++) != '%')
		if (!c) {
			fputc('\n', e->f);
			return;
		} else
			fputc(c, e->f);
	switch ((c = *s++)) {
	case '%':
		fputc('%', e->f);
		break;
	case 'k':
		fputs(clstoa[i->cls], e->f);
		break;
	case '0':
	case '1':
	case '=':
		sz = KWIDE(i->cls) ? SLong : SWord;
		s--;
		goto Label_Ref;
	case 'D':
	case 'S':
		sz = SLong; /* does not matter for floats */
	Label_Ref:
		c = *s++;
		ref = qbe_amd64_emit_getarg(c, i);
		switch (rtype(ref)) {
		case RTmp:
			SQ_ASSERT(isreg(ref));
			fprintf(e->f, "%%%s", qbe_amd64_emit_regtoa(ref.val, sz));
			break;
		case RSlot:
			fprintf(e->f, "%d(%%%s)",
				qbe_amd64_emit_slot(ref, e),
				qbe_amd64_emit_regtoa(e->fp, SLong)
			);
			break;
		case RMem:
		Label_Mem:
			m = &e->fn->mem[ref.val];
			if (rtype(m->base) == RSlot) {
				off.type = CBits;
				off.bits.i = qbe_amd64_emit_slot(m->base, e);
				addcon(&m->offset, &off, 1);
				m->base = TMP(e->fp);
			}
			if (m->offset.type != CUndef)
				qbe_amd64_emit_emitcon(&m->offset, e);
			fputc('(', e->f);
			if (!req(m->base, NULL_R))
				fprintf(e->f, "%%%s",
					qbe_amd64_emit_regtoa(m->base.val, SLong)
				);
			else if (m->offset.type == CAddr)
				fprintf(e->f, "%%rip");
			if (!req(m->index, NULL_R))
				fprintf(e->f, ", %%%s, %d",
					qbe_amd64_emit_regtoa(m->index.val, SLong),
					m->scale
				);
			fputc(')', e->f);
			break;
		case RCon:
			fputc('$', e->f);
			qbe_amd64_emit_emitcon(&e->fn->con[ref.val], e);
			break;
		default:
			die("unreachable");
		}
		break;
	case 'L':
		sz = SLong;
		goto Label_Ref;
	case 'W':
		sz = SWord;
		goto Label_Ref;
	case 'H':
		sz = SShort;
		goto Label_Ref;
	case 'B':
		sz = SByte;
		goto Label_Ref;
	case 'M':
		c = *s++;
		ref = qbe_amd64_emit_getarg(c, i);
		switch (rtype(ref)) {
		case RMem:
			goto Label_Mem;
		case RSlot:
			fprintf(e->f, "%d(%%%s)",
				qbe_amd64_emit_slot(ref, e),
				qbe_amd64_emit_regtoa(e->fp, SLong)
			);
			break;
		case RCon:
			off = e->fn->con[ref.val];
			qbe_amd64_emit_emitcon(&off, e);
			if (off.type == CAddr)
			if (off.sym.type != SThr || GC(T).apple)
				fprintf(e->f, "(%%rip)");
			break;
		case RTmp:
			SQ_ASSERT(isreg(ref));
			fprintf(e->f, "(%%%s)", qbe_amd64_emit_regtoa(ref.val, SLong));
			break;
		default:
			die("unreachable");
		}
		break;
	default:
		die("invalid format specifier %%%c", c);
	}
	goto Next;
}

static bits negmask[4] = {
	[Ks] = 0x80000000,
	[Kd] = 0x8000000000000000,
};

static void
qbe_amd64_emit_emitins(Ins i, QBE_AMD64_EMIT_E *e)
{
	Ref r;
	int64_t val;
	int o, t0;
	Ins ineg;
	Con *con;
	char *sym;

	switch (i.op) {
	default:
		if (isxsel(i.op))
			goto case_Oxsel;
	Table:
		/* most instructions are just pulled out of
		 * the table qbe_amd64_emit_omap[], some special cases are
		 * detailed below */
		for (o=0;; o++) {
			/* this linear search should really be a binary
			 * search */
			if (qbe_amd64_emit_omap[o].op == NOp)
				die("no match for %s(%c)",
					optab[i.op].name, "wlsd"[i.cls]);
			if (qbe_amd64_emit_omap[o].op == i.op)
			if (qbe_amd64_emit_omap[o].cls == i.cls
			|| (qbe_amd64_emit_omap[o].cls == QBE_AMD64_EMIT_Ki && KBASE(i.cls) == 0)
			|| (qbe_amd64_emit_omap[o].cls == QBE_AMD64_EMIT_Ka))
				break;
		}
		qbe_amd64_emit_emitf(qbe_amd64_emit_omap[o].fmt, &i, e);
		break;
	case Onop:
		/* just do nothing for nops, they are inserted
		 * by some passes */
		break;
	case Omul:
		/* here, we try to use the 3-addresss form
		 * of multiplication when possible */
		if (rtype(i.arg[1]) == RCon) {
			r = i.arg[0];
			i.arg[0] = i.arg[1];
			i.arg[1] = r;
		}
		if (KBASE(i.cls) == 0 /* only available for ints */
		&& rtype(i.arg[0]) == RCon
		&& rtype(i.arg[1]) == RTmp) {
			qbe_amd64_emit_emitf("imul%k %0, %1, %=", &i, e);
			break;
		}
		goto Table;
	case Osub:
		/* we have to use the negation trick to handle
		 * some 3-address subtractions */
		if (req(i.to, i.arg[1]) && !req(i.arg[0], i.to)) {
			ineg = (Ins){Oneg, i.cls, i.to, {i.to}};
			qbe_amd64_emit_emitins(ineg, e);
			qbe_amd64_emit_emitf("add%k %0, %=", &i, e);
			break;
		}
		goto Table;
	case Oneg:
		if (!req(i.to, i.arg[0]))
			qbe_amd64_emit_emitf("mov%k %0, %=", &i, e);
		if (KBASE(i.cls) == 0)
			qbe_amd64_emit_emitf("neg%k %=", &i, e);
		else
			fprintf(e->f,
				"\txorp%c %sfp%d(%%rip), %%%s\n",
				"xxsd"[i.cls],
				GC(T).asloc,
				stashbits(negmask[i.cls], 16),
				qbe_amd64_emit_regtoa(i.to.val, SLong)
			);
		break;
	case Odiv:
		/* use xmm15 to adjust the instruction when the
		 * conversion to 2-address in qbe_amd64_emit_emitf() would fail */
		if (req(i.to, i.arg[1])) {
			i.arg[1] = TMP(QBE_AMD64_XMM0+15);
			qbe_amd64_emit_emitf("mov%k %=, %1", &i, e);
			qbe_amd64_emit_emitf("mov%k %0, %=", &i, e);
			i.arg[0] = i.to;
		}
		goto Table;
	case Ocopy:
		/* copies are used for many things; see my note
		 * to understand how to load big constants:
		 * https://c9x.me/notes/2015-09-19.html */
		SQ_ASSERT(rtype(i.to) != RMem);
		if (req(i.to, NULL_R) || req(i.arg[0], NULL_R))
			break;
		if (req(i.to, i.arg[0]))
			break;
		t0 = rtype(i.arg[0]);
		if (i.cls == Kl
		&& t0 == RCon
		&& e->fn->con[i.arg[0].val].type == CBits) {
			val = e->fn->con[i.arg[0].val].bits.i;
			if (isreg(i.to))
			if (val >= 0 && val <= UINT32_MAX) {
				qbe_amd64_emit_emitf("movl %W0, %W=", &i, e);
				break;
			}
			if (rtype(i.to) == RSlot)
			if (val < INT32_MIN || val > INT32_MAX) {
				qbe_amd64_emit_emitf("movl %0, %=", &i, e);
				qbe_amd64_emit_emitf("movl %0>>32, 4+%=", &i, e);
				break;
			}
		}
		if (isreg(i.to)
		&& t0 == RCon
		&& e->fn->con[i.arg[0].val].type == CAddr) {
			qbe_amd64_emit_emitf("lea%k %M0, %=", &i, e);
			break;
		}
		if (rtype(i.to) == RSlot
		&& (t0 == RSlot || t0 == RMem)) {
			i.cls = KWIDE(i.cls) ? Kd : Ks;
			i.arg[1] = TMP(QBE_AMD64_XMM0+15);
			qbe_amd64_emit_emitf("mov%k %0, %1", &i, e);
			qbe_amd64_emit_emitf("mov%k %1, %=", &i, e);
			break;
		}
		/* conveniently, the assembler knows if it
		 * should use movabsq when reading movq */
		qbe_amd64_emit_emitf("mov%k %0, %=", &i, e);
		break;
	case Oaddr:
		if (!GC(T).apple
		&& rtype(i.arg[0]) == RCon
		&& e->fn->con[i.arg[0].val].sym.type == SThr) {
			/* derive the symbol address from the TCB
			 * address at offset 0 of %fs */
			SQ_ASSERT(isreg(i.to));
			con = &e->fn->con[i.arg[0].val];
			sym = str(con->sym.id);
			qbe_amd64_emit_emitf("movq %%fs:0, %L=", &i, e);
			fprintf(e->f, "\tleaq %s%s@tpoff",
				sym[0] == '"' ? "" : GC(T).assym, sym);
			if (con->bits.i)
				fprintf(e->f, "%+"PRId64,
					con->bits.i);
			fprintf(e->f, "(%%%s), %%%s\n",
				qbe_amd64_emit_regtoa(i.to.val, SLong),
				qbe_amd64_emit_regtoa(i.to.val, SLong));
			break;
		}
		goto Table;
	case Ocall:
		/* calls simply have a weird syntax in AT&T
		 * assembly... */
		switch (rtype(i.arg[0])) {
		case RCon:
			fprintf(e->f, "\tcallq ");
			qbe_amd64_emit_emitcon(&e->fn->con[i.arg[0].val], e);
			fprintf(e->f, "\n");
			break;
		case RTmp:
			qbe_amd64_emit_emitf("callq *%L0", &i, e);
			break;
		default:
			die("invalid call argument");
		}
		break;
	case Osalloc:
		/* there is no good reason why this is here
		 * maybe we should split Osalloc in 2 different
		 * instructions depending on the result
		 */
		SQ_ASSERT(e->fp == QBE_AMD64_RBP);
		qbe_amd64_emit_emitf("subq %L0, %%rsp", &i, e);
		if (!req(i.to, NULL_R))
			qbe_amd64_emit_emitcopy(i.to, TMP(QBE_AMD64_RSP), Kl, e);
		break;
	case Oswap:
		if (KBASE(i.cls) == 0)
			goto Table;
		/* for floats, there is no swap instruction
		 * so we use xmm15 as a temporary
		 */
		qbe_amd64_emit_emitcopy(TMP(QBE_AMD64_XMM0+15), i.arg[0], i.cls, e);
		qbe_amd64_emit_emitcopy(i.arg[0], i.arg[1], i.cls, e);
		qbe_amd64_emit_emitcopy(i.arg[1], TMP(QBE_AMD64_XMM0+15), i.cls, e);
		break;
	case Odbgloc:
		emitdbgloc(i.arg[0].val, i.arg[1].val, e->f);
		break;
	case_Oxsel:
		if (req(i.to, i.arg[1]))
			qbe_amd64_emit_emitf(cmov[i.op-Oxsel][0], &i, e);
		else {
			if (!req(i.to, i.arg[0]))
				qbe_amd64_emit_emitf("mov %0, %=", &i, e);
			qbe_amd64_emit_emitf(cmov[i.op-Oxsel][1], &i, e);
		}
		break;
	}
}

static void
qbe_amd64_emit_sysv_framesz(QBE_AMD64_EMIT_E *e)
{
	uint64_t i, o, f;

	/* specific to NAlign == 3 */
	o = 0;
	if (!e->fn->leaf) {
		for (i=0, o=0; i<QBE_AMD64_NCLR_SYSV; i++)
			o ^= e->fn->reg >> amd64_sysv_rclob[i];
		o &= 1;
	}
	f = e->fn->slot;
	f = (f + 3) & -4;
	if (f > 0
	&& e->fp == QBE_AMD64_RSP
	&& e->fn->salign == 4)
		f += 2;
	e->fsz = 4*f + 8*o + 176*e->fn->vararg;
}

void
amd64_sysv_emitfn(Fn *fn, FILE *f)
{
	static char *ctoa[] = {
	#define X(c, s, _) [c] = s,
		CMP(X)
	#undef X
	};
	Blk *b, *s;
	Ins *i, itmp;
	int *r, c, o, n, lbl;
	uint p;
	QBE_AMD64_EMIT_E *e;

	e = &(QBE_AMD64_EMIT_E){.f = f, .fn = fn};
	emitfnlnk(fn->name, &fn->lnk, f);
	fputs("\tendbr64\n", f);
	if (!fn->leaf || fn->vararg || fn->dynalloc) {
		e->fp = QBE_AMD64_RBP;
		fputs("\tpushq %rbp\n\tmovq %rsp, %rbp\n", f);
	} else
		e->fp = QBE_AMD64_RSP;
	qbe_amd64_emit_sysv_framesz(e);
	if (e->fsz)
		fprintf(f, "\tsubq $%"PRIu64", %%rsp\n", e->fsz);
	if (fn->vararg) {
		o = -176;
		for (r=amd64_sysv_rsave; r<&amd64_sysv_rsave[6]; r++, o+=8)
			fprintf(f, "\tmovq %%%s, %d(%%rbp)\n", qbe_amd64_emit_rname[*r][0], o);
		for (n=0; n<8; ++n, o+=16)
			fprintf(f, "\tmovaps %%xmm%d, %d(%%rbp)\n", n, o);
	}
	for (r=amd64_sysv_rclob; r<&amd64_sysv_rclob[QBE_AMD64_NCLR_SYSV]; r++)
		if (fn->reg & BIT(*r)) {
			itmp.arg[0] = TMP(*r);
			qbe_amd64_emit_emitf("pushq %L0", &itmp, e);
			e->nclob++;
		}

	for (lbl=0, b=fn->start; b; b=b->link) {
		if (lbl || b->npred > 1) {
			for (p=0; p<b->npred; p++)
				if (b->pred[p]->id >= b->id)
					break;
			if (p != b->npred)
				fprintf(f, ".p2align 4\n");
			fprintf(f, "%sbb%d:\n", GC(T).asloc, G(amd64_sysv_emitfn_id0)+b->id);
		}
		for (i=b->ins; i!=&b->ins[b->nins]; i++)
			qbe_amd64_emit_emitins(*i, e);
		lbl = 1;
		switch (b->jmp.type) {
		case Jhlt:
			fprintf(f, "\tud2\n");
			break;
		case Jret0:
			if (fn->dynalloc)
				fprintf(f,
					"\tmovq %%rbp, %%rsp\n"
					"\tsubq $%"PRIu64", %%rsp\n",
					e->fsz + e->nclob * 8);
			for (r=&amd64_sysv_rclob[QBE_AMD64_NCLR_SYSV]; r>amd64_sysv_rclob;)
				if (fn->reg & BIT(*--r)) {
					itmp.arg[0] = TMP(*r);
					qbe_amd64_emit_emitf("popq %L0", &itmp, e);
				}
			if (e->fp == QBE_AMD64_RBP)
				fputs("\tleave\n", f);
			else if (e->fsz)
				fprintf(f,
					"\taddq $%"PRIu64", %%rsp\n",
					e->fsz);
			fputs("\tret\n", f);
			break;
		case Jjmp:
		Label_Jmp:
			if (b->s1 != b->link)
				fprintf(f, "\tjmp %sbb%d\n",
					GC(T).asloc, G(amd64_sysv_emitfn_id0)+b->s1->id);
			else
				lbl = 0;
			break;
		default:
			c = b->jmp.type - Jjf;
			if (0 <= c && c <= NCmp) {
				if (b->link == b->s2) {
					s = b->s1;
					b->s1 = b->s2;
					b->s2 = s;
				} else
					c = cmpneg(c);
				fprintf(f, "\tj%s %sbb%d\n", ctoa[c],
					GC(T).asloc, G(amd64_sysv_emitfn_id0)+b->s2->id);
				goto Label_Jmp;
			}
			die("unhandled jump %d", b->jmp.type);
		}
	}
	G(amd64_sysv_emitfn_id0) += fn->nblk;
	if (!GC(T).apple)
		elf_emitfnfin(fn->name, f);
}

static void
qbe_amd64_emit_winabi_framesz(QBE_AMD64_EMIT_E *e)
{
	uint64_t i, o, f;

	/* specific to NAlign == 3 */
	o = 0;
	if (!e->fn->leaf) {
		for (i=0, o=0; i<QBE_AMD64_NCLR_WIN; i++)
			o ^= e->fn->reg >> amd64_winabi_rclob[i];
		o &= 1;
	}
	f = e->fn->slot;
	f = (f + 3) & -4;
	if (f > 0
	&& e->fp == QBE_AMD64_RSP
	&& e->fn->salign == 4)
		f += 2;
	e->fsz = 4*f + 8*o;
}

void
amd64_winabi_emitfn(Fn *fn, FILE *f)
{
	static char *ctoa[] = {
	#define X(c, s, _) [c] = s,
		CMP(X)
	#undef X
	};
	Blk *b, *s;
	Ins *i, itmp;
	int *r, c, lbl;
	QBE_AMD64_EMIT_E *e;

	e = &(QBE_AMD64_EMIT_E){.f = f, .fn = fn};
	emitfnlnk(fn->name, &fn->lnk, f);
	fputs("\tendbr64\n", f);
	if (fn->vararg) {
		fprintf(f, "\tmovq %%rcx, 0x8(%%rsp)\n");
		fprintf(f, "\tmovq %%rdx, 0x10(%%rsp)\n");
		fprintf(f, "\tmovq %%r8, 0x18(%%rsp)\n");
		fprintf(f, "\tmovq %%r9, 0x20(%%rsp)\n");
	}
	if (!fn->leaf || fn->vararg || fn->dynalloc) {
		e->fp = QBE_AMD64_RBP;
		fputs("\tpushq %rbp\n\tmovq %rsp, %rbp\n", f);
	} else
		e->fp = QBE_AMD64_RSP;
	qbe_amd64_emit_winabi_framesz(e);
	if (e->fsz)
		fprintf(f, "\tsubq $%"PRIu64", %%rsp\n", e->fsz);
	for (r=amd64_winabi_rclob; r<&amd64_winabi_rclob[QBE_AMD64_NCLR_WIN]; r++)
		if (fn->reg & BIT(*r)) {
			itmp.arg[0] = TMP(*r);
			qbe_amd64_emit_emitf("pushq %L0", &itmp, e);
			e->nclob++;
		}

	for (lbl=0, b=fn->start; b; b=b->link) {
		if (lbl || b->npred > 1)
			fprintf(f, "%sbb%d:\n", GC(T).asloc, G(amd64_winabi_emitfn_id0)+b->id);
		for (i=b->ins; i!=&b->ins[b->nins]; i++)
			qbe_amd64_emit_emitins(*i, e);
		lbl = 1;
		switch (b->jmp.type) {
		case Jhlt:
			fprintf(f, "\tud2\n");
			break;
		case Jret0:
			if (fn->dynalloc)
				fprintf(f,
					"\tmovq %%rbp, %%rsp\n"
					"\tsubq $%"PRIu64", %%rsp\n",
					e->fsz + e->nclob * 8);
			for (r=&amd64_winabi_rclob[QBE_AMD64_NCLR_WIN]; r>amd64_winabi_rclob;)
				if (fn->reg & BIT(*--r)) {
					itmp.arg[0] = TMP(*r);
					qbe_amd64_emit_emitf("popq %L0", &itmp, e);
				}
			if (e->fp == QBE_AMD64_RBP)
				fputs("\tleave\n", f);
			else if (e->fsz)
				fprintf(f,
					"\taddq $%"PRIu64", %%rsp\n",
					e->fsz);
			fputs("\tret\n", f);
			break;
		case Jjmp:
		Label_Jmp:
			if (b->s1 != b->link)
				fprintf(f, "\tjmp %sbb%d\n",
					GC(T).asloc, G(amd64_winabi_emitfn_id0)+b->s1->id);
			else
				lbl = 0;
			break;
		default:
			c = b->jmp.type - Jjf;
			if (0 <= c && c <= NCmp) {
				if (b->link == b->s2) {
					s = b->s1;
					b->s1 = b->s2;
					b->s2 = s;
				} else
					c = cmpneg(c);
				fprintf(f, "\tj%s %sbb%d\n", ctoa[c],
					GC(T).asloc, G(amd64_winabi_emitfn_id0)+b->s2->id);
				goto Label_Jmp;
			}
			die("unhandled jump %d", b->jmp.type);
		}
	}
	G(amd64_winabi_emitfn_id0) += fn->nblk;
}
#undef CMP
#undef G
/*** END FILE: amd64/emit.c ***/
/*** START FILE: amd64/isel.c ***/
/* skipping all.h */
#include <limits.h>

/* For x86_64, do the following:
 *
 * - check that constants are used only in
 *   places allowed
 * - ensure immediates always fit in 32b
 * - expose machine register contraints
 *   on instructions like division.
 * - implement fast locals (the streak of
 *   constant allocX in the first basic block)
 * - recognize complex addressing modes
 *
 * Invariant: the use counts that are used
 *            in qbe_amd64_isel_sel() must be sound.  This
 *            is not so trivial, maybe the
 *            dce should be moved out...
 */

static int qbe_amd64_isel_amatch(Addr *, Num *, Ref, Fn *);

static int
qbe_amd64_isel_noimm(Ref r, Fn *fn)
{
	int64_t val;

	if (rtype(r) != RCon)
		return 0;
	switch (fn->con[r.val].type) {
	case CAddr:
		/* we only support the 'small'
		 * code model of the ABI, this
		 * means that we can always
		 * address data with 32bits
		 */
		return 0;
	case CBits:
		val = fn->con[r.val].bits.i;
		return (val < INT32_MIN || val > INT32_MAX);
	default:
		die("invalid constant");
	}
}

static int
qbe_amd64_isel_rslot(Ref r, Fn *fn)
{
	if (rtype(r) != RTmp)
		return -1;
	return fn->tmp[r.val].slot;
}

static int
qbe_amd64_isel_hascon(Ref r, Con **pc, Fn *fn)
{
	switch (rtype(r)) {
	case RCon:
		*pc = &fn->con[r.val];
		return 1;
	case RMem:
		*pc = &fn->mem[r.val].offset;
		return 1;
	default:
		return 0;
	}
}

static void
qbe_amd64_isel_fixarg(Ref *r, int k, Ins *i, Fn *fn)
{
	char buf[32];
	Addr a, *m;
	Con cc, *c;
	Ref r0, r1, r2, r3;
	int s, n, op;

	r1 = r0 = *r;
	s = qbe_amd64_isel_rslot(r0, fn);
	op = i ? i->op : Ocopy;
	if (KBASE(k) == 1 && rtype(r0) == RCon) {
		/* load floating points from memory
		 * slots, they can't be used as
		 * immediates
		 */
		r1 = MEM(fn->nmem);
		vgrow(&fn->mem, ++fn->nmem);
		memset(&a, 0, sizeof a);
		a.offset.type = CAddr;
		n = stashbits(fn->con[r0.val].bits.i, KWIDE(k) ? 8 : 4);
		/* quote the name so that we do not
		 * add symbol prefixes on the apple
		 * target variant
		 */
		snprintf(buf, sizeof(buf), "\"%sfp%d\"", GC(T).asloc, n);
		a.offset.sym.id = intern(buf);
		fn->mem[fn->nmem-1] = a;
	}
	else if (op == Ocall && r == &i->arg[0]
	&& rtype(r0) == RCon && fn->con[r0.val].type != CAddr) {
		/* use a temporary register so that we
		 * produce an indirect call
		 */
		r1 = newtmp("isel", Kl, fn);
		emit(Ocopy, Kl, r1, r0, NULL_R);
	}
	else if (op != Ocopy && k == Kl && qbe_amd64_isel_noimm(r0, fn)) {
		/* load constants that do not fit in
		 * a 32bit signed integer into a
		 * long temporary
		 */
		r1 = newtmp("isel", Kl, fn);
		emit(Ocopy, Kl, r1, r0, NULL_R);
	}
	else if (s != -1) {
		/* load fast locals' addresses into
		 * temporaries right before the
		 * instruction
		 */
		r1 = newtmp("isel", Kl, fn);
		emit(Oaddr, Kl, r1, SLOT(s), NULL_R);
	}
	else if (GC(T).apple && qbe_amd64_isel_hascon(r0, &c, fn)
	&& c->type == CAddr && c->sym.type == SThr) {
		r1 = newtmp("isel", Kl, fn);
		if (c->bits.i) {
			r2 = newtmp("isel", Kl, fn);
			cc = (Con){.type = CBits};
			cc.bits.i = c->bits.i;
			r3 = newcon(&cc, fn);
			emit(Oadd, Kl, r1, r2, r3);
		} else
			r2 = r1;
		emit(Ocopy, Kl, r2, TMP(QBE_AMD64_RAX), NULL_R);
		r2 = newtmp("isel", Kl, fn);
		r3 = newtmp("isel", Kl, fn);
		emit(Ocall, 0, NULL_R, r3, CALL(17));
		emit(Ocopy, Kl, TMP(QBE_AMD64_RDI), r2, NULL_R);
		emit(Oload, Kl, r3, r2, NULL_R);
		cc = *c;
		cc.bits.i = 0;
		r3 = newcon(&cc, fn);
		emit(Oload, Kl, r2, r3, NULL_R);
		if (rtype(r0) == RMem) {
			m = &fn->mem[r0.val];
			m->offset.type = CUndef;
			m->base = r1;
			r1 = r0;
		}
	}
	else if (!(isstore(op) && r == &i->arg[1])
	&& !isload(op) && op != Ocall && rtype(r0) == RCon
	&& fn->con[r0.val].type == CAddr) {
		/* apple as does not support 32-bit
		 * absolute addressing, use a rip-
		 * relative leaq instead
		 */
		r1 = newtmp("isel", Kl, fn);
		emit(Oaddr, Kl, r1, r0, NULL_R);
	}
	else if (rtype(r0) == RMem) {
		/* eliminate memory operands of
		 * the form $foo(%rip, ...)
		 */
		m = &fn->mem[r0.val];
		if (req(m->base, NULL_R))
		if (m->offset.type == CAddr) {
			r0 = newtmp("isel", Kl, fn);
			emit(Oaddr, Kl, r0, newcon(&m->offset, fn), NULL_R);
			m->offset.type = CUndef;
			m->base = r0;
		}
	}
	else if (isxsel(op) && rtype(*r) == RCon) {
		r1 = newtmp("isel", i->cls, fn);
		emit(Ocopy, i->cls, r1, *r, NULL_R);
	}
	*r = r1;
}

static void
qbe_amd64_isel_seladdr(Ref *r, Num *tn, Fn *fn)
{
	Addr a;
	Ref r0;

	r0 = *r;
	if (rtype(r0) == RTmp) {
		memset(&a, 0, sizeof a);
		if (!qbe_amd64_isel_amatch(&a, tn, r0, fn))
			return;
		if (!req(a.base, NULL_R))
		if (a.offset.type == CAddr) {
			/* apple as does not support
			 * $foo(%r0, %r1, M); try to
			 * rewrite it or bail out if
			 * impossible
			 */
			if (!req(a.index, NULL_R) || rtype(a.base) != RTmp)
				return;
			else {
				a.index = a.base;
				a.scale = 1;
				a.base = NULL_R;
			}
		}
		chuse(r0, -1, fn);
		vgrow(&fn->mem, ++fn->nmem);
		fn->mem[fn->nmem-1] = a;
		chuse(a.base, +1, fn);
		chuse(a.index, +1, fn);
		*r = MEM(fn->nmem-1);
	}
}

static int
qbe_amd64_isel_cmpswap(Ref arg[2], int op)
{
	switch (op) {
	case NCmpI+Cflt:
	case NCmpI+Cfle:
		return 1;
	case NCmpI+Cfgt:
	case NCmpI+Cfge:
		return 0;
	}
	return rtype(arg[0]) == RCon;
}

static void
qbe_amd64_isel_selcmp(Ref arg[2], int k, int swap, Fn *fn)
{
	Ref r;
	Ins *icmp;

	if (swap) {
		r = arg[1];
		arg[1] = arg[0];
		arg[0] = r;
	}
	emit(Oxcmp, k, NULL_R, arg[1], arg[0]);
	icmp = GC(curi);
	if (rtype(arg[0]) == RCon) {
		SQ_ASSERT(k != Kw);
		icmp->arg[1] = newtmp("isel", k, fn);
		emit(Ocopy, k, icmp->arg[1], arg[0], NULL_R);
		qbe_amd64_isel_fixarg(&GC(curi)->arg[0], k, GC(curi), fn);
	}
	qbe_amd64_isel_fixarg(&icmp->arg[0], k, icmp, fn);
	qbe_amd64_isel_fixarg(&icmp->arg[1], k, icmp, fn);
}

static void
qbe_amd64_isel_sel(Ins i, Num *tn, Fn *fn)
{
	Ref r0, r1, tmp[7];
	int x, j, k, kc, sh, swap;
	Ins *i0, *i1;

	if (rtype(i.to) == RTmp)
	if (!isreg(i.to) && !isreg(i.arg[0]) && !isreg(i.arg[1]))
	if (fn->tmp[i.to.val].nuse == 0) {
		chuse(i.arg[0], -1, fn);
		chuse(i.arg[1], -1, fn);
		return;
	}
	i0 = GC(curi);
	k = i.cls;
	switch (i.op) {
	case Odiv:
	case Orem:
	case Oudiv:
	case Ourem:
		if (KBASE(k) == 1)
			goto Emit;
		if (i.op == Odiv || i.op == Oudiv)
			r0 = TMP(QBE_AMD64_RAX), r1 = TMP(QBE_AMD64_RDX);
		else
			r0 = TMP(QBE_AMD64_RDX), r1 = TMP(QBE_AMD64_RAX);
		emit(Ocopy, k, i.to, r0, NULL_R);
		emit(Ocopy, k, NULL_R, r1, NULL_R);
		if (rtype(i.arg[1]) == RCon) {
			/* immediates not allowed for
			 * divisions in x86
			 */
			r0 = newtmp("isel", k, fn);
		} else
			r0 = i.arg[1];
		if (fn->tmp[r0.val].slot != -1)
			err("unlikely argument %%%s in %s",
				fn->tmp[r0.val].name, optab[i.op].name);
		if (i.op == Odiv || i.op == Orem) {
			emit(Oxidiv, k, NULL_R, r0, NULL_R);
			emit(Osign, k, TMP(QBE_AMD64_RDX), TMP(QBE_AMD64_RAX), NULL_R);
		} else {
			emit(Oxdiv, k, NULL_R, r0, NULL_R);
			emit(Ocopy, k, TMP(QBE_AMD64_RDX), CON_Z, NULL_R);
		}
		emit(Ocopy, k, TMP(QBE_AMD64_RAX), i.arg[0], NULL_R);
		qbe_amd64_isel_fixarg(&GC(curi)->arg[0], k, GC(curi), fn);
		if (rtype(i.arg[1]) == RCon)
			emit(Ocopy, k, r0, i.arg[1], NULL_R);
		break;
	case Osar:
	case Oshr:
	case Oshl:
		r0 = i.arg[1];
		if (rtype(r0) == RCon)
			goto Emit;
		if (fn->tmp[r0.val].slot != -1)
			err("unlikely argument %%%s in %s",
				fn->tmp[r0.val].name, optab[i.op].name);
		i.arg[1] = TMP(QBE_AMD64_RCX);
		emit(Ocopy, Kw, NULL_R, TMP(QBE_AMD64_RCX), NULL_R);
		emiti(i);
		i1 = GC(curi);
		emit(Ocopy, Kw, TMP(QBE_AMD64_RCX), r0, NULL_R);
		qbe_amd64_isel_fixarg(&i1->arg[0], argcls(&i, 0), i1, fn);
		break;
	case Ouwtof:
		r0 = newtmp("utof", Kl, fn);
		emit(Osltof, k, i.to, r0, NULL_R);
		emit(Oextuw, Kl, r0, i.arg[0], NULL_R);
		qbe_amd64_isel_fixarg(&GC(curi)->arg[0], k, GC(curi), fn);
		break;
	case Oultof:
		/* %mask =l and %arg.0, 1
		 * %isbig =l shr %arg.0, 63
		 * %divided =l shr %arg.0, %isbig
		 * %or =l or %mask, %divided
		 * %float =d sltof %or
		 * %cast =l cast %float
		 * %addend =l shl %isbig, 52
		 * %sum =l add %cast, %addend
		 * %result =d cast %sum
		 */
		r0 = newtmp("utof", k, fn);
		if (k == Ks)
			kc = Kw, sh = 23;
		else
			kc = Kl, sh = 52;
		for (j=0; j<4; j++)
			tmp[j] = newtmp("utof", Kl, fn);
		for (; j<7; j++)
			tmp[j] = newtmp("utof", kc, fn);
		emit(Ocast, k, i.to, tmp[6], NULL_R);
		emit(Oadd, kc, tmp[6], tmp[4], tmp[5]);
		emit(Oshl, kc, tmp[5], tmp[1], getcon(sh, fn));
		emit(Ocast, kc, tmp[4], r0, NULL_R);
		emit(Osltof, k, r0, tmp[3], NULL_R);
		emit(Oor, Kl, tmp[3], tmp[0], tmp[2]);
		emit(Oshr, Kl, tmp[2], i.arg[0], tmp[1]);
		qbe_amd64_isel_sel(*GC(curi)++, 0, fn);
		ret_on_err();
		emit(Oshr, Kl, tmp[1], i.arg[0], getcon(63, fn));
		qbe_amd64_isel_fixarg(&GC(curi)->arg[0], Kl, GC(curi), fn);
		emit(Oand, Kl, tmp[0], i.arg[0], getcon(1, fn));
		qbe_amd64_isel_fixarg(&GC(curi)->arg[0], Kl, GC(curi), fn);
		break;
	case Ostoui:
		i.op = Ostosi;
		kc = Ks;
		tmp[4] = getcon(0xdf000000, fn);
		goto Oftoui;
	case Odtoui:
		i.op = Odtosi;
		kc = Kd;
		tmp[4] = getcon(0xc3e0000000000000, fn);
	Oftoui:
		if (k == Kw) {
			r0 = newtmp("ftou", Kl, fn);
			emit(Ocopy, Kw, i.to, r0, NULL_R);
			i.cls = Kl;
			i.to = r0;
			goto Emit;
		}
		/* %try0 =l {s,d}tosi %fp
		 * %mask =l sar %try0, 63
		 *
		 *    mask is all ones if the first
		 *    try was oob, all zeroes o.w.
		 *
		 * %fps ={s,d} sub %fp, (1<<63)
		 * %try1 =l {s,d}tosi %fps
		 *
		 * %tmp =l and %mask, %try1
		 * %res =l or %tmp, %try0
		 */
		r0 = newtmp("ftou", kc, fn);
		for (j=0; j<4; j++)
			tmp[j] = newtmp("ftou", Kl, fn);
		emit(Oor, Kl, i.to, tmp[0], tmp[3]);
		emit(Oand, Kl, tmp[3], tmp[2], tmp[1]);
		emit(i.op, Kl, tmp[2], r0, NULL_R);
		emit(Oadd, kc, r0, tmp[4], i.arg[0]);
		i1 = GC(curi); /* qbe_amd64_isel_fixarg() can change curi */
		qbe_amd64_isel_fixarg(&i1->arg[0], kc, i1, fn);
		qbe_amd64_isel_fixarg(&i1->arg[1], kc, i1, fn);
		emit(Osar, Kl, tmp[1], tmp[0], getcon(63, fn));
		emit(i.op, Kl, tmp[0], i.arg[0], NULL_R);
		qbe_amd64_isel_fixarg(&GC(curi)->arg[0], Kl, GC(curi), fn);
		break;
	case Onop:
		break;
	case Ostored:
	case Ostores:
	case Ostorel:
	case Ostorew:
	case Ostoreh:
	case Ostoreb:
		if (rtype(i.arg[0]) == RCon) {
			if (i.op == Ostored)
				i.op = Ostorel;
			if (i.op == Ostores)
				i.op = Ostorew;
		}
		qbe_amd64_isel_seladdr(&i.arg[1], tn, fn);
		goto Emit;
	case_Oload:
		qbe_amd64_isel_seladdr(&i.arg[0], tn, fn);
		goto Emit;
	case Odbgloc:
	case Ocall:
	case Osalloc:
	case Ocopy:
	case Oadd:
	case Osub:
	case Oneg:
	case Omul:
	case Oand:
	case Oor:
	case Oxor:
	case Oxtest:
	case Ostosi:
	case Odtosi:
	case Oswtof:
	case Osltof:
	case Oexts:
	case Otruncd:
	case Ocast:
	case_Oxsel:
	case_Oext:
Emit:
		emiti(i);
		i1 = GC(curi); /* qbe_amd64_isel_fixarg() can change curi */
		qbe_amd64_isel_fixarg(&i1->arg[0], argcls(&i, 0), i1, fn);
		qbe_amd64_isel_fixarg(&i1->arg[1], argcls(&i, 1), i1, fn);
		break;
	case Oalloc4:
	case Oalloc8:
	case Oalloc16:
		salloc(i.to, i.arg[0], fn);
		break;
	default:
		if (isext(i.op))
			goto case_Oext;
		if (isxsel(i.op))
			goto case_Oxsel;
		if (isload(i.op))
			goto case_Oload;
		if (iscmp(i.op, &kc, &x)) {
			switch (x) {
			case NCmpI+Cfeq:
				/* zf is set when operands are
				 * unordered, so we may have to
				 * check pf
				 */
				r0 = newtmp("isel", Kw, fn);
				r1 = newtmp("isel", Kw, fn);
				emit(Oand, Kw, i.to, r0, r1);
				emit(Oflagfo, k, r1, NULL_R, NULL_R);
				i.to = r0;
				break;
			case NCmpI+Cfne:
				r0 = newtmp("isel", Kw, fn);
				r1 = newtmp("isel", Kw, fn);
				emit(Oor, Kw, i.to, r0, r1);
				emit(Oflagfuo, k, r1, NULL_R, NULL_R);
				i.to = r0;
				break;
			}
			swap = qbe_amd64_isel_cmpswap(i.arg, x);
			if (swap)
				x = cmpop(x);
			emit(Oflag+x, k, i.to, NULL_R, NULL_R);
			qbe_amd64_isel_selcmp(i.arg, kc, swap, fn);
			break;
		}
		die("unknown instruction %s", optab[i.op].name);
	}

	while (i0>GC(curi) && --i0) {
		SQ_ASSERT(qbe_amd64_isel_rslot(i0->arg[0], fn) == -1);
		SQ_ASSERT(qbe_amd64_isel_rslot(i0->arg[1], fn) == -1);
	}
}

static Ins *
qbe_amd64_isel_flagi(Ins *i0, Ins *i)
{
	while (i>i0) {
		i--;
		if (amd64_op[i->op].zflag)
			return i;
		if (amd64_op[i->op].lflag)
			continue;
		return 0;
	}
	return 0;
}

static Ins*
qbe_amd64_isel_selsel(Fn *fn, Blk *b, Ins *i, Num *tn)
{
	Ref r, cr[2];
	int c, k, swap, gencmp, gencpy;
	Ins *isel0, *isel1, *fi;
	Tmp *t;

	SQ_ASSERT(i->op == Osel1);
	for (isel0=i; b->ins<isel0; isel0--) {
		if (isel0->op == Osel0)
			break;
		SQ_ASSERT(isel0->op == Osel1);
	}
	SQ_ASSERT(isel0->op == Osel0);
	r = isel0->arg[0];
	SQ_ASSERT(rtype(r) == RTmp);
	t = &fn->tmp[r.val];
	fi = qbe_amd64_isel_flagi(b->ins, isel0);
	cr[0] = cr[1] = NULL_R;
	gencmp = gencpy = swap = 0;
	k = Kw;
	c = Cine;
	if (!fi || !req(fi->to, r)) {
		gencmp = 1;
		cr[0] = r;
		cr[1] = CON_Z;
	}
	else if (iscmp(fi->op, &k, &c)) {
		if (c == NCmpI+Cfeq
		|| c == NCmpI+Cfne) {
			/* these are selected as 'and'
			 * or 'or', so we check their
			 * result with Cine
			 */
			c = Cine;
			goto Other;
		}
		swap = qbe_amd64_isel_cmpswap(fi->arg, c);
		if (swap)
			c = cmpop(c);
		if (t->nuse == 1) {
			gencmp = 1;
			cr[0] = fi->arg[0];
			cr[1] = fi->arg[1];
			*fi = (Ins){.op = Onop};
		}
	}
	else if (fi->op == Oand && t->nuse == 1
	     && (rtype(fi->arg[0]) == RTmp ||
	         rtype(fi->arg[1]) == RTmp)) {
		fi->op = Oxtest;
		fi->to = NULL_R;
		if (rtype(fi->arg[1]) == RCon) {
			r = fi->arg[1];
			fi->arg[1] = fi->arg[0];
			fi->arg[0] = r;
		}
	}
	else {
	Other:
		/* since flags are not tracked in liveness,
		 * the result of the flag-setting instruction
		 * has to be marked as live
		 */
		if (t->nuse == 1)
			gencpy = 1;
	}
	/* generate conditional moves */
	for (isel1=i; isel0<isel1; --isel1) {
		isel1->op = Oxsel+c;
		qbe_amd64_isel_sel(*isel1, tn, fn);
    ret_on_err_i();
	}
	SQ_ASSERT(!gencmp || !gencpy);
	if (gencmp)
		qbe_amd64_isel_selcmp(cr, k, swap, fn);
	if (gencpy)
		emit(Ocopy, Kw, NULL_R, r, NULL_R);
	*isel0 = (Ins){.op = Onop};
	return isel0;
}

static void
qbe_amd64_isel_seljmp(Blk *b, Fn *fn)
{
	Ref r;
	int c, k, swap;
	Ins *fi;
	Tmp *t;

	if (b->jmp.type == Jret0
	|| b->jmp.type == Jjmp
	|| b->jmp.type == Jhlt)
		return;
	SQ_ASSERT(b->jmp.type == Jjnz);
	r = b->jmp.arg;
	t = &fn->tmp[r.val];
	b->jmp.arg = NULL_R;
	SQ_ASSERT(rtype(r) == RTmp);
	if (b->s1 == b->s2) {
		chuse(r, -1, fn);
		b->jmp.type = Jjmp;
		b->s2 = 0;
		return;
	}
	fi = qbe_amd64_isel_flagi(b->ins, &b->ins[b->nins]);
	if (!fi || !req(fi->to, r)) {
		qbe_amd64_isel_selcmp((Ref[2]){r, CON_Z}, Kw, 0, fn);
		b->jmp.type = Jjf + Cine;
	}
	else if (iscmp(fi->op, &k, &c)
	     && c != NCmpI+Cfeq /* see qbe_amd64_isel_sel(), qbe_amd64_isel_selsel() */
	     && c != NCmpI+Cfne) {
		swap = qbe_amd64_isel_cmpswap(fi->arg, c);
		if (swap)
			c = cmpop(c);
		if (t->nuse == 1) {
			qbe_amd64_isel_selcmp(fi->arg, k, swap, fn);
			*fi = (Ins){.op = Onop};
		}
		b->jmp.type = Jjf + c;
	}
	else if (fi->op == Oand && t->nuse == 1
	     && (rtype(fi->arg[0]) == RTmp ||
	         rtype(fi->arg[1]) == RTmp)) {
		fi->op = Oxtest;
		fi->to = NULL_R;
		b->jmp.type = Jjf + Cine;
		if (rtype(fi->arg[1]) == RCon) {
			r = fi->arg[1];
			fi->arg[1] = fi->arg[0];
			fi->arg[0] = r;
		}
	}
	else {
		/* since flags are not tracked in liveness,
		 * the result of the flag-setting instruction
		 * has to be marked as live
		 */
		if (t->nuse == 1)
			emit(Ocopy, Kw, NULL_R, r, NULL_R);
		b->jmp.type = Jjf + Cine;
	}
}

enum {
	Pob,
	Pbis,
	Pois,
	Pobis,
	Pbi1,
	Pobi1,
};

/* mgen generated code
 *
 * (with-vars (o b i s)
 *   (patterns
 *     (ob   (add (con o) (tmp b)))
 *     (bis  (add (tmp b) (mul (tmp i) (con s 1 2 4 8))))
 *     (ois  (add (con o) (mul (tmp i) (con s 1 2 4 8))))
 *     (obis (add (con o) (tmp b) (mul (tmp i) (con s 1 2 4 8))))
 *     (bi1  (add (tmp b) (tmp i)))
 *     (obi1 (add (con o) (tmp b) (tmp i)))
 * ))
 */

static int
qbe_amd64_isel_opn(int op, int l, int r)
{
	static uchar Oaddtbl[91] = {
		2,
		2,2,
		4,4,5,
		6,6,8,8,
		4,4,9,10,9,
		7,7,5,8,9,5,
		4,4,12,10,12,12,12,
		4,4,9,10,9,9,12,9,
		11,11,5,8,9,5,12,9,5,
		7,7,5,8,9,5,12,9,5,5,
		11,11,5,8,9,5,12,9,5,5,5,
		4,4,9,10,9,9,12,9,9,9,9,9,
		7,7,5,8,9,5,12,9,5,5,5,9,5,
	};
	int t;

	if (l < r)
		t = l, l = r, r = t;
	switch (op) {
	case Omul:
		if (2 <= l)
		if (r == 0) {
			return 3;
		}
		return 2;
	case Oadd:
		return Oaddtbl[(l + l*l)/2 + r];
	default:
		return 2;
	}
}

static int
qbe_amd64_isel_refn(Ref r, Num *tn, Con *con)
{
	int64_t n;

	switch (rtype(r)) {
	case RTmp:
		if (!tn[r.val].n)
			tn[r.val].n = 2;
		return tn[r.val].n;
	case RCon:
		if (con[r.val].type != CBits)
			return 1;
		n = con[r.val].bits.i;
		if (n == 8 || n == 4 || n == 2 || n == 1)
			return 0;
		return 1;
	default:
		return INT_MIN;
	}
}

static bits match[13] = {
	[4] = BIT(Pob),
	[5] = BIT(Pbi1),
	[6] = BIT(Pob) | BIT(Pois),
	[7] = BIT(Pob) | BIT(Pobi1),
	[8] = BIT(Pbi1) | BIT(Pbis),
	[9] = BIT(Pbi1) | BIT(Pobi1),
	[10] = BIT(Pbi1) | BIT(Pbis) | BIT(Pobi1) | BIT(Pobis),
	[11] = BIT(Pob) | BIT(Pobi1) | BIT(Pobis),
	[12] = BIT(Pbi1) | BIT(Pobi1) | BIT(Pobis),
};

static uchar *matcher[] = {
	[Pbi1] = (uchar[]){
		1,3,1,3,2,0
	},
	[Pbis] = (uchar[]){
		5,1,8,5,27,1,5,1,2,5,13,3,1,1,3,3,3,2,0,1,
		3,3,3,2,3,1,0,1,29
	},
	[Pob] = (uchar[]){
		1,3,0,3,1,0
	},
	[Pobi1] = (uchar[]){
		5,3,9,9,10,33,12,35,45,1,5,3,11,9,7,9,4,9,
		17,1,3,0,3,1,3,2,0,3,1,1,3,0,34,1,37,1,5,2,
		5,7,2,7,8,37,29,1,3,0,1,32
	},
	[Pobis] = (uchar[]){
		5,2,10,7,11,19,49,1,1,3,3,3,2,1,3,0,3,1,0,
		1,3,0,5,1,8,5,25,1,5,1,2,5,13,3,1,1,3,3,3,
		2,0,1,3,3,3,2,26,1,51,1,5,1,6,5,9,1,3,0,51,
		3,1,1,3,0,45
	},
	[Pois] = (uchar[]){
		1,3,0,1,3,3,3,2,0
	},
};

/* end of generated code */

static void
qbe_amd64_isel_anumber(Num *tn, Blk *b, Con *con)
{
	Ins *i;
	Num *n;

	for (i=b->ins; i<&b->ins[b->nins]; i++) {
		if (rtype(i->to) != RTmp)
			continue;
		n = &tn[i->to.val];
		n->l = i->arg[0];
		n->r = i->arg[1];
		n->nl = qbe_amd64_isel_refn(n->l, tn, con);
		n->nr = qbe_amd64_isel_refn(n->r, tn, con);
		n->n = qbe_amd64_isel_opn(i->op, n->nl, n->nr);
	}
}

static Ref
qbe_amd64_isel_adisp(Con *c, Num *tn, Ref r, Fn *fn, int s)
{
	Ref v[2];
	int n;

	while (!req(r, NULL_R)) {
		SQ_ASSERT(rtype(r) == RTmp);
		n = qbe_amd64_isel_refn(r, tn, fn->con);
		if (!(match[n] & BIT(Pob)))
			break;
		runmatch(matcher[Pob], tn, r, v);
		SQ_ASSERT(rtype(v[0]) == RCon);
		addcon(c, &fn->con[v[0].val], s);
		r = v[1];
	}
	return r;
}

static int
qbe_amd64_isel_amatch(Addr *a, Num *tn, Ref r, Fn *fn)
{
	static int pat[] = {Pobis, Pobi1, Pbis, Pois, Pbi1, -1};
	Ref ro, rb, ri, rs, v[4];
	Con *c, co;
	int s, n, *p;

	if (rtype(r) != RTmp)
		return 0;

	n = qbe_amd64_isel_refn(r, tn, fn->con);
	memset(v, 0, sizeof v);
	for (p=pat; *p>=0; p++)
		if (match[n] & BIT(*p)) {
			runmatch(matcher[*p], tn, r, v);
			break;
		}
	if (*p < 0)
		v[1] = r;

	memset(&co, 0, sizeof co);
	ro = v[0];
	rb = qbe_amd64_isel_adisp(&co, tn, v[1], fn, 1);
	ri = v[2];
	rs = v[3];
	s = 1;

	if (*p < 0 && co.type != CUndef)
	if (qbe_amd64_isel_amatch(a, tn, rb, fn))
		return addcon(&a->offset, &co, 1);
	if (!req(ro, NULL_R)) {
		SQ_ASSERT(rtype(ro) == RCon);
		c = &fn->con[ro.val];
		if (!addcon(&co, c, 1))
			return 0;
	}
	if (!req(rs, NULL_R)) {
		SQ_ASSERT(rtype(rs) == RCon);
		c = &fn->con[rs.val];
		SQ_ASSERT(c->type == CBits);
		s = c->bits.i;
	}
	ri = qbe_amd64_isel_adisp(&co, tn, ri, fn, s);
	*a = (Addr){co, rb, ri, s};

	if (rtype(ri) == RTmp)
	if (fn->tmp[ri.val].slot != -1) {
		if (a->scale != 1
		|| fn->tmp[rb.val].slot != -1)
			return 0;
		a->base = ri;
		a->index = rb;
	}
	if (!req(a->base, NULL_R)) {
		SQ_ASSERT(rtype(a->base) == RTmp);
		s = fn->tmp[a->base.val].slot;
		if (s != -1)
			a->base = SLOT(s);
	}
	return 1;
}

/* instruction selection
 * requires use counts (as given by parsing)
 */
void
amd64_isel(Fn *fn)
{
	Blk *b, **sb;
	Ins *i;
	Phi *p;
	uint a;
	int n, al;
	int64_t sz;
	Num *num;

	/* assign slots to fast allocs */
	b = fn->start;
	/* specific to NAlign == 3 */ /* or change n=4 and sz /= 4 below */
	for (al=Oalloc, n=4; al<=Oalloc1; al++, n*=2)
		for (i=b->ins; i<&b->ins[b->nins]; i++)
			if (i->op == al) {
				if (rtype(i->arg[0]) != RCon)
					break;
				sz = fn->con[i->arg[0].val].bits.i;
				if (sz < 0 || sz >= INT_MAX-15)
					err("invalid alloc size %"PRId64, sz);
				sz = (sz + n-1) & -n;
				sz /= 4;
				if (sz > INT_MAX - fn->slot)
					die("alloc too large");
				fn->tmp[i->to.val].slot = fn->slot;
				fn->slot += sz;
				fn->salign = 2 + al - Oalloc;
				*i = (Ins){.op = Onop};
			}

	/* process basic blocks */
	n = fn->ntmp;
	num = emalloc(n * sizeof num[0]);
	for (b=fn->start; b; b=b->link) {
		GC(curi) = &GC(insb)[NIns];
		for (sb=(Blk*[3]){b->s1, b->s2, 0}; *sb; sb++)
			for (p=(*sb)->phi; p; p=p->link) {
				for (a=0; p->blk[a] != b; a++)
					SQ_ASSERT(a+1 < p->narg);
				qbe_amd64_isel_fixarg(&p->arg[a], p->cls, 0, fn);
			}
		memset(num, 0, n * sizeof num[0]);
		qbe_amd64_isel_anumber(num, b, fn->con);
		qbe_amd64_isel_seljmp(b, fn);
		for (i=&b->ins[b->nins]; i!=b->ins;) {
			--i;
			SQ_ASSERT(i->op != Osel0);
			if (i->op == Osel1) {
				i = qbe_amd64_isel_selsel(fn, b, i, num);
        ret_on_err();
      } else {
				qbe_amd64_isel_sel(*i, num, fn);
        ret_on_err();
      }
		}
		idup(b, GC(curi), &GC(insb)[NIns]-GC(curi));
	}
	qbe_free(num);

	if (GC(debug)['I']) {
		fprintf(stderr, "\n> After instruction selection:\n");
		printfn(fn, stderr);
	}
}
#undef G
/*** END FILE: amd64/isel.c ***/
/*** START FILE: amd64/sysv.c ***/
/* skipping all.h */

typedef struct QBE_AMD64_SYSV_AClass QBE_AMD64_SYSV_AClass;
typedef struct QBE_AMD64_SYSV_RAlloc QBE_AMD64_SYSV_RAlloc;

struct QBE_AMD64_SYSV_AClass {
	Typ *type;
	int inmem;
	int align;
	uint size;
	int cls[2];
	Ref ref[2];
};

struct QBE_AMD64_SYSV_RAlloc {
	Ins i;
	QBE_AMD64_SYSV_RAlloc *link;
};

static void
qbe_amd64_sysv_classify(QBE_AMD64_SYSV_AClass *a, Typ *t, uint s)
{
	Field *f;
	int *cls;
	uint n, s1;

	for (n=0, s1=s; n<t->nunion; n++, s=s1)
		for (f=t->fields[n]; f->type!=FEnd; f++) {
			SQ_ASSERT(s <= 16);
			cls = &a->cls[s/8];
			switch (f->type) {
			case FEnd:
				die("unreachable");
			case FPad:
				/* don't change anything */
				s += f->len;
				break;
			case Fs:
			case Fd:
				if (*cls == Kx)
					*cls = Kd;
				s += f->len;
				break;
			case Fb:
			case Fh:
			case Fw:
			case Fl:
				*cls = Kl;
				s += f->len;
				break;
			case FTyp:
				qbe_amd64_sysv_classify(a, &GC(typ)[f->len], s);
				s += GC(typ)[f->len].size;
				break;
			}
		}
}

static void
qbe_amd64_sysv_typclass(QBE_AMD64_SYSV_AClass *a, Typ *t)
{
	uint sz, al;

	sz = t->size;
	al = 1u << t->align;

	/* the ABI requires sizes to be rounded
	 * up to the nearest multiple of 8, moreover
	 * it makes it easy load and store structures
	 * in registers
	 */
	if (al < 8)
		al = 8;
	sz = (sz + al-1) & -al;

	a->type = t;
	a->size = sz;
	a->align = t->align;

	if (t->isdark || sz > 16 || sz == 0) {
		/* large or unaligned structures are
		 * required to be passed in memory
		 */
		a->inmem = 1;
		return;
	}

	a->cls[0] = Kx;
	a->cls[1] = Kx;
	a->inmem = 0;
	qbe_amd64_sysv_classify(a, t, 0);
}

static int
qbe_amd64_sysv_retr(Ref reg[2], QBE_AMD64_SYSV_AClass *aret)
{
	static int retreg[2][2] = {{QBE_AMD64_RAX, QBE_AMD64_RDX}, {QBE_AMD64_XMM0, QBE_AMD64_XMM0+1}};
	int n, k, ca, nr[2];

	nr[0] = nr[1] = 0;
	ca = 0;
	for (n=0; (uint)n*8<aret->size; n++) {
		k = KBASE(aret->cls[n]);
		reg[n] = TMP(retreg[k][nr[k]++]);
		ca += 1 << (2 * k);
	}
	return ca;
}

static void
qbe_amd64_sysv_selret(Blk *b, Fn *fn)
{
	int j, k, ca;
	Ref r, r0, reg[2];
	QBE_AMD64_SYSV_AClass aret;

	j = b->jmp.type;

	if (!isret(j) || j == Jret0)
		return;

	r0 = b->jmp.arg;
	b->jmp.type = Jret0;

	if (j == Jretc) {
		qbe_amd64_sysv_typclass(&aret, &GC(typ)[fn->retty]);
		if (aret.inmem) {
			SQ_ASSERT(rtype(fn->retr) == RTmp);
			emit(Ocopy, Kl, TMP(QBE_AMD64_RAX), fn->retr, NULL_R);
			emit(Oblit1, 0, NULL_R, INT(aret.type->size), NULL_R);
			emit(Oblit0, 0, NULL_R, r0, fn->retr);
			ca = 1;
		} else {
			ca = qbe_amd64_sysv_retr(reg, &aret);
			if (aret.size > 8) {
				r = newtmp("abi", Kl, fn);
				emit(Oload, Kl, reg[1], r, NULL_R);
				emit(Oadd, Kl, r, r0, getcon(8, fn));
			}
			emit(Oload, Kl, reg[0], r0, NULL_R);
		}
	} else {
		k = j - Jretw;
		if (KBASE(k) == 0) {
			emit(Ocopy, k, TMP(QBE_AMD64_RAX), r0, NULL_R);
			ca = 1;
		} else {
			emit(Ocopy, k, TMP(QBE_AMD64_XMM0), r0, NULL_R);
			ca = 1 << 2;
		}
	}

	b->jmp.arg = CALL(ca);
}

static int
qbe_amd64_sysv_argsclass(Ins *i0, Ins *i1, QBE_AMD64_SYSV_AClass *ac, int op, QBE_AMD64_SYSV_AClass *aret, Ref *env)
{
	int varc, envc, nint, ni, nsse, ns, n, *pn;
	QBE_AMD64_SYSV_AClass *a;
	Ins *i;

	if (aret && aret->inmem)
		nint = 5; /* hidden argument */
	else
		nint = 6;
	nsse = 8;
	varc = 0;
	envc = 0;
	for (i=i0, a=ac; i<i1; i++, a++)
		switch (i->op - op + Oarg) {
		case Oarg:
			if (KBASE(i->cls) == 0)
				pn = &nint;
			else
				pn = &nsse;
			if (*pn > 0) {
				--*pn;
				a->inmem = 0;
			} else
				a->inmem = 2;
			a->align = 3;
			a->size = 8;
			a->cls[0] = i->cls;
			break;
		case Oargc:
			n = i->arg[0].val;
			qbe_amd64_sysv_typclass(a, &GC(typ)[n]);
			if (a->inmem)
				continue;
			ni = ns = 0;
			for (n=0; (uint)n*8<a->size; n++)
				if (KBASE(a->cls[n]) == 0)
					ni++;
				else
					ns++;
			if (nint >= ni && nsse >= ns) {
				nint -= ni;
				nsse -= ns;
			} else
				a->inmem = 1;
			break;
		case Oarge:
			envc = 1;
			if (op == Opar)
				*env = i->to;
			else
				*env = i->arg[0];
			break;
		case Oargv:
			varc = 1;
			break;
		default:
			die("unreachable");
		}

	if (varc && envc)
		err_i("sysv abi does not support variadic env calls");

	return ((varc|envc) << 12) | ((6-nint) << 4) | ((8-nsse) << 8);
}

static int amd64_sysv_rsave[] = {
	QBE_AMD64_RDI, QBE_AMD64_RSI, QBE_AMD64_RDX, QBE_AMD64_RCX, QBE_AMD64_R8, QBE_AMD64_R9, QBE_AMD64_R10, QBE_AMD64_R11, QBE_AMD64_RAX,
	QBE_AMD64_XMM0, QBE_AMD64_XMM1, QBE_AMD64_XMM2, QBE_AMD64_XMM3, QBE_AMD64_XMM4, QBE_AMD64_XMM5, QBE_AMD64_XMM6, QBE_AMD64_XMM7,
	QBE_AMD64_XMM8, QBE_AMD64_XMM9, QBE_AMD64_XMM10, QBE_AMD64_XMM11, QBE_AMD64_XMM12, QBE_AMD64_XMM13, QBE_AMD64_XMM14, -1
};
static int amd64_sysv_rclob[] = {QBE_AMD64_RBX, QBE_AMD64_R12, QBE_AMD64_R13, QBE_AMD64_R14, QBE_AMD64_R15, -1};

MAKESURE(sysv_arrays_ok,
	sizeof amd64_sysv_rsave == (QBE_AMD64_NGPS_SYSV+QBE_AMD64_NFPS+1) * sizeof(int) &&
	sizeof amd64_sysv_rclob == (QBE_AMD64_NCLR_SYSV+1) * sizeof(int)
);

/* layout of call's second argument (RCall)
 *
 *  29     12    8    4  3  0
 *  |0...00|x|xxxx|xxxx|xx|xx|                  range
 *          |    |    |  |  ` gp regs returned (0..2)
 *          |    |    |  ` sse regs returned   (0..2)
 *          |    |    ` gp regs passed         (0..6)
 *          |    ` sse regs passed             (0..8)
 *          ` 1 if rax is used to pass data    (0..1)
 */

bits
amd64_sysv_retregs(Ref r, int p[2])
{
	bits b;
	int ni, nf;

	SQ_ASSERT(rtype(r) == RCall);
	b = 0;
	ni = r.val & 3;
	nf = (r.val >> 2) & 3;
	if (ni >= 1)
		b |= BIT(QBE_AMD64_RAX);
	if (ni >= 2)
		b |= BIT(QBE_AMD64_RDX);
	if (nf >= 1)
		b |= BIT(QBE_AMD64_XMM0);
	if (nf >= 2)
		b |= BIT(QBE_AMD64_XMM1);
	if (p) {
		p[0] = ni;
		p[1] = nf;
	}
	return b;
}

bits
amd64_sysv_argregs(Ref r, int p[2])
{
	bits b;
	int j, ni, nf, ra;

	SQ_ASSERT(rtype(r) == RCall);
	b = 0;
	ni = (r.val >> 4) & 15;
	nf = (r.val >> 8) & 15;
	ra = (r.val >> 12) & 1;
	for (j=0; j<ni; j++)
		b |= BIT(amd64_sysv_rsave[j]);
	for (j=0; j<nf; j++)
		b |= BIT(QBE_AMD64_XMM0+j);
	if (p) {
		p[0] = ni + ra;
		p[1] = nf;
	}
	return b | (ra ? BIT(QBE_AMD64_RAX) : 0);
}

static Ref
qbe_amd64_sysv_rarg(int ty, int *ni, int *ns)
{
	if (KBASE(ty) == 0)
		return TMP(amd64_sysv_rsave[(*ni)++]);
	else
		return TMP(QBE_AMD64_XMM0 + (*ns)++);
}

static void
qbe_amd64_sysv_selcall(Fn *fn, Ins *i0, Ins *i1, QBE_AMD64_SYSV_RAlloc **rap)
{
	Ins *i;
	QBE_AMD64_SYSV_AClass *ac, *a, aret;
	int ca, ni, ns, al;
	uint stk, off;
	Ref r, r1, r2, reg[2], env;
	QBE_AMD64_SYSV_RAlloc *ra;

	env = NULL_R;
	ac = alloc((i1-i0) * sizeof ac[0]);

	if (!req(i1->arg[1], NULL_R)) {
		SQ_ASSERT(rtype(i1->arg[1]) == RType);
		qbe_amd64_sysv_typclass(&aret, &GC(typ)[i1->arg[1].val]);
		ca = qbe_amd64_sysv_argsclass(i0, i1, ac, Oarg, &aret, &env);
	} else
		ca = qbe_amd64_sysv_argsclass(i0, i1, ac, Oarg, 0, &env);
	ret_on_err();

	for (stk=0, a=&ac[i1-i0]; a>ac;)
		if ((--a)->inmem) {
			if (a->align > 4)
				err("sysv abi requires alignments of 16 or less");
			stk += a->size;
			if (a->align == 4)
				stk += stk & 15;
		}
	stk += stk & 15;
	if (stk) {
		r = getcon(-(int64_t)stk, fn);
		emit(Osalloc, Kl, NULL_R, r, NULL_R);
	}

	if (!req(i1->arg[1], NULL_R)) {
		if (aret.inmem) {
			/* get the return location from eax
			 * it saves one callee-save reg */
			r1 = newtmp("abi", Kl, fn);
			emit(Ocopy, Kl, i1->to, TMP(QBE_AMD64_RAX), NULL_R);
			ca += 1;
		} else {
			/* todo, may read out of bounds.
			 * gcc did this up until 5.2, but
			 * this should still be fixed.
			 */
			if (aret.size > 8) {
				r = newtmp("abi", Kl, fn);
				aret.ref[1] = newtmp("abi", aret.cls[1], fn);
				emit(Ostorel, 0, NULL_R, aret.ref[1], r);
				emit(Oadd, Kl, r, i1->to, getcon(8, fn));
			}
			aret.ref[0] = newtmp("abi", aret.cls[0], fn);
			emit(Ostorel, 0, NULL_R, aret.ref[0], i1->to);
			ca += qbe_amd64_sysv_retr(reg, &aret);
			if (aret.size > 8)
				emit(Ocopy, aret.cls[1], aret.ref[1], reg[1], NULL_R);
			emit(Ocopy, aret.cls[0], aret.ref[0], reg[0], NULL_R);
			r1 = i1->to;
		}
		/* allocate return pad */
		ra = alloc(sizeof *ra);
		/* specific to NAlign == 3 */
		al = aret.align >= 2 ? aret.align - 2 : 0;
		ra->i = (Ins){Oalloc+al, Kl, r1, {getcon(aret.size, fn)}};
		ra->link = (*rap);
		*rap = ra;
	} else {
		ra = 0;
		if (KBASE(i1->cls) == 0) {
			emit(Ocopy, i1->cls, i1->to, TMP(QBE_AMD64_RAX), NULL_R);
			ca += 1;
		} else {
			emit(Ocopy, i1->cls, i1->to, TMP(QBE_AMD64_XMM0), NULL_R);
			ca += 1 << 2;
		}
	}

	emit(Ocall, i1->cls, NULL_R, i1->arg[0], CALL(ca));

	if (!req(NULL_R, env))
		emit(Ocopy, Kl, TMP(QBE_AMD64_RAX), env, NULL_R);
	else if ((ca >> 12) & 1) /* vararg call */
		emit(Ocopy, Kw, TMP(QBE_AMD64_RAX), getcon((ca >> 8) & 15, fn), NULL_R);

	ni = ns = 0;
	if (ra && aret.inmem)
		emit(Ocopy, Kl, qbe_amd64_sysv_rarg(Kl, &ni, &ns), ra->i.to, NULL_R); /* pass hidden argument */

	for (i=i0, a=ac; i<i1; i++, a++) {
		if (i->op >= Oarge || a->inmem)
			continue;
		r1 = qbe_amd64_sysv_rarg(a->cls[0], &ni, &ns);
		if (i->op == Oargc) {
			if (a->size > 8) {
				r2 = qbe_amd64_sysv_rarg(a->cls[1], &ni, &ns);
				r = newtmp("abi", Kl, fn);
				emit(Oload, a->cls[1], r2, r, NULL_R);
				emit(Oadd, Kl, r, i->arg[1], getcon(8, fn));
			}
			emit(Oload, a->cls[0], r1, i->arg[1], NULL_R);
		} else
			emit(Ocopy, i->cls, r1, i->arg[0], NULL_R);
	}

	if (!stk)
		return;

	r = newtmp("abi", Kl, fn);
	for (i=i0, a=ac, off=0; i<i1; i++, a++) {
		if (i->op >= Oarge || !a->inmem)
			continue;
		r1 = newtmp("abi", Kl, fn);
		if (i->op == Oargc) {
			if (a->align == 4)
				off += off & 15;
			emit(Oblit1, 0, NULL_R, INT(a->type->size), NULL_R);
			emit(Oblit0, 0, NULL_R, i->arg[1], r1);
		} else
			emit(Ostorel, 0, NULL_R, i->arg[0], r1);
		emit(Oadd, Kl, r1, r, getcon(off, fn));
		off += a->size;
	}
	emit(Osalloc, Kl, r, getcon(stk, fn), NULL_R);
}

static int
qbe_amd64_sysv_selpar(Fn *fn, Ins *i0, Ins *i1)
{
	QBE_AMD64_SYSV_AClass *ac, *a, aret;
	Ins *i;
	int ni, ns, s, al, fa;
	Ref r, env;

	env = NULL_R;
	ac = alloc((i1-i0) * sizeof ac[0]);
	GC(curi) = &GC(insb)[NIns];
	ni = ns = 0;

	if (fn->retty >= 0) {
		qbe_amd64_sysv_typclass(&aret, &GC(typ)[fn->retty]);
		fa = qbe_amd64_sysv_argsclass(i0, i1, ac, Opar, &aret, &env);
	} else
		fa = qbe_amd64_sysv_argsclass(i0, i1, ac, Opar, 0, &env);
	ret_on_err_i();
	fn->reg = amd64_sysv_argregs(CALL(fa), 0);

	for (i=i0, a=ac; i<i1; i++, a++) {
		if (i->op != Oparc || a->inmem)
			continue;
		if (a->size > 8) {
			r = newtmp("abi", Kl, fn);
			a->ref[1] = newtmp("abi", Kl, fn);
			emit(Ostorel, 0, NULL_R, a->ref[1], r);
			emit(Oadd, Kl, r, i->to, getcon(8, fn));
		}
		a->ref[0] = newtmp("abi", Kl, fn);
		emit(Ostorel, 0, NULL_R, a->ref[0], i->to);
		/* specific to NAlign == 3 */
		al = a->align >= 2 ? a->align - 2 : 0;
		emit(Oalloc+al, Kl, i->to, getcon(a->size, fn), NULL_R);
	}

	if (fn->retty >= 0 && aret.inmem) {
		r = newtmp("abi", Kl, fn);
		emit(Ocopy, Kl, r, qbe_amd64_sysv_rarg(Kl, &ni, &ns), NULL_R);
		fn->retr = r;
	}

	for (i=i0, a=ac, s=4; i<i1; i++, a++) {
		switch (a->inmem) {
		case 1:
			if (a->align > 4)
				err_i("sysv abi requires alignments of 16 or less");
			if (a->align == 4)
				s = (s+3) & -4;
			fn->tmp[i->to.val].slot = -s;
			s += a->size / 4;
			continue;
		case 2:
			emit(Oload, i->cls, i->to, SLOT(-s), NULL_R);
			s += 2;
			continue;
		}
		if (i->op == Opare)
			continue;
		r = qbe_amd64_sysv_rarg(a->cls[0], &ni, &ns);
		if (i->op == Oparc) {
			emit(Ocopy, a->cls[0], a->ref[0], r, NULL_R);
			if (a->size > 8) {
				r = qbe_amd64_sysv_rarg(a->cls[1], &ni, &ns);
				emit(Ocopy, a->cls[1], a->ref[1], r, NULL_R);
			}
		} else
			emit(Ocopy, i->cls, i->to, r, NULL_R);
	}

	if (!req(NULL_R, env))
		emit(Ocopy, Kl, env, TMP(QBE_AMD64_RAX), NULL_R);

	return fa | (s*4)<<12;
}

static Blk *
qbe_amd64_sysv_split(Fn *fn, Blk *b)
{
	Blk *bn;

	++fn->nblk;
	bn = newblk();
	idup(bn, GC(curi), &GC(insb)[NIns]-GC(curi));
	GC(curi) = &GC(insb)[NIns];
	bn->visit = ++b->visit;
	strf(bn->name, "%s.%d", b->name, b->visit);
	bn->loop = b->loop;
	bn->link = b->link;
	b->link = bn;
	return bn;
}

static void
qbe_amd64_sysv_chpred(Blk *b, Blk *bp, Blk *bp1)
{
	Phi *p;
	uint a;

	for (p=b->phi; p; p=p->link) {
		for (a=0; p->blk[a]!=bp; a++)
			SQ_ASSERT(a+1<p->narg);
		p->blk[a] = bp1;
	}
}

static void
qbe_amd64_sysv_selvaarg(Fn *fn, Blk *b, Ins *i)
{
	Ref loc, lreg, lstk, nr, r0, r1, c4, c8, c16, c, ap;
	Blk *b0, *bstk, *breg;
	int isint;

	c4 = getcon(4, fn);
	c8 = getcon(8, fn);
	c16 = getcon(16, fn);
	ap = i->arg[0];
	isint = KBASE(i->cls) == 0;

	/* @b [...]
	       r0 =l add ap, (0 or 4)
	       nr =l loadsw r0
	       r1 =w cultw nr, (48 or 176)
	       jnz r1, @breg, @bstk
	   @breg
	       r0 =l add ap, 16
	       r1 =l loadl r0
	       lreg =l add r1, nr
	       r0 =w add nr, (8 or 16)
	       r1 =l add ap, (0 or 4)
	       storew r0, r1
	   @bstk
	       r0 =l add ap, 8
	       lstk =l loadl r0
	       r1 =l add lstk, 8
	       storel r1, r0
	   @b0
	       %loc =l phi @breg %lreg, @bstk %lstk
	       i->to =(i->cls) load %loc
	*/

	loc = newtmp("abi", Kl, fn);
	emit(Oload, i->cls, i->to, loc, NULL_R);
	b0 = qbe_amd64_sysv_split(fn, b);
	b0->jmp = b->jmp;
	b0->s1 = b->s1;
	b0->s2 = b->s2;
	if (b->s1)
		qbe_amd64_sysv_chpred(b->s1, b, b0);
	if (b->s2 && b->s2 != b->s1)
		qbe_amd64_sysv_chpred(b->s2, b, b0);

	lreg = newtmp("abi", Kl, fn);
	nr = newtmp("abi", Kl, fn);
	r0 = newtmp("abi", Kw, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Ostorew, Kw, NULL_R, r0, r1);
	emit(Oadd, Kl, r1, ap, isint ? CON_Z : c4);
	emit(Oadd, Kw, r0, nr, isint ? c8 : c16);
	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Oadd, Kl, lreg, r1, nr);
	emit(Oload, Kl, r1, r0, NULL_R);
	emit(Oadd, Kl, r0, ap, c16);
	breg = qbe_amd64_sysv_split(fn, b);
	breg->jmp.type = Jjmp;
	breg->s1 = b0;

	lstk = newtmp("abi", Kl, fn);
	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, NULL_R, r1, r0);
	emit(Oadd, Kl, r1, lstk, c8);
	emit(Oload, Kl, lstk, r0, NULL_R);
	emit(Oadd, Kl, r0, ap, c8);
	bstk = qbe_amd64_sysv_split(fn, b);
	bstk->jmp.type = Jjmp;
	bstk->s1 = b0;

	b0->phi = alloc(sizeof *b0->phi);
	*b0->phi = (Phi){
		.cls = Kl, .to = loc,
		.narg = 2,
		.blk = vnew(2, sizeof b0->phi->blk[0], PFn),
		.arg = vnew(2, sizeof b0->phi->arg[0], PFn),
	};
	b0->phi->blk[0] = bstk;
	b0->phi->blk[1] = breg;
	b0->phi->arg[0] = lstk;
	b0->phi->arg[1] = lreg;
	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kw, fn);
	b->jmp.type = Jjnz;
	b->jmp.arg = r1;
	b->s1 = breg;
	b->s2 = bstk;
	c = getcon(isint ? 48 : 176, fn);
	emit(Ocmpw+Ciult, Kw, r1, nr, c);
	emit(Oloadsw, Kl, nr, r0, NULL_R);
	emit(Oadd, Kl, r0, ap, isint ? CON_Z : c4);
}

static void
qbe_amd64_sysv_selvastart(Fn *fn, int fa, Ref ap)
{
	Ref r0, r1;
	int gp, fp, sp;

	gp = ((fa >> 4) & 15) * 8;
	fp = 48 + ((fa >> 8) & 15) * 16;
	sp = fa >> 12;
	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, NULL_R, r1, r0);
	emit(Oadd, Kl, r1, TMP(QBE_AMD64_RBP), getcon(-176, fn));
	emit(Oadd, Kl, r0, ap, getcon(16, fn));
	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, NULL_R, r1, r0);
	emit(Oadd, Kl, r1, TMP(QBE_AMD64_RBP), getcon(sp, fn));
	emit(Oadd, Kl, r0, ap, getcon(8, fn));
	r0 = newtmp("abi", Kl, fn);
	emit(Ostorew, Kw, NULL_R, getcon(fp, fn), r0);
	emit(Oadd, Kl, r0, ap, getcon(4, fn));
	emit(Ostorew, Kw, NULL_R, getcon(gp, fn), ap);
}

void
amd64_sysv_abi(Fn *fn)
{
	Blk *b;
	Ins *i, *i0;
	QBE_AMD64_SYSV_RAlloc *ral;
	int n0, n1, ioff, fa;

	for (b=fn->start; b; b=b->link)
		b->visit = 0;

	/* lower parameters */
	for (b=fn->start, i=b->ins; i<&b->ins[b->nins]; i++)
		if (!ispar(i->op))
			break;
	fa = qbe_amd64_sysv_selpar(fn, b->ins, i);
  ret_on_err();
	n0 = &GC(insb)[NIns] - GC(curi);
	ioff = i - b->ins;
	n1 = b->nins - ioff;
	vgrow(&b->ins, n0+n1);
	icpy(b->ins+n0, b->ins+ioff, n1);
	icpy(b->ins, GC(curi), n0);
	b->nins = n0+n1;

	/* lower calls, returns, and vararg instructions */
	ral = 0;
	b = fn->start;
	do {
		if (!(b = b->link))
			b = fn->start; /* do it last */
		if (b->visit)
			continue;
		GC(curi) = &GC(insb)[NIns];
		qbe_amd64_sysv_selret(b, fn);
		for (i=&b->ins[b->nins]; i!=b->ins;)
			switch ((--i)->op) {
			default:
				emiti(*i);
				break;
			case Ocall:
				for (i0=i; i0>b->ins; i0--)
					if (!isarg((i0-1)->op))
						break;
				qbe_amd64_sysv_selcall(fn, i0, i, &ral);
				ret_on_err();
				i = i0;
				break;
			case Ovastart:
				qbe_amd64_sysv_selvastart(fn, fa, i->arg[0]);
				break;
			case Ovaarg:
				qbe_amd64_sysv_selvaarg(fn, b, i);
				break;
			case Oarg:
			case Oargc:
				die("unreachable");
			}
		if (b == fn->start)
			for (; ral; ral=ral->link)
				emiti(ral->i);
		idup(b, GC(curi), &GC(insb)[NIns]-GC(curi));
	} while (b != fn->start);

	if (GC(debug)['A']) {
		fprintf(stderr, "\n> After ABI lowering:\n");
		printfn(fn, stderr);
	}
}
#undef G
/*** END FILE: amd64/sysv.c ***/
/*** START FILE: amd64/targ.c ***/
/* skipping all.h */

static Amd64Op amd64_op[NOp] = {
#define O(op, t, x) [O##op] =
#define X(nm, zf, lf) { nm, zf, lf, },
/* ------------------------------------------------------------including ops.h */
#ifndef X /* amd64 */
	#define X(NMemArgs, SetsZeroFlag, LeavesFlags)
#endif

#ifndef V /* riscv64 */
	#define V(Imm)
#endif

#ifndef F
#define F(a,b,c,d,e,f,g,h,i,j)
#endif

#define T(a,b,c,d,e,f,g,h) {                          \
	{[Kw]=K##a, [Kl]=K##b, [Ks]=K##c, [Kd]=K##d}, \
	{[Kw]=K##e, [Kl]=K##f, [Ks]=K##g, [Kd]=K##h}  \
}

/*********************/
/* PUBLIC OPERATIONS */
/*********************/

/*                                can fold                        */
/*                                | has identity                  */
/*                                | | identity value for arg[1]   */
/*                                | | | commutative               */
/*                                | | | | associative             */
/*                                | | | | | idempotent            */
/*                                | | | | | | c{eq,ne}[wl]        */
/*                                | | | | | | | c[us][gl][et][wl] */
/*                                | | | | | | | | value if = args */
/*                                | | | | | | | | | pinned        */
/* Arithmetic and Bits            v v v v v v v v v v             */
O(add,     T(w,l,s,d, w,l,s,d), F(1,1,0,1,1,0,0,0,0,0)) X(2,1,0) V(1)
O(sub,     T(w,l,s,d, w,l,s,d), F(1,1,0,0,0,0,0,0,0,0)) X(2,1,0) V(0)
O(neg,     T(w,l,s,d, x,x,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(1,1,0) V(0)
O(div,     T(w,l,s,d, w,l,s,d), F(1,1,1,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(rem,     T(w,l,e,e, w,l,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(udiv,    T(w,l,e,e, w,l,e,e), F(1,1,1,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(urem,    T(w,l,e,e, w,l,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(mul,     T(w,l,s,d, w,l,s,d), F(1,1,1,1,0,0,0,0,0,0)) X(2,0,0) V(0)
O(and,     T(w,l,e,e, w,l,e,e), F(1,0,0,1,1,1,0,0,0,0)) X(2,1,0) V(1)
O(or,      T(w,l,e,e, w,l,e,e), F(1,1,0,1,1,1,0,0,0,0)) X(2,1,0) V(1)
O(xor,     T(w,l,e,e, w,l,e,e), F(1,1,0,1,1,0,0,0,0,0)) X(2,1,0) V(1)
O(sar,     T(w,l,e,e, w,w,e,e), F(1,1,0,0,0,0,0,0,0,0)) X(1,1,0) V(1)
O(shr,     T(w,l,e,e, w,w,e,e), F(1,1,0,0,0,0,0,0,0,0)) X(1,1,0) V(1)
O(shl,     T(w,l,e,e, w,w,e,e), F(1,1,0,0,0,0,0,0,0,0)) X(1,1,0) V(1)

/* Comparisons */
O(ceqw,    T(w,w,e,e, w,w,e,e), F(1,1,1,1,0,0,1,0,1,0)) X(0,1,0) V(0)
O(cnew,    T(w,w,e,e, w,w,e,e), F(1,1,0,1,0,0,1,0,0,0)) X(0,1,0) V(0)
O(csgew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csgtw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(cslew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csltw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)
O(cugew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cugtw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(culew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cultw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)

O(ceql,    T(l,l,e,e, l,l,e,e), F(1,0,0,1,0,0,1,0,1,0)) X(0,1,0) V(0)
O(cnel,    T(l,l,e,e, l,l,e,e), F(1,0,0,1,0,0,1,0,0,0)) X(0,1,0) V(0)
O(csgel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csgtl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(cslel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csltl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)
O(cugel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cugtl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(culel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cultl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)

O(ceqs,    T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cges,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cgts,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cles,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(clts,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cnes,    T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cos,     T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cuos,    T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)

O(ceqd,    T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cged,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cgtd,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cled,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cltd,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cned,    T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cod,     T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cuod,    T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)

/* Memory */
O(storeb,  T(w,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(storeh,  T(w,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(storew,  T(w,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(storel,  T(l,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(stores,  T(s,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(stored,  T(d,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

O(loadsb,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loadub,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loadsh,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loaduh,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loadsw,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loaduw,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(load,    T(m,m,m,m, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

/* Extensions and Truncations */
O(extsb,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extub,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extsh,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extuh,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extsw,   T(e,w,e,e, e,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extuw,   T(e,w,e,e, e,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

O(exts,    T(e,e,e,s, e,e,e,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(truncd,  T(e,e,d,e, e,e,x,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(stosi,   T(s,s,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(stoui,   T(s,s,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(dtosi,   T(d,d,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(dtoui,   T(d,d,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(swtof,   T(e,e,w,w, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(uwtof,   T(e,e,w,w, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(sltof,   T(e,e,l,l, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(ultof,   T(e,e,l,l, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(cast,    T(s,d,w,l, x,x,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

/* Stack Allocation */
O(alloc4,  T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(alloc8,  T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(alloc16, T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)

/* Variadic Function Helpers */
O(vaarg,   T(m,m,m,m, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(vastart, T(m,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)

O(copy,    T(w,l,s,d, x,x,x,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

/* Debug */
O(dbgloc,  T(w,e,e,e, w,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

/****************************************/
/* INTERNAL OPERATIONS (keep nop first) */
/****************************************/

/* Miscellaneous and Architecture-Specific Operations */
O(nop,     T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(addr,    T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(blit0,   T(m,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,1,0) V(0)
O(blit1,   T(w,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,1,0) V(0)
O(sel0,    T(w,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(sel1,    T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(swap,    T(w,l,s,d, w,l,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(1,0,0) V(0)
O(sign,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(salloc,  T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xidiv,   T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(1,0,0) V(0)
O(xdiv,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(1,0,0) V(0)
O(xcmp,    T(w,l,s,d, w,l,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(1,1,0) V(0)
O(xtest,   T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(1,1,0) V(0)
O(acmp,    T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(acmn,    T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(afcmp,   T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(reqz,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(rnez,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)

/* Arguments, Parameters, and Calls */
O(par,     T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parsb,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parub,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parsh,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(paruh,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parc,    T(e,x,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(pare,    T(e,x,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(arg,     T(w,l,s,d, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argsb,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argub,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argsh,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(arguh,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argc,    T(e,x,e,e, e,l,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(arge,    T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argv,    T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(call,    T(m,m,m,m, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)

/* Flags Setting */
O(flagieq,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagine,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagisge, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagisgt, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagisle, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagislt, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiuge, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiugt, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiule, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiult, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfeq,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfge,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfgt,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfle,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagflt,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfne,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfo,   T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfuo,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

/* Backend Flag Select (Condition Move) */
O(xselieq,  T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseline,  T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselisge, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselisgt, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselisle, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselislt, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliuge, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliugt, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliule, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliult, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfeq,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfge,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfgt,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfle,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselflt,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfne,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfo,   T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfuo,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)

#undef T
#undef X
#undef V
#undef O

/*
| column -t -o ' '
*/
/* ------------------------------------------------------------end of ops.h */
};

static int
amd64_memargs(int op)
{
	return amd64_op[op].nmem;
}

#define AMD64_COMMON \
	.gpr0 = QBE_AMD64_RAX, \
	.ngpr = QBE_AMD64_NGPR, \
	.fpr0 = QBE_AMD64_XMM0, \
	.nfpr = QBE_AMD64_NFPR, \
	.rglob = BIT(QBE_AMD64_RBP) | BIT(QBE_AMD64_RSP), \
	.nrglob = 2, \
	.memargs = amd64_memargs, \
	.abi0 = elimsb, \
	.isel = amd64_isel, \
	.cansel = 1,

static Target T_amd64_sysv = {
	.name = "amd64_sysv",
	.emitfin = elf_emitfin,
	.asloc = ".L",
	.abi1 = amd64_sysv_abi,
	.rsave = amd64_sysv_rsave,
	.nrsave = {QBE_AMD64_NGPS_SYSV, QBE_AMD64_NFPS},
	.retregs = amd64_sysv_retregs,
	.argregs = amd64_sysv_argregs,
	.emitfn = amd64_sysv_emitfn,
	AMD64_COMMON
};

static Target T_amd64_apple = {
	.name = "amd64_apple",
	.apple = 1,
	.emitfin = macho_emitfin,
	.asloc = "L",
	.assym = "_",
	.abi1 = amd64_sysv_abi,
	.rsave = amd64_sysv_rsave,
	.nrsave = {QBE_AMD64_NGPS_SYSV, QBE_AMD64_NFPS},
	.retregs = amd64_sysv_retregs,
	.argregs = amd64_sysv_argregs,
	.emitfn = amd64_sysv_emitfn,
	AMD64_COMMON
};

static Target T_amd64_win = {
	.name = "amd64_win",
	.windows = 1,
	.emitfin = pe_emitfin,
	.asloc = "L",
	.abi1 = amd64_winabi_abi,
	.rsave = amd64_winabi_rsave,
	.nrsave = {QBE_AMD64_NGPS_WIN, QBE_AMD64_NFPS},
	.retregs = amd64_winabi_retregs,
	.argregs = amd64_winabi_argregs,
	.emitfn = amd64_winabi_emitfn,
	AMD64_COMMON
};
#undef G
/*** END FILE: amd64/targ.c ***/
/*** START FILE: amd64/winabi.c ***/
/* skipping all.h */

#include <stdbool.h>

typedef enum QBE_AMD64_WINABI_ArgPassStyle {
  APS_Invalid = 0,
  APS_Register,
  APS_InlineOnStack,
  APS_CopyAndPointerInRegister,
  APS_CopyAndPointerOnStack,
  APS_VarargsTag,
  APS_EnvTag,
} QBE_AMD64_WINABI_ArgPassStyle;

typedef struct QBE_AMD64_WINABI_ArgClass {
  Typ* type;
  QBE_AMD64_WINABI_ArgPassStyle style;
  int align;
  uint size;
  int cls;
  Ref ref;
} QBE_AMD64_WINABI_ArgClass;

typedef struct QBE_AMD64_WINABI_ExtraAlloc QBE_AMD64_WINABI_ExtraAlloc;
struct QBE_AMD64_WINABI_ExtraAlloc {
  Ins instr;
  QBE_AMD64_WINABI_ExtraAlloc* link;
};

#define ALIGN_DOWN(n, a) ((n) & ~((a)-1))
#define ALIGN_UP(n, a) ALIGN_DOWN((n) + (a)-1, (a))

// Number of stack bytes required be reserved for the callee.
#define SHADOW_SPACE_SIZE 32

static int amd64_winabi_rsave[] = {QBE_AMD64_RCX,  QBE_AMD64_RDX,   QBE_AMD64_R8,    QBE_AMD64_R9,    QBE_AMD64_R10,   QBE_AMD64_R11,   QBE_AMD64_RAX,  QBE_AMD64_XMM0,
                            QBE_AMD64_XMM1, QBE_AMD64_XMM2,  QBE_AMD64_XMM3,  QBE_AMD64_XMM4,  QBE_AMD64_XMM5,  QBE_AMD64_XMM6,  QBE_AMD64_XMM7, QBE_AMD64_XMM8,
                            QBE_AMD64_XMM9, QBE_AMD64_XMM10, QBE_AMD64_XMM11, QBE_AMD64_XMM12, QBE_AMD64_XMM13, QBE_AMD64_XMM14, -1};
static int amd64_winabi_rclob[] = {QBE_AMD64_RBX, QBE_AMD64_R12, QBE_AMD64_R13, QBE_AMD64_R14, QBE_AMD64_R15, QBE_AMD64_RSI, QBE_AMD64_RDI, -1};

MAKESURE(winabi_arrays_ok,
         sizeof amd64_winabi_rsave == (QBE_AMD64_NGPS_WIN + QBE_AMD64_NFPS + 1) * sizeof(int) &&
             sizeof amd64_winabi_rclob == (QBE_AMD64_NCLR_WIN + 1) * sizeof(int));

// layout of call's second argument (RCall)
//
// bit 0: rax returned
// bit 1: xmm0 returned
// bits 23: 0
// bits 4567: rcx, rdx, r8, r9 passed
// bits 89ab: xmm0,1,2,3 passed
// bit c: env call (rax passed)
// bits d..1f: 0

bits amd64_winabi_retregs(Ref r, int p[2]) {
  SQ_ASSERT(rtype(r) == RCall);

  bits b = 0;
  int num_int_returns = r.val & 1;
  int num_float_returns = r.val & 2;
  if (num_int_returns == 1) {
    b |= BIT(QBE_AMD64_RAX);
  } else {
    b |= BIT(QBE_AMD64_XMM0);
  }
  if (p) {
    p[0] = num_int_returns;
    p[1] = num_float_returns;
  }
  return b;
}

static uint amd64_winbi_popcnt(bits b) {
  b = (b & 0x5555555555555555) + ((b >> 1) & 0x5555555555555555);
  b = (b & 0x3333333333333333) + ((b >> 2) & 0x3333333333333333);
  b = (b & 0x0f0f0f0f0f0f0f0f) + ((b >> 4) & 0x0f0f0f0f0f0f0f0f);
  b += (b >> 8);
  b += (b >> 16);
  b += (b >> 32);
  return b & 0xff;
}

bits amd64_winabi_argregs(Ref r, int p[2]) {
  SQ_ASSERT(rtype(r) == RCall);

  // On SysV, these are counts. Here, a count isn't sufficient, we actually need
  // to know which ones are in use because they're not necessarily contiguous.
  int int_passed = (r.val >> 4) & 15;
  int float_passed = (r.val >> 8) & 15;
  bool env_param = (r.val >> 12) & 1;

  bits b = 0;
  b |= (int_passed & 1) ? BIT(QBE_AMD64_RCX) : 0;
  b |= (int_passed & 2) ? BIT(QBE_AMD64_RDX) : 0;
  b |= (int_passed & 4) ? BIT(QBE_AMD64_R8) : 0;
  b |= (int_passed & 8) ? BIT(QBE_AMD64_R9) : 0;
  b |= (float_passed & 1) ? BIT(QBE_AMD64_XMM0) : 0;
  b |= (float_passed & 2) ? BIT(QBE_AMD64_XMM1) : 0;
  b |= (float_passed & 4) ? BIT(QBE_AMD64_XMM2) : 0;
  b |= (float_passed & 8) ? BIT(QBE_AMD64_XMM3) : 0;
  b |= env_param ? BIT(QBE_AMD64_RAX) : 0;
  if (p) {
    // TODO: The only place this is used is live.c. I'm not sure what should be
    // returned here wrt to using the same counter for int/float regs on win.
    // For now, try the number of registers in use even though they're not
    // contiguous.
    p[0] = amd64_winbi_popcnt(int_passed);
    p[1] = amd64_winbi_popcnt(float_passed);
  }
  return b;
}

typedef struct QBE_AMD64_WINABI_RegisterUsage {
  // Counter for both int/float as they're counted together. Only if the bool's
  // set in regs_passed is the given register *actually* needed for a value
  // (i.e. needs to be saved, etc.).
  int num_regs_passed;

  // Indexed first by 0=int, 1=float, use KBASE(cls).
  // Indexed second by register index in calling convention, so for integer,
  // 0=QBE_AMD64_RCX, 1=QBE_AMD64_RDX, 2=QBE_AMD64_R8, 3=QBE_AMD64_R9, and for float QBE_AMD64_XMM0, QBE_AMD64_XMM1, QBE_AMD64_XMM2, QBE_AMD64_XMM3.
  bool regs_passed[2][4];

  bool rax_returned;
  bool xmm0_returned;

  // This is also used as where the va_start will start for varargs functions
  // (there's no 'Oparv', so we need to keep track of a count here.)
  int num_named_args_passed;

  // This is set when classifying the arguments for a call (but not when
  // classifying the parameters of a function definition).
  bool is_varargs_call;

  bool has_env;
} QBE_AMD64_WINABI_RegisterUsage;

static int register_usage_to_call_arg_value(QBE_AMD64_WINABI_RegisterUsage reg_usage) {
  return (reg_usage.rax_returned << 0) |        //
         (reg_usage.xmm0_returned << 1) |       //
         (reg_usage.regs_passed[0][0] << 4) |   //
         (reg_usage.regs_passed[0][1] << 5) |   //
         (reg_usage.regs_passed[0][2] << 6) |   //
         (reg_usage.regs_passed[0][3] << 7) |   //
         (reg_usage.regs_passed[1][0] << 8) |   //
         (reg_usage.regs_passed[1][1] << 9) |   //
         (reg_usage.regs_passed[1][2] << 10) |  //
         (reg_usage.regs_passed[1][3] << 11) |  //
         (reg_usage.has_env << 12);
}

// Assigns the argument to a register if there's any left according to the
// calling convention, and updates the regs_passed bools. Otherwise marks the
// value as needing stack space to be passed.
static void assign_register_or_stack(QBE_AMD64_WINABI_RegisterUsage* reg_usage,
                                     QBE_AMD64_WINABI_ArgClass* arg,
                                     bool is_float,
                                     bool by_copy) {
  if (reg_usage->num_regs_passed == 4) {
    arg->style = by_copy ? APS_CopyAndPointerOnStack : APS_InlineOnStack;
  } else {
    reg_usage->regs_passed[is_float][reg_usage->num_regs_passed] = true;
    ++reg_usage->num_regs_passed;
    arg->style = by_copy ? APS_CopyAndPointerInRegister : APS_Register;
  }
  ++reg_usage->num_named_args_passed;
}

static bool type_is_by_copy(Typ* type) {
  // Note that only these sizes are passed by register, even though e.g. a
  // 5 byte struct would "fit", it still is passed by copy-and-pointer.
  return type->isdark || (type->size != 1 && type->size != 2 &&
                          type->size != 4 && type->size != 8);
}

// This function is used for both arguments and parameters.
// begin_instr should either point at the first Oarg or Opar, and end_instr
// should point past the last one (so to the Ocall for arguments, or to the
// first 'real' instruction of the function for parameters).
static void classify_arguments(QBE_AMD64_WINABI_RegisterUsage* reg_usage,
                               Ins* begin_instr,
                               Ins* end_instr,
                               QBE_AMD64_WINABI_ArgClass* arg_classes,
                               Ref* env) {
  QBE_AMD64_WINABI_ArgClass* arg = arg_classes;
  // For each argument, determine how it will be passed (int, float, stack)
  // and update the `reg_usage` counts. Additionally, fill out arg_classes for
  // each argument.
  for (Ins* instr = begin_instr; instr < end_instr; ++instr, ++arg) {
    switch (instr->op) {
      case Oarg:
      case Opar:
        assign_register_or_stack(reg_usage, arg, KBASE(instr->cls),
                                 /*by_copy=*/false);
        arg->cls = instr->cls;
        arg->align = 3;
        arg->size = 8;
        break;
      case Oargc:
      case Oparc: {
        int typ_index = instr->arg[0].val;
        Typ* type = &GC(typ)[typ_index];
        bool by_copy = type_is_by_copy(type);
        assign_register_or_stack(reg_usage, arg, /*is_float=*/false, by_copy);
        arg->cls = Kl;
        if (!by_copy && type->size <= 4) {
          arg->cls = Kw;
        }
        arg->align = 3;
        arg->size = type->size;
        break;
      }
      case Oarge:
        *env = instr->arg[0];
        arg->style = APS_EnvTag;
        reg_usage->has_env = true;
        break;
      case Opare:
        *env = instr->to;
        arg->style = APS_EnvTag;
        reg_usage->has_env = true;
        break;
      case Oargv:
        reg_usage->is_varargs_call = true;
        arg->style = APS_VarargsTag;
        break;
    }
  }

  if (reg_usage->has_env && reg_usage->is_varargs_call) {
    die("can't use env with varargs");
  }

  // During a varargs call, float arguments have to be duplicated to their
  // associated integer register, so mark them as in-use too.
  if (reg_usage->is_varargs_call) {
    for (int i = 0; i < 4; ++i) {
      if (reg_usage->regs_passed[/*float*/ 1][i]) {
        reg_usage->regs_passed[/*int*/ 0][i] = true;
      }
    }
  }
}

static bool is_integer_type(int ty) {
  SQ_ASSERT(ty >= 0 && ty < 4 && "expecting Kw Kl Ks Kd");
  return KBASE(ty) == 0;
}

static Ref register_for_arg(int cls, int counter) {
  SQ_ASSERT(counter < 4);
  if (is_integer_type(cls)) {
    return TMP(amd64_winabi_rsave[counter]);
  } else {
    return TMP(QBE_AMD64_XMM0 + counter);
  }
}

static Ins* lower_call(Fn* func,
                       Blk* block,
                       Ins* call_instr,
                       QBE_AMD64_WINABI_ExtraAlloc** pextra_alloc) {
  // Call arguments are instructions. Walk through them to find the end of the
  // call+args that we need to process (and return the instruction past the body
  // of the instruction for continuing processing).
  Ins* instr_past_args = call_instr - 1;
  for (; instr_past_args >= block->ins; --instr_past_args) {
    if (!isarg(instr_past_args->op)) {
      break;
    }
  }
  Ins* earliest_arg_instr = instr_past_args + 1;

  // Don't need an QBE_AMD64_WINABI_ArgClass for the call itself, so one less than the total
  // number of instructions we're dealing with.
  uint num_args = call_instr - earliest_arg_instr;
  QBE_AMD64_WINABI_ArgClass* arg_classes = alloc(num_args * sizeof(QBE_AMD64_WINABI_ArgClass));

  QBE_AMD64_WINABI_RegisterUsage reg_usage = {0};
  QBE_AMD64_WINABI_ArgClass ret_arg_class = {0};

  // Ocall's two arguments are the the function to be called in 0, and, if the
  // the function returns a non-basic type, then arg[1] is a reference to the
  // type of the return. req checks if Refs are equal; `R` is 0.
  bool il_has_struct_return = !req(call_instr->arg[1], NULL_R);
  bool is_struct_return = false;
  if (il_has_struct_return) {
    Typ* ret_type = &GC(typ)[call_instr->arg[1].val];
    is_struct_return = type_is_by_copy(ret_type);
    if (is_struct_return) {
      assign_register_or_stack(&reg_usage, &ret_arg_class, /*is_float=*/false,
                               /*by_copy=*/true);
    }
    ret_arg_class.size = ret_type->size;
  }
  Ref env = NULL_R;
  classify_arguments(&reg_usage, earliest_arg_instr, call_instr, arg_classes,
                     &env);

  // We now know which arguments are on the stack and which are in registers, so
  // we can allocate the correct amount of space to stash the stack-located ones
  // into.
  uint stack_usage = 0;
  for (uint i = 0; i < num_args; ++i) {
    QBE_AMD64_WINABI_ArgClass* arg = &arg_classes[i];
    // stack_usage only accounts for pushes that are for values that don't have
    // enough registers. Large struct copies are alloca'd separately, and then
    // only have (potentially) 8 bytes to add to stack_usage here.
    if (arg->style == APS_InlineOnStack) {
      if (arg->align > 4) {
        err_null("win abi cannot pass alignments > 16");
      }
      stack_usage += arg->size;
    } else if (arg->style == APS_CopyAndPointerOnStack) {
      stack_usage += 8;
    }
  }
  stack_usage = ALIGN_UP(stack_usage, 16);

  // Note that here we're logically 'after' the call (due to emitting
  // instructions in reverse order), so we're doing a negative stack
  // allocation to clean up after the call.
  Ref stack_size_ref =
      getcon(-(int64_t)(stack_usage + SHADOW_SPACE_SIZE), func);
  emit(Osalloc, Kl, NULL_R, stack_size_ref, NULL_R);

  QBE_AMD64_WINABI_ExtraAlloc* return_pad = NULL;
  if (is_struct_return) {
    return_pad = alloc(sizeof(QBE_AMD64_WINABI_ExtraAlloc));
    Ref ret_pad_ref = newtmp("abi.ret_pad", Kl, func);
    return_pad->instr =
        (Ins){Oalloc8, Kl, ret_pad_ref, {getcon(ret_arg_class.size, func)}};
    return_pad->link = (*pextra_alloc);
    *pextra_alloc = return_pad;
    reg_usage.rax_returned = true;
    emit(Ocopy, call_instr->cls, call_instr->to, TMP(QBE_AMD64_RAX), NULL_R);
  } else {
    if (il_has_struct_return) {
      // In the case that at the IL level, a struct return was specified, but as
      // far as the calling convention is concerned it's not actually by
      // pointer, we need to store the return value into an alloca because
      // subsequent IL will still be treating the function return as a pointer.
      QBE_AMD64_WINABI_ExtraAlloc* return_copy = alloc(sizeof(QBE_AMD64_WINABI_ExtraAlloc));
      return_copy->instr =
          (Ins){Oalloc8, Kl, call_instr->to, {getcon(8, func)}};
      return_copy->link = (*pextra_alloc);
      *pextra_alloc = return_copy;
      Ref copy = newtmp("abi.copy", Kl, func);
      emit(Ostorel, 0, NULL_R, copy, call_instr->to);
      emit(Ocopy, Kl, copy, TMP(QBE_AMD64_RAX), NULL_R);
      reg_usage.rax_returned = true;
    } else if (is_integer_type(call_instr->cls)) {
      // Only a basic type returned from the call, integer.
      emit(Ocopy, call_instr->cls, call_instr->to, TMP(QBE_AMD64_RAX), NULL_R);
      reg_usage.rax_returned = true;
    } else {
      // Basic type, floating point.
      emit(Ocopy, call_instr->cls, call_instr->to, TMP(QBE_AMD64_XMM0), NULL_R);
      reg_usage.xmm0_returned = true;
    }
  }

  // Emit the actual call instruction. There's no 'to' value by this point
  // because we've lowered it into register manipulation (that's the `R`),
  // arg[0] of the call is the function, and arg[1] is register usage is
  // documented as above (copied from SysV).
  emit(Ocall, call_instr->cls, NULL_R, call_instr->arg[0],
       CALL(register_usage_to_call_arg_value(reg_usage)));

  if (!req(NULL_R, env)) {
    // If there's an env arg to be passed, it gets stashed in QBE_AMD64_RAX.
    emit(Ocopy, Kl, TMP(QBE_AMD64_RAX), env, NULL_R);
  }

  if (reg_usage.is_varargs_call) {
    // Any float arguments need to be duplicated to integer registers. This is
    // required by the calling convention so that dumping to shadow space can be
    // done without a prototype and for varargs.
#define DUP_IF_USED(index, floatreg, intreg)        \
  if (reg_usage.regs_passed[/*float*/ 1][index]) {  \
    emit(Ocast, Kl, TMP(intreg), TMP(floatreg), NULL_R); \
  }
    DUP_IF_USED(0, QBE_AMD64_XMM0, QBE_AMD64_RCX);
    DUP_IF_USED(1, QBE_AMD64_XMM1, QBE_AMD64_RDX);
    DUP_IF_USED(2, QBE_AMD64_XMM2, QBE_AMD64_R8);
    DUP_IF_USED(3, QBE_AMD64_XMM3, QBE_AMD64_R9);
#undef DUP_IF_USED
  }

  int reg_counter = 0;
  if (is_struct_return) {
    Ref first_reg = register_for_arg(Kl, reg_counter++);
    emit(Ocopy, Kl, first_reg, return_pad->instr.to, NULL_R);
  }

  // This is where we actually do the load of values into registers or into
  // stack slots.
  Ref arg_stack_slots = newtmp("abi.args", Kl, func);
  uint slot_offset = SHADOW_SPACE_SIZE;
  QBE_AMD64_WINABI_ArgClass* arg = arg_classes;
  for (Ins* instr = earliest_arg_instr; instr != call_instr; ++instr, ++arg) {
    switch (arg->style) {
      case APS_Register: {
        Ref into = register_for_arg(arg->cls, reg_counter++);
        if (instr->op == Oargc) {
          // If this is a small struct being passed by value. The value in the
          // instruction in this case is a pointer, but it needs to be loaded
          // into the register.
          emit(Oload, arg->cls, into, instr->arg[1], NULL_R);
        } else {
          // Otherwise, a normal value passed in a register.
          emit(Ocopy, instr->cls, into, instr->arg[0], NULL_R);
        }
        break;
      }
      case APS_InlineOnStack: {
        Ref slot = newtmp("abi.off", Kl, func);
        if (instr->op == Oargc) {
          // This is a small struct, so it's not passed by copy, but the
          // instruction is a pointer. So we need to copy it into the stack
          // slot. (And, remember that these are emitted backwards, so store,
          // then load.)
          Ref smalltmp = newtmp("abi.smalltmp", arg->cls, func);
          emit(Ostorel, 0, NULL_R, smalltmp, slot);
          emit(Oload, arg->cls, smalltmp, instr->arg[1], NULL_R);
        } else {
          // Stash the value into the stack slot.
          emit(Ostorel, 0, NULL_R, instr->arg[0], slot);
        }
        emit(Oadd, Kl, slot, arg_stack_slots, getcon(slot_offset, func));
        slot_offset += arg->size;
        break;
      }
      case APS_CopyAndPointerInRegister:
      case APS_CopyAndPointerOnStack: {
        // Alloca a space to copy into, and blit the value from the instr to the
        // copied location.
        QBE_AMD64_WINABI_ExtraAlloc* arg_copy = alloc(sizeof(QBE_AMD64_WINABI_ExtraAlloc));
        Ref copy_ref = newtmp("abi.copy", Kl, func);
        arg_copy->instr =
            (Ins){Oalloc8, Kl, copy_ref, {getcon(arg->size, func)}};
        arg_copy->link = (*pextra_alloc);
        *pextra_alloc = arg_copy;
        emit(Oblit1, 0, NULL_R, INT(arg->size), NULL_R);
        emit(Oblit0, 0, NULL_R, instr->arg[1], copy_ref);

        // Now load the pointer into the correct register or stack slot.
        if (arg->style == APS_CopyAndPointerInRegister) {
          Ref into = register_for_arg(arg->cls, reg_counter++);
          emit(Ocopy, Kl, into, copy_ref, NULL_R);
        } else {
          SQ_ASSERT(arg->style == APS_CopyAndPointerOnStack);
          Ref slot = newtmp("abi.off", Kl, func);
          emit(Ostorel, 0, NULL_R, copy_ref, slot);
          emit(Oadd, Kl, slot, arg_stack_slots, getcon(slot_offset, func));
          slot_offset += 8;
        }
        break;
      }
      case APS_EnvTag:
      case APS_VarargsTag:
        // Nothing to do here, see right before the call for reg dupe.
        break;
      case APS_Invalid:
        die("unreachable");
    }
  }

  if (stack_usage) {
    // The last (first in call order) thing we do is allocate the the stack
    // space we're going to fill with temporaries.
    emit(Osalloc, Kl, arg_stack_slots,
         getcon(stack_usage + SHADOW_SPACE_SIZE, func), NULL_R);
  } else {
    // When there's no usage for temporaries, we can add this into the other
    // alloca, but otherwise emit it separately (not storing into a reference)
    // so that it doesn't get removed later for being useless.
    emit(Osalloc, Kl, NULL_R, getcon(SHADOW_SPACE_SIZE, func), NULL_R);
  }

  return instr_past_args;
}

static void lower_block_return(Fn* func, Blk* block) {
  int jmp_type = block->jmp.type;

  if (!isret(jmp_type) || jmp_type == Jret0) {
    return;
  }

  // Save the argument, and set the block to be a void return because once it's
  // lowered it's handled by the the register/stack manipulation.
  Ref ret_arg = block->jmp.arg;
  block->jmp.type = Jret0;

  QBE_AMD64_WINABI_RegisterUsage reg_usage = {0};

  if (jmp_type == Jretc) {
    Typ* type = &GC(typ)[func->retty];
    if (type_is_by_copy(type)) {
      SQ_ASSERT(rtype(func->retr) == RTmp);
      emit(Ocopy, Kl, TMP(QBE_AMD64_RAX), func->retr, NULL_R);
      emit(Oblit1, 0, NULL_R, INT(type->size), NULL_R);
      emit(Oblit0, 0, NULL_R, ret_arg, func->retr);
    } else {
      emit(Oload, Kl, TMP(QBE_AMD64_RAX), ret_arg, NULL_R);
    }
    reg_usage.rax_returned = true;
  } else {
    int k = jmp_type - Jretw;
    if (is_integer_type(k)) {
      emit(Ocopy, k, TMP(QBE_AMD64_RAX), ret_arg, NULL_R);
      reg_usage.rax_returned = true;
    } else {
      emit(Ocopy, k, TMP(QBE_AMD64_XMM0), ret_arg, NULL_R);
      reg_usage.xmm0_returned = true;
    }
  }
  block->jmp.arg = CALL(register_usage_to_call_arg_value(reg_usage));
}

static void lower_vastart(Fn* func,
                          QBE_AMD64_WINABI_RegisterUsage* param_reg_usage,
                          Ref valist) {
  SQ_ASSERT(func->vararg);
  // In varargs functions:
  // 1. the int registers are already dumped to the shadow stack space;
  // 2. any parameters passed in floating point registers have
  //    been duplicated to the integer registers
  // 3. we ensure (later) that for varargs functions we're always using an rbp
  //    frame pointer.
  // So, the ... argument is just indexed past rbp by the number of named values
  // that were actually passed.

  Ref offset = newtmp("abi.vastart", Kl, func);
  emit(Ostorel, 0, NULL_R, offset, valist);

  // *8 for sizeof(u64), +16 because the return address and rbp have been pushed
  // by the time we get to the body of the function.
  emit(Oadd, Kl, offset, TMP(QBE_AMD64_RBP),
       getcon(param_reg_usage->num_named_args_passed * 8 + 16, func));
}

static void lower_vaarg(Fn* func, Ins* vaarg_instr) {
  // va_list is just a void** on winx64, so load the pointer, then load the
  // argument from that pointer, then increment the pointer to the next arg.
  // (All emitted backwards as usual.)
  Ref inc = newtmp("abi.vaarg.inc", Kl, func);
  Ref ptr = newtmp("abi.vaarg.ptr", Kl, func);
  emit(Ostorel, 0, NULL_R, inc, vaarg_instr->arg[0]);
  emit(Oadd, Kl, inc, ptr, getcon(8, func));
  emit(Oload, vaarg_instr->cls, vaarg_instr->to, ptr, NULL_R);
  emit(Oload, Kl, ptr, vaarg_instr->arg[0], NULL_R);
}

static void lower_args_for_block(Fn* func,
                                 Blk* block,
                                 QBE_AMD64_WINABI_RegisterUsage* param_reg_usage,
                                 QBE_AMD64_WINABI_ExtraAlloc** pextra_alloc) {
  // global temporary buffer used by emit. Reset to the end, and predecremented
  // when adding to it.
  GC(curi) = &GC(insb)[NIns];

  lower_block_return(func, block);

  if (block->nins) {
    // Work backwards through the instructions, either copying them unchanged,
    // or modifying as necessary.
    for (Ins* instr = &block->ins[block->nins - 1]; instr >= block->ins;) {
      switch (instr->op) {
        case Ocall:
          instr = lower_call(func, block, instr, pextra_alloc);
          ret_on_err();
          break;
        case Ovastart:
          lower_vastart(func, param_reg_usage, instr->arg[0]);
          --instr;
          break;
        case Ovaarg:
          lower_vaarg(func, instr);
          --instr;
          break;
        case Oarg:
        case Oargc:
          die("unreachable");
        default:
          emiti(*instr);
          --instr;
          break;
      }
    }
  }

  // This it the start block, which is processed last. Add any allocas that
  // other blocks needed.
  bool is_start_block = block == func->start;
  if (is_start_block) {
    for (QBE_AMD64_WINABI_ExtraAlloc* ea = *pextra_alloc; ea; ea = ea->link) {
      emiti(ea->instr);
    }
  }

  // emit/emiti add instructions from the end to the beginning of the temporary
  // global buffer. dup the final version into the final block storage.
  block->nins = &GC(insb)[NIns] - GC(curi);
  idup(block, GC(curi), block->nins);
}

static Ins* find_end_of_func_parameters(Blk* start_block) {
  Ins* i;
  for (i = start_block->ins; i < &start_block->ins[start_block->nins]; ++i) {
    if (!ispar(i->op)) {
      break;
    }
  }
  return i;
}

// Copy from registers/stack into values.
static QBE_AMD64_WINABI_RegisterUsage lower_func_parameters(Fn* func) {
  // This is half-open, so end points after the last Opar.
  Blk* start_block = func->start;
  Ins* start_of_params = start_block->ins;
  Ins* end_of_params = find_end_of_func_parameters(start_block);

  size_t num_params = end_of_params - start_of_params;
  QBE_AMD64_WINABI_ArgClass* arg_classes = alloc(num_params * sizeof(QBE_AMD64_WINABI_ArgClass));
  QBE_AMD64_WINABI_ArgClass arg_ret = {0};

  // global temporary buffer used by emit. Reset to the end, and predecremented
  // when adding to it.
  GC(curi) = &GC(insb)[NIns];

  int reg_counter = 0;
  QBE_AMD64_WINABI_RegisterUsage reg_usage = {0};
  if (func->retty >= 0) {
    bool by_copy = type_is_by_copy(&GC(typ)[func->retty]);
    if (by_copy) {
      assign_register_or_stack(&reg_usage, &arg_ret, /*is_float=*/false,
                               by_copy);
      Ref ret_ref = newtmp("abi.ret", Kl, func);
      emit(Ocopy, Kl, ret_ref, TMP(QBE_AMD64_RCX), NULL_R);
      func->retr = ret_ref;
      ++reg_counter;
    }
  }
  Ref env = NULL_R;
  classify_arguments(&reg_usage, start_of_params, end_of_params, arg_classes,
                     &env);
  func->reg = amd64_winabi_argregs(
      CALL(register_usage_to_call_arg_value(reg_usage)), NULL);

  // Copy from the registers or stack slots into the named parameters. Depending
  // on how they're passed, they either need to be copied or loaded.
  QBE_AMD64_WINABI_ArgClass* arg = arg_classes;
  uint slot_offset = SHADOW_SPACE_SIZE / 4 + 4;
  for (Ins* instr = start_of_params; instr < end_of_params; ++instr, ++arg) {
    switch (arg->style) {
      case APS_Register: {
        Ref from = register_for_arg(arg->cls, reg_counter++);
        // If it's a struct at the IL level, we need to copy the register into
        // an alloca so we have something to point at (same for InlineOnStack).
        if (instr->op == Oparc) {
          arg->ref = newtmp("abi", Kl, func);
          emit(Ostorel, 0, NULL_R, arg->ref, instr->to);
          emit(Ocopy, instr->cls, arg->ref, from, NULL_R);
          emit(Oalloc8, Kl, instr->to, getcon(arg->size, func), NULL_R);
        } else {
          emit(Ocopy, instr->cls, instr->to, from, NULL_R);
        }
        break;
      }
      case APS_InlineOnStack:
        if (instr->op == Oparc) {
          arg->ref = newtmp("abi", Kl, func);
          emit(Ostorel, 0, NULL_R, arg->ref, instr->to);
          emit(Ocopy, instr->cls, arg->ref, SLOT(-slot_offset), NULL_R);
          emit(Oalloc8, Kl, instr->to, getcon(arg->size, func), NULL_R);
        } else {
          emit(Ocopy, Kl, instr->to, SLOT(-slot_offset), NULL_R);
        }
        slot_offset += 2;
        break;
      case APS_CopyAndPointerOnStack:
        emit(Oload, Kl, instr->to, SLOT(-slot_offset), NULL_R);
        slot_offset += 2;
        break;
      case APS_CopyAndPointerInRegister: {
        // Because this has to be a copy (that we own), it is sufficient to just
        // copy the register to the target.
        Ref from = register_for_arg(Kl, reg_counter++);
        emit(Ocopy, Kl, instr->to, from, NULL_R);
        break;
      }
      case APS_EnvTag:
        break;
      case APS_VarargsTag:
      case APS_Invalid:
        die("unreachable");
    }
  }

  // If there was an `env`, it was passed in QBE_AMD64_RAX, so copy it into the env ref.
  if (!req(NULL_R, env)) {
    emit(Ocopy, Kl, env, TMP(QBE_AMD64_RAX), NULL_R);
  }

  int num_created_instrs = &GC(insb)[NIns] - GC(curi);
  int num_other_after_instrs = (int)(start_block->nins - num_params);
  int new_total_instrs = num_other_after_instrs + num_created_instrs;
  Ins* new_instrs = vnew(new_total_instrs, sizeof(Ins), PFn);
  Ins* instr_p = icpy(new_instrs, GC(curi), num_created_instrs);
  icpy(instr_p, end_of_params, num_other_after_instrs);
  start_block->nins = new_total_instrs;
  start_block->ins = new_instrs;

  return reg_usage;
}

// The main job of this function is to lower generic instructions into the
// specific details of how arguments are passed, and parameters are
// interpreted for win x64. A useful reference is
// https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention .
//
// Some of the major differences from SysV if you're comparing the code
// (non-exhaustive):
// - only 4 int and 4 float regs are used
// - when an int register is assigned a value, its associated float register is
//   left unused (and vice versa). i.e. there's only one counter as you assign
//   arguments to registers.
// - any structs that aren't 1/2/4/8 bytes in size are passed by pointer, not
//   by copying them into the stack. So e.g. if you pass something like
//   `struct { void*, int64_t }` by value, it first needs to be copied to
//   another alloca (in order to maintain value semantics at the language
//   level), then the pointer to that copy is treated as a regular integer
//   argument (which then itself may *also* be copied to the stack in the case
//   there's no integer register remaining.)
// - when calling a varargs functions, floating point values must be duplicated
//   integer registers. Along with the above restrictions, this makes varargs
//   handling simpler for the callee than SysV.
void amd64_winabi_abi(Fn* func) {
  // The first thing to do is lower incoming parameters to this function.
  QBE_AMD64_WINABI_RegisterUsage param_reg_usage = lower_func_parameters(func);

  // This is the second larger part of the job. We walk all blocks, and rewrite
  // instructions returns, calls, and handling of varargs into their win x64
  // specific versions. Any other instructions are just passed through unchanged
  // by using `emiti`.

  // Skip over the entry block, and do it at the end so that our later
  // modifications can add allocations to the start block. In particular, we
  // need to add stack allocas for copies when structs are passed or returned by
  // value.
  QBE_AMD64_WINABI_ExtraAlloc* extra_alloc = NULL;
  for (Blk* block = func->start->link; block; block = block->link) {
    lower_args_for_block(func, block, &param_reg_usage, &extra_alloc);
    ret_on_err();
  }
  lower_args_for_block(func, func->start, &param_reg_usage, &extra_alloc);
  ret_on_err();

  if (GC(debug)['A']) {
    fprintf(stderr, "\n> After ABI lowering:\n");
    printfn(func, stderr);
  }
}
#undef G
/*** END FILE: amd64/winabi.c ***/
/*** START FILE: arm64/abi.c ***/
/* skipping all.h */

typedef struct Abi Abi;
typedef struct QBE_ARM64_ABI_Class QBE_ARM64_ABI_Class;
typedef struct QBE_ARM64_ABI_Insl QBE_ARM64_ABI_Insl;
typedef struct QBE_ARM64_ABI_Params QBE_ARM64_ABI_Params;

enum {
	QBE_ARM64_ABI_Cstk = 1, /* pass on the stack */
	QBE_ARM64_ABI_Cptr = 2, /* replaced by a pointer */
};

struct QBE_ARM64_ABI_Class {
	char class;
	char ishfa;
	struct {
		char base;
		uchar size;
	} hfa;
	uint size;
	uint align;
	Typ *t;
	uchar nreg;
	uchar ngp;
	uchar nfp;
	int reg[4];
	int cls[4];
};

struct QBE_ARM64_ABI_Insl {
	Ins i;
	QBE_ARM64_ABI_Insl *link;
};

struct QBE_ARM64_ABI_Params {
	uint ngp;
	uint nfp;
	uint stk;
};

static int qbe_arm64_abi_gpreg[12] = {QBE_ARM64_R0, QBE_ARM64_R1, QBE_ARM64_R2, QBE_ARM64_R3, QBE_ARM64_R4, QBE_ARM64_R5, QBE_ARM64_R6, QBE_ARM64_R7};
static int qbe_arm64_abi_fpreg[12] = {QBE_ARM64_V0, QBE_ARM64_V1, QBE_ARM64_V2, QBE_ARM64_V3, QBE_ARM64_V4, QBE_ARM64_V5, QBE_ARM64_V6, QBE_ARM64_V7};
static int store[] = {
	[Kw] = Ostorew, [Kl] = Ostorel,
	[Ks] = Ostores, [Kd] = Ostored
};

/* layout of call's second argument (RCall)
 *
 *         13
 *  29   14 |    9    5   2  0
 *  |0.00|x|x|xxxx|xxxx|xxx|xx|                  range
 *        | |    |    |   |  ` gp regs returned (0..2)
 *        | |    |    |   ` fp regs returned    (0..4)
 *        | |    |    ` gp regs passed          (0..8)
 *        | |     ` fp regs passed              (0..8)
 *        | ` indirect result register x8 used  (0..1)
 *        ` env pointer passed in x9            (0..1)
 */

static int
qbe_arm64_abi_isfloatv(Typ *t, char *cls)
{
	Field *f;
	uint n;

	for (n=0; n<t->nunion; n++)
		for (f=t->fields[n]; f->type != FEnd; f++)
			switch (f->type) {
			case Fs:
				if (*cls == Kd)
					return 0;
				*cls = Ks;
				break;
			case Fd:
				if (*cls == Ks)
					return 0;
				*cls = Kd;
				break;
			case FTyp:
				if (qbe_arm64_abi_isfloatv(&GC(typ)[f->len], cls))
					break;
				/* fall through */
			default:
				return 0;
			}
	return 1;
}

static void
qbe_arm64_abi_typclass(QBE_ARM64_ABI_Class *c, Typ *t, int *gp, int *fp)
{
	uint64_t sz, hfasz;
	uint n;

	sz = (t->size + 7) & -8;
	c->t = t;
	c->class = 0;
	c->ngp = 0;
	c->nfp = 0;
	c->align = 8;

	if (t->align > 3)
		err("alignments larger than 8 are not supported");

	c->size = sz;
	c->hfa.base = Kx;
	c->ishfa = qbe_arm64_abi_isfloatv(t, &c->hfa.base);
	hfasz = t->size/(KWIDE(c->hfa.base) ? 8 : 4);
	c->ishfa &= !t->isdark && hfasz <= 4;
	c->hfa.size = hfasz;

	if (c->ishfa) {
		for (n=0; n<hfasz; n++, c->nfp++) {
			c->reg[n] = *fp++;
			c->cls[n] = c->hfa.base;
		}
		c->nreg = n;
	}
	else if (t->isdark || sz > 16 || sz == 0) {
		/* large structs are replaced by a
		 * pointer to some caller-allocated
		 * memory */
		c->class |= QBE_ARM64_ABI_Cptr;
		c->size = 8;
		c->ngp = 1;
		*c->reg = *gp;
		*c->cls = Kl;
	}
	else {
		for (n=0; n<sz/8; n++, c->ngp++) {
			c->reg[n] = *gp++;
			c->cls[n] = Kl;
		}
		c->nreg = n;
	}
}

static void
qbe_arm64_abi_sttmps(Ref tmp[], int cls[], uint nreg, Ref mem, Fn *fn)
{
	uint n;
	uint64_t off;
	Ref r;

	SQ_ASSERT(nreg <= 4);
	off = 0;
	for (n=0; n<nreg; n++) {
		tmp[n] = newtmp("abi", cls[n], fn);
		r = newtmp("abi", Kl, fn);
		emit(store[cls[n]], 0, NULL_R, tmp[n], r);
		emit(Oadd, Kl, r, mem, getcon(off, fn));
		off += KWIDE(cls[n]) ? 8 : 4;
	}
}

/* todo, may read out of bounds */
static void
qbe_arm64_abi_ldregs(int reg[], int cls[], int n, Ref mem, Fn *fn)
{
	int i;
	uint64_t off;
	Ref r;

	off = 0;
	for (i=0; i<n; i++) {
		r = newtmp("abi", Kl, fn);
		emit(Oload, cls[i], TMP(reg[i]), r, NULL_R);
		emit(Oadd, Kl, r, mem, getcon(off, fn));
		off += KWIDE(cls[i]) ? 8 : 4;
	}
}

static void
qbe_arm64_abi_selret(Blk *b, Fn *fn)
{
	int j, k, cty;
	Ref r;
	QBE_ARM64_ABI_Class cr;

	j = b->jmp.type;

	if (!isret(j) || j == Jret0)
		return;

	r = b->jmp.arg;
	b->jmp.type = Jret0;

	if (j == Jretc) {
		qbe_arm64_abi_typclass(&cr, &GC(typ)[fn->retty], qbe_arm64_abi_gpreg, qbe_arm64_abi_fpreg);
		ret_on_err();
		if (cr.class & QBE_ARM64_ABI_Cptr) {
			SQ_ASSERT(rtype(fn->retr) == RTmp);
			emit(Oblit1, 0, NULL_R, INT(cr.t->size), NULL_R);
			emit(Oblit0, 0, NULL_R, r, fn->retr);
			cty = 0;
		} else {
			qbe_arm64_abi_ldregs(cr.reg, cr.cls, cr.nreg, r, fn);
			cty = (cr.nfp << 2) | cr.ngp;
		}
	} else {
		k = j - Jretw;
		if (KBASE(k) == 0) {
			emit(Ocopy, k, TMP(QBE_ARM64_R0), r, NULL_R);
			cty = 1;
		} else {
			emit(Ocopy, k, TMP(QBE_ARM64_V0), r, NULL_R);
			cty = 1 << 2;
		}
	}

	b->jmp.arg = CALL(cty);
}

static int
qbe_arm64_abi_argsclass(Ins *i0, Ins *i1, QBE_ARM64_ABI_Class *carg)
{
	int va, envc, ngp, nfp, *gp, *fp;
	QBE_ARM64_ABI_Class *c;
	Ins *i;

	va = 0;
	envc = 0;
	gp = qbe_arm64_abi_gpreg;
	fp = qbe_arm64_abi_fpreg;
	ngp = 8;
	nfp = 8;
	for (i=i0, c=carg; i<i1; i++, c++)
		switch (i->op) {
		case Oargsb:
		case Oargub:
		case Oparsb:
		case Oparub:
			c->size = 1;
			goto Scalar;
		case Oargsh:
		case Oarguh:
		case Oparsh:
		case Oparuh:
			c->size = 2;
			goto Scalar;
		case Opar:
		case Oarg:
			c->size = 8;
			if (GC(T).apple && !KWIDE(i->cls))
				c->size = 4;
		Scalar:
			c->align = c->size;
			*c->cls = i->cls;
			if (va) {
				c->class |= QBE_ARM64_ABI_Cstk;
				c->size = 8;
				c->align = 8;
				break;
			}
			if (KBASE(i->cls) == 0 && ngp > 0) {
				ngp--;
				*c->reg = *gp++;
				break;
			}
			if (KBASE(i->cls) == 1 && nfp > 0) {
				nfp--;
				*c->reg = *fp++;
				break;
			}
			c->class |= QBE_ARM64_ABI_Cstk;
			break;
		case Oparc:
		case Oargc:
			qbe_arm64_abi_typclass(c, &GC(typ)[i->arg[0].val], gp, fp);
			ret_on_err_i();
			if (c->ngp <= ngp) {
				if (c->nfp <= nfp) {
					ngp -= c->ngp;
					nfp -= c->nfp;
					gp += c->ngp;
					fp += c->nfp;
					break;
				} else
					nfp = 0;
			} else
				ngp = 0;
			c->class |= QBE_ARM64_ABI_Cstk;
			break;
		case Opare:
		case Oarge:
			*c->reg = QBE_ARM64_R9;
			*c->cls = Kl;
			envc = 1;
			break;
		case Oargv:
			va = GC(T).apple != 0;
			break;
		default:
			die("unreachable");
		}

	return envc << 14 | (gp-qbe_arm64_abi_gpreg) << 5 | (fp-qbe_arm64_abi_fpreg) << 9;
}

bits
arm64_retregs(Ref r, int p[2])
{
	bits b;
	int ngp, nfp;

	SQ_ASSERT(rtype(r) == RCall);
	ngp = r.val & 3;
	nfp = (r.val >> 2) & 7;
	if (p) {
		p[0] = ngp;
		p[1] = nfp;
	}
	b = 0;
	while (ngp--)
		b |= BIT(QBE_ARM64_R0+ngp);
	while (nfp--)
		b |= BIT(QBE_ARM64_V0+nfp);
	return b;
}

bits
arm64_argregs(Ref r, int p[2])
{
	bits b;
	int ngp, nfp, x8, x9;

	SQ_ASSERT(rtype(r) == RCall);
	ngp = (r.val >> 5) & 15;
	nfp = (r.val >> 9) & 15;
	x8 = (r.val >> 13) & 1;
	x9 = (r.val >> 14) & 1;
	if (p) {
		p[0] = ngp + x8 + x9;
		p[1] = nfp;
	}
	b = 0;
	while (ngp--)
		b |= BIT(QBE_ARM64_R0+ngp);
	while (nfp--)
		b |= BIT(QBE_ARM64_V0+nfp);
	return b | ((bits)x8 << QBE_ARM64_R8) | ((bits)x9 << QBE_ARM64_R9);
}

static void
qbe_arm64_abi_stkblob(Ref r, QBE_ARM64_ABI_Class *c, Fn *fn, QBE_ARM64_ABI_Insl **ilp)
{
	QBE_ARM64_ABI_Insl *il;
	int al;
	uint64_t sz;

	il = alloc(sizeof *il);
	al = c->t->align - 2; /* NAlign == 3 */
	if (al < 0)
		al = 0;
	sz = c->class & QBE_ARM64_ABI_Cptr ? c->t->size : c->size;
	il->i = (Ins){Oalloc+al, Kl, r, {getcon(sz, fn)}};
	il->link = *ilp;
	*ilp = il;
}

static uint
qbe_arm64_abi_align(uint x, uint al)
{
	return (x + al-1) & -al;
}

static void
qbe_arm64_abi_selcall(Fn *fn, Ins *i0, Ins *i1, QBE_ARM64_ABI_Insl **ilp)
{
	Ins *i;
	QBE_ARM64_ABI_Class *ca, *c, cr;
	int op, cty;
	uint n, stk, off;;
	Ref r, rstk, tmp[4];

	ca = alloc((i1-i0) * sizeof ca[0]);
	cty = qbe_arm64_abi_argsclass(i0, i1, ca);

	stk = 0;
	for (i=i0, c=ca; i<i1; i++, c++) {
		if (c->class & QBE_ARM64_ABI_Cptr) {
			i->arg[0] = newtmp("abi", Kl, fn);
			qbe_arm64_abi_stkblob(i->arg[0], c, fn, ilp);
			i->op = Oarg;
		}
		if (c->class & QBE_ARM64_ABI_Cstk) {
			stk = qbe_arm64_abi_align(stk, c->align);
			stk += c->size;
		}
	}
	stk = qbe_arm64_abi_align(stk, 16);
	rstk = getcon(stk, fn);
	if (stk)
		emit(Oadd, Kl, TMP(QBE_ARM64_SP), TMP(QBE_ARM64_SP), rstk);

	if (!req(i1->arg[1], NULL_R)) {
		qbe_arm64_abi_typclass(&cr, &GC(typ)[i1->arg[1].val], qbe_arm64_abi_gpreg, qbe_arm64_abi_fpreg);
		ret_on_err();
		qbe_arm64_abi_stkblob(i1->to, &cr, fn, ilp);
		cty |= (cr.nfp << 2) | cr.ngp;
		if (cr.class & QBE_ARM64_ABI_Cptr) {
			/* spill & rega expect calls to be
			 * followed by copies from regs,
			 * so we emit a dummy
			 */
			cty |= 1 << 13 | 1;
			emit(Ocopy, Kw, NULL_R, TMP(QBE_ARM64_R0), NULL_R);
		} else {
			qbe_arm64_abi_sttmps(tmp, cr.cls, cr.nreg, i1->to, fn);
			for (n=0; n<cr.nreg; n++) {
				r = TMP(cr.reg[n]);
				emit(Ocopy, cr.cls[n], tmp[n], r, NULL_R);
			}
		}
	} else {
		if (KBASE(i1->cls) == 0) {
			emit(Ocopy, i1->cls, i1->to, TMP(QBE_ARM64_R0), NULL_R);
			cty |= 1;
		} else {
			emit(Ocopy, i1->cls, i1->to, TMP(QBE_ARM64_V0), NULL_R);
			cty |= 1 << 2;
		}
	}

	emit(Ocall, 0, NULL_R, i1->arg[0], CALL(cty));

	if (cty & (1 << 13))
		/* struct return argument */
		emit(Ocopy, Kl, TMP(QBE_ARM64_R8), i1->to, NULL_R);

	for (i=i0, c=ca; i<i1; i++, c++) {
		if ((c->class & QBE_ARM64_ABI_Cstk) != 0)
			continue;
		if (i->op == Oarg || i->op == Oarge || isargbh(i->op))
			emit(Ocopy, *c->cls, TMP(*c->reg), i->arg[0], NULL_R);
		if (i->op == Oargc)
			qbe_arm64_abi_ldregs(c->reg, c->cls, c->nreg, i->arg[1], fn);
	}

	/* populate the stack */
	off = 0;
	for (i=i0, c=ca; i<i1; i++, c++) {
		if ((c->class & QBE_ARM64_ABI_Cstk) == 0)
			continue;
		off = qbe_arm64_abi_align(off, c->align);
		r = newtmp("abi", Kl, fn);
		if (i->op == Oarg || isargbh(i->op)) {
			switch (c->size) {
			case 1: op = Ostoreb; break;
			case 2: op = Ostoreh; break;
			case 4:
			case 8: op = store[*c->cls]; break;
			default: die("unreachable");
			}
			emit(op, 0, NULL_R, i->arg[0], r);
		} else {
			SQ_ASSERT(i->op == Oargc);
			emit(Oblit1, 0, NULL_R, INT(c->size), NULL_R);
			emit(Oblit0, 0, NULL_R, i->arg[1], r);
		}
		emit(Oadd, Kl, r, TMP(QBE_ARM64_SP), getcon(off, fn));
		off += c->size;
	}
	if (stk)
		emit(Osub, Kl, TMP(QBE_ARM64_SP), TMP(QBE_ARM64_SP), rstk);

	for (i=i0, c=ca; i<i1; i++, c++)
		if (c->class & QBE_ARM64_ABI_Cptr) {
			emit(Oblit1, 0, NULL_R, INT(c->t->size), NULL_R);
			emit(Oblit0, 0, NULL_R, i->arg[1], i->arg[0]);
		}
}

static QBE_ARM64_ABI_Params
qbe_arm64_abi_selpar(Fn *fn, Ins *i0, Ins *i1)
{
	QBE_ARM64_ABI_Class *ca, *c, cr;
	QBE_ARM64_ABI_Insl *il;
	Ins *i;
	int op, n, cty;
	uint off;
	Ref r, tmp[16], *t;

	ca = alloc((i1-i0) * sizeof ca[0]);
	GC(curi) = &GC(insb)[NIns];

	cty = qbe_arm64_abi_argsclass(i0, i1, ca);
	fn->reg = arm64_argregs(CALL(cty), 0);

	il = 0;
	t = tmp;
	for (i=i0, c=ca; i<i1; i++, c++) {
		if (i->op != Oparc || (c->class & (QBE_ARM64_ABI_Cptr|QBE_ARM64_ABI_Cstk)))
			continue;
		qbe_arm64_abi_sttmps(t, c->cls, c->nreg, i->to, fn);
		qbe_arm64_abi_stkblob(i->to, c, fn, &il);
		t += c->nreg;
	}
	for (; il; il=il->link)
		emiti(il->i);

	if (fn->retty >= 0) {
		qbe_arm64_abi_typclass(&cr, &GC(typ)[fn->retty], qbe_arm64_abi_gpreg, qbe_arm64_abi_fpreg);
		if (GC(in_error)) { return (QBE_ARM64_ABI_Params){0}; }
		if (cr.class & QBE_ARM64_ABI_Cptr) {
			fn->retr = newtmp("abi", Kl, fn);
			emit(Ocopy, Kl, fn->retr, TMP(QBE_ARM64_R8), NULL_R);
			fn->reg |= BIT(QBE_ARM64_R8);
		}
	}

	t = tmp;
	off = 0;
	for (i=i0, c=ca; i<i1; i++, c++)
		if (i->op == Oparc && !(c->class & QBE_ARM64_ABI_Cptr)) {
			if (c->class & QBE_ARM64_ABI_Cstk) {
				off = qbe_arm64_abi_align(off, c->align);
				fn->tmp[i->to.val].slot = -(off+2);
				off += c->size;
			} else
				for (n=0; n<c->nreg; n++) {
					r = TMP(c->reg[n]);
					emit(Ocopy, c->cls[n], *t++, r, NULL_R);
				}
		} else if (c->class & QBE_ARM64_ABI_Cstk) {
			off = qbe_arm64_abi_align(off, c->align);
			if (isparbh(i->op))
				op = Oloadsb + (i->op - Oparsb);
			else
				op = Oload;
			emit(op, *c->cls, i->to, SLOT(-(off+2)), NULL_R);
			off += c->size;
		} else {
			emit(Ocopy, *c->cls, i->to, TMP(*c->reg), NULL_R);
		}

	return (QBE_ARM64_ABI_Params){
		.stk = qbe_arm64_abi_align(off, 8),
		.ngp = (cty >> 5) & 15,
		.nfp = (cty >> 9) & 15
	};
}

static Blk *
qbe_arm64_abi_split(Fn *fn, Blk *b)
{
	Blk *bn;

	++fn->nblk;
	bn = newblk();
	idup(bn, GC(curi), &GC(insb)[NIns]-GC(curi));
	GC(curi) = &GC(insb)[NIns];
	bn->visit = ++b->visit;
	strf(bn->name, "%s.%d", b->name, b->visit);
	bn->loop = b->loop;
	bn->link = b->link;
	b->link = bn;
	return bn;
}

static void
qbe_arm64_abi_chpred(Blk *b, Blk *bp, Blk *bp1)
{
	Phi *p;
	uint a;

	for (p=b->phi; p; p=p->link) {
		for (a=0; p->blk[a]!=bp; a++)
			SQ_ASSERT(a+1<p->narg);
		p->blk[a] = bp1;
	}
}

static void
qbe_arm64_abi_apple_selvaarg(Fn *fn, Blk *b, Ins *i)
{
	Ref ap, stk, stk8, c8;

	(void)b;
	c8 = getcon(8, fn);
	ap = i->arg[0];
	stk8 = newtmp("abi", Kl, fn);
	stk = newtmp("abi", Kl, fn);

	emit(Ostorel, 0, NULL_R, stk8, ap);
	emit(Oadd, Kl, stk8, stk, c8);
	emit(Oload, i->cls, i->to, stk, NULL_R);
	emit(Oload, Kl, stk, ap, NULL_R);
}

static void
qbe_arm64_abi_arm64_selvaarg(Fn *fn, Blk *b, Ins *i)
{
	Ref loc, lreg, lstk, nr, r0, r1, c8, c16, c24, c28, ap;
	Blk *b0, *bstk, *breg;
	int isgp;

	c8 = getcon(8, fn);
	c16 = getcon(16, fn);
	c24 = getcon(24, fn);
	c28 = getcon(28, fn);
	ap = i->arg[0];
	isgp = KBASE(i->cls) == 0;

	/* @b [...]
	       r0 =l add ap, (24 or 28)
	       nr =l loadsw r0
	       r1 =w csltw nr, 0
	       jnz r1, @breg, @bstk
	   @breg
	       r0 =l add ap, (8 or 16)
	       r1 =l loadl r0
	       lreg =l add r1, nr
	       r0 =w add nr, (8 or 16)
	       r1 =l add ap, (24 or 28)
	       storew r0, r1
	   @bstk
	       lstk =l loadl ap
	       r0 =l add lstk, 8
	       storel r0, ap
	   @b0
	       %loc =l phi @breg %lreg, @bstk %lstk
	       i->to =(i->cls) load %loc
	*/

	loc = newtmp("abi", Kl, fn);
	emit(Oload, i->cls, i->to, loc, NULL_R);
	b0 = qbe_arm64_abi_split(fn, b);
	b0->jmp = b->jmp;
	b0->s1 = b->s1;
	b0->s2 = b->s2;
	if (b->s1)
		qbe_arm64_abi_chpred(b->s1, b, b0);
	if (b->s2 && b->s2 != b->s1)
		qbe_arm64_abi_chpred(b->s2, b, b0);

	lreg = newtmp("abi", Kl, fn);
	nr = newtmp("abi", Kl, fn);
	r0 = newtmp("abi", Kw, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Ostorew, Kw, NULL_R, r0, r1);
	emit(Oadd, Kl, r1, ap, isgp ? c24 : c28);
	emit(Oadd, Kw, r0, nr, isgp ? c8 : c16);
	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Oadd, Kl, lreg, r1, nr);
	emit(Oload, Kl, r1, r0, NULL_R);
	emit(Oadd, Kl, r0, ap, isgp ? c8 : c16);
	breg = qbe_arm64_abi_split(fn, b);
	breg->jmp.type = Jjmp;
	breg->s1 = b0;

	lstk = newtmp("abi", Kl, fn);
	r0 = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, NULL_R, r0, ap);
	emit(Oadd, Kl, r0, lstk, c8);
	emit(Oload, Kl, lstk, ap, NULL_R);
	bstk = qbe_arm64_abi_split(fn, b);
	bstk->jmp.type = Jjmp;
	bstk->s1 = b0;

	b0->phi = alloc(sizeof *b0->phi);
	*b0->phi = (Phi){
		.cls = Kl, .to = loc,
		.narg = 2,
		.blk = vnew(2, sizeof b0->phi->blk[0], PFn),
		.arg = vnew(2, sizeof b0->phi->arg[0], PFn),
	};
	b0->phi->blk[0] = bstk;
	b0->phi->blk[1] = breg;
	b0->phi->arg[0] = lstk;
	b0->phi->arg[1] = lreg;
	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kw, fn);
	b->jmp.type = Jjnz;
	b->jmp.arg = r1;
	b->s1 = breg;
	b->s2 = bstk;
	emit(Ocmpw+Cislt, Kw, r1, nr, CON_Z);
	emit(Oloadsw, Kl, nr, r0, NULL_R);
	emit(Oadd, Kl, r0, ap, isgp ? c24 : c28);
}

static void
qbe_arm64_abi_apple_selvastart(Fn *fn, QBE_ARM64_ABI_Params p, Ref ap)
{
	Ref off, stk, arg;

	off = getcon(p.stk, fn);
	stk = newtmp("abi", Kl, fn);
	arg = newtmp("abi", Kl, fn);

	emit(Ostorel, 0, NULL_R, arg, ap);
	emit(Oadd, Kl, arg, stk, off);
	emit(Oaddr, Kl, stk, SLOT(-1), NULL_R);
}

static void
qbe_arm64_abi_arm64_selvastart(Fn *fn, QBE_ARM64_ABI_Params p, Ref ap)
{
	Ref r0, r1, rsave;

	rsave = newtmp("abi", Kl, fn);

	r0 = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, NULL_R, r0, ap);
	emit(Oadd, Kl, r0, rsave, getcon(p.stk + 192, fn));

	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, NULL_R, r1, r0);
	emit(Oadd, Kl, r1, rsave, getcon(64, fn));
	emit(Oadd, Kl, r0, ap, getcon(8, fn));

	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, NULL_R, r1, r0);
	emit(Oadd, Kl, r1, rsave, getcon(192, fn));
	emit(Oaddr, Kl, rsave, SLOT(-1), NULL_R);
	emit(Oadd, Kl, r0, ap, getcon(16, fn));

	r0 = newtmp("abi", Kl, fn);
	emit(Ostorew, Kw, NULL_R, getcon((p.ngp-8)*8, fn), r0);
	emit(Oadd, Kl, r0, ap, getcon(24, fn));

	r0 = newtmp("abi", Kl, fn);
	emit(Ostorew, Kw, NULL_R, getcon((p.nfp-8)*16, fn), r0);
	emit(Oadd, Kl, r0, ap, getcon(28, fn));
}

void
arm64_abi(Fn *fn)
{
	Blk *b;
	Ins *i, *i0;
	QBE_ARM64_ABI_Insl *il;
	int n0, n1, ioff;
	QBE_ARM64_ABI_Params p;

	for (b=fn->start; b; b=b->link)
		b->visit = 0;

	/* lower parameters */
	for (b=fn->start, i=b->ins; i<&b->ins[b->nins]; i++)
		if (!ispar(i->op))
			break;
	p = qbe_arm64_abi_selpar(fn, b->ins, i);
	n0 = &GC(insb)[NIns] - GC(curi);
	ioff = i - b->ins;
	n1 = b->nins - ioff;
	vgrow(&b->ins, n0+n1);
	icpy(b->ins+n0, b->ins+ioff, n1);
	icpy(b->ins, GC(curi), n0);
	b->nins = n0+n1;

	/* lower calls, returns, and vararg instructions */
	il = 0;
	b = fn->start;
	do {
		if (!(b = b->link))
			b = fn->start; /* do it last */
		if (b->visit)
			continue;
		GC(curi) = &GC(insb)[NIns];
		qbe_arm64_abi_selret(b, fn);
		ret_on_err();
		for (i=&b->ins[b->nins]; i!=b->ins;)
			switch ((--i)->op) {
			default:
				emiti(*i);
				break;
			case Ocall:
				for (i0=i; i0>b->ins; i0--)
					if (!isarg((i0-1)->op))
						break;
				qbe_arm64_abi_selcall(fn, i0, i, &il);
				i = i0;
				break;
			case Ovastart:
				if (GC(T).apple)
					qbe_arm64_abi_apple_selvastart(fn, p, i->arg[0]);
				else
					qbe_arm64_abi_arm64_selvastart(fn, p, i->arg[0]);
				break;
			case Ovaarg:
				if (GC(T).apple)
					qbe_arm64_abi_apple_selvaarg(fn, b, i);
				else
					qbe_arm64_abi_arm64_selvaarg(fn, b, i);
				break;
			case Oarg:
			case Oargc:
				die("unreachable");
			}
		if (b == fn->start)
			for (; il; il=il->link)
				emiti(il->i);
		idup(b, GC(curi), &GC(insb)[NIns]-GC(curi));
	} while (b != fn->start);

	if (GC(debug)['A']) {
		fprintf(stderr, "\n> After ABI lowering:\n");
		printfn(fn, stderr);
	}
}

/* abi0 for apple target; introduces
 * necessary sign extensions in calls
 * and returns
 */
void
apple_extsb(Fn *fn)
{
	Blk *b;
	Ins *i0, *i1, *i;
	int j, op;
	Ref r;

	for (b=fn->start; b; b=b->link) {
		GC(curi) = &GC(insb)[NIns];
		j = b->jmp.type;
		if (isretbh(j)) {
			r = newtmp("abi", Kw, fn);
			op = Oextsb + (j - Jretsb);
			emit(op, Kw, r, b->jmp.arg, NULL_R);
			b->jmp.arg = r;
			b->jmp.type = Jretw;
		}
		for (i=&b->ins[b->nins]; i>b->ins;) {
			emiti(*--i);
			if (i->op != Ocall)
				continue;
			for (i0=i1=i; i0>b->ins; i0--)
				if (!isarg((i0-1)->op))
					break;
			for (i=i1; i>i0;) {
				emiti(*--i);
				if (isargbh(i->op)) {
					i->to = newtmp("abi", Kl, fn);
					GC(curi)->arg[0] = i->to;
				}
			}
			for (i=i1; i>i0;)
				if (isargbh((--i)->op)) {
					op = Oextsb + (i->op - Oargsb);
					emit(op, Kw, i->to, i->arg[0], NULL_R);
				}
		}
		idup(b, GC(curi), &GC(insb)[NIns]-GC(curi));
	}

	if (GC(debug)['A']) {
		fprintf(stderr, "\n> After Apple pre-ABI:\n");
		printfn(fn, stderr);
	}
}
#undef G
/*** END FILE: arm64/abi.c ***/
/*** START FILE: arm64/emit.c ***/
/* skipping all.h */

#define G(x) global_context.arm64_emit__##x

typedef struct QBE_ARM64_EMIT_E QBE_ARM64_EMIT_E;

struct QBE_ARM64_EMIT_E {
	FILE *f;
	Fn *fn;
	uint64_t frame;
	uint padding;
};

#define CMP(X) \
	X(Cieq,       "eq") \
	X(Cine,       "ne") \
	X(Cisge,      "ge") \
	X(Cisgt,      "gt") \
	X(Cisle,      "le") \
	X(Cislt,      "lt") \
	X(Ciuge,      "cs") \
	X(Ciugt,      "hi") \
	X(Ciule,      "ls") \
	X(Ciult,      "cc") \
	X(NCmpI+Cfeq, "eq") \
	X(NCmpI+Cfge, "ge") \
	X(NCmpI+Cfgt, "gt") \
	X(NCmpI+Cfle, "ls") \
	X(NCmpI+Cflt, "mi") \
	X(NCmpI+Cfne, "ne") \
	X(NCmpI+Cfo,  "vc") \
	X(NCmpI+Cfuo, "vs")

enum {
	QBE_ARM64_EMIT_Ki = -1, /* matches Kw and Kl */
	QBE_ARM64_EMIT_Ka = -2, /* matches all classes */
};

static struct {
	short op;
	short cls;
	char *fmt;
} qbe_arm64_emit_omap[] = {
	{ Oadd,    QBE_ARM64_EMIT_Ki, "add %=, %0, %1" },
	{ Oadd,    QBE_ARM64_EMIT_Ka, "fadd %=, %0, %1" },
	{ Osub,    QBE_ARM64_EMIT_Ki, "sub %=, %0, %1" },
	{ Osub,    QBE_ARM64_EMIT_Ka, "fsub %=, %0, %1" },
	{ Oneg,    QBE_ARM64_EMIT_Ki, "neg %=, %0" },
	{ Oneg,    QBE_ARM64_EMIT_Ka, "fneg %=, %0" },
	{ Oand,    QBE_ARM64_EMIT_Ki, "and %=, %0, %1" },
	{ Oor,     QBE_ARM64_EMIT_Ki, "orr %=, %0, %1" },
	{ Oxor,    QBE_ARM64_EMIT_Ki, "eor %=, %0, %1" },
	{ Osar,    QBE_ARM64_EMIT_Ki, "asr %=, %0, %1" },
	{ Oshr,    QBE_ARM64_EMIT_Ki, "lsr %=, %0, %1" },
	{ Oshl,    QBE_ARM64_EMIT_Ki, "lsl %=, %0, %1" },
	{ Omul,    QBE_ARM64_EMIT_Ki, "mul %=, %0, %1" },
	{ Omul,    QBE_ARM64_EMIT_Ka, "fmul %=, %0, %1" },
	{ Odiv,    QBE_ARM64_EMIT_Ki, "sdiv %=, %0, %1" },
	{ Odiv,    QBE_ARM64_EMIT_Ka, "fdiv %=, %0, %1" },
	{ Oudiv,   QBE_ARM64_EMIT_Ki, "udiv %=, %0, %1" },
	{ Orem,    QBE_ARM64_EMIT_Ki, "sdiv %?, %0, %1\n\tmsub\t%=, %?, %1, %0" },
	{ Ourem,   QBE_ARM64_EMIT_Ki, "udiv %?, %0, %1\n\tmsub\t%=, %?, %1, %0" },
	{ Ocopy,   QBE_ARM64_EMIT_Ki, "mov %=, %0" },
	{ Ocopy,   QBE_ARM64_EMIT_Ka, "fmov %=, %0" },
	{ Oswap,   QBE_ARM64_EMIT_Ki, "mov %?, %0\n\tmov\t%0, %1\n\tmov\t%1, %?" },
	{ Oswap,   QBE_ARM64_EMIT_Ka, "fmov %?, %0\n\tfmov\t%0, %1\n\tfmov\t%1, %?" },
	{ Ostoreb, Kw, "strb %W0, %M1" },
	{ Ostoreh, Kw, "strh %W0, %M1" },
	{ Ostorew, Kw, "str %W0, %M1" },
	{ Ostorel, Kw, "str %L0, %M1" },
	{ Ostores, Kw, "str %S0, %M1" },
	{ Ostored, Kw, "str %D0, %M1" },
	{ Oloadsb, QBE_ARM64_EMIT_Ki, "ldrsb %=, %M0" },
	{ Oloadub, QBE_ARM64_EMIT_Ki, "ldrb %W=, %M0" },
	{ Oloadsh, QBE_ARM64_EMIT_Ki, "ldrsh %=, %M0" },
	{ Oloaduh, QBE_ARM64_EMIT_Ki, "ldrh %W=, %M0" },
	{ Oloadsw, Kw, "ldr %=, %M0" },
	{ Oloadsw, Kl, "ldrsw %=, %M0" },
	{ Oloaduw, QBE_ARM64_EMIT_Ki, "ldr %W=, %M0" },
	{ Oload,   QBE_ARM64_EMIT_Ka, "ldr %=, %M0" },
	{ Oextsb,  QBE_ARM64_EMIT_Ki, "sxtb %=, %W0" },
	{ Oextub,  QBE_ARM64_EMIT_Ki, "uxtb %W=, %W0" },
	{ Oextsh,  QBE_ARM64_EMIT_Ki, "sxth %=, %W0" },
	{ Oextuh,  QBE_ARM64_EMIT_Ki, "uxth %W=, %W0" },
	{ Oextsw,  QBE_ARM64_EMIT_Ki, "sxtw %L=, %W0" },
	{ Oextuw,  QBE_ARM64_EMIT_Ki, "mov %W=, %W0" },
	{ Oexts,   Kd, "fcvt %=, %S0" },
	{ Otruncd, Ks, "fcvt %=, %D0" },
	{ Ocast,   Kw, "fmov %=, %S0" },
	{ Ocast,   Kl, "fmov %=, %D0" },
	{ Ocast,   Ks, "fmov %=, %W0" },
	{ Ocast,   Kd, "fmov %=, %L0" },
	{ Ostosi,  QBE_ARM64_EMIT_Ka, "fcvtzs %=, %S0" },
	{ Ostoui,  QBE_ARM64_EMIT_Ka, "fcvtzu %=, %S0" },
	{ Odtosi,  QBE_ARM64_EMIT_Ka, "fcvtzs %=, %D0" },
	{ Odtoui,  QBE_ARM64_EMIT_Ka, "fcvtzu %=, %D0" },
	{ Oswtof,  QBE_ARM64_EMIT_Ka, "scvtf %=, %W0" },
	{ Ouwtof,  QBE_ARM64_EMIT_Ka, "ucvtf %=, %W0" },
	{ Osltof,  QBE_ARM64_EMIT_Ka, "scvtf %=, %L0" },
	{ Oultof,  QBE_ARM64_EMIT_Ka, "ucvtf %=, %L0" },
	{ Ocall,   Kw, "blr %L0" },

	{ Oacmp,   QBE_ARM64_EMIT_Ki, "cmp %0, %1" },
	{ Oacmn,   QBE_ARM64_EMIT_Ki, "cmn %0, %1" },
	{ Oafcmp,  QBE_ARM64_EMIT_Ka, "fcmpe %0, %1" },

#define X(c, str) \
	{ Oflag+c, QBE_ARM64_EMIT_Ki, "cset %=, " str },
	CMP(X)
#undef X
	{ NOp, 0, 0 }
};

enum {
	V31 = 0x1fffffff,  /* local name for V31 */
};

static char* w_regs[] = {
    "w0",  "w1",  "w2",  "w3",  "w4",  "w5",  "w6",  "w7",  "w8",  "w9",  "w10",
    "w11", "w12", "w13", "w14", "w15", "w16", "w17", "w18", "w19", "w20", "w21",
    "w22", "w23", "w24", "w25", "w26", "w27", "w28", "w29", "w30",
};

static char* x_regs[] = {
    "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",  "x8",  "x9",  "x10",
    "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x19", "x20", "x21",
    "x22", "x23", "x24", "x25", "x26", "x27", "x28", "x29", "x30",
};

static char* s_regs[] = {
    "s0",  "s1",  "s2",  "s3",  "s4",  "s5",  "s6",  "s7",  "s8",  "s9",  "s10",
    "s11", "s12", "s13", "s14", "s15", "s16", "s17", "s18", "s19", "s20", "s21",
    "s22", "s23", "s24", "s25", "s26", "s27", "s28", "s29", "s30",
};

static char* d_regs[] = {
    "d0",  "d1",  "d2",  "d3",  "d4",  "d5",  "d6",  "d7",  "d8",  "d9",  "d10",
    "d11", "d12", "d13", "d14", "d15", "d16", "d17", "d18", "d19", "d20", "d21",
    "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30",
};

static char *
qbe_arm64_emit_rname(int r, int k)
{
	if (r == QBE_ARM64_SP) {
		SQ_ASSERT(k == Kl);
    return "sp";
	}
	else if (QBE_ARM64_R0 <= r && r <= QBE_ARM64_LR)
		switch (k) {
		default: die("invalid class");
		case Kw: return w_regs[r-QBE_ARM64_R0]; break;
		case Kx:
		case Kl: return x_regs[r-QBE_ARM64_R0]; break;
		}
	else if (QBE_ARM64_V0 <= r && r <= QBE_ARM64_V30)
		switch (k) {
		default: die("invalid class");
		case Ks: return s_regs[r-QBE_ARM64_V0]; break;
		case Kx:
		case Kd: return d_regs[r-QBE_ARM64_V0]; break;
		}
	else if (r == V31)
		switch (k) {
		default: die("invalid class");
		case Ks: return "s31";
		case Kd: return "d31";
		}
	else
		die("invalid register");
}

static uint64_t
qbe_arm64_emit_slot(Ref r, QBE_ARM64_EMIT_E *e)
{
	int s;

	s = rsval(r);
	if (s == -1)
		return 16 + e->frame;
	if (s < 0) {
		if (e->fn->vararg && !GC(T).apple)
			return 16 + e->frame + 192 - (s+2);
		else
			return 16 + e->frame - (s+2);
	} else
		return 16 + e->padding + 4 * s;
}

static void
qbe_arm64_emit_emitf(char *s, Ins *i, QBE_ARM64_EMIT_E *e)
{
	Ref r;
	int k, c;
	Con *pc;
	uint64_t n;
	uint sp;

	fputc('\t', e->f);

	sp = 0;
	for (;;) {
		k = i->cls;
		while ((c = *s++) != '%')
			if (c == ' ' && !sp) {
				fputc('\t', e->f);
				sp = 1;
			} else if (!c) {
				fputc('\n', e->f);
				return;
			} else
				fputc(c, e->f);
	Switch:
		switch ((c = *s++)) {
		default:
			die("invalid escape");
		case 'W':
			k = Kw;
			goto Switch;
		case 'L':
			k = Kl;
			goto Switch;
		case 'S':
			k = Ks;
			goto Switch;
		case 'D':
			k = Kd;
			goto Switch;
		case '?':
			if (KBASE(k) == 0)
				fputs(qbe_arm64_emit_rname(QBE_ARM64_IP1, k), e->f);
			else
				fputs(qbe_arm64_emit_rname(V31, k), e->f);
			break;
		case '=':
		case '0':
			r = c == '=' ? i->to : i->arg[0];
			SQ_ASSERT(isreg(r) || req(r, TMP(V31)));
			fputs(qbe_arm64_emit_rname(r.val, k), e->f);
			break;
		case '1':
			r = i->arg[1];
			switch (rtype(r)) {
			default:
				die("invalid second argument");
			case RTmp:
				SQ_ASSERT(isreg(r));
				fputs(qbe_arm64_emit_rname(r.val, k), e->f);
				break;
			case RCon:
				pc = &e->fn->con[r.val];
				n = pc->bits.i;
				SQ_ASSERT(pc->type == CBits);
				if (n >> 24) {
					SQ_ASSERT(arm64_logimm(n, k));
					fprintf(e->f, "#%"PRIu64, n);
				} else if (n & 0xfff000) {
					SQ_ASSERT(!(n & ~0xfff000ull));
					fprintf(e->f, "#%"PRIu64", lsl #12",
						n>>12);
				} else {
					SQ_ASSERT(!(n & ~0xfffull));
					fprintf(e->f, "#%"PRIu64, n);
				}
				break;
			}
			break;
		case 'M':
			c = *s++;
			SQ_ASSERT(c == '0' || c == '1' || c == '=');
			r = c == '=' ? i->to : i->arg[c - '0'];
			switch (rtype(r)) {
			default:
				die("todo (arm emit): unhandled ref");
			case RTmp:
				SQ_ASSERT(isreg(r));
				fprintf(e->f, "[%s]", qbe_arm64_emit_rname(r.val, Kl));
				break;
			case RSlot:
				fprintf(e->f, "[x29, %"PRIu64"]", qbe_arm64_emit_slot(r, e));
				break;
			}
			break;
		}
	}
}

static void
qbe_arm64_emit_loadaddr(Con *c, char *rn, QBE_ARM64_EMIT_E *e)
{
	char *p, *l, *s;

	switch (c->sym.type) {
	default:
		die("unreachable");
	case SGlo:
		if (GC(T).apple)
			s = "\tadrp\tR, S@pageO\n"
			    "\tadd\tR, R, S@pageoffO\n";
		else
			s = "\tadrp\tR, SO\n"
			    "\tadd\tR, R, #:lo12:SO\n";
		break;
	case SThr:
		if (GC(T).apple)
			s = "\tadrp\tR, S@tlvppage\n"
			    "\tldr\tR, [R, S@tlvppageoff]\n";
		else
			s = "\tmrs\tR, tpidr_el0\n"
			    "\tadd\tR, R, #:tprel_hi12:SO, lsl #12\n"
			    "\tadd\tR, R, #:tprel_lo12_nc:SO\n";
		break;
	}

	l = str(c->sym.id);
	p = l[0] == '"' ? "" : GC(T).assym;
	for (; *s; s++)
		switch (*s) {
		default:
			fputc(*s, e->f);
			break;
		case 'R':
			fputs(rn, e->f);
			break;
		case 'S':
			fputs(p, e->f);
			fputs(l, e->f);
			break;
		case 'O':
			if (c->bits.i)
				/* todo, handle large offsets */
				fprintf(e->f, "+%"PRIi64, c->bits.i);
			break;
		}
}

static void
qbe_arm64_emit_loadcon(Con *c, int r, int k, QBE_ARM64_EMIT_E *e)
{
	char *rn;
	int64_t n;
	int w, sh;

	w = KWIDE(k);
	rn = qbe_arm64_emit_rname(r, k);
	n = c->bits.i;
	if (c->type == CAddr) {
		rn = qbe_arm64_emit_rname(r, Kl);
		qbe_arm64_emit_loadaddr(c, rn, e);
		return;
	}
	SQ_ASSERT(c->type == CBits);
	if (!w)
		n = (int32_t)n;
	if ((n | 0xffff) == -1 || arm64_logimm(n, k)) {
		fprintf(e->f, "\tmov\t%s, #%"PRIi64"\n", rn, n);
	} else {
		fprintf(e->f, "\tmov\t%s, #%d\n",
			rn, (int)(n & 0xffff));
		for (sh=16; n>>=16; sh+=16) {
			if ((!w && sh == 32) || sh == 64)
				break;
			fprintf(e->f, "\tmovk\t%s, #0x%x, lsl #%d\n",
				rn, (uint)(n & 0xffff), sh);
		}
	}
}

static void qbe_arm64_emit_emitins(Ins *, QBE_ARM64_EMIT_E *);

static int
qbe_arm64_emit_fixarg(Ref *pr, int sz, int t, QBE_ARM64_EMIT_E *e)
{
	Ins *i;
	Ref r;
	uint64_t s;

	r = *pr;
	if (rtype(r) == RSlot) {
		s = qbe_arm64_emit_slot(r, e);
		if (s > sz * 4095u) {
			if (t < 0)
				return 1;
			i = &(Ins){Oaddr, Kl, TMP(t), {r}};
			qbe_arm64_emit_emitins(i, e);
			*pr = TMP(t);
		}
	}
	return 0;
}

static void
qbe_arm64_emit_emitins(Ins *i, QBE_ARM64_EMIT_E *e)
{
	char *l, *p, *rn;
	uint64_t s;
	int o, t;
	Ref r;
	Con *c;

	switch (i->op) {
	default:
		if (isload(i->op))
			qbe_arm64_emit_fixarg(&i->arg[0], loadsz(i), QBE_ARM64_IP1, e);
		if (isstore(i->op)) {
			t = GC(T).apple ? -1 : QBE_ARM64_R18;
			if (qbe_arm64_emit_fixarg(&i->arg[1], storesz(i), t, e)) {
				if (req(i->arg[0], TMP(QBE_ARM64_IP1))) {
					fprintf(e->f,
						"\tfmov\t%c31, %c17\n",
						"ds"[i->cls == Kw],
						"xw"[i->cls == Kw]);
					i->arg[0] = TMP(V31);
					i->op = Ostores + (i->cls-Kw);
				}
				qbe_arm64_emit_fixarg(&i->arg[1], storesz(i), QBE_ARM64_IP1, e);
			}
		}
	Table:
		/* most instructions are just pulled out of
		 * the table qbe_arm64_emit_omap[], some special cases are
		 * detailed below */
		for (o=0;; o++) {
			/* this linear search should really be a binary
			 * search */
			if (qbe_arm64_emit_omap[o].op == NOp)
				die("no match for %s(%c)",
					optab[i->op].name, "wlsd"[i->cls]);
			if (qbe_arm64_emit_omap[o].op == i->op)
			if (qbe_arm64_emit_omap[o].cls == i->cls || qbe_arm64_emit_omap[o].cls == QBE_ARM64_EMIT_Ka
			|| (qbe_arm64_emit_omap[o].cls == QBE_ARM64_EMIT_Ki && KBASE(i->cls) == 0))
				break;
		}
		qbe_arm64_emit_emitf(qbe_arm64_emit_omap[o].fmt, i, e);
		break;
	case Onop:
		break;
	case Ocopy:
		if (req(i->to, i->arg[0]))
			break;
		if (rtype(i->to) == RSlot) {
			r = i->to;
			if (!isreg(i->arg[0])) {
				i->to = TMP(QBE_ARM64_IP1);
				qbe_arm64_emit_emitins(i, e);
				i->arg[0] = i->to;
			}
			i->op = Ostorew + i->cls;
			i->cls = Kw;
			i->arg[1] = r;
			qbe_arm64_emit_emitins(i, e);
			break;
		}
		SQ_ASSERT(isreg(i->to));
		switch (rtype(i->arg[0])) {
		case RCon:
			c = &e->fn->con[i->arg[0].val];
			qbe_arm64_emit_loadcon(c, i->to.val, i->cls, e);
			break;
		case RSlot:
			i->op = Oload;
			qbe_arm64_emit_emitins(i, e);
			break;
		default:
			SQ_ASSERT(i->to.val != QBE_ARM64_IP1);
			goto Table;
		}
		break;
	case Oaddr:
		SQ_ASSERT(rtype(i->arg[0]) == RSlot);
		rn = qbe_arm64_emit_rname(i->to.val, Kl);
		s = qbe_arm64_emit_slot(i->arg[0], e);
		if (s <= 4095)
			fprintf(e->f, "\tadd\t%s, x29, #%"PRIu64"\n", rn, s);
		else if (s <= 65535)
			fprintf(e->f,
				"\tmov\t%s, #%"PRIu64"\n"
				"\tadd\t%s, x29, %s\n",
				rn, s, rn, rn
			);
		else
			fprintf(e->f,
				"\tmov\t%s, #%"PRIu64"\n"
				"\tmovk\t%s, #%"PRIu64", lsl #16\n"
				"\tadd\t%s, x29, %s\n",
				rn, s & 0xFFFF, rn, s >> 16, rn, rn
			);
		break;
	case Ocall:
		if (rtype(i->arg[0]) != RCon)
			goto Table;
		c = &e->fn->con[i->arg[0].val];
		if (c->type != CAddr
		|| c->sym.type != SGlo
		|| c->bits.i)
			die("invalid call argument");
		l = str(c->sym.id);
		p = l[0] == '"' ? "" : GC(T).assym;
		fprintf(e->f, "\tbl\t%s%s\n", p, l);
		break;
	case Osalloc:
		qbe_arm64_emit_emitf("sub sp, sp, %0", i, e);
		if (!req(i->to, NULL_R))
			qbe_arm64_emit_emitf("mov %=, sp", i, e);
		break;
	case Odbgloc:
		emitdbgloc(i->arg[0].val, i->arg[1].val, e->f);
		break;
	}
}

static void
qbe_arm64_emit_framelayout(QBE_ARM64_EMIT_E *e)
{
	int *r;
	uint o;
	uint64_t f;

	for (o=0, r=arm64_rclob; *r>=0; r++)
		o += 1 & (e->fn->reg >> *r);
	f = e->fn->slot;
	f = (f + 3) & -4;
	o += o & 1;
	e->padding = 4*(f-e->fn->slot);
	e->frame = 4*f + 8*o;
}

/*

  Stack-frame layout:

  +=============+
  | varargs     |
  |  save area  |
  +-------------+
  | callee-save |  ^
  |  registers  |  |
  +-------------+  |
  |    ...      |  |
  | spill slots |  |
  |    ...      |  | e->frame
  +-------------+  |
  |    ...      |  |
  |   locals    |  |
  |    ...      |  |
  +-------------+  |
  | e->padding  |  v
  +-------------+
  |  saved x29  |
  |  saved x30  |
  +=============+ <- x29

*/

void
arm64_emitfn(Fn *fn, FILE *out)
{
	static char *ctoa[] = {
	#define X(c, s) [c] = s,
		CMP(X)
	#undef X
	};
	int s, n, c, lbl, *r;
	uint64_t o;
	Blk *b, *t;
	Ins *i;
	QBE_ARM64_EMIT_E *e;

	e = &(QBE_ARM64_EMIT_E){.f = out, .fn = fn};
	if (GC(T).apple)
		e->fn->lnk.align = 4;
	emitfnlnk(e->fn->name, &e->fn->lnk, e->f);
	fputs("\thint\t#34\n", e->f);
	qbe_arm64_emit_framelayout(e);

	if (e->fn->vararg && !GC(T).apple) {
		for (n=7; n>=0; n--)
			fprintf(e->f, "\tstr\tq%d, [sp, -16]!\n", n);
		for (n=7; n>=0; n-=2)
			fprintf(e->f, "\tstp\tx%d, x%d, [sp, -16]!\n", n-1, n);
	}

	if (e->frame + 16 <= 512)
		fprintf(e->f,
			"\tstp\tx29, x30, [sp, -%"PRIu64"]!\n",
			e->frame + 16
		);
	else if (e->frame <= 4095)
		fprintf(e->f,
			"\tsub\tsp, sp, #%"PRIu64"\n"
			"\tstp\tx29, x30, [sp, -16]!\n",
			e->frame
		);
	else if (e->frame <= 65535)
		fprintf(e->f,
			"\tmov\tx16, #%"PRIu64"\n"
			"\tsub\tsp, sp, x16\n"
			"\tstp\tx29, x30, [sp, -16]!\n",
			e->frame
		);
	else
		fprintf(e->f,
			"\tmov\tx16, #%"PRIu64"\n"
			"\tmovk\tx16, #%"PRIu64", lsl #16\n"
			"\tsub\tsp, sp, x16\n"
			"\tstp\tx29, x30, [sp, -16]!\n",
			e->frame & 0xFFFF, e->frame >> 16
		);
	fputs("\tmov\tx29, sp\n", e->f);
	s = (e->frame - e->padding) / 4;
	for (r=arm64_rclob; *r>=0; r++)
		if (e->fn->reg & BIT(*r)) {
			s -= 2;
			i = &(Ins){.arg = {TMP(*r), SLOT(s)}};
			i->op = *r >= QBE_ARM64_V0 ? Ostored : Ostorel;
			qbe_arm64_emit_emitins(i, e);
		}

	for (lbl=0, b=e->fn->start; b; b=b->link) {
		if (lbl || b->npred > 1)
			fprintf(e->f, "%s%d:\n", GC(T).asloc, G(arm64_emitfn_id0)+b->id);
		for (i=b->ins; i!=&b->ins[b->nins]; i++)
			qbe_arm64_emit_emitins(i, e);
		lbl = 1;
		switch (b->jmp.type) {
		case Jhlt:
			fprintf(e->f, "\tbrk\t#1000\n");
			break;
		case Jret0:
			s = (e->frame - e->padding) / 4;
			for (r=arm64_rclob; *r>=0; r++)
				if (e->fn->reg & BIT(*r)) {
					s -= 2;
					i = &(Ins){Oload, 0, TMP(*r), {SLOT(s)}};
					i->cls = *r >= QBE_ARM64_V0 ? Kd : Kl;
					qbe_arm64_emit_emitins(i, e);
				}
			if (e->fn->dynalloc)
				fputs("\tmov sp, x29\n", e->f);
			o = e->frame + 16;
			if (e->fn->vararg && !GC(T).apple)
				o += 192;
			if (o <= 504)
				fprintf(e->f,
					"\tldp\tx29, x30, [sp], %"PRIu64"\n",
					o
				);
			else if (o - 16 <= 4095)
				fprintf(e->f,
					"\tldp\tx29, x30, [sp], 16\n"
					"\tadd\tsp, sp, #%"PRIu64"\n",
					o - 16
				);
			else if (o - 16 <= 65535)
				fprintf(e->f,
					"\tldp\tx29, x30, [sp], 16\n"
					"\tmov\tx16, #%"PRIu64"\n"
					"\tadd\tsp, sp, x16\n",
					o - 16
				);
			else
				fprintf(e->f,
					"\tldp\tx29, x30, [sp], 16\n"
					"\tmov\tx16, #%"PRIu64"\n"
					"\tmovk\tx16, #%"PRIu64", lsl #16\n"
					"\tadd\tsp, sp, x16\n",
					(o - 16) & 0xFFFF, (o - 16) >> 16
				);
			fprintf(e->f, "\tret\n");
			break;
		case Jjmp:
		Label_Jmp:
			if (b->s1 != b->link)
				fprintf(e->f,
					"\tb\t%s%d\n",
					GC(T).asloc, G(arm64_emitfn_id0)+b->s1->id
				);
			else
				lbl = 0;
			break;
		default:
			c = b->jmp.type - Jjf;
			if (c < 0 || c > NCmp)
				die("unhandled jump %d", b->jmp.type);
			if (b->link == b->s2) {
				t = b->s1;
				b->s1 = b->s2;
				b->s2 = t;
			} else {
				c = cmpneg(c);
				/* cmpneg of ordered float comparisons gives
				 * conditions that don't fire for unordered (NaN)
				 * after fcmpe; remap to include the unordered case */
				switch (c) {
				case NCmpI+Cfge: c = Ciuge; break; /* bge -> bcs */
				case NCmpI+Cfgt: c = Ciugt; break; /* bgt -> bhi */
				case NCmpI+Cfle: c = Cisle; break; /* bls -> ble */
				case NCmpI+Cflt: c = Cislt; break; /* bmi -> blt */
				}
			}
			fprintf(e->f,
				"\tb%s\t%s%d\n",
				ctoa[c], GC(T).asloc, G(arm64_emitfn_id0)+b->s2->id
			);
			goto Label_Jmp;
		}
	}
	G(arm64_emitfn_id0) += e->fn->nblk;
	if (!GC(T).apple)
		elf_emitfnfin(fn->name, out);
}
#undef CMP
#undef G
/*** END FILE: arm64/emit.c ***/
/*** START FILE: arm64/emitmacho.c ***/
/* skipping all.h */

/* skipping emitmacho.h */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Mach-O constants
#define MH_MAGIC_64 0xFEEDFACFu
#define CPU_TYPE_ARM64 0x0100000Cu
#define MH_OBJECT 1u
#define MH_SUBSECTIONS_VIA_SYMBOLS 0x2000u
#define LC_SEGMENT_64 0x19u
#define LC_SYMTAB 0x2u
#define LC_DYSYMTAB 0xBu
#define LC_BUILD_VERSION 0x32u
#define PLATFORM_MACOS 1u
#define N_UNDF 0x0u
#define N_SECT 0xEu
#define N_EXT 0x1u
#define ARM64_RELOC_UNSIGNED 0u
#define ARM64_RELOC_BRANCH26 2u
#define ARM64_RELOC_PAGE21 3u
#define ARM64_RELOC_PAGEOFF12 4u
#define ARM64_RELOC_TLVP_LOAD_PAGE21 8u
#define ARM64_RELOC_TLVP_LOAD_PAGEOFF12 9u
#define ARM64_RELOC_ADDEND 10u
#define S_REGULAR 0u
#define S_ZEROFILL 1u
#define S_ATTR_PURE_INSTRUCTIONS 0x80000000u
#define S_ATTR_SOME_INSTRUCTIONS 0x00000400u

// Sentinel sect values: resolved to real section numbers in macho_write
#define SECT_DATA 0xFEu
#define SECT_BSS 0xFDu
#define SECT_LIT4 0xFCu
#define SECT_LIT8 0xFBu
// S_4BYTE_LITERALS / S_8BYTE_LITERALS section type flags
#define S_4BYTE_LITERALS 0x3u
#define S_8BYTE_LITERALS 0x4u

// Mach-O structs (no system headers, exact field layout)

struct mach_header_64 {
  uint32_t magic, cputype, cpusubtype, filetype;
  uint32_t ncmds, sizeofcmds, flags, reserved;
};

struct segment_command_64 {
  uint32_t cmd, cmdsize;
  char segname[16];
  uint64_t vmaddr, vmsize, fileoff, filesize;
  uint32_t maxprot, initprot, nsects, flags;
};

struct section_64 {
  char sectname[16];
  char segname[16];
  uint64_t addr, size;
  uint32_t offset, align, reloff, nreloc, flags;
  uint32_t reserved1, reserved2, reserved3;
};

struct symtab_command {
  uint32_t cmd, cmdsize, symoff, nsyms, stroff, strsize;
};

struct dysymtab_command {
  uint32_t cmd, cmdsize;
  uint32_t ilocalsym, nlocalsym;
  uint32_t iextdefsym, nextdefsym;
  uint32_t iundefsym, nundefsym;
  uint32_t tocoff, ntoc;
  uint32_t modtaboff, nmodtab;
  uint32_t extrefsymoff, nextrefsyms;
  uint32_t indirectsymoff, nindirectsyms;
  uint32_t extreloff, nextrel;
  uint32_t locreloff, nlocrel;
};

struct nlist_64 {
  uint32_t n_strx;
  uint8_t n_type, n_sect;
  uint16_t n_desc;
  uint64_t n_value;
};

struct relocation_info {
  int32_t r_address;
  uint32_t r_info;
};

struct build_version_command {
  uint32_t cmd, cmdsize, platform, minos, sdk, ntools;
};

MAKESURE(mach_header_64_size, sizeof(struct mach_header_64) == 32);
MAKESURE(segment_command_64_size, sizeof(struct segment_command_64) == 72);
MAKESURE(section_64_size, sizeof(struct section_64) == 80);
MAKESURE(symtab_command_size, sizeof(struct symtab_command) == 24);
MAKESURE(dysymtab_command_size, sizeof(struct dysymtab_command) == 80);
MAKESURE(nlist_64_size, sizeof(struct nlist_64) == 16);
MAKESURE(relocation_info_size, sizeof(struct relocation_info) == 8);
MAKESURE(build_version_command, sizeof(struct build_version_command) == 24);

typedef struct {
  int32_t addr;
  uint32_t info;
} MReloc;

typedef struct {
  uint32_t strx;
  uint8_t type, sect;
  uint16_t desc;
  uint64_t value;
} MSym;

struct MachoCtx {
  uint8_t* text;
  uint32_t textsz, textcap;
  uint8_t* lit4;  // __TEXT,__literal4 (4-byte QBE_ARM64_FP constants)
  uint32_t lit4sz, lit4cap;
  uint8_t* lit8;  // __TEXT,__literal8 (8-byte QBE_ARM64_FP constants)
  uint32_t lit8sz, lit8cap;
  uint8_t* data;
  uint32_t datasz, datacap;
  uint32_t bsssz;
  MReloc* dreloc;
  uint32_t ndreloc, drelocap;
  MReloc* treloc;
  uint32_t ntreloc, trelocap;
  MSym* syms;
  uint32_t nsyms, symcap;
  char* strtab;
  uint32_t strtabsz, strtabcap;
  // forward BL fixups: resolved in macho_write
  struct {
    uint32_t off;
    uint32_t symidx;
  }* blfixup;
  uint32_t nblfixup, blfixupap;
  // current data item state
  char datname[NString];
  Lnk* datlnk;
  int64_t dleadzero;  // >=0: accumulating leading zeros; -1: data started
  uint32_t datstart;  // ctx->datasz at DStart
};

static void* qbe_realloc(void* oldp, uint32_t old_size, size_t new_size) {
  void* newp = emalloc(new_size);
  if (oldp) {
    memcpy(newp, oldp, old_size);
    qbe_free(oldp);
  }
  return newp;
}

static void buf_append(uint8_t** buf, uint32_t* sz, uint32_t* cap, const void* src, uint32_t n) {
  if (*sz + n > *cap) {
    uint32_t newcap = *cap ? *cap * 2 : 64;
    uint32_t oldcap = *cap;
    while (newcap < *sz + n) {
      newcap *= 2;
    }
    *buf = qbe_realloc(*buf, oldcap, newcap);
    if (!*buf) {
      die("obj: out of memory");
    }
    *cap = newcap;
  }
  memcpy(*buf + *sz, src, n);
  *sz += n;
}

static uint32_t strintern(MachoCtx* ctx, const char* s) {
  uint32_t off = ctx->strtabsz;
  uint32_t n = (uint32_t)strlen(s) + 1;
  buf_append((uint8_t**)&ctx->strtab, &ctx->strtabsz, &ctx->strtabcap, s, n);
  return off;
}

static void symname(char out[NString], const char* n) {
  if (n[0] == '"') {
    size_t len = strlen(n);
    int inner = len >= 2 ? (int)(len - 2) : 0;
    snprintf(out, NString, "%.*s", inner, n + 1);
  } else {
    snprintf(out, NString, "_%s", n);
  }
}

static uint32_t symadd(MachoCtx* ctx,
                       const char* name,
                       uint8_t type,
                       uint8_t sect,
                       uint64_t value) {
  MSym sym;
  sym.strx = strintern(ctx, name);
  sym.type = type;
  sym.sect = sect;
  sym.desc = 0;
  sym.value = value;
  if (ctx->nsyms == ctx->symcap) {
    uint32_t oldcap = ctx->symcap;
    ctx->symcap = ctx->symcap ? ctx->symcap * 2 : 8;
    ctx->syms = qbe_realloc(ctx->syms, oldcap * sizeof(MSym), ctx->symcap * sizeof(MSym));
    if (!ctx->syms) {
      die("obj: out of memory");
    }
  }
  ctx->syms[ctx->nsyms] = sym;
  return ctx->nsyms++;
}

static void relocadd(MachoCtx* ctx, int32_t addr, uint32_t symidx, int type, int ext, int length) {
  MReloc r;
  r.addr = addr;
  r.info = (symidx & 0xFFFFFFu) | ((uint32_t)length << 25) | ((uint32_t)ext << 27) |
           ((uint32_t)type << 28);
  if (ctx->ndreloc == ctx->drelocap) {
    uint32_t oldcap = ctx->drelocap;
    ctx->drelocap = ctx->drelocap ? ctx->drelocap * 2 : 8;
    ctx->dreloc = qbe_realloc(ctx->dreloc, oldcap * sizeof(MReloc), ctx->drelocap * sizeof(MReloc));
    if (!ctx->dreloc) {
      die("obj: out of memory");
    }
  }
  ctx->dreloc[ctx->ndreloc++] = r;
}

static void trelocadd(MachoCtx* ctx,
                      int32_t addr,
                      uint32_t symidx,
                      int type,
                      int length,
                      int pcrel) {
  MReloc r;
  r.addr = addr;
  r.info = (symidx & 0xFFFFFFu) | ((uint32_t)pcrel << 24)  // r_pcrel
           | ((uint32_t)length << 25) | (1u << 27)         // r_extern = 1
           | ((uint32_t)type << 28);
  if (ctx->ntreloc == ctx->trelocap) {
    uint32_t oldcap = ctx->trelocap;
    ctx->trelocap = ctx->trelocap ? ctx->trelocap * 2 : 8;
    ctx->treloc = qbe_realloc(ctx->treloc, oldcap * sizeof(MReloc), ctx->trelocap * sizeof(MReloc));
    if (!ctx->treloc) {
      die("obj: out of memory");
    }
  }
  ctx->treloc[ctx->ntreloc++] = r;
}

// ARM64_RELOC_ADDEND for text section: r_extern=0, r_symbolnum=addend value.
static void trelocadd_addend(MachoCtx* ctx, int32_t addr, uint32_t addend) {
  MReloc r;
  r.addr = addr;
  r.info = (addend & 0xFFFFFFu) | (0u << 24) | (2u << 25) | (0u << 27)
           | ((uint32_t)ARM64_RELOC_ADDEND << 28);
  if (ctx->ntreloc == ctx->trelocap) {
    uint32_t oldcap = ctx->trelocap;
    ctx->trelocap = ctx->trelocap ? ctx->trelocap * 2 : 8;
    ctx->treloc = qbe_realloc(ctx->treloc, oldcap * sizeof(MReloc), ctx->trelocap * sizeof(MReloc));
    if (!ctx->treloc) {
      die("obj: out of memory");
    }
  }
  ctx->treloc[ctx->ntreloc++] = r;
}

static uint32_t callee_symidx(MachoCtx* ctx, const char* name) {
  uint32_t i;
  for (i = 0; i < ctx->nsyms; i++) {
    if (strcmp(ctx->strtab + ctx->syms[i].strx, name) == 0) {
      return i;
    }
  }
  // not present: add as undefined external
  return symadd(ctx, name, N_UNDF | N_EXT, 0, 0);
}

static uint32_t align_up(uint32_t v, uint32_t a) {
  return (v + a - 1) & ~(a - 1);
}

static void emit_u32(MachoCtx* ctx, uint32_t insn) {
  buf_append(&ctx->text, &ctx->textsz, &ctx->textcap, &insn, 4);
}

// ARM64 condition codes indexed by CmpI
static const uint8_t arm64cond[NCmp] = {
    // integer comparisons
    [Cieq] = 0x0,
    [Cine] = 0x1,
    [Cisge] = 0xA,
    [Cisgt] = 0xC,
    [Cisle] = 0xD,
    [Cislt] = 0xB,
    [Ciuge] = 0x2,
    [Ciugt] = 0x8,
    [Ciule] = 0x9,
    [Ciult] = 0x3,
    // float comparisons (after FCMPE, ARM64 QBE_ARM64_FP flags = integer flags)
    [NCmpI + Cfeq] = 0x0,
    [NCmpI + Cfge] = 0xA,
    [NCmpI + Cfgt] = 0xC,
    [NCmpI + Cfle] = 0x9,
    [NCmpI + Cflt] = 0x4,
    [NCmpI + Cfne] = 0x1,
    [NCmpI + Cfo] = 0x7,
    [NCmpI + Cfuo] = 0x6,
};

// Branch fixup: records a branch instruction to patch after all blocks emitted
typedef struct {
  uint32_t off;
  Blk* tgt;
  int cond;
} BrFix;

static void grow_fixes(BrFix** fixes, uint32_t* fixcap) {
  uint32_t oldcap = *fixcap;
  *fixcap *= 2;
  *fixes = qbe_realloc(*fixes, oldcap * sizeof(**fixes), *fixcap * sizeof(**fixes));
  if (!*fixes) {
    die("emitobj: qbe_realloc");
  }
}

MachoCtx*
macho_new(void) {
  MachoCtx* ctx = emalloc(sizeof *ctx);
  memset(ctx, 0, sizeof *ctx);
  // string table always starts with a null byte
  ctx->strtab = emalloc(64);
  ctx->strtab[0] = '\0';
  ctx->strtabsz = 1;
  ctx->strtabcap = 64;
  return ctx;
}

void
macho_free(MachoCtx* ctx) {
  qbe_free(ctx->text);
  qbe_free(ctx->lit4);
  qbe_free(ctx->lit8);
  qbe_free(ctx->data);
  qbe_free(ctx->dreloc);
  qbe_free(ctx->treloc);
  qbe_free(ctx->syms);
  qbe_free(ctx->strtab);
  qbe_free(ctx->blfixup);
  qbe_free(ctx);
}

// Encode a constant into rd, matching loadcon() in arm64/emit.c.
static void emit_movcon(MachoCtx* ctx, int rd, int cls, Con* c) {
  int64_t n;
  int wide, sh;
  uint32_t movz, movn, movk;

  if (c->type == CAddr) {
    char sname[NString];
    uint32_t symidx;
    symname(sname, str(c->sym.id));
    symidx = callee_symidx(ctx, sname);
    {
      // An addend (e.g. sym+8) is expressed as an ARM64_RELOC_ADDEND
      // reloc at the same address as the PAGE21/PAGEOFF12 reloc.
      // The sort in macho_write ensures ADDEND appears first.
      uint32_t addend = (uint32_t)(c->bits.i & 0xFFFFFFu);
      if (c->sym.type == SThr) {
        // Thread-local: ADRP Xd, sym@tlvppage
        //             + LDR  Xd, [Xd, sym@tlvppageoff]
        if (addend)
          trelocadd_addend(ctx, (int32_t)ctx->textsz, addend);
        trelocadd(ctx, (int32_t)ctx->textsz, symidx, ARM64_RELOC_TLVP_LOAD_PAGE21, 2, 1);
        emit_u32(ctx, 0x90000000u | (uint32_t)rd);
        if (addend)
          trelocadd_addend(ctx, (int32_t)ctx->textsz, addend);
        trelocadd(ctx, (int32_t)ctx->textsz, symidx, ARM64_RELOC_TLVP_LOAD_PAGEOFF12, 2, 0);
        // LDR Xd, [Xd, #0] - loads descriptor ptr
        emit_u32(ctx, 0xF9400000u | ((uint32_t)rd << 5) | (uint32_t)rd);
      } else {
        // Global: ADRP Xd, sym@page
        //       + ADD  Xd, Xd, sym@pageoff
        if (addend)
          trelocadd_addend(ctx, (int32_t)ctx->textsz, addend);
        trelocadd(ctx, (int32_t)ctx->textsz, symidx, ARM64_RELOC_PAGE21, 2, 1);
        emit_u32(ctx, 0x90000000u | (uint32_t)rd);
        if (addend)
          trelocadd_addend(ctx, (int32_t)ctx->textsz, addend);
        trelocadd(ctx, (int32_t)ctx->textsz, symidx, ARM64_RELOC_PAGEOFF12, 2, 0);
        emit_u32(ctx, 0x91000000u | ((uint32_t)rd << 5) | (uint32_t)rd);
      }
    }
    return;
  }
  SQ_ASSERT(c->type == CBits);

  wide = (cls == Kl);
  n = c->bits.i;
  if (!wide) {
    n = (int64_t)(int32_t)n;  // sign-extend to 64 bits
  }

  movz = wide ? 0xD2800000u : 0x52800000u;  // MOVZ
  movn = wide ? 0x92800000u : 0x12800000u;  // MOVN
  movk = wide ? 0xF2800000u : 0x72800000u;  // MOVK

  {
    // For Kw work with the 32-bit unsigned value; Kl uses all 64.
    uint64_t un = wide ? (uint64_t)n : (uint64_t)(uint32_t)n;
    uint64_t cn = wide ? ~un : (~un & 0xFFFFFFFFu);
    int nhw = wide ? 4 : 2;
    int hw;
    uint64_t mask;
    uint32_t imm16;

    // Case 1: single MOVZ - un has exactly one nonzero 16-bit chunk.
    for (hw = 0; hw < nhw; hw++) {
      mask = (uint64_t)0xFFFF << (hw * 16);
      if ((un & ~mask) == 0) {
        imm16 = (uint32_t)((un >> (hw * 16)) & 0xFFFF);
        emit_u32(ctx, movz | ((uint32_t)hw << 21) | (imm16 << 5) | (uint32_t)rd);
        return;
      }
    }

    // Case 2: single MOVN - ~un has exactly one nonzero 16-bit chunk.
    // Matches loadcon's "(n | 0xffff) == -1" and similar patterns.
    for (hw = 0; hw < nhw; hw++) {
      mask = (uint64_t)0xFFFF << (hw * 16);
      if ((cn & ~mask) == 0) {
        imm16 = (uint32_t)((cn >> (hw * 16)) & 0xFFFF);
        emit_u32(ctx, movn | ((uint32_t)hw << 21) | (imm16 << 5) | (uint32_t)rd);
        return;
      }
    }
  }

  // Case 3: MOVZ + optional MOVKs, matching loadcon()'s else branch.
  emit_u32(ctx, movz | (((uint32_t)(n & 0xffff)) << 5) | (uint32_t)rd);
  for (sh = 16; (n >>= 16) != 0; sh += 16) {
    if ((!wide && sh == 32) || sh == 64) {
      break;
    }
    emit_u32(ctx,
             movk | ((uint32_t)(sh / 16) << 21) | (((uint32_t)(n & 0xffff)) << 5) | (uint32_t)rd);
  }
}

// Mirror arm64/emit.c slot() for RSlot -> x29-relative byte offset.
// s == -1: frame-start marker used by apple_selvastart (Oaddr SLOT(-1)).
// s < 0:   above-frame caller argument at byte offset -(s+2) above frame.
// s >= 0:  local / callee-save slot.
static uint32_t slot_off(int s, int padding, int framesz) {
  if (s == -1) {
    return (uint32_t)(16 + framesz);
  }
  if (s < 0) {
    return (uint32_t)(16 + framesz - (s + 2));
  }
  return (uint32_t)(16 + padding + 4 * s);
}

void
macho_emitfn(Fn* fn, MachoCtx* ctx) {
  char name[NString];
  Blk* b;
  Ins* i;
  int rd, rn, rm, wide;
  int ncallee, framesz, total, padding;
  uint32_t enc;
  uint32_t* blkoff;
  uint32_t nfix, fixcap;
  BrFix* fixes;

  symname(name, fn->name);
  symadd(ctx, name, N_SECT | (fn->lnk.export ? N_EXT : 0),
         1,  // __text
         (uint64_t)ctx->textsz);

  {
    int* r;
    // count all callee-saves (GPR QBE_ARM64_R19-QBE_ARM64_R28 then QBE_ARM64_FP QBE_ARM64_V8-QBE_ARM64_V15)
    ncallee = 0;
    for (r = arm64_rclob; *r >= 0; r++) {
      if (fn->reg & BIT(*r)) {
        ncallee++;
      }
    }
    if (ncallee & 1) {
      ncallee++;  // round up to even for alignment
    }
    // Apple vararg: no register save area; frame is identical
    // to non-vararg.  dynalloc is handled via mov sp, x29 in the epilogue.
  }
  {
    int slotbytes = ((fn->slot + 3) & -4) * 4;
    padding = slotbytes - fn->slot * 4;
    framesz = ncallee * 8 + slotbytes;
  }
  total = framesz + 16;  // + x29/x30 pair

  blkoff = emalloc(fn->nblk * sizeof blkoff[0]);
  nfix = 0;
  fixcap = 8;
  fixes = emalloc(fixcap * sizeof fixes[0]);

  // prologue
  {
    int* r;
    int k;
    emit_u32(ctx, 0xD503245Fu);  // hint #34 (BTI jc)
    if (total <= 512) {
      // small frame: single pre-indexed STP
      int imm7 = -(total / 8);
      emit_u32(ctx, 0xA9800000u | ((uint32_t)(imm7 & 0x7F) << 15) | (30u << 10) | (31u << 5) | 29u);
    } else {
      // large frame: separate sub + fixed-16 STP.
      // framesz must fit in a 12-bit immediate.
      SQ_ASSERT(framesz <= 4095);
      // sub sp, sp, #framesz
      emit_u32(ctx, 0xD1000000u | ((uint32_t)framesz << 10) | (31u << 5) | 31u);
      emit_u32(ctx, 0xA9BF7BFDu);  // stp x29,x30,[sp,-16]!
    }
    emit_u32(ctx, 0x910003FDu);  // mov x29, sp
    // save callee-saves (GPRs then QBE_ARM64_FP): str rN, [x29, #off]
    k = 0;
    for (r = arm64_rclob; *r >= 0; r++) {
      if (fn->reg & BIT(*r)) {
        k++;
        uint32_t off = (uint32_t)(16 + framesz - 8 * k);
        uint32_t imm12 = off / 8;
        if (*r >= QBE_ARM64_V0) {
          // STR Dn, [X29, #off]
          emit_u32(ctx, 0xFD000000u | (imm12 << 10) | (29u << 5) | (uint32_t)(*r - QBE_ARM64_V0));
        } else {
          // STR Xn, [X29, #off]
          emit_u32(ctx, 0xF9000000u | (imm12 << 10) | (29u << 5) | (uint32_t)(*r - QBE_ARM64_R0));
        }
      }
    }
  }

  for (b = fn->start; b; b = b->link) {
    blkoff[b->id] = ctx->textsz;
    for (i = b->ins; i < &b->ins[b->nins]; i++) {
      // CSET: integer and float flag ops
      if ((i->op >= Oflag && i->op < Oflag + NCmpI) ||
          (i->op >= Oflag + NCmpI && i->op <= Oflag1)) {
        int c = i->op - Oflag;  // 0..NCmp-1
        SQ_ASSERT(isreg(i->to));
        rd = i->to.val - QBE_ARM64_R0;
        // CSET Wd,cond = CSINC Wd,WZR,WZR,invert(cond)
        emit_u32(ctx, 0x1A9F07E0u | ((uint32_t)(arm64cond[c] ^ 1) << 12) | (uint32_t)rd);
        continue;
      }
      switch (i->op) {
        case Onop:
          break;
        case Oadd:
        case Osub:
          SQ_ASSERT(isreg(i->to) && isreg(i->arg[0]));
          if (rtype(i->arg[1]) == RCon) {
            // ADD/SUB Xd/Wd, Xn/Wn, #imm12
            // (emitted by isel TLS offset lowering)
            Con* cc = &fn->con[i->arg[1].val];
            int64_t imm;
            SQ_ASSERT(cc->type == CBits);
            imm = cc->bits.i;
            rd = i->to.val - QBE_ARM64_R0;
            rn = i->arg[0].val - QBE_ARM64_R0;
            wide = (i->cls == Kl);
            SQ_ASSERT(imm >= 0 && imm < 4096);
            if (i->op == Oadd) {
              enc = wide ? 0x91000000u : 0x11000000u;
            } else {
              enc = wide ? 0xD1000000u : 0x51000000u;
            }
            emit_u32(ctx, enc | ((uint32_t)imm << 10) | ((uint32_t)rn << 5) | (uint32_t)rd);
            break;
          }
          SQ_ASSERT(isreg(i->arg[1]));
          if (i->cls == Ks || i->cls == Kd) {
            // FADD/FSUB Sd/Dd, Sn/Dn, Sm/Dm
            rd = i->to.val - QBE_ARM64_V0;
            rn = i->arg[0].val - QBE_ARM64_V0;
            rm = i->arg[1].val - QBE_ARM64_V0;
            if (i->op == Oadd) {
              enc = (i->cls == Kd) ? 0x1E602800u : 0x1E202800u;
            } else {
              enc = (i->cls == Kd) ? 0x1E603800u : 0x1E203800u;
            }
            emit_u32(ctx, enc | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd);
            break;
          }
          rd = i->to.val - QBE_ARM64_R0;
          rn = i->arg[0].val - QBE_ARM64_R0;
          rm = i->arg[1].val - QBE_ARM64_R0;
          wide = (i->cls == Kl);
          if (i->op == Oadd) {
            enc = wide ? 0x8B000000u : 0x0B000000u;
          } else {
            enc = wide ? 0xCB000000u : 0x4B000000u;
          }
          if (wide && (rd == 31 || rn == 31)) {
            // QBE_ARM64_SP involved: use extended-register form (bit21=1,
            // option=UXTX=011) so that register 31 encodes as QBE_ARM64_SP, not XZR
            enc |= 0x00200000u | (3u << 13);
          }
          emit_u32(ctx, enc | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd);
          break;
        case Omul:
          SQ_ASSERT(isreg(i->to) && isreg(i->arg[0]) && isreg(i->arg[1]));
          if (i->cls == Ks || i->cls == Kd) {
            // FMUL Sd/Dd, Sn/Dn, Sm/Dm
            rd = i->to.val - QBE_ARM64_V0;
            rn = i->arg[0].val - QBE_ARM64_V0;
            rm = i->arg[1].val - QBE_ARM64_V0;
            enc = (i->cls == Kd) ? 0x1E600800u : 0x1E200800u;
            emit_u32(ctx, enc | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd);
            break;
          }
          rd = i->to.val - QBE_ARM64_R0;
          rn = i->arg[0].val - QBE_ARM64_R0;
          rm = i->arg[1].val - QBE_ARM64_R0;
          wide = (i->cls == Kl);
          // MUL = MADD with Ra=XZR (31)
          enc = wide ? 0x9B007C00u : 0x1B007C00u;
          emit_u32(ctx, enc | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd);
          break;
        case Oswap: {
          // Three-move swap via QBE_ARM64_IP1 (X17) scratch. Integer only; float swap
          // (fmov) not yet implemented.
          int sc = QBE_ARM64_IP1 - QBE_ARM64_R0;  // = 17 = X17
          SQ_ASSERT(i->cls == Kw || i->cls == Kl);
          SQ_ASSERT(isreg(i->arg[0]) && isreg(i->arg[1]));
          rn = i->arg[0].val - QBE_ARM64_R0;
          rm = i->arg[1].val - QBE_ARM64_R0;
          wide = (i->cls == Kl);
          enc = wide ? 0xAA0003E0u : 0x2A0003E0u;
          emit_u32(ctx, enc | ((uint32_t)rn << 16) | (uint32_t)sc);  // mov QBE_ARM64_IP1, rn
          emit_u32(ctx, enc | ((uint32_t)rm << 16) | (uint32_t)rn);  // mov rn, rm
          emit_u32(ctx, enc | ((uint32_t)sc << 16) | (uint32_t)rm);  // mov rm, QBE_ARM64_IP1
          break;
        }
        case Ocopy:
          if (req(i->to, i->arg[0])) {
            break;  // no-op
          }
          SQ_ASSERT(isreg(i->to));
          if (i->cls == Ks || i->cls == Kd) {
            // FMOV Sd/Dd, Sn/Dn
            SQ_ASSERT(isreg(i->arg[0]));
            rd = i->to.val - QBE_ARM64_V0;
            rm = i->arg[0].val - QBE_ARM64_V0;
            enc = (i->cls == Kd) ? 0x1E604000u : 0x1E204000u;
            emit_u32(ctx, enc | ((uint32_t)rm << 5) | (uint32_t)rd);
            break;
          }
          rd = i->to.val - QBE_ARM64_R0;
          wide = (i->cls == Kl);
          if (rtype(i->arg[0]) == RCon) {
            emit_movcon(ctx, rd, i->cls, &fn->con[i->arg[0].val]);
            break;
          }
          SQ_ASSERT(isreg(i->arg[0]));
          rm = i->arg[0].val - QBE_ARM64_R0;
          // MOV Wd,Wm = ORR Wd,WZR,Wm
          enc = wide ? 0xAA0003E0u : 0x2A0003E0u;
          emit_u32(ctx, enc | ((uint32_t)rm << 16) | (uint32_t)rd);
          break;
        case Oacmp:
        case Oacmn:
          SQ_ASSERT(isreg(i->arg[0]));
          rn = i->arg[0].val - QBE_ARM64_R0;
          wide = (i->cls == Kl);
          if (rtype(i->arg[1]) == RCon) {
            // CMP/CMN immediate
            Con* c = &fn->con[i->arg[1].val];
            int64_t imm = c->bits.i;
            SQ_ASSERT(imm >= 0 && imm < 4096);
            if (i->op == Oacmp) {
              enc = wide ? 0xF100001Fu : 0x7100001Fu;
            } else {
              enc = wide ? 0xB100001Fu : 0x3100001Fu;
            }
            emit_u32(ctx, enc | ((uint32_t)imm << 10) | ((uint32_t)rn << 5));
          } else {
            // CMP/CMN register
            SQ_ASSERT(isreg(i->arg[1]));
            rm = i->arg[1].val - QBE_ARM64_R0;
            if (i->op == Oacmp) {
              enc = wide ? 0xEB00001Fu : 0x6B00001Fu;
            } else {
              enc = wide ? 0xAB00001Fu : 0x2B00001Fu;
            }
            emit_u32(ctx, enc | ((uint32_t)rm << 16) | ((uint32_t)rn << 5));
          }
          break;
        case Ostoreb:
        case Ostoreh:
        case Ostorew:
        case Ostorel: {
          int scale;
          Ref addr;
          SQ_ASSERT(isreg(i->arg[0]));
          addr = i->arg[1];
          // float store: Ostorew Ks / Ostorel Kd
          if (i->arg[0].val >= QBE_ARM64_V0) {
            rd = i->arg[0].val - QBE_ARM64_V0;
            enc = (i->op == Ostorel) ? 0xFD000000u : 0xBD000000u;
            scale = (i->op == Ostorel) ? 8 : 4;
            if (isreg(addr)) {
              rn = addr.val - QBE_ARM64_R0;
              emit_u32(ctx, enc | ((uint32_t)rn << 5) | (uint32_t)rd);
            } else {
              uint32_t off;
              SQ_ASSERT(rtype(addr) == RSlot);
              off = slot_off(rsval(addr), padding, framesz);
              emit_u32(ctx, enc | ((off / (uint32_t)scale) << 10) | (29u << 5) | (uint32_t)rd);
            }
            break;
          }
          rd = i->arg[0].val - QBE_ARM64_R0;  // value (rt)
          switch (i->op) {
            case Ostoreb:
              enc = 0x39000000u;
              scale = 1;
              break;
            case Ostoreh:
              enc = 0x79000000u;
              scale = 2;
              break;
            case Ostorew:
              enc = 0xB9000000u;
              scale = 4;
              break;
            default:
              enc = 0xF9000000u;
              scale = 8;
              break;
          }
          if (isreg(addr)) {
            rn = addr.val - QBE_ARM64_R0;
            emit_u32(ctx, enc | ((uint32_t)rn << 5) | (uint32_t)rd);
          } else {
            uint32_t off;
            SQ_ASSERT(rtype(addr) == RSlot);
            off = slot_off(rsval(addr), padding, framesz);
            emit_u32(ctx, enc | ((off / (uint32_t)scale) << 10) | (29u << 5) | (uint32_t)rd);
          }
          break;
        }
        case Oloaduw:
        case Oloadsw: {
          // loaduw -> LDR Wt,[Xn]; loadsw/Kl -> LDRSW Xt,[Xn]
          Ref addr;
          SQ_ASSERT(isreg(i->to));
          rd = i->to.val - QBE_ARM64_R0;
          addr = i->arg[0];
          enc = (i->op == Oloadsw && i->cls == Kl) ? 0xB9800000u   // LDRSW Xt
                                                   : 0xB9400000u;  // LDR   Wt
          if (isreg(addr)) {
            rn = addr.val - QBE_ARM64_R0;
            emit_u32(ctx, enc | ((uint32_t)rn << 5) | (uint32_t)rd);
          } else {
            uint32_t off;
            SQ_ASSERT(rtype(addr) == RSlot);
            off = slot_off(rsval(addr), padding, framesz);
            emit_u32(ctx, enc | ((off / 4u) << 10) | (29u << 5) | (uint32_t)rd);
          }
          break;
        }
        case Oloadsb:
        case Oloadub:
        case Oloadsh:
        case Oloaduh:
        case Oload: {
          int scale;
          Ref addr;
          SQ_ASSERT(isreg(i->to));
          rd = i->to.val - QBE_ARM64_R0;
          addr = i->arg[0];
          wide = (i->cls == Kl);
          switch (i->op) {
            case Oloadsb:
              enc = wide ? 0x39800000u : 0x39C00000u;
              scale = 1;
              break;
            case Oloadub:
              enc = 0x39400000u;
              scale = 1;
              break;
            case Oloadsh:
              enc = wide ? 0x79800000u : 0x79C00000u;
              scale = 2;
              break;
            case Oloaduh:
              enc = 0x79400000u;
              scale = 2;
              break;
            default:  // Oload
              if (i->cls == Ks) {
                rd = i->to.val - QBE_ARM64_V0;
                enc = 0xBD400000u;
                scale = 4;
              } else if (i->cls == Kd) {
                rd = i->to.val - QBE_ARM64_V0;
                enc = 0xFD400000u;
                scale = 8;
              } else {
                enc = wide ? 0xF9400000u : 0xB9400000u;
                scale = wide ? 8 : 4;
              }
              break;
          }
          if (isreg(addr)) {
            rn = addr.val - QBE_ARM64_R0;
            emit_u32(ctx, enc | ((uint32_t)rn << 5) | (uint32_t)rd);
          } else {
            uint32_t off;
            SQ_ASSERT(rtype(addr) == RSlot);
            off = slot_off(rsval(addr), padding, framesz);
            emit_u32(ctx, enc | ((off / (uint32_t)scale) << 10) | (29u << 5) | (uint32_t)rd);
          }
          break;
        }
        case Oextsb:
        case Oextub:
        case Oextsh:
        case Oextuh:
        case Oextsw:
        case Oextuw:
          SQ_ASSERT(isreg(i->to) && isreg(i->arg[0]));
          rd = i->to.val - QBE_ARM64_R0;
          rn = i->arg[0].val - QBE_ARM64_R0;
          wide = (i->cls == Kl);
          switch (i->op) {
            case Oextsb:
              // SXTB: Kl->Xt, Kw->Wt
              enc = wide ? 0x93401C00u : 0x13001C00u;
              emit_u32(ctx, enc | ((uint32_t)rn << 5) | (uint32_t)rd);
              break;
            case Oextub:
              // UXTB Wt,Wn - always 32-bit
              emit_u32(ctx, 0x53001C00u | ((uint32_t)rn << 5) | (uint32_t)rd);
              break;
            case Oextsh:
              // SXTH: Kl->Xt, Kw->Wt
              enc = wide ? 0x93403C00u : 0x13003C00u;
              emit_u32(ctx, enc | ((uint32_t)rn << 5) | (uint32_t)rd);
              break;
            case Oextuh:
              // UXTH Wt,Wn - always 32-bit
              emit_u32(ctx, 0x53003C00u | ((uint32_t)rn << 5) | (uint32_t)rd);
              break;
            case Oextsw:
              // SXTW Xt,Wn - always Kl
              emit_u32(ctx, 0x93407C00u | ((uint32_t)rn << 5) | (uint32_t)rd);
              break;
            default:  // Oextuw
              // MOV Wd,Wm = ORR Wd,WZR,Wm (zero-extend)
              emit_u32(ctx, 0x2A0003E0u | ((uint32_t)rn << 16) | (uint32_t)rd);
              break;
          }
          break;
        case Odiv:
        case Oudiv:
          SQ_ASSERT(isreg(i->to) && isreg(i->arg[0]) && isreg(i->arg[1]));
          if (i->cls == Ks || i->cls == Kd) {
            // FDIV Sd/Dd, Sn/Dn, Sm/Dm
            rd = i->to.val - QBE_ARM64_V0;
            rn = i->arg[0].val - QBE_ARM64_V0;
            rm = i->arg[1].val - QBE_ARM64_V0;
            enc = (i->cls == Kd) ? 0x1E601800u : 0x1E201800u;
            emit_u32(ctx, enc | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd);
            break;
          }
          rd = i->to.val - QBE_ARM64_R0;
          rn = i->arg[0].val - QBE_ARM64_R0;
          rm = i->arg[1].val - QBE_ARM64_R0;
          wide = (i->cls == Kl);
          enc = (i->op == Odiv) ? (wide ? 0x9AC00C00u : 0x1AC00C00u)   // SDIV
                                : (wide ? 0x9AC00800u : 0x1AC00800u);  // UDIV
          emit_u32(ctx, enc | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd);
          break;
        case Orem:
        case Ourem: {
          int scratch = QBE_ARM64_IP1 - QBE_ARM64_R0;  // x17 - intra-procedure scratch
          SQ_ASSERT(isreg(i->to) && isreg(i->arg[0]) && isreg(i->arg[1]));
          rd = i->to.val - QBE_ARM64_R0;
          rn = i->arg[0].val - QBE_ARM64_R0;  // dividend
          rm = i->arg[1].val - QBE_ARM64_R0;  // divisor
          wide = (i->cls == Kl);
          // step 1: scratch = dividend / divisor
          enc = (i->op == Orem) ? (wide ? 0x9AC00C00u : 0x1AC00C00u)   // SDIV
                                : (wide ? 0x9AC00800u : 0x1AC00800u);  // UDIV
          emit_u32(ctx, enc | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)scratch);
          // step 2: rd = rn - scratch*rm (MSUB rd, scratch, rm, rn)
          enc = wide ? 0x9B008000u : 0x1B008000u;
          emit_u32(ctx, enc | ((uint32_t)rm << 16) | ((uint32_t)rn << 10) |
                            ((uint32_t)scratch << 5) | (uint32_t)rd);
          break;
        }
        case Oand:
        case Oor:
        case Oxor:
          SQ_ASSERT(isreg(i->to) && isreg(i->arg[0]) && isreg(i->arg[1]));
          rd = i->to.val - QBE_ARM64_R0;
          rn = i->arg[0].val - QBE_ARM64_R0;
          rm = i->arg[1].val - QBE_ARM64_R0;
          wide = (i->cls == Kl);
          if (i->op == Oand) {
            enc = wide ? 0x8A000000u : 0x0A000000u;
          } else if (i->op == Oor) {
            enc = wide ? 0xAA000000u : 0x2A000000u;
          } else {
            enc = wide ? 0xCA000000u : 0x4A000000u;
          }
          emit_u32(ctx, enc | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd);
          break;
        case Oneg:
          SQ_ASSERT(isreg(i->to) && isreg(i->arg[0]));
          if (i->cls == Ks || i->cls == Kd) {
            // FNEG Sd/Dd, Sn/Dn
            rd = i->to.val - QBE_ARM64_V0;
            rm = i->arg[0].val - QBE_ARM64_V0;
            enc = (i->cls == Kd) ? 0x1E614000u : 0x1E214000u;
            emit_u32(ctx, enc | ((uint32_t)rm << 5) | (uint32_t)rd);
            break;
          }
          rd = i->to.val - QBE_ARM64_R0;
          rm = i->arg[0].val - QBE_ARM64_R0;
          wide = (i->cls == Kl);
          enc = wide ? 0xCB0003E0u : 0x4B0003E0u;
          emit_u32(ctx, enc | ((uint32_t)rm << 16) | (uint32_t)rd);
          break;
        case Osar:
        case Oshr:
        case Oshl:
          SQ_ASSERT(isreg(i->to) && isreg(i->arg[0]) && isreg(i->arg[1]));
          rd = i->to.val - QBE_ARM64_R0;
          rn = i->arg[0].val - QBE_ARM64_R0;
          rm = i->arg[1].val - QBE_ARM64_R0;
          wide = (i->cls == Kl);
          if (i->op == Osar) {
            enc = wide ? 0x9AC02800u : 0x1AC02800u;
          } else if (i->op == Oshr) {
            enc = wide ? 0x9AC02400u : 0x1AC02400u;
          } else {
            enc = wide ? 0x9AC02000u : 0x1AC02000u;
          }
          emit_u32(ctx, enc | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd);
          break;
        case Ocall: {
          if (rtype(i->arg[0]) == RTmp) {
            // BLR Xn - indirect call
            SQ_ASSERT(isreg(i->arg[0]));
            rn = i->arg[0].val - QBE_ARM64_R0;
            emit_u32(ctx, 0xD63F0000u | ((uint32_t)rn << 5));
          } else {
            // BL sym - direct call
            Con* c;
            char cname[NString];
            uint32_t symidx;
            SQ_ASSERT(rtype(i->arg[0]) == RCon);
            c = &fn->con[i->arg[0].val];
            SQ_ASSERT(c->type == CAddr && !c->bits.i);
            symname(cname, str(c->sym.id));
            symidx = callee_symidx(ctx, cname);
            if ((ctx->syms[symidx].type & 0x0Eu) == N_SECT) {
              // backward ref: direct offset
              int32_t off = ((int32_t)ctx->syms[symidx].value - (int32_t)ctx->textsz) / 4;
              emit_u32(ctx, 0x94000000u | ((uint32_t)off & 0x3FFFFFFu));
            } else {
              // forward/external: placeholder; resolved in macho_write
              if (ctx->nblfixup == ctx->blfixupap) {
                uint32_t oldcap = ctx->blfixupap;
                ctx->blfixupap = ctx->blfixupap ? ctx->blfixupap * 2 : 8;
                ctx->blfixup = qbe_realloc(ctx->blfixup, oldcap * sizeof *ctx->blfixup,
                                           ctx->blfixupap * sizeof *ctx->blfixup);
                if (!ctx->blfixup) {
                  die("obj: qbe_realloc");
                }
              }
              ctx->blfixup[ctx->nblfixup].off = ctx->textsz;
              ctx->blfixup[ctx->nblfixup].symidx = symidx;
              ctx->nblfixup++;
              emit_u32(ctx, 0x94000000u);
            }
          }
          break;
        }
        case Oaddr: {
          uint32_t off;
          SQ_ASSERT(isreg(i->to));
          SQ_ASSERT(rtype(i->arg[0]) == RSlot);
          rd = i->to.val - QBE_ARM64_R0;
          off = slot_off(rsval(i->arg[0]), padding, framesz);
          SQ_ASSERT(off <= 4095);
          // ADD Xd, X29, #off
          emit_u32(ctx, 0x91000000u | (off << 10) | (29u << 5) | (uint32_t)rd);
          break;
        }
        case Oafcmp:
          // FCMPE Sn, Sm / FCMPE Dn, Dm
          SQ_ASSERT(isreg(i->arg[0]) && isreg(i->arg[1]));
          rn = i->arg[0].val - QBE_ARM64_V0;
          rm = i->arg[1].val - QBE_ARM64_V0;
          enc = (i->cls == Kd) ? 0x1E602010u : 0x1E202010u;
          emit_u32(ctx, enc | ((uint32_t)rm << 16) | ((uint32_t)rn << 5));
          break;
        case Ocast:
          // FMOV between QBE_ARM64_FP and GPR (bit-cast)
          SQ_ASSERT(isreg(i->to) && isreg(i->arg[0]));
          switch (i->cls) {
            case Kw:  // fmov Wd, Sn: GPR=to, QBE_ARM64_FP=arg[0]
              rd = i->to.val - QBE_ARM64_R0;
              rn = i->arg[0].val - QBE_ARM64_V0;
              emit_u32(ctx, 0x1E260000u | ((uint32_t)rn << 5) | (uint32_t)rd);
              break;
            case Kl:  // fmov Xd, Dn
              rd = i->to.val - QBE_ARM64_R0;
              rn = i->arg[0].val - QBE_ARM64_V0;
              emit_u32(ctx, 0x9E660000u | ((uint32_t)rn << 5) | (uint32_t)rd);
              break;
            case Ks:  // fmov Sd, Wn: QBE_ARM64_FP=to, GPR=arg[0]
              rd = i->to.val - QBE_ARM64_V0;
              rn = i->arg[0].val - QBE_ARM64_R0;
              emit_u32(ctx, 0x1E270000u | ((uint32_t)rn << 5) | (uint32_t)rd);
              break;
            default:  // Kd: fmov Dd, Xn
              rd = i->to.val - QBE_ARM64_V0;
              rn = i->arg[0].val - QBE_ARM64_R0;
              emit_u32(ctx, 0x9E670000u | ((uint32_t)rn << 5) | (uint32_t)rd);
              break;
          }
          break;
        case Ostosi:
        case Ostoui:
        case Odtosi:
        case Odtoui: {
          // FCVTZS/FCVTZU: QBE_ARM64_FP->GPR integer conversion
          int sfp = (i->op == Odtosi || i->op == Odtoui);
          int uns = (i->op == Ostoui || i->op == Odtoui);
          int wx = (i->cls == Kl);
          rd = i->to.val - QBE_ARM64_R0;
          rn = i->arg[0].val - QBE_ARM64_V0;
          if (sfp) {
            enc = uns ? (wx ? 0x9E790000u : 0x1E790000u) : (wx ? 0x9E780000u : 0x1E780000u);
          } else {
            enc = uns ? (wx ? 0x9E390000u : 0x1E390000u) : (wx ? 0x9E380000u : 0x1E380000u);
          }
          emit_u32(ctx, enc | ((uint32_t)rn << 5) | (uint32_t)rd);
          break;
        }
        case Oswtof:
        case Ouwtof:
        case Osltof:
        case Oultof: {
          // SCVTF/UCVTF: GPR->QBE_ARM64_FP conversion
          int xl = (i->op == Osltof || i->op == Oultof);
          int uns = (i->op == Ouwtof || i->op == Oultof);
          int dbl = (i->cls == Kd);
          rd = i->to.val - QBE_ARM64_V0;
          rn = i->arg[0].val - QBE_ARM64_R0;
          if (xl) {
            enc = uns ? (dbl ? 0x9E630000u : 0x9E230000u) : (dbl ? 0x9E620000u : 0x9E220000u);
          } else {
            enc = uns ? (dbl ? 0x1E630000u : 0x1E230000u) : (dbl ? 0x1E620000u : 0x1E220000u);
          }
          emit_u32(ctx, enc | ((uint32_t)rn << 5) | (uint32_t)rd);
          break;
        }
        case Oexts:
          // FCVT Dd, Sn - widen single to double
          SQ_ASSERT(isreg(i->to) && isreg(i->arg[0]));
          rd = i->to.val - QBE_ARM64_V0;
          rn = i->arg[0].val - QBE_ARM64_V0;
          emit_u32(ctx, 0x1E22C000u | ((uint32_t)rn << 5) | (uint32_t)rd);
          break;
        case Otruncd:
          // FCVT Sd, Dn - narrow double to single
          SQ_ASSERT(isreg(i->to) && isreg(i->arg[0]));
          rd = i->to.val - QBE_ARM64_V0;
          rn = i->arg[0].val - QBE_ARM64_V0;
          emit_u32(ctx, 0x1E624000u | ((uint32_t)rn << 5) | (uint32_t)rd);
          break;
        case Ostores:
        case Ostored: {
          // STR Sn/Dn, [Xm] or [X29, #slot]
          // arg[0]=float value, arg[1]=address
          int fscale = (i->op == Ostored) ? 8 : 4;
          Ref faddr = i->arg[1];
          SQ_ASSERT(isreg(i->arg[0]));
          rd = i->arg[0].val - QBE_ARM64_V0;
          enc = (i->op == Ostored) ? 0xFD000000u : 0xBD000000u;
          if (isreg(faddr)) {
            rn = faddr.val - QBE_ARM64_R0;
            emit_u32(ctx, enc | ((uint32_t)rn << 5) | (uint32_t)rd);
          } else {
            uint32_t off;
            SQ_ASSERT(rtype(faddr) == RSlot);
            off = slot_off(rsval(faddr), padding, framesz);
            emit_u32(ctx, enc | ((off / (uint32_t)fscale) << 10) | (29u << 5) | (uint32_t)rd);
          }
          break;
        }
        case Osalloc:
          // sub sp, sp, Xn (extended-register form: QBE_ARM64_SP involved)
          SQ_ASSERT(isreg(i->arg[0]));
          rm = i->arg[0].val - QBE_ARM64_R0;
          emit_u32(ctx, 0xCB206000u | ((uint32_t)rm << 16) | (31u << 5) | 31u);
          if (!req(i->to, NULL_R)) {
            // mov Xd, sp = ADD Xd, QBE_ARM64_SP, #0
            SQ_ASSERT(isreg(i->to));
            rd = i->to.val - QBE_ARM64_R0;
            emit_u32(ctx, 0x91000000u | (31u << 5) | (uint32_t)rd);
          }
          break;
        default:
          die("obj: %s: unhandled op %s", fn->name, optab[i->op].name);
      }
    }
    // jump
    switch (b->jmp.type) {
      case Jret0: {
        int* r;
        int k;
        // restore callee-saves (same order as prologue)
        k = 0;
        for (r = arm64_rclob; *r >= 0; r++) {
          if (fn->reg & BIT(*r)) {
            k++;
            uint32_t off = (uint32_t)(16 + framesz - 8 * k);
            uint32_t imm12 = off / 8;
            if (*r >= QBE_ARM64_V0) {
              // LDR Dn, [X29, #off]
              emit_u32(ctx, 0xFD400000u | (imm12 << 10) | (29u << 5) | (uint32_t)(*r - QBE_ARM64_V0));
            } else {
              // LDR Xn, [X29, #off]
              emit_u32(ctx, 0xF9400000u | (imm12 << 10) | (29u << 5) | (uint32_t)(*r - QBE_ARM64_R0));
            }
          }
        }
        if (fn->dynalloc) {
          // reset sp to frame pointer before restoring frame
          emit_u32(ctx, 0x910003BFu);  // mov sp, x29 = ADD QBE_ARM64_SP, X29, #0
        }
        if (total <= 512) {
          // small frame: single post-indexed LDP
          emit_u32(ctx,
                   0xA8C00000u | ((uint32_t)(total / 8) << 15) | (30u << 10) | (31u << 5) | 29u);
        } else {
          // large frame: fixed-16 LDP + add sp
          emit_u32(ctx, 0xA8C17BFDu);  // ldp x29,x30,[sp],16
          // add sp, sp, #framesz
          emit_u32(ctx, 0x91000000u | ((uint32_t)framesz << 10) | (31u << 5) | 31u);
        }
        emit_u32(ctx, 0xD65F03C0u);  // ret
        break;
      }
      case Jhlt:
        emit_u32(ctx, 0xD4207D00u);  // brk #1000
        break;
      case Jjmp:
        if (b->s1 != b->link) {
          uint32_t boff = ctx->textsz;
          emit_u32(ctx, 0x14000000u);  // B (placeholder)
          if (nfix == fixcap) {
            grow_fixes(&fixes, &fixcap);
          }
          fixes[nfix++] = (BrFix){boff, b->s1, -1};
        }
        break;
      default: {
        int c = b->jmp.type - Jjf;
        Blk* brtgt;
        if (c < 0 || c >= NCmp) {
          die("obj: %s: unhandled jump %d", fn->name, b->jmp.type);
        }
        if (b->link == b->s2) {
          brtgt = b->s1;  // s2 falls through; branch to s1 if c
        } else {
          c = cmpneg(c);  // s1 falls through; branch to s2 if !c
          brtgt = b->s2;
          // cmpneg of some ordered float conditions gives the NaN-excluding
          // form; remap to NaN-inclusive conditions so NaN causes the branch
          // (matching the text assembler's behaviour after fcmpe)
          // clang-format off
          switch (c) {
            case NCmpI + Cfge: c = Ciuge; break;  // ge->cs
            case NCmpI + Cfgt: c = Ciugt; break;  // gt->hi
            case NCmpI + Cfle: c = Cisle; break;  // le->ls
            case NCmpI + Cflt: c = Cislt; break;  // lt->mi
            default: break;
          }
          // clang-format on
        }
        {
          uint32_t boff = ctx->textsz;
          emit_u32(ctx, 0x54000000u | (uint32_t)arm64cond[c]);
          if (nfix == fixcap) {
            grow_fixes(&fixes, &fixcap);
          }
          fixes[nfix++] = (BrFix){boff, brtgt, arm64cond[c]};
        }
        // if neither successor falls through, also branch to s1
        if (b->link != b->s1 && b->link != b->s2) {
          uint32_t boff = ctx->textsz;
          emit_u32(ctx, 0x14000000u);
          if (nfix == fixcap) {
            grow_fixes(&fixes, &fixcap);
          }
          fixes[nfix++] = (BrFix){boff, b->s1, -1};
        }
        break;
      }
    }
  }

  // apply branch fixups
  for (uint32_t f = 0; f < nfix; f++) {
    uint32_t toff = blkoff[fixes[f].tgt->id];
    int32_t delta = ((int32_t)toff - (int32_t)fixes[f].off) / 4;
    uint32_t* p = (uint32_t*)&ctx->text[fixes[f].off];
    if (fixes[f].cond >= 0) {
      *p |= (uint32_t)(delta & 0x7FFFFu) << 5;  // B.cond imm19
    } else {
      *p |= (uint32_t)(delta & 0x3FFFFFFu);  // B imm26
    }
  }
  qbe_free(blkoff);
  qbe_free(fixes);
}

void
macho_emitdat(Dat* d, MachoCtx* ctx) {
  static const int64_t masks[] = {
      [DB] = 0xFFL,
      [DH] = 0xFFFFL,
      [DW] = 0xFFFFFFFFL,
      [DL] = -1L,
  };
  static const uint8_t zeros[8] = {0};
  uint64_t v;
  uint32_t refidx;
  char mname[NString];
  const char* p;
  int n;

  switch (d->type) {
    case DStart:
      strncpy(ctx->datname, d->name, NString - 1);
      ctx->datname[NString - 1] = '\0';
      ctx->datlnk = d->lnk;
      ctx->dleadzero = 0;
      // apply alignment padding before this data object
      if (d->lnk->align) {
        uint32_t aln = (uint32_t)(unsigned char)d->lnk->align;
        while (ctx->datasz % aln != 0) {
          buf_append(&ctx->data, &ctx->datasz, &ctx->datacap, zeros, 1);
        }
      }
      ctx->datstart = ctx->datasz;
      break;

    case DZ:
      if (ctx->dleadzero >= 0) {
        ctx->dleadzero += d->u.num;
      } else {
        uint64_t rem = (uint64_t)d->u.num;
        while (rem > 0) {
          uint32_t chunk = (rem > 8) ? 8 : (uint32_t)rem;
          buf_append(&ctx->data, &ctx->datasz, &ctx->datacap, zeros, chunk);
          rem -= chunk;
        }
      }
      break;

    case DEnd:
      if (ctx->datlnk->common) {
        die("obj: common data not supported");
      }
      if (ctx->dleadzero >= 0) {
        // all-zero data -> BSS
        char name[NString];
        symname(name, ctx->datname);
        symadd(ctx, name, N_SECT | (ctx->datlnk->export ? N_EXT : 0), SECT_BSS,
               (uint64_t)ctx->bsssz);
        ctx->bsssz += (uint32_t)ctx->dleadzero;
      } else {
        // real data
        char name[NString];
        symname(name, ctx->datname);
        symadd(ctx, name, N_SECT | (ctx->datlnk->export ? N_EXT : 0), SECT_DATA,
               (uint64_t)ctx->datstart);
      }
      break;

    default:  // DB, DH, DW, DL
      // flush any accumulated leading zeros
      if (ctx->dleadzero >= 0) {
        uint64_t rem = (uint64_t)ctx->dleadzero;
        while (rem > 0) {
          uint32_t chunk = (rem > 8) ? 8 : (uint32_t)rem;
          buf_append(&ctx->data, &ctx->datasz, &ctx->datacap, zeros, chunk);
          rem -= chunk;
        }
        ctx->dleadzero = -1;
      }
      if (d->isstr) {
        if (d->type != DB) {
          die("obj: strings only supported for 'b'");
        }
        // unescape and emit raw bytes
        p = d->u.str;
        SQ_ASSERT(p[0] == '"');
        p++;
        while (*p && *p != '"') {
          uint8_t c;
          if (*p == '\\') {
            p++;
            // clang-format off
            switch (*p) {
              case 'n': c = '\n'; break;
              case 't': c = '\t'; break;
              case 'r': c = '\r'; break;
              case '\\': c = '\\'; break;
              case '"': c = '"'; break;
              case '0': c = '\0'; break;
              default: c = (uint8_t)*p; break;
            }
            // clang-format on
          } else {
            c = (uint8_t)*p;
          }
          buf_append(&ctx->data, &ctx->datasz, &ctx->datacap, &c, 1);
          p++;
        }
      } else if (d->isref) {
        if (d->type != DL) {
          die("obj: pointer ref only supported in .quad");
        }
        // find or create undef symbol for the reference
        symname(mname, d->u.ref.name);
        refidx = ctx->nsyms;  // sentinel: not found
        for (uint32_t i = 0; i < ctx->nsyms; i++) {
          if (strcmp(ctx->strtab + ctx->syms[i].strx, mname) == 0) {
            refidx = i;
            break;
          }
        }
        if (refidx == ctx->nsyms) {
          refidx = symadd(ctx, mname, N_UNDF | N_EXT, 0, 0);
        }
        // ADDEND reloc if offset is nonzero
        if (d->u.ref.off != 0) {
          uint32_t addend = (uint32_t)(d->u.ref.off & 0xFFFFFFu);
          relocadd(ctx, (int32_t)ctx->datasz, addend, ARM64_RELOC_ADDEND, 0, 2);
        }
        // UNSIGNED reloc for the 8-byte pointer slot
        relocadd(ctx, (int32_t)ctx->datasz, refidx, ARM64_RELOC_UNSIGNED, 1, 3);
        // 8 zero bytes; linker patches at link time
        buf_append(&ctx->data, &ctx->datasz, &ctx->datacap, zeros, 8);
      } else {
        // plain numeric value, little-endian
        n = (d->type == DB) ? 1 : (d->type == DH) ? 2 : (d->type == DW) ? 4 : 8;
        v = (uint64_t)(d->u.num & masks[d->type]);
        buf_append(&ctx->data, &ctx->datasz, &ctx->datacap, &v, (uint32_t)n);
      }
      break;
  }
}

// Returns 0=local, 1=extdef, 2=undef
static int symcat(uint8_t type) {
  int isext = (type & N_EXT) != 0;
  int issect = (type & 0x0Eu) == N_SECT;
  if (issect && !isext) {
    return 0;
  }
  if (issect && isext) {
    return 1;
  }
  return 2;
}

static int reloc_cmp(const void* a, const void* b) {
  const MReloc *ra = a, *rb = b;
  // primary: descending by address
  if (ra->addr != rb->addr) {
    return (rb->addr > ra->addr) ? 1 : -1;
  }
  // tie-break: ADDEND (type=10) before UNSIGNED (type=0)
  int ta = ((ra->info >> 28) == ARM64_RELOC_ADDEND) ? 0 : 1;
  int tb = ((rb->info >> 28) == ARM64_RELOC_ADDEND) ? 0 : 1;
  return ta - tb;
}

static void
qbe_arm64_emitmacho_fp_const_cb(int idx, bits n, int size, void* ud) {
  MachoCtx* ctx = ud;
  static const uint8_t zeros[8] = {0};
  char sname[NString];
  uint32_t j, sym_off;
  uint8_t sect_sentinel;
  uint8_t** buf;
  uint32_t* sz;
  uint32_t* cap;
  uint64_t v;

  // route 4-byte constants to __literal4, 8-byte to __literal8
  if (size == 4) {
    buf = &ctx->lit4; sz = &ctx->lit4sz; cap = &ctx->lit4cap;
    sect_sentinel = SECT_LIT4;
  } else {
    buf = &ctx->lit8; sz = &ctx->lit8sz; cap = &ctx->lit8cap;
    sect_sentinel = SECT_LIT8;
  }

  // align within the literal section to 'size' bytes
  while (*sz % (uint32_t)size != 0)
    buf_append(buf, sz, cap, zeros, 1);
  sym_off = *sz;

  // emit the constant bytes (little-endian, native)
  v = (uint64_t)n;
  buf_append(buf, sz, cap, &v, (uint32_t)size);

  // symbol name matches isel.c: "{asloc}fp{idx}"
  snprintf(sname, NString, "%sfp%d", GC(T).asloc, idx);

  // if already referenced (added as N_UNDF by callee_symidx), update to defined
  for (j = 0; j < ctx->nsyms; j++) {
    if (strcmp(ctx->strtab + ctx->syms[j].strx, sname) == 0) {
      ctx->syms[j].type = N_SECT;
      ctx->syms[j].sect = sect_sentinel;
      ctx->syms[j].value = (uint64_t)sym_off;
      return;
    }
  }
  // not yet referenced - add as local defined symbol
  symadd(ctx, sname, N_SECT, sect_sentinel, (uint64_t)sym_off);
}

void
macho_emitfin_obj(MachoCtx* ctx) {
  Asmbits* b;
  int idx;
  for (b = GC(emit__stash), idx = 0; b; b = b->link, idx++)
    qbe_arm64_emitmacho_fp_const_cb(idx, b->n, b->size, ctx);
  while ((b = GC(emit__stash))) {
    GC(emit__stash) = b->link;
    qbe_free(b);
  }
}

void
macho_write(MachoCtx* ctx, FILE* f) {
  uint32_t i;
  uint32_t nsects, lit4sect, lit8sect, datasect, bsssect;
  uint32_t textsz, lit4sz, lit8sz, datasz, bsssz, nsyms, ndreloc, ntreloc;
  uint32_t lit4_addr, lit8_addr, data_addr, bss_addr;  // section VM addresses
  uint32_t seg_size, cmds_size;
  uint32_t dataoff, text_foff, lit4_foff, lit8_foff, data_foff;
  uint32_t dreloc_foff, treloc_foff;
  uint32_t symtab_foff, strtab_foff;
  uint32_t cnt[3], ci[3];
  uint32_t ilocal, iextdef, iundef;
  uint32_t nlocal, nextdef, nundef;
  uint32_t* remap = 0;
  MSym* sorted = 0;
  uint32_t cur;
  struct mach_header_64 hdr;
  struct segment_command_64 seg;
  struct section_64 sec;
  struct symtab_command symc;
  struct dysymtab_command dysym;
  struct nlist_64 nl;
  struct relocation_info ri;
  static const uint8_t zeros[8] = {0};

  // Step 0: resolve forward BL fixups
  for (i = 0; i < ctx->nblfixup; i++) {
    uint32_t bsymidx = ctx->blfixup[i].symidx;
    uint32_t boff = ctx->blfixup[i].off;
    const char* bname = ctx->strtab + ctx->syms[bsymidx].strx;
    uint32_t j, found = 0;
    for (j = 0; j < ctx->nsyms; j++) {
      if ((ctx->syms[j].type & 0x0Eu) == N_SECT &&
          strcmp(ctx->strtab + ctx->syms[j].strx, bname) == 0) {
        // Same-unit: patch instruction
        int32_t disp = ((int32_t)ctx->syms[j].value - (int32_t)boff) / 4;
        uint32_t* pinstr = (uint32_t*)(ctx->text + boff);
        *pinstr = 0x94000000u | ((uint32_t)disp & 0x3FFFFFFu);
        found = 1;
        break;
      }
    }
    if (!found) {
      // External: add BRANCH26 reloc
      trelocadd(ctx, (int32_t)boff, bsymidx, ARM64_RELOC_BRANCH26, 2, 1);
    }
  }

  textsz = ctx->textsz;
  lit4sz = ctx->lit4sz;
  lit8sz = ctx->lit8sz;
  datasz = ctx->datasz;
  bsssz = ctx->bsssz;
  nsyms = ctx->nsyms;
  ndreloc = ctx->ndreloc;
  ntreloc = ctx->ntreloc;

  // Step 1: determine section numbering
  // Order: __text, __literal4 (opt), __literal8 (opt), __data (opt), __bss (opt)
  nsects = 1;  // __text is always section 1
  lit4sect = lit8sect = datasect = bsssect = 0;
  if (lit4sz > 0) lit4sect = ++nsects;
  if (lit8sz > 0) lit8sect = ++nsects;
  if (datasz > 0) datasect = ++nsects;
  if (bsssz  > 0) bsssect  = ++nsects;

  // VM addresses: __literal4/8 are in __TEXT and follow __text directly.
  // __data must be aligned to 8 (linker rejects "pointer not aligned" otherwise).
  lit4_addr = align_up(textsz, 4);
  lit8_addr = align_up(lit4_addr + lit4sz, 8);
  data_addr = align_up(lit8_addr + lit8sz, 8);
  bss_addr  = align_up(data_addr + datasz, 8);

  // Step 2: fix up symbol values and sect sentinel fields
  for (i = 0; i < nsyms; i++) {
    if (ctx->syms[i].sect == SECT_LIT4) {
      ctx->syms[i].value += lit4_addr;
      ctx->syms[i].sect = (uint8_t)lit4sect;
    } else if (ctx->syms[i].sect == SECT_LIT8) {
      ctx->syms[i].value += lit8_addr;
      ctx->syms[i].sect = (uint8_t)lit8sect;
    } else if (ctx->syms[i].sect == SECT_DATA) {
      ctx->syms[i].value += data_addr;
      ctx->syms[i].sect = (uint8_t)datasect;
    } else if (ctx->syms[i].sect == SECT_BSS) {
      ctx->syms[i].value += bss_addr;
      ctx->syms[i].sect = (uint8_t)bsssect;
    }
  }

  // Step 3: sort symbols -> locals, extdefs, undefs
  if (nsyms > 0) {
    cnt[0] = cnt[1] = cnt[2] = 0;
    for (i = 0; i < nsyms; i++) {
      cnt[symcat(ctx->syms[i].type)]++;
    }

    ci[0] = 0;
    ci[1] = cnt[0];
    ci[2] = cnt[0] + cnt[1];

    sorted = emalloc(nsyms * sizeof(MSym));
    remap = emalloc(nsyms * sizeof(uint32_t));
    {
      uint32_t pos[3];
      pos[0] = ci[0];
      pos[1] = ci[1];
      pos[2] = ci[2];
      for (i = 0; i < nsyms; i++) {
        int cat = symcat(ctx->syms[i].type);
        uint32_t ni = pos[cat]++;
        sorted[ni] = ctx->syms[i];
        remap[i] = ni;
      }
    }
    memcpy(ctx->syms, sorted, nsyms * sizeof(MSym));
    qbe_free(sorted);

    ilocal = ci[0];
    nlocal = cnt[0];
    iextdef = ci[1];
    nextdef = cnt[1];
    iundef = ci[2];
    nundef = cnt[2];

    // Step 4: patch reloc symnum fields using remap
    for (i = 0; i < ndreloc; i++) {
      uint32_t type = ctx->dreloc[i].info >> 28;
      if (type != ARM64_RELOC_ADDEND) {
        uint32_t oldidx = ctx->dreloc[i].info & 0xFFFFFFu;
        ctx->dreloc[i].info = (ctx->dreloc[i].info & 0xFF000000u) | remap[oldidx];
      }
    }
    for (i = 0; i < ntreloc; i++) {
      uint32_t type = ctx->treloc[i].info >> 28;
      if (type != ARM64_RELOC_ADDEND) {
        uint32_t oldidx = ctx->treloc[i].info & 0xFFFFFFu;
        ctx->treloc[i].info = (ctx->treloc[i].info & 0xFF000000u) | remap[oldidx];
      }
    }
    qbe_free(remap);
  } else {
    ilocal = iextdef = iundef = 0;
    nlocal = nextdef = nundef = 0;
  }

  // Step 5: sort relocs descending by address
  if (ndreloc > 1) {
    qsort(ctx->dreloc, ndreloc, sizeof(MReloc), reloc_cmp);
  }
  if (ntreloc > 1) {
    qsort(ctx->treloc, ntreloc, sizeof(MReloc), reloc_cmp);
  }

  // Step 6: compute file layout
  seg_size = 72 + nsects * 80;
  cmds_size = seg_size + 24 + 80 + 24;
  dataoff = 32 + cmds_size;
  text_foff = dataoff;
  lit4_foff = align_up(text_foff + textsz, 4);
  lit8_foff = align_up(lit4_foff + lit4sz, 8);
  data_foff = align_up(lit8_foff + lit8sz, 8);
  dreloc_foff = align_up(data_foff + datasz, 4);
  treloc_foff = align_up(dreloc_foff + ndreloc * 8, 4);
  symtab_foff = align_up(treloc_foff + ntreloc * 8, 8);
  strtab_foff = symtab_foff + nsyms * 16;

  // Step 7: write mach_header_64
  memset(&hdr, 0, sizeof hdr);
  hdr.magic = MH_MAGIC_64;
  hdr.cputype = CPU_TYPE_ARM64;
  hdr.cpusubtype = 0;
  hdr.filetype = MH_OBJECT;
  hdr.ncmds = 4;
  hdr.sizeofcmds = cmds_size;
  hdr.flags = MH_SUBSECTIONS_VIA_SYMBOLS;
  hdr.reserved = 0;
  fwrite(&hdr, sizeof hdr, 1, f);
  cur = 32;

  // Step 8: write LC_SEGMENT_64
  memset(&seg, 0, sizeof seg);
  seg.cmd = LC_SEGMENT_64;
  seg.cmdsize = seg_size;
  // segname = "" (all zeros)
  seg.vmaddr = 0;
  seg.vmsize = (uint64_t)(bss_addr + bsssz);
  seg.fileoff = (uint64_t)dataoff;
  {
    // filesize covers all file-backed sections (no zerofill)
    uint32_t fend = text_foff + textsz;
    if (lit4sz > 0) fend = lit4_foff + lit4sz;
    if (lit8sz > 0) fend = lit8_foff + lit8sz;
    if (datasz > 0) fend = data_foff + datasz;
    seg.filesize = (uint64_t)(fend - text_foff);
  }
  seg.maxprot = 7;
  seg.initprot = 7;
  seg.nsects = nsects;
  seg.flags = 0;
  fwrite(&seg, sizeof seg, 1, f);
  cur += 72;

  // Step 9: write section_64 headers
  // __TEXT,__text
  memset(&sec, 0, sizeof sec);
  memcpy(sec.sectname, "__text", 6);
  memcpy(sec.segname, "__TEXT", 6);
  sec.addr = 0;
  sec.size = (uint64_t)textsz;
  sec.offset = text_foff;
  sec.align = 2;  // 2^2 = 4 bytes
  sec.reloff = (ntreloc > 0) ? treloc_foff : 0;
  sec.nreloc = ntreloc;
  sec.flags = S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS | S_REGULAR;
  fwrite(&sec, sizeof sec, 1, f);
  cur += 80;

  if (lit4sect) {
    memset(&sec, 0, sizeof sec);
    memcpy(sec.sectname, "__literal4", 10);
    memcpy(sec.segname, "__TEXT", 6);
    sec.addr = (uint64_t)lit4_addr;
    sec.size = (uint64_t)lit4sz;
    sec.offset = lit4_foff;
    sec.align = 2;  // 2^2 = 4 bytes
    sec.reloff = 0;
    sec.nreloc = 0;
    sec.flags = S_4BYTE_LITERALS;
    fwrite(&sec, sizeof sec, 1, f);
    cur += 80;
  }

  if (lit8sect) {
    memset(&sec, 0, sizeof sec);
    memcpy(sec.sectname, "__literal8", 10);
    memcpy(sec.segname, "__TEXT", 6);
    sec.addr = (uint64_t)lit8_addr;
    sec.size = (uint64_t)lit8sz;
    sec.offset = lit8_foff;
    sec.align = 3;  // 2^3 = 8 bytes
    sec.reloff = 0;
    sec.nreloc = 0;
    sec.flags = S_8BYTE_LITERALS;
    fwrite(&sec, sizeof sec, 1, f);
    cur += 80;
  }

  if (datasect) {
    memset(&sec, 0, sizeof sec);
    memcpy(sec.sectname, "__data", 6);
    memcpy(sec.segname, "__DATA", 6);
    sec.addr = (uint64_t)data_addr;
    sec.size = (uint64_t)datasz;
    sec.offset = data_foff;
    sec.align = 3;  // 2^3 = 8 bytes
    sec.reloff = (ndreloc > 0) ? dreloc_foff : 0;
    sec.nreloc = ndreloc;
    sec.flags = S_REGULAR;
    fwrite(&sec, sizeof sec, 1, f);
    cur += 80;
  }

  if (bsssect) {
    memset(&sec, 0, sizeof sec);
    memcpy(sec.sectname, "__bss", 5);
    memcpy(sec.segname, "__DATA", 6);
    sec.addr = (uint64_t)bss_addr;
    sec.size = (uint64_t)bsssz;
    sec.offset = 0;  // S_ZEROFILL: no file data
    sec.align = 3;
    sec.reloff = 0;
    sec.nreloc = 0;
    sec.flags = S_ZEROFILL;
    fwrite(&sec, sizeof sec, 1, f);
    cur += 80;
  }

  // Step 10: write LC_SYMTAB
  memset(&symc, 0, sizeof symc);
  symc.cmd = LC_SYMTAB;
  symc.cmdsize = 24;
  symc.symoff = symtab_foff;
  symc.nsyms = nsyms;
  symc.stroff = strtab_foff;
  symc.strsize = ctx->strtabsz;
  fwrite(&symc, sizeof symc, 1, f);
  cur += 24;

  // Step 11: write LC_DYSYMTAB
  memset(&dysym, 0, sizeof dysym);
  dysym.cmd = LC_DYSYMTAB;
  dysym.cmdsize = 80;
  dysym.ilocalsym = ilocal;
  dysym.nlocalsym = nlocal;
  dysym.iextdefsym = iextdef;
  dysym.nextdefsym = nextdef;
  dysym.iundefsym = iundef;
  dysym.nundefsym = nundef;
  fwrite(&dysym, sizeof dysym, 1, f);
  cur += 80;

  // Step 11b: write LC_BUILD_VERSION
  {
    struct build_version_command bvc;
    memset(&bvc, 0, sizeof bvc);
    bvc.cmd = LC_BUILD_VERSION;
    bvc.cmdsize = 24;
    bvc.platform = PLATFORM_MACOS;
    bvc.minos = (11u << 16);  // macOS 11.0
    bvc.sdk = 0;
    bvc.ntools = 0;
    fwrite(&bvc, sizeof bvc, 1, f);
    cur += 24;
  }

  SQ_ASSERT(cur == dataoff);

  // Step 12: write section data
  if (textsz > 0) {
    fwrite(ctx->text, 1, textsz, f);
    cur += textsz;
  }
  while (cur < lit4_foff) { fwrite(zeros, 1, 1, f); cur++; }
  if (lit4sz > 0) {
    fwrite(ctx->lit4, 1, lit4sz, f);
    cur += lit4sz;
  }
  while (cur < lit8_foff) { fwrite(zeros, 1, 1, f); cur++; }
  if (lit8sz > 0) {
    fwrite(ctx->lit8, 1, lit8sz, f);
    cur += lit8sz;
  }
  while (cur < data_foff) { fwrite(zeros, 1, 1, f); cur++; }
  if (datasz > 0) {
    fwrite(ctx->data, 1, datasz, f);
    cur += datasz;
  }

  // Step 13: write data relocation records
  while (cur < dreloc_foff) {
    fwrite(zeros, 1, 1, f);
    cur++;
  }
  for (i = 0; i < ndreloc; i++) {
    ri.r_address = ctx->dreloc[i].addr;
    ri.r_info = ctx->dreloc[i].info;
    fwrite(&ri, sizeof ri, 1, f);
    cur += 8;
  }

  // Step 13b: write text relocation records
  while (cur < treloc_foff) {
    fwrite(zeros, 1, 1, f);
    cur++;
  }
  for (i = 0; i < ntreloc; i++) {
    ri.r_address = ctx->treloc[i].addr;
    ri.r_info = ctx->treloc[i].info;
    fwrite(&ri, sizeof ri, 1, f);
    cur += 8;
  }

  // padding before symtab
  while (cur < symtab_foff) {
    fwrite(zeros, 1, 1, f);
    cur++;
  }

  // Step 14: write symbol table
  for (i = 0; i < nsyms; i++) {
    nl.n_strx = ctx->syms[i].strx;
    nl.n_type = ctx->syms[i].type;
    nl.n_sect = ctx->syms[i].sect;
    nl.n_desc = ctx->syms[i].desc;
    nl.n_value = ctx->syms[i].value;
    fwrite(&nl, sizeof nl, 1, f);
  }

  // Step 15: write string table
  fwrite(ctx->strtab, 1, ctx->strtabsz, f);
}
#undef G
/*** END FILE: arm64/emitmacho.c ***/
/*** START FILE: arm64/isel.c ***/
/* skipping all.h */

enum Imm {
	Iother,
	Iplo12,
	Iphi12,
	Iplo24,
	Inlo12,
	Inhi12,
	Inlo24
};

static enum Imm
qbe_arm64_isel_imm(Con *c, int k, int64_t *pn)
{
	int64_t n;
	int i;

	if (c->type != CBits)
		return Iother;
	n = c->bits.i;
	if (k == Kw)
		n = (int32_t)n;
	i = Iplo12;
	if (n < 0) {
		i = Inlo12;
		n = -(uint64_t)n;
	}
	*pn = n;
	if ((n & 0x000fff) == n)
		return i;
	if ((n & 0xfff000) == n)
		return i + 1;
	if ((n & 0xffffff) == n)
		return i + 2;
	return Iother;
}

int
arm64_logimm(uint64_t x, int k)
{
	uint64_t n;

	if (k == Kw)
		x = (x & 0xffffffff) | x << 32;
	if (x & 1)
		x = ~x;
	if (x == 0)
		return 0;
	if (x == 0xaaaaaaaaaaaaaaaa)
		return 1;
	n = x & 0xf;
	if (0x1111111111111111 * n == x)
		goto Check;
	n = x & 0xff;
	if (0x0101010101010101 * n == x)
		goto Check;
	n = x & 0xffff;
	if (0x0001000100010001 * n == x)
		goto Check;
	n = x & 0xffffffff;
	if (0x0000000100000001 * n == x)
		goto Check;
	n = x;
Check:
	return (n & (n + (n & -n))) == 0;
}

static void
qbe_arm64_isel_fixarg(Ref *pr, int k, int phi, Fn *fn)
{
	char buf[32];
	Con *c, cc;
	Ref r0, r1, r2, r3;
	int s, n;

	r0 = *pr;
	switch (rtype(r0)) {
	case RCon:
		c = &fn->con[r0.val];
		if (GC(T).apple
		&& c->type == CAddr
		&& c->sym.type == SThr) {
			r1 = newtmp("isel", Kl, fn);
			*pr = r1;
			if (c->bits.i) {
				r2 = newtmp("isel", Kl, fn);
				cc = (Con){.type = CBits};
				cc.bits.i = c->bits.i;
				r3 = newcon(&cc, fn);
				emit(Oadd, Kl, r1, r2, r3);
				r1 = r2;
			}
			emit(Ocopy, Kl, r1, TMP(QBE_ARM64_R0), NULL_R);
			r1 = newtmp("isel", Kl, fn);
			r2 = newtmp("isel", Kl, fn);
			emit(Ocall, 0, NULL_R, r1, CALL(33));
			emit(Ocopy, Kl, TMP(QBE_ARM64_R0), r2, NULL_R);
			emit(Oload, Kl, r1, r2, NULL_R);
			cc = *c;
			cc.bits.i = 0;
			r3 = newcon(&cc, fn);
			emit(Ocopy, Kl, r2, r3, NULL_R);
			break;
		}
		if (KBASE(k) == 0 && phi)
			return;
		r1 = newtmp("isel", k, fn);
		if (KBASE(k) == 0) {
			emit(Ocopy, k, r1, r0, NULL_R);
		} else {
			n = stashbits(c->bits.i, KWIDE(k) ? 8 : 4);
			vgrow(&fn->con, ++fn->ncon);
			c = &fn->con[fn->ncon-1];
			snprintf(buf, sizeof(buf), "\"%sfp%d\"", GC(T).asloc, n);
			*c = (Con){.type = CAddr};
			c->sym.id = intern(buf);
			r2 = newtmp("isel", Kl, fn);
			emit(Oload, k, r1, r2, NULL_R);
			emit(Ocopy, Kl, r2, CON(c-fn->con), NULL_R);
		}
		*pr = r1;
		break;
	case RTmp:
		s = fn->tmp[r0.val].slot;
		if (s == -1)
			break;
		r1 = newtmp("isel", Kl, fn);
		emit(Oaddr, Kl, r1, SLOT(s), NULL_R);
		*pr = r1;
		break;
	}
}

static int
qbe_arm64_isel_selcmp(Ref arg[2], int k, Fn *fn)
{
	Ref r, *iarg;
	Con *c;
	int swap, cmp, fix;
	int64_t n;

	if (KBASE(k) == 1) {
		emit(Oafcmp, k, NULL_R, arg[0], arg[1]);
		iarg = GC(curi)->arg;
		qbe_arm64_isel_fixarg(&iarg[0], k, 0, fn);
		qbe_arm64_isel_fixarg(&iarg[1], k, 0, fn);
		return 0;
	}
	swap = rtype(arg[0]) == RCon;
	if (swap) {
		r = arg[1];
		arg[1] = arg[0];
		arg[0] = r;
	}
	fix = 1;
	cmp = Oacmp;
	r = arg[1];
	if (rtype(r) == RCon) {
		c = &fn->con[r.val];
		switch (qbe_arm64_isel_imm(c, k, &n)) {
		default:
			break;
		case Iplo12:
		case Iphi12:
			fix = 0;
			break;
		case Inlo12:
		case Inhi12:
			cmp = Oacmn;
			r = getcon(n, fn);
			fix = 0;
			break;
		}
	}
	emit(cmp, k, NULL_R, arg[0], r);
	iarg = GC(curi)->arg;
	qbe_arm64_isel_fixarg(&iarg[0], k, 0, fn);
	if (fix)
		qbe_arm64_isel_fixarg(&iarg[1], k, 0, fn);
	return swap;
}

static int
qbe_arm64_isel_callable(Ref r, Fn *fn)
{
	Con *c;

	if (rtype(r) == RTmp)
		return 1;
	if (rtype(r) == RCon) {
		c = &fn->con[r.val];
		if (c->type == CAddr)
		if (c->bits.i == 0)
			return 1;
	}
	return 0;
}

static void
qbe_arm64_isel_sel(Ins i, Fn *fn)
{
	Ref *iarg;
	Ins *i0;
	int ck, cc;

	if (INRANGE(i.op, Oalloc, Oalloc1)) {
		i0 = GC(curi) - 1;
		salloc(i.to, i.arg[0], fn);
		qbe_arm64_isel_fixarg(&i0->arg[0], Kl, 0, fn);
		return;
	}
	if (iscmp(i.op, &ck, &cc)) {
		emit(Oflag, i.cls, i.to, NULL_R, NULL_R);
		i0 = GC(curi);
		if (qbe_arm64_isel_selcmp(i.arg, ck, fn))
			i0->op += cmpop(cc);
		else
			i0->op += cc;
		return;
	}
	if (i.op == Ocall)
	if (qbe_arm64_isel_callable(i.arg[0], fn)) {
		emiti(i);
		return;
	}
	if (i.op != Onop) {
		emiti(i);
		iarg = GC(curi)->arg; /* qbe_arm64_isel_fixarg() can change curi */
		qbe_arm64_isel_fixarg(&iarg[0], argcls(&i, 0), 0, fn);
		qbe_arm64_isel_fixarg(&iarg[1], argcls(&i, 1), 0, fn);
	}
}

static void
qbe_arm64_isel_seljmp(Blk *b, Fn *fn)
{
	Ref r;
	Ins *i, *ir;
	int ck, cc, use;

	if (b->jmp.type == Jret0
	|| b->jmp.type == Jjmp
	|| b->jmp.type == Jhlt)
		return;
	SQ_ASSERT(b->jmp.type == Jjnz);
	r = b->jmp.arg;
	use = -1;
	b->jmp.arg = NULL_R;
	ir = 0;
	i = &b->ins[b->nins];
	while (i > b->ins)
		if (req((--i)->to, r)) {
			use = fn->tmp[r.val].nuse;
			ir = i;
			break;
		}
	if (ir && use == 1
	&& iscmp(ir->op, &ck, &cc)) {
		if (qbe_arm64_isel_selcmp(ir->arg, ck, fn))
			cc = cmpop(cc);
		b->jmp.type = Jjf + cc;
		*ir = (Ins){.op = Onop};
	}
	else {
		qbe_arm64_isel_selcmp((Ref[]){r, CON_Z}, Kw, fn);
		b->jmp.type = Jjfine;
	}
}

void
arm64_isel(Fn *fn)
{
	Blk *b, **sb;
	Ins *i;
	Phi *p;
	uint n, al;
	int64_t sz;

	/* assign slots to fast allocs */
	b = fn->start;
	/* specific to NAlign == 3 */ /* or change n=4 and sz /= 4 below */
	for (al=Oalloc, n=4; al<=Oalloc1; al++, n*=2)
		for (i=b->ins; i<&b->ins[b->nins]; i++)
			if (i->op == al) {
				if (rtype(i->arg[0]) != RCon)
					break;
				sz = fn->con[i->arg[0].val].bits.i;
				if (sz < 0 || sz >= INT_MAX-15)
					err("invalid alloc size %"PRId64, sz);
				sz = (sz + n-1) & -n;
				sz /= 4;
				fn->tmp[i->to.val].slot = fn->slot;
				fn->slot += sz;
				*i = (Ins){.op = Onop};
			}

	for (b=fn->start; b; b=b->link) {
		GC(curi) = &GC(insb)[NIns];
		for (sb=(Blk*[3]){b->s1, b->s2, 0}; *sb; sb++)
			for (p=(*sb)->phi; p; p=p->link) {
				for (n=0; p->blk[n] != b; n++)
					SQ_ASSERT(n+1 < p->narg);
				qbe_arm64_isel_fixarg(&p->arg[n], p->cls, 1, fn);
			}
		qbe_arm64_isel_seljmp(b, fn);
		for (i=&b->ins[b->nins]; i!=b->ins;)
			qbe_arm64_isel_sel(*--i, fn);
		idup(b, GC(curi), &GC(insb)[NIns]-GC(curi));
	}

	if (GC(debug)['I']) {
		fprintf(stderr, "\n> After instruction selection:\n");
		printfn(fn, stderr);
	}
}
#undef G
/*** END FILE: arm64/isel.c ***/
/*** START FILE: arm64/targ.c ***/
/* skipping all.h */

static int arm64_rsave[] = {
	QBE_ARM64_R0,  QBE_ARM64_R1,  QBE_ARM64_R2,  QBE_ARM64_R3,  QBE_ARM64_R4,  QBE_ARM64_R5,  QBE_ARM64_R6,  QBE_ARM64_R7,
	QBE_ARM64_R8,  QBE_ARM64_R9,  QBE_ARM64_R10, QBE_ARM64_R11, QBE_ARM64_R12, QBE_ARM64_R13, QBE_ARM64_R14, QBE_ARM64_R15,
	QBE_ARM64_IP0, QBE_ARM64_IP1, QBE_ARM64_R18, QBE_ARM64_LR,
	QBE_ARM64_V0,  QBE_ARM64_V1,  QBE_ARM64_V2,  QBE_ARM64_V3,  QBE_ARM64_V4,  QBE_ARM64_V5,  QBE_ARM64_V6,  QBE_ARM64_V7,
	QBE_ARM64_V16, QBE_ARM64_V17, QBE_ARM64_V18, QBE_ARM64_V19, QBE_ARM64_V20, QBE_ARM64_V21, QBE_ARM64_V22, QBE_ARM64_V23,
	QBE_ARM64_V24, QBE_ARM64_V25, QBE_ARM64_V26, QBE_ARM64_V27, QBE_ARM64_V28, QBE_ARM64_V29, QBE_ARM64_V30,
	-1
};
static int arm64_rclob[] = {
	QBE_ARM64_R19, QBE_ARM64_R20, QBE_ARM64_R21, QBE_ARM64_R22, QBE_ARM64_R23, QBE_ARM64_R24, QBE_ARM64_R25, QBE_ARM64_R26,
	QBE_ARM64_R27, QBE_ARM64_R28,
	QBE_ARM64_V8,  QBE_ARM64_V9,  QBE_ARM64_V10, QBE_ARM64_V11, QBE_ARM64_V12, QBE_ARM64_V13, QBE_ARM64_V14, QBE_ARM64_V15,
	-1
};

#define RGLOB (BIT(QBE_ARM64_FP) | BIT(QBE_ARM64_SP) | BIT(QBE_ARM64_IP1) | BIT(QBE_ARM64_R18))

static int
arm64_memargs(int op)
{
	(void)op;
	return 0;
}

#define ARM64_COMMON \
	.gpr0 = QBE_ARM64_R0, \
	.ngpr = QBE_ARM64_NGPR, \
	.fpr0 = QBE_ARM64_V0, \
	.nfpr = QBE_ARM64_NFPR, \
	.rglob = RGLOB, \
	.nrglob = 4, \
	.rsave = arm64_rsave, \
	.nrsave = {QBE_ARM64_NGPS, QBE_ARM64_NFPS}, \
	.retregs = arm64_retregs, \
	.argregs = arm64_argregs, \
	.memargs = arm64_memargs, \
	.isel = arm64_isel, \
	.abi1 = arm64_abi, \
	.emitfn = arm64_emitfn, \
	.cansel = 0, \

static Target T_arm64 = {
	.name = "arm64",
	.abi0 = elimsb,
	.emitfin = elf_emitfin,
	.asloc = ".L",
	ARM64_COMMON
};

static Target T_arm64_apple = {
	.name = "arm64_apple",
	.apple = 1,
	.abi0 = apple_extsb,
	.emitfin = macho_emitfin,
	.asloc = "L",
	.assym = "_",
	ARM64_COMMON
};

MAKESURE(globals_are_not_arguments,
	(RGLOB & (BIT(QBE_ARM64_R8+1) - 1)) == 0
);
MAKESURE(arrays_size_ok,
	sizeof arm64_rsave == (QBE_ARM64_NGPS+QBE_ARM64_NFPS+1) * sizeof(int) &&
	sizeof arm64_rclob == (QBE_ARM64_NCLR+1) * sizeof(int)
);
#undef G
/*** END FILE: arm64/targ.c ***/
/*** START FILE: rv64/abi.c ***/
/* skipping all.h */

/* the risc-v lp64d abi */

typedef struct QBE_RV64_ABI_Class QBE_RV64_ABI_Class;
typedef struct QBE_RV64_ABI_Insl QBE_RV64_ABI_Insl;
typedef struct QBE_RV64_ABI_Params QBE_RV64_ABI_Params;

enum {
	QBE_RV64_ABI_Cptr  = 1, /* replaced by a pointer */
	QBE_RV64_ABI_Cstk1 = 2, /* pass first XLEN on the stack */
	QBE_RV64_ABI_Cstk2 = 4, /* pass second XLEN on the stack */
	QBE_RV64_ABI_Cstk = QBE_RV64_ABI_Cstk1 | QBE_RV64_ABI_Cstk2,
	QBE_RV64_ABI_Cfpint = 8, /* float passed like integer */
};

struct QBE_RV64_ABI_Class {
	char class;
	Typ *type;
	int reg[2];
	int cls[2];
	int off[2];
	char ngp; /* only valid after qbe_rv64_abi_typclass() */
	char nfp; /* ditto */
	char nreg;
};

struct QBE_RV64_ABI_Insl {
	Ins i;
	QBE_RV64_ABI_Insl *link;
};

struct QBE_RV64_ABI_Params {
	int ngp;
	int nfp;
	int stk; /* stack offset for varargs */
};

static int qbe_rv64_abi_gpreg[10] = {QBE_RV64_A0, QBE_RV64_A1, QBE_RV64_A2, QBE_RV64_A3, QBE_RV64_A4, QBE_RV64_A5, QBE_RV64_A6, QBE_RV64_A7};
static int qbe_rv64_abi_fpreg[10] = {QBE_RV64_FA0, QBE_RV64_FA1, QBE_RV64_FA2, QBE_RV64_FA3, QBE_RV64_FA4, QBE_RV64_FA5, QBE_RV64_FA6, QBE_RV64_FA7};

/* layout of call's second argument (RCall)
 *
 *  29   12    8    4  2  0
 *  |0.00|x|xxxx|xxxx|xx|xx|                  range
 *        |   |    |  |  ` gp regs returned (0..2)
 *        |   |    |  ` fp regs returned    (0..2)
 *        |   |    ` gp regs passed         (0..8)
 *        |    ` fp regs passed             (0..8)
 *        ` env pointer passed in t5        (0..1)
 */

bits
rv64_retregs(Ref r, int p[2])
{
	bits b;
	int ngp, nfp;

	SQ_ASSERT(rtype(r) == RCall);
	ngp = r.val & 3;
	nfp = (r.val >> 2) & 3;
	if (p) {
		p[0] = ngp;
		p[1] = nfp;
	}
	b = 0;
	while (ngp--)
		b |= BIT(QBE_RV64_A0+ngp);
	while (nfp--)
		b |= BIT(QBE_RV64_FA0+nfp);
	return b;
}

bits
rv64_argregs(Ref r, int p[2])
{
	bits b;
	int ngp, nfp, t5;

	SQ_ASSERT(rtype(r) == RCall);
	ngp = (r.val >> 4) & 15;
	nfp = (r.val >> 8) & 15;
	t5 = (r.val >> 12) & 1;
	if (p) {
		p[0] = ngp + t5;
		p[1] = nfp;
	}
	b = 0;
	while (ngp--)
		b |= BIT(QBE_RV64_A0+ngp);
	while (nfp--)
		b |= BIT(QBE_RV64_FA0+nfp);
	return b | ((bits)t5 << QBE_RV64_T5);
}

static int
qbe_rv64_abi_fpstruct(Typ *t, int off, QBE_RV64_ABI_Class *c)
{
	Field *f;
	int n;

	if (t->isunion)
		return -1;

	for (f=*t->fields; f->type != FEnd; f++)
		if (f->type == FPad)
			off += f->len;
		else if (f->type == FTyp) {
			if (qbe_rv64_abi_fpstruct(&GC(typ)[f->len], off, c) == -1)
				return -1;
		}
		else {
			n = c->nfp + c->ngp;
			if (n == 2)
				return -1;
			switch (f->type) {
			default: die("unreachable");
			case Fb:
			case Fh:
			case Fw: c->cls[n] = Kw; c->ngp++; break;
			case Fl: c->cls[n] = Kl; c->ngp++; break;
			case Fs: c->cls[n] = Ks; c->nfp++; break;
			case Fd: c->cls[n] = Kd; c->nfp++; break;
			}
			c->off[n] = off;
			off += f->len;
		}

	return c->nfp;
}

static void
qbe_rv64_abi_typclass(QBE_RV64_ABI_Class *c, Typ *t, int fpabi, int *gp, int *fp)
{
	uint n;
	int i;

	c->type = t;
	c->class = 0;
	c->ngp = 0;
	c->nfp = 0;

	if (t->align > 4)
		err("alignments larger than 16 are not supported");

	if (t->isdark || t->size > 16 || t->size == 0) {
		/* large structs are replaced by a
		 * pointer to some caller-allocated
		 * memory
		 */
		c->class |= QBE_RV64_ABI_Cptr;
		*c->cls = Kl;
		*c->off = 0;
		c->ngp = 1;
	}
	else if (!fpabi || qbe_rv64_abi_fpstruct(t, 0, c) <= 0) {
		for (n=0; 8*n<t->size; n++) {
			c->cls[n] = Kl;
			c->off[n] = 8*n;
		}
		c->nfp = 0;
		c->ngp = n;
	}

	c->nreg = c->nfp + c->ngp;
	for (i=0; i<c->nreg; i++)
		if (KBASE(c->cls[i]) == 0)
			c->reg[i] = *gp++;
		else
			c->reg[i] = *fp++;
}

static void
qbe_rv64_abi_sttmps(Ref tmp[], int ntmp, QBE_RV64_ABI_Class *c, Ref mem, Fn *fn)
{
	static int st[] = {
		[Kw] = Ostorew, [Kl] = Ostorel,
		[Ks] = Ostores, [Kd] = Ostored
	};
	int i;
	Ref r;

	SQ_ASSERT(ntmp > 0);
	SQ_ASSERT(ntmp <= 2);
	for (i=0; i<ntmp; i++) {
		tmp[i] = newtmp("abi", c->cls[i], fn);
		r = newtmp("abi", Kl, fn);
		emit(st[c->cls[i]], 0, NULL_R, tmp[i], r);
		emit(Oadd, Kl, r, mem, getcon(c->off[i], fn));
	}
}

static void
qbe_rv64_abi_ldregs(QBE_RV64_ABI_Class *c, Ref mem, Fn *fn)
{
	int i;
	Ref r;

	for (i=0; i<c->nreg; i++) {
		r = newtmp("abi", Kl, fn);
		emit(Oload, c->cls[i], TMP(c->reg[i]), r, NULL_R);
		emit(Oadd, Kl, r, mem, getcon(c->off[i], fn));
	}
}

static void
qbe_rv64_abi_selret(Blk *b, Fn *fn)
{
	int j, k, cty;
	Ref r;
	QBE_RV64_ABI_Class cr;

	j = b->jmp.type;

	if (!isret(j) || j == Jret0)
		return;

	r = b->jmp.arg;
	b->jmp.type = Jret0;

	if (j == Jretc) {
		qbe_rv64_abi_typclass(&cr, &GC(typ)[fn->retty], 1, qbe_rv64_abi_gpreg, qbe_rv64_abi_fpreg);
		ret_on_err();
		if (cr.class & QBE_RV64_ABI_Cptr) {
			SQ_ASSERT(rtype(fn->retr) == RTmp);
			emit(Oblit1, 0, NULL_R, INT(cr.type->size), NULL_R);
			emit(Oblit0, 0, NULL_R, r, fn->retr);
			cty = 0;
		} else {
			qbe_rv64_abi_ldregs(&cr, r, fn);
			cty = (cr.nfp << 2) | cr.ngp;
		}
	} else {
		k = j - Jretw;
		if (KBASE(k) == 0) {
			emit(Ocopy, k, TMP(QBE_RV64_A0), r, NULL_R);
			cty = 1;
		} else {
			emit(Ocopy, k, TMP(QBE_RV64_FA0), r, NULL_R);
			cty = 1 << 2;
		}
	}

	b->jmp.arg = CALL(cty);
}

static int
qbe_rv64_abi_argsclass(Ins *i0, Ins *i1, QBE_RV64_ABI_Class *carg, int retptr)
{
	int ngp, nfp, *gp, *fp, vararg, envc;
	QBE_RV64_ABI_Class *c;
	Typ *t;
	Ins *i;

	gp = qbe_rv64_abi_gpreg;
	fp = qbe_rv64_abi_fpreg;
	ngp = 8;
	nfp = 8;
	vararg = 0;
	envc = 0;
	if (retptr) {
		gp++;
		ngp--;
	}
	for (i=i0, c=carg; i<i1; i++, c++) {
		switch (i->op) {
		case Opar:
		case Oarg:
			*c->cls = i->cls;
			if (!vararg && KBASE(i->cls) == 1 && nfp > 0) {
				nfp--;
				*c->reg = *fp++;
			} else if (ngp > 0) {
				if (KBASE(i->cls) == 1)
					c->class |= QBE_RV64_ABI_Cfpint;
				ngp--;
				*c->reg = *gp++;
			} else
				c->class |= QBE_RV64_ABI_Cstk1;
			break;
		case Oargv:
			vararg = 1;
			break;
		case Oparc:
		case Oargc:
			t = &GC(typ)[i->arg[0].val];
			qbe_rv64_abi_typclass(c, t, 1, gp, fp);
			ret_on_err_i();
			if (c->nfp > 0)
			if (c->nfp >= nfp || c->ngp >= ngp) {
				qbe_rv64_abi_typclass(c, t, 0, gp, fp);
				ret_on_err_i();
			}
			SQ_ASSERT(c->nfp <= nfp);
			if (c->ngp <= ngp) {
				ngp -= c->ngp;
				nfp -= c->nfp;
				gp += c->ngp;
				fp += c->nfp;
			} else if (ngp > 0) {
				SQ_ASSERT(c->ngp == 2);
				SQ_ASSERT(c->class == 0);
				c->class |= QBE_RV64_ABI_Cstk2;
				c->nreg = 1;
				ngp--;
				gp++;
			} else {
				c->class |= QBE_RV64_ABI_Cstk1;
				if (c->nreg > 1)
					c->class |= QBE_RV64_ABI_Cstk2;
				c->nreg = 0;
			}
			break;
		case Opare:
		case Oarge:
			*c->reg = QBE_RV64_T5;
			*c->cls = Kl;
			envc = 1;
			break;
		}
	}
	return envc << 12 | (gp-qbe_rv64_abi_gpreg) << 4 | (fp-qbe_rv64_abi_fpreg) << 8;
}

static void
qbe_rv64_abi_stkblob(Ref r, Typ *t, Fn *fn, QBE_RV64_ABI_Insl **ilp)
{
	QBE_RV64_ABI_Insl *il;
	int al;
	uint64_t sz;

	il = alloc(sizeof *il);
	al = t->align - 2; /* specific to NAlign == 3 */
	if (al < 0)
		al = 0;
	sz = (t->size + 7) & ~7;
	il->i = (Ins){Oalloc+al, Kl, r, {getcon(sz, fn)}};
	il->link = *ilp;
	*ilp = il;
}

static void
qbe_rv64_abi_selcall(Fn *fn, Ins *i0, Ins *i1, QBE_RV64_ABI_Insl **ilp)
{
	Ins *i;
	QBE_RV64_ABI_Class *ca, *c, cr;
	int j, k, cty;
	uint64_t stk, off;
	Ref r, r1, r2, tmp[2];

	ca = alloc((i1-i0) * sizeof ca[0]);
	cr.class = 0;

	if (!req(i1->arg[1], NULL_R)) {
		qbe_rv64_abi_typclass(&cr, &GC(typ)[i1->arg[1].val], 1, qbe_rv64_abi_gpreg, qbe_rv64_abi_fpreg);
		ret_on_err();
	}

	cty = qbe_rv64_abi_argsclass(i0, i1, ca, cr.class & QBE_RV64_ABI_Cptr);
	stk = 0;
	for (i=i0, c=ca; i<i1; i++, c++) {
		if (i->op == Oargv)
			continue;
		if (c->class & QBE_RV64_ABI_Cptr) {
			i->arg[0] = newtmp("abi", Kl, fn);
			qbe_rv64_abi_stkblob(i->arg[0], c->type, fn, ilp);
			i->op = Oarg;
		}
		if (c->class & QBE_RV64_ABI_Cstk1)
			stk += 8;
		if (c->class & QBE_RV64_ABI_Cstk2)
			stk += 8;
	}
	stk += stk & 15;
	if (stk)
		emit(Osalloc, Kl, NULL_R, getcon(-stk, fn), NULL_R);

	if (!req(i1->arg[1], NULL_R)) {
		qbe_rv64_abi_stkblob(i1->to, cr.type, fn, ilp);
		cty |= (cr.nfp << 2) | cr.ngp;
		if (cr.class & QBE_RV64_ABI_Cptr)
			/* spill & rega expect calls to be
			 * followed by copies from regs,
			 * so we emit a dummy
			 */
			emit(Ocopy, Kw, NULL_R, TMP(QBE_RV64_A0), NULL_R);
		else {
			qbe_rv64_abi_sttmps(tmp, cr.nreg, &cr, i1->to, fn);
			for (j=0; j<cr.nreg; j++) {
				r = TMP(cr.reg[j]);
				emit(Ocopy, cr.cls[j], tmp[j], r, NULL_R);
			}
		}
	} else if (KBASE(i1->cls) == 0) {
		emit(Ocopy, i1->cls, i1->to, TMP(QBE_RV64_A0), NULL_R);
		cty |= 1;
	} else {
		emit(Ocopy, i1->cls, i1->to, TMP(QBE_RV64_FA0), NULL_R);
		cty |= 1 << 2;
	}

	emit(Ocall, 0, NULL_R, i1->arg[0], CALL(cty));

	if (cr.class & QBE_RV64_ABI_Cptr)
		/* struct return argument */
		emit(Ocopy, Kl, TMP(QBE_RV64_A0), i1->to, NULL_R);

	/* move arguments into registers */
	for (i=i0, c=ca; i<i1; i++, c++) {
		if (i->op == Oargv || c->class & QBE_RV64_ABI_Cstk1)
			continue;
		if (i->op == Oargc) {
			qbe_rv64_abi_ldregs(c, i->arg[1], fn);
		} else if (c->class & QBE_RV64_ABI_Cfpint) {
			k = KWIDE(*c->cls) ? Kl : Kw;
			r = newtmp("abi", k, fn);
			emit(Ocopy, k, TMP(*c->reg), r, NULL_R);
			*c->reg = r.val;
		} else {
			emit(Ocopy, *c->cls, TMP(*c->reg), i->arg[0], NULL_R);
		}
	}

	for (i=i0, c=ca; i<i1; i++, c++) {
		if (c->class & QBE_RV64_ABI_Cfpint) {
			k = KWIDE(*c->cls) ? Kl : Kw;
			emit(Ocast, k, TMP(*c->reg), i->arg[0], NULL_R);
		}
		if (c->class & QBE_RV64_ABI_Cptr) {
			emit(Oblit1, 0, NULL_R, INT(c->type->size), NULL_R);
			emit(Oblit0, 0, NULL_R, i->arg[1], i->arg[0]);
		}
	}

	if (!stk)
		return;

	/* populate the stack */
	off = 0;
	r = newtmp("abi", Kl, fn);
	for (i=i0, c=ca; i<i1; i++, c++) {
		if (i->op == Oargv || !(c->class & QBE_RV64_ABI_Cstk))
			continue;
		if (i->op == Oarg) {
			r1 = newtmp("abi", Kl, fn);
			emit(Ostorew+i->cls, Kw, NULL_R, i->arg[0], r1);
			if (i->cls == Kw) {
				/* TODO: we only need this sign
				 * extension for l temps passed
				 * as w arguments
				 * (see rv64/isel.c:fixarg)
				 */
				GC(curi)->op = Ostorel;
				GC(curi)->arg[0] = newtmp("abi", Kl, fn);
				emit(Oextsw, Kl, GC(curi)->arg[0], i->arg[0], NULL_R);
			}
			emit(Oadd, Kl, r1, r, getcon(off, fn));
			off += 8;
		}
		if (i->op == Oargc) {
			if (c->class & QBE_RV64_ABI_Cstk1) {
				r1 = newtmp("abi", Kl, fn);
				r2 = newtmp("abi", Kl, fn);
				emit(Ostorel, 0, NULL_R, r2, r1);
				emit(Oadd, Kl, r1, r, getcon(off, fn));
				emit(Oload, Kl, r2, i->arg[1], NULL_R);
				off += 8;
			}
			if (c->class & QBE_RV64_ABI_Cstk2) {
				r1 = newtmp("abi", Kl, fn);
				r2 = newtmp("abi", Kl, fn);
				emit(Ostorel, 0, NULL_R, r2, r1);
				emit(Oadd, Kl, r1, r, getcon(off, fn));
				r1 = newtmp("abi", Kl, fn);
				emit(Oload, Kl, r2, r1, NULL_R);
				emit(Oadd, Kl, r1, i->arg[1], getcon(8, fn));
				off += 8;
			}
		}
	}
	emit(Osalloc, Kl, r, getcon(stk, fn), NULL_R);
}

static QBE_RV64_ABI_Params
qbe_rv64_abi_selpar(Fn *fn, Ins *i0, Ins *i1)
{
	QBE_RV64_ABI_Class *ca, *c, cr;
	QBE_RV64_ABI_Insl *il;
	Ins *i;
	int j, k, s, cty, nt;
	Ref r, tmp[17], *t;

	ca = alloc((i1-i0) * sizeof ca[0]);
	cr.class = 0;
	GC(curi) = &GC(insb)[NIns];

	if (fn->retty >= 0) {
		qbe_rv64_abi_typclass(&cr, &GC(typ)[fn->retty], 1, qbe_rv64_abi_gpreg, qbe_rv64_abi_fpreg);
		if (GC(in_error)) { return (QBE_RV64_ABI_Params){0}; }
		if (cr.class & QBE_RV64_ABI_Cptr) {
			fn->retr = newtmp("abi", Kl, fn);
			emit(Ocopy, Kl, fn->retr, TMP(QBE_RV64_A0), NULL_R);
		}
	}

	cty = qbe_rv64_abi_argsclass(i0, i1, ca, cr.class & QBE_RV64_ABI_Cptr);
	fn->reg = rv64_argregs(CALL(cty), 0);

	il = 0;
	t = tmp;
	for (i=i0, c=ca; i<i1; i++, c++) {
		if (c->class & QBE_RV64_ABI_Cfpint) {
			r = i->to;
			k = *c->cls;
			*c->cls = KWIDE(k) ? Kl : Kw;
			i->to = newtmp("abi", k, fn);
			emit(Ocast, k, r, i->to, NULL_R);
		}
		if (i->op == Oparc)
		if (!(c->class & QBE_RV64_ABI_Cptr))
		if (c->nreg != 0) {
			nt = c->nreg;
			if (c->class & QBE_RV64_ABI_Cstk2) {
				c->cls[1] = Kl;
				c->off[1] = 8;
				SQ_ASSERT(nt == 1);
				nt = 2;
			}
			qbe_rv64_abi_sttmps(t, nt, c, i->to, fn);
			qbe_rv64_abi_stkblob(i->to, c->type, fn, &il);
			t += nt;
		}
	}
	for (; il; il=il->link)
		emiti(il->i);

	t = tmp;
	s = 2 + 8*fn->vararg;
	for (i=i0, c=ca; i<i1; i++, c++)
		if (i->op == Oparc && !(c->class & QBE_RV64_ABI_Cptr)) {
			if (c->nreg == 0) {
				fn->tmp[i->to.val].slot = -s;
				s += (c->class & QBE_RV64_ABI_Cstk2) ? 2 : 1;
				continue;
			}
			for (j=0; j<c->nreg; j++) {
				r = TMP(c->reg[j]);
				emit(Ocopy, c->cls[j], *t++, r, NULL_R);
			}
			if (c->class & QBE_RV64_ABI_Cstk2) {
				emit(Oload, Kl, *t, SLOT(-s), NULL_R);
				t++, s++;
			}
		} else if (c->class & QBE_RV64_ABI_Cstk1) {
			emit(Oload, *c->cls, i->to, SLOT(-s), NULL_R);
			s++;
		} else {
			emit(Ocopy, *c->cls, i->to, TMP(*c->reg), NULL_R);
		}

	return (QBE_RV64_ABI_Params){
		.stk = s,
		.ngp = (cty >> 4) & 15,
		.nfp = (cty >> 8) & 15,
	};
}

static void
qbe_rv64_abi_selvaarg(Fn *fn, Ins *i)
{
	Ref loc, newloc;

	loc = newtmp("abi", Kl, fn);
	newloc = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, NULL_R, newloc, i->arg[0]);
	emit(Oadd, Kl, newloc, loc, getcon(8, fn));
	emit(Oload, i->cls, i->to, loc, NULL_R);
	emit(Oload, Kl, loc, i->arg[0], NULL_R);
}

static void
qbe_rv64_abi_selvastart(Fn *fn, QBE_RV64_ABI_Params p, Ref ap)
{
	Ref rsave;
	int s;

	rsave = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, NULL_R, rsave, ap);
	s = p.stk > 2 + 8 * fn->vararg ? p.stk : 2 + p.ngp;
	emit(Oaddr, Kl, rsave, SLOT(-s), NULL_R);
}

void
rv64_abi(Fn *fn)
{
	Blk *b;
	Ins *i, *i0;
	QBE_RV64_ABI_Insl *il;
	int n0, n1, ioff;
	QBE_RV64_ABI_Params p;

	for (b=fn->start; b; b=b->link)
		b->visit = 0;

	/* lower parameters */
	for (b=fn->start, i=b->ins; i<&b->ins[b->nins]; i++)
		if (!ispar(i->op))
			break;
	p = qbe_rv64_abi_selpar(fn, b->ins, i);
  ret_on_err();
	n0 = &GC(insb)[NIns] - GC(curi);
	ioff = i - b->ins;
	n1 = b->nins - ioff;
	vgrow(&b->ins, n0+n1);
	icpy(b->ins+n0, b->ins+ioff, n1);
	icpy(b->ins, GC(curi), n0);
	b->nins = n0+n1;

	/* lower calls, returns, and vararg instructions */
	il = 0;
	b = fn->start;
	do {
		if (!(b = b->link))
			b = fn->start; /* do it last */
		if (b->visit)
			continue;
		GC(curi) = &GC(insb)[NIns];
		qbe_rv64_abi_selret(b, fn);
		ret_on_err();
		for (i=&b->ins[b->nins]; i!=b->ins;)
			switch ((--i)->op) {
			default:
				emiti(*i);
				break;
			case Ocall:
				for (i0=i; i0>b->ins; i0--)
					if (!isarg((i0-1)->op))
						break;
				qbe_rv64_abi_selcall(fn, i0, i, &il);
				ret_on_err();
				i = i0;
				break;
			case Ovastart:
				qbe_rv64_abi_selvastart(fn, p, i->arg[0]);
				break;
			case Ovaarg:
				qbe_rv64_abi_selvaarg(fn, i);
				break;
			case Oarg:
			case Oargc:
				die("unreachable");
			}
		if (b == fn->start)
			for (; il; il=il->link)
				emiti(il->i);
		idup(b, GC(curi), &GC(insb)[NIns]-GC(curi));
	} while (b != fn->start);

	if (GC(debug)['A']) {
		fprintf(stderr, "\n> After ABI lowering:\n");
		printfn(fn, stderr);
	}
}
#undef G
/*** END FILE: rv64/abi.c ***/
/*** START FILE: rv64/emit.c ***/
/* skipping all.h */

#define G(x) global_context.rv64_emit__##x

enum {
	QBE_RV64_EMIT_Ki = -1, /* matches Kw and Kl */
	QBE_RV64_EMIT_Ka = -2, /* matches all classes */
};

static struct {
	short op;
	short cls;
	char *fmt;
} qbe_rv64_emit_omap[] = {
	{ Oadd,    QBE_RV64_EMIT_Ki, "add%k %=, %0, %1" },
	{ Oadd,    QBE_RV64_EMIT_Ka, "fadd.%k %=, %0, %1" },
	{ Osub,    QBE_RV64_EMIT_Ki, "sub%k %=, %0, %1" },
	{ Osub,    QBE_RV64_EMIT_Ka, "fsub.%k %=, %0, %1" },
	{ Oneg,    QBE_RV64_EMIT_Ki, "neg%k %=, %0" },
	{ Oneg,    QBE_RV64_EMIT_Ka, "fneg.%k %=, %0" },
	{ Odiv,    QBE_RV64_EMIT_Ki, "div%k %=, %0, %1" },
	{ Odiv,    QBE_RV64_EMIT_Ka, "fdiv.%k %=, %0, %1" },
	{ Orem,    QBE_RV64_EMIT_Ki, "rem%k %=, %0, %1" },
	{ Orem,    Kl, "rem %=, %0, %1" },
	{ Oudiv,   QBE_RV64_EMIT_Ki, "divu%k %=, %0, %1" },
	{ Ourem,   QBE_RV64_EMIT_Ki, "remu%k %=, %0, %1" },
	{ Omul,    QBE_RV64_EMIT_Ki, "mul%k %=, %0, %1" },
	{ Omul,    QBE_RV64_EMIT_Ka, "fmul.%k %=, %0, %1" },
	{ Oand,    QBE_RV64_EMIT_Ki, "and %=, %0, %1" },
	{ Oor,     QBE_RV64_EMIT_Ki, "or %=, %0, %1" },
	{ Oxor,    QBE_RV64_EMIT_Ki, "xor %=, %0, %1" },
	{ Osar,    QBE_RV64_EMIT_Ki, "sra%k %=, %0, %1" },
	{ Oshr,    QBE_RV64_EMIT_Ki, "srl%k %=, %0, %1" },
	{ Oshl,    QBE_RV64_EMIT_Ki, "sll%k %=, %0, %1" },
	{ Ocsltl,  QBE_RV64_EMIT_Ki, "slt %=, %0, %1" },
	{ Ocultl,  QBE_RV64_EMIT_Ki, "sltu %=, %0, %1" },
	{ Oceqs,   QBE_RV64_EMIT_Ki, "feq.s %=, %0, %1" },
	{ Ocges,   QBE_RV64_EMIT_Ki, "fge.s %=, %0, %1" },
	{ Ocgts,   QBE_RV64_EMIT_Ki, "fgt.s %=, %0, %1" },
	{ Ocles,   QBE_RV64_EMIT_Ki, "fle.s %=, %0, %1" },
	{ Oclts,   QBE_RV64_EMIT_Ki, "flt.s %=, %0, %1" },
	{ Oceqd,   QBE_RV64_EMIT_Ki, "feq.d %=, %0, %1" },
	{ Ocged,   QBE_RV64_EMIT_Ki, "fge.d %=, %0, %1" },
	{ Ocgtd,   QBE_RV64_EMIT_Ki, "fgt.d %=, %0, %1" },
	{ Ocled,   QBE_RV64_EMIT_Ki, "fle.d %=, %0, %1" },
	{ Ocltd,   QBE_RV64_EMIT_Ki, "flt.d %=, %0, %1" },
	{ Ostoreb, Kw, "sb %0, %M1" },
	{ Ostoreh, Kw, "sh %0, %M1" },
	{ Ostorew, Kw, "sw %0, %M1" },
	{ Ostorel, QBE_RV64_EMIT_Ki, "sd %0, %M1" },
	{ Ostores, Kw, "fsw %0, %M1" },
	{ Ostored, Kw, "fsd %0, %M1" },
	{ Oloadsb, QBE_RV64_EMIT_Ki, "lb %=, %M0" },
	{ Oloadub, QBE_RV64_EMIT_Ki, "lbu %=, %M0" },
	{ Oloadsh, QBE_RV64_EMIT_Ki, "lh %=, %M0" },
	{ Oloaduh, QBE_RV64_EMIT_Ki, "lhu %=, %M0" },
	{ Oloadsw, QBE_RV64_EMIT_Ki, "lw %=, %M0" },
	/* riscv64 always sign-extends 32-bit
	 * values stored in 64-bit registers
	 */
	{ Oloaduw, Kw, "lw %=, %M0" },
	{ Oloaduw, Kl, "lwu %=, %M0" },
	{ Oload,   Kw, "lw %=, %M0" },
	{ Oload,   Kl, "ld %=, %M0" },
	{ Oload,   Ks, "flw %=, %M0" },
	{ Oload,   Kd, "fld %=, %M0" },
	{ Oextsb,  QBE_RV64_EMIT_Ki, "sext.b %=, %0" },
	{ Oextub,  QBE_RV64_EMIT_Ki, "zext.b %=, %0" },
	{ Oextsh,  QBE_RV64_EMIT_Ki, "sext.h %=, %0" },
	{ Oextuh,  QBE_RV64_EMIT_Ki, "zext.h %=, %0" },
	{ Oextsw,  Kl, "sext.w %=, %0" },
	{ Oextuw,  Kl, "zext.w %=, %0" },
	{ Otruncd, Ks, "fcvt.s.d %=, %0" },
	{ Oexts,   Kd, "fcvt.d.s %=, %0" },
	{ Ostosi,  Kw, "fcvt.w.s %=, %0, rtz" },
	{ Ostosi,  Kl, "fcvt.l.s %=, %0, rtz" },
	{ Ostoui,  Kw, "fcvt.wu.s %=, %0, rtz" },
	{ Ostoui,  Kl, "fcvt.lu.s %=, %0, rtz" },
	{ Odtosi,  Kw, "fcvt.w.d %=, %0, rtz" },
	{ Odtosi,  Kl, "fcvt.l.d %=, %0, rtz" },
	{ Odtoui,  Kw, "fcvt.wu.d %=, %0, rtz" },
	{ Odtoui,  Kl, "fcvt.lu.d %=, %0, rtz" },
	{ Oswtof,  QBE_RV64_EMIT_Ka, "fcvt.%k.w %=, %0" },
	{ Ouwtof,  QBE_RV64_EMIT_Ka, "fcvt.%k.wu %=, %0" },
	{ Osltof,  QBE_RV64_EMIT_Ka, "fcvt.%k.l %=, %0" },
	{ Oultof,  QBE_RV64_EMIT_Ka, "fcvt.%k.lu %=, %0" },
	{ Ocast,   Kw, "fmv.x.w %=, %0" },
	{ Ocast,   Kl, "fmv.x.d %=, %0" },
	{ Ocast,   Ks, "fmv.w.x %=, %0" },
	{ Ocast,   Kd, "fmv.d.x %=, %0" },
	{ Ocopy,   QBE_RV64_EMIT_Ki, "mv %=, %0" },
	{ Ocopy,   QBE_RV64_EMIT_Ka, "fmv.%k %=, %0" },
	{ Oswap,   QBE_RV64_EMIT_Ki, "mv %?, %0\n\tmv %0, %1\n\tmv %1, %?" },
	{ Oswap,   QBE_RV64_EMIT_Ka, "fmv.%k %?, %0\n\tfmv.%k %0, %1\n\tfmv.%k %1, %?" },
	{ Oreqz,   QBE_RV64_EMIT_Ki, "seqz %=, %0" },
	{ Ornez,   QBE_RV64_EMIT_Ki, "snez %=, %0" },
	{ Ocall,   Kw, "jalr %0" },
	{ NOp, 0, 0 }
};

static char *qbe_rv64_emit_rname[] = {
	[QBE_RV64_FP] = "fp",
	[QBE_RV64_SP] = "sp",
	[QBE_RV64_GP] = "gp",
	[QBE_RV64_TP] = "tp",
	[QBE_RV64_RA] = "ra",
	[QBE_RV64_T0] = "t0", "t1", "t2", "t3", "t4", "t5",
	[QBE_RV64_A0] = "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
	[QBE_RV64_S1] = "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8",
	       "s9", "s10", "s11",
	[QBE_RV64_FT0] = "ft0", "ft1", "ft2", "ft3", "ft4", "ft5", "ft6", "ft7",
	        "ft8", "ft9", "ft10",
	[QBE_RV64_FA0] = "fa0", "fa1", "fa2", "fa3", "fa4", "fa5", "fa6", "fa7",
	[QBE_RV64_FS0] = "fs0", "fs1", "fs2", "fs3", "fs4", "fs5", "fs6", "fs7",
	        "fs8", "fs9", "fs10", "fs11",
	[QBE_RV64_T6] = "t6",
	[QBE_RV64_FT11] = "ft11",
};

static int64_t
qbe_rv64_emit_slot(Ref r, Fn *fn)
{
	int s;

	s = rsval(r);
	SQ_ASSERT(s <= fn->slot);
	if (s < 0)
		return 8 * -s;
	else
		return -4 * (fn->slot - s);
}

static void
qbe_rv64_emit_emitaddr(Con *c, FILE *f)
{
	SQ_ASSERT(c->sym.type == SGlo);
	fputs(str(c->sym.id), f);
	if (c->bits.i)
		fprintf(f, "+%"PRIi64, c->bits.i);
}

static void
qbe_rv64_emit_emitf(char *s, Ins *i, Fn *fn, FILE *f)
{
	static char clschr[] = {'w', 'l', 's', 'd'};
	Ref r;
	int k, c;
	Con *pc;
	int64_t offset;

	fputc('\t', f);
	for (;;) {
		k = i->cls;
		while ((c = *s++) != '%')
			if (!c) {
				fputc('\n', f);
				return;
			} else
				fputc(c, f);
		switch ((c = *s++)) {
		default:
			die("invalid escape");
		case '?':
			if (KBASE(k) == 0)
				fputs("t6", f);
			else
				fputs("ft11", f);
			break;
		case 'k':
			if (i->cls != Kl)
				fputc(clschr[i->cls], f);
			break;
		case '=':
		case '0':
			r = c == '=' ? i->to : i->arg[0];
			SQ_ASSERT(isreg(r));
			fputs(qbe_rv64_emit_rname[r.val], f);
			break;
		case '1':
			r = i->arg[1];
			switch (rtype(r)) {
			default:
				die("invalid second argument");
			case RTmp:
				SQ_ASSERT(isreg(r));
				fputs(qbe_rv64_emit_rname[r.val], f);
				break;
			case RCon:
				pc = &fn->con[r.val];
				SQ_ASSERT(pc->type == CBits);
				SQ_ASSERT(pc->bits.i >= -2048 && pc->bits.i < 2048);
				fprintf(f, "%d", (int)pc->bits.i);
				break;
			}
			break;
		case 'M':
			c = *s++;
			SQ_ASSERT(c == '0' || c == '1');
			r = i->arg[c - '0'];
			switch (rtype(r)) {
			default:
				die("invalid address argument");
			case RTmp:
				fprintf(f, "0(%s)", qbe_rv64_emit_rname[r.val]);
				break;
			case RCon:
				pc = &fn->con[r.val];
				SQ_ASSERT(pc->type == CAddr);
				qbe_rv64_emit_emitaddr(pc, f);
				if (isstore(i->op)
				|| (isload(i->op) && KBASE(i->cls) == 1)) {
					/* store (and float load)
					 * pseudo-instructions need a
					 * temporary register in which to
					 * load the address
					 */
					fprintf(f, ", t6");
				}
				break;
			case RSlot:
				offset = qbe_rv64_emit_slot(r, fn);
				SQ_ASSERT(offset >= -2048 && offset <= 2047);
				fprintf(f, "%d(fp)", (int)offset);
				break;
			}
			break;
		}
	}
}

static void
qbe_rv64_emit_loadaddr(Con *c, char *rn, FILE *f)
{
	char off[32];

	if (c->sym.type == SThr) {
		if (c->bits.i)
			snprintf(off, sizeof(off), "+%"PRIi64, c->bits.i);
		else
			off[0] = 0;
		fprintf(f, "\tlui %s, %%tprel_hi(%s)%s\n",
			rn, str(c->sym.id), off);
		fprintf(f, "\tadd %s, %s, tp, %%tprel_add(%s)%s\n",
			rn, rn, str(c->sym.id), off);
		fprintf(f, "\taddi %s, %s, %%tprel_lo(%s)%s\n",
			rn, rn, str(c->sym.id), off);
	} else {
		fprintf(f, "\tla %s, ", rn);
		qbe_rv64_emit_emitaddr(c, f);
		fputc('\n', f);
	}
}

static void
qbe_rv64_emit_loadcon(Con *c, int r, int k, FILE *f)
{
	char *rn;
	int64_t n;

	rn = qbe_rv64_emit_rname[r];
	switch (c->type) {
	case CAddr:
		qbe_rv64_emit_loadaddr(c, rn, f);
		break;
	case CBits:
		n = c->bits.i;
		if (!KWIDE(k))
			n = (int32_t)n;
		fprintf(f, "\tli %s, %"PRIi64"\n", rn, n);
		break;
	default:
		die("invalid constant");
	}
}

static void
qbe_rv64_emit_fixmem(Ref *pr, Fn *fn, FILE *f)
{
	Ref r;
	int64_t s;
	Con *c;

	r = *pr;
	if (rtype(r) == RCon) {
		c = &fn->con[r.val];
		if (c->type == CAddr)
		if (c->sym.type == SThr) {
			qbe_rv64_emit_loadcon(c, QBE_RV64_T6, Kl, f);
			*pr = TMP(QBE_RV64_T6);
		}
	}
	if (rtype(r) == RSlot) {
		s = qbe_rv64_emit_slot(r, fn);
		if (s < -2048 || s > 2047) {
			fprintf(f, "\tli t6, %"PRId64"\n", s);
			fprintf(f, "\tadd t6, fp, t6\n");
			*pr = TMP(QBE_RV64_T6);
		}
	}
}

static void
qbe_rv64_emit_emitins(Ins *i, Fn *fn, FILE *f)
{
	int o;
	char *rn;
	int64_t s;
	Con *con;

	switch (i->op) {
	default:
		if (isload(i->op))
			qbe_rv64_emit_fixmem(&i->arg[0], fn, f);
		else if (isstore(i->op))
			qbe_rv64_emit_fixmem(&i->arg[1], fn, f);
	Table:
		/* most instructions are just pulled out of
		 * the table qbe_rv64_emit_omap[], some special cases are
		 * detailed below */
		for (o=0;; o++) {
			/* this linear search should really be a binary
			 * search */
			if (qbe_rv64_emit_omap[o].op == NOp)
				die("no match for %s(%c)",
					optab[i->op].name, "wlsd"[i->cls]);
			if (qbe_rv64_emit_omap[o].op == i->op)
			if (qbe_rv64_emit_omap[o].cls == i->cls || qbe_rv64_emit_omap[o].cls == QBE_RV64_EMIT_Ka
			|| (qbe_rv64_emit_omap[o].cls == QBE_RV64_EMIT_Ki && KBASE(i->cls) == 0))
				break;
		}
		qbe_rv64_emit_emitf(qbe_rv64_emit_omap[o].fmt, i, fn, f);
		break;
	case Ocopy:
		if (req(i->to, i->arg[0]))
			break;
		if (rtype(i->to) == RSlot) {
			switch (rtype(i->arg[0])) {
			case RSlot:
			case RCon:
				die("unimplemented");
				break;
			default:
				SQ_ASSERT(isreg(i->arg[0]));
				i->arg[1] = i->to;
				i->to = NULL_R;
				switch (i->cls) {
				case Kw: i->op = Ostorew; break;
				case Kl: i->op = Ostorel; break;
				case Ks: i->op = Ostores; break;
				case Kd: i->op = Ostored; break;
				}
				qbe_rv64_emit_fixmem(&i->arg[1], fn, f);
				goto Table;
			}
			break;
		}
		SQ_ASSERT(isreg(i->to));
		switch (rtype(i->arg[0])) {
		case RCon:
			qbe_rv64_emit_loadcon(&fn->con[i->arg[0].val], i->to.val, i->cls, f);
			break;
		case RSlot:
			i->op = Oload;
			qbe_rv64_emit_fixmem(&i->arg[0], fn, f);
			goto Table;
		default:
			SQ_ASSERT(isreg(i->arg[0]));
			goto Table;
		}
		break;
	case Onop:
		break;
	case Oaddr:
		SQ_ASSERT(rtype(i->arg[0]) == RSlot);
		rn = qbe_rv64_emit_rname[i->to.val];
		s = qbe_rv64_emit_slot(i->arg[0], fn);
		if (-s < 2048) {
			fprintf(f, "\tadd %s, fp, %"PRId64"\n", rn, s);
		} else {
			fprintf(f,
				"\tli %s, %"PRId64"\n"
				"\tadd %s, fp, %s\n",
				rn, s, rn, rn
			);
		}
		break;
	case Ocall:
		switch (rtype(i->arg[0])) {
		case RCon:
			con = &fn->con[i->arg[0].val];
			if (con->type != CAddr
			|| con->sym.type != SGlo
			|| con->bits.i)
				goto Invalid;
			fprintf(f, "\tcall %s\n", str(con->sym.id));
			break;
		case RTmp:
			qbe_rv64_emit_emitf("jalr %0", i, fn, f);
			break;
		default:
		Invalid:
			die("invalid call argument");
		}
		break;
	case Osalloc:
		qbe_rv64_emit_emitf("sub sp, sp, %0", i, fn, f);
		if (!req(i->to, NULL_R))
			qbe_rv64_emit_emitf("mv %=, sp", i, fn, f);
		break;
	case Odbgloc:
		emitdbgloc(i->arg[0].val, i->arg[1].val, f);
		break;
	}
}

/*

  Stack-frame layout:

  +=============+
  | varargs     |
  |  save area  |
  +-------------+
  |  saved ra   |
  |  saved fp   |
  +-------------+ <- fp
  |    ...      |
  | spill slots |
  |    ...      |
  +-------------+
  |    ...      |
  |   locals    |
  |    ...      |
  +-------------+
  |   padding   |
  +-------------+
  | callee-save |
  |  registers  |
  +=============+

*/

void
rv64_emitfn(Fn *fn, FILE *f)
{
	int lbl, neg, off, frame, *pr, r;
	Blk *b, *s;
	Ins *i, ii;

	emitfnlnk(fn->name, &fn->lnk, f);

	if (fn->vararg) {
		/* TODO: only need space for registers
		 * unused by named arguments
		 */
		fprintf(f, "\tadd sp, sp, -64\n");
		for (r=QBE_RV64_A0; r<=QBE_RV64_A7; r++)
			fprintf(f,
				"\tsd %s, %d(sp)\n",
				qbe_rv64_emit_rname[r], 8 * (r - QBE_RV64_A0)
			);
	}
	fprintf(f, "\tsd fp, -16(sp)\n");
	fprintf(f, "\tsd ra, -8(sp)\n");
	fprintf(f, "\tadd fp, sp, -16\n");

	frame = (16 + 4 * fn->slot + 15) & ~15;
	for (pr=rv64_rclob; *pr>=0; pr++) {
		if (fn->reg & BIT(*pr))
			frame += 8;
	}
	frame = (frame + 15) & ~15;

	if (frame <= 2048)
		fprintf(f,
			"\tadd sp, sp, -%d\n",
			frame
		);
	else
		fprintf(f,
			"\tli t6, %d\n"
			"\tsub sp, sp, t6\n",
			frame
		);
	for (pr=rv64_rclob, off=0; *pr>=0; pr++) {
		if (fn->reg & BIT(*pr)) {
			fprintf(f,
				"\t%s %s, %d(sp)\n",
				*pr < QBE_RV64_FT0 ? "sd" : "fsd",
				qbe_rv64_emit_rname[*pr], off
			);
			off += 8;
		}
	}

	for (lbl=0, b=fn->start; b; b=b->link) {
		if (lbl || b->npred > 1)
			fprintf(f, ".L%d:\n", G(rv64_emitfn_id0)+b->id);
		for (i=b->ins; i!=&b->ins[b->nins]; i++)
			qbe_rv64_emit_emitins(i, fn, f);
		lbl = 1;
		switch (b->jmp.type) {
		case Jhlt:
			fprintf(f, "\tebreak\n");
			break;
		case Jret0:
			if (fn->dynalloc) {
				if (frame - 16 <= 2048)
					fprintf(f,
						"\tadd sp, fp, -%d\n",
						frame - 16
					);
				else
					fprintf(f,
						"\tli t6, %d\n"
						"\tsub sp, fp, t6\n",
						frame - 16
					);
			}
			for (pr=rv64_rclob, off=0; *pr>=0; pr++) {
				if (fn->reg & BIT(*pr)) {
					fprintf(f,
						"\t%s %s, %d(sp)\n",
						*pr < QBE_RV64_FT0 ? "ld" : "fld",
						qbe_rv64_emit_rname[*pr], off
					);
					off += 8;
				}
			}
			fprintf(f,
				"\tadd sp, fp, %d\n"
				"\tld ra, 8(fp)\n"
				"\tld fp, 0(fp)\n"
				"\tret\n",
				16 + fn->vararg * 64
			);
			break;
		case Jjmp:
		Label_Jmp:
			if (b->s1 != b->link)
				fprintf(f, "\tj .L%d\n", G(rv64_emitfn_id0)+b->s1->id);
			else
				lbl = 0;
			break;
		case Jjnz:
			neg = 0;
			if (b->link == b->s2) {
				s = b->s1;
				b->s1 = b->s2;
				b->s2 = s;
				neg = 1;
			}
			if (rtype(b->jmp.arg) == RSlot) {
				ii.arg[0] = b->jmp.arg;
				qbe_rv64_emit_emitf("lw t6, %M0", &ii, fn, f);
				b->jmp.arg = TMP(QBE_RV64_T6);
			}
			SQ_ASSERT(isreg(b->jmp.arg));
			fprintf(f,
				"\tb%sz %s, .L%d\n",
				neg ? "ne" : "eq",
				qbe_rv64_emit_rname[b->jmp.arg.val],
				G(rv64_emitfn_id0)+b->s2->id
			);
			goto Label_Jmp;
		}
	}
	G(rv64_emitfn_id0) += fn->nblk;
	elf_emitfnfin(fn->name, f);
}
#undef CMP
#undef G
/*** END FILE: rv64/emit.c ***/
/*** START FILE: rv64/isel.c ***/
/* skipping all.h */

static int
qbe_rv64_isel_memarg(Ref *r, int op, Ins *i)
{
	if (isload(op) || op == Ocall)
		return r == &i->arg[0];
	if (isstore(op))
		return r == &i->arg[1];
	return 0;
}

static int
qbe_rv64_isel_immarg(Ref *r, int op, Ins *i)
{
	return rv64_op[op].imm && r == &i->arg[1];
}

static void
qbe_rv64_isel_fixarg(Ref *r, int k, Ins *i, Fn *fn)
{
	char buf[32];
	Ref r0, r1;
	int s, n, op;
	Con *c;

	r0 = r1 = *r;
	op = i ? i->op : Ocopy;
	switch (rtype(r0)) {
	case RCon:
		c = &fn->con[r0.val];
		if (c->type == CAddr && qbe_rv64_isel_memarg(r, op, i))
			break;
		if (KBASE(k) == 0)
		if (c->type == CBits && qbe_rv64_isel_immarg(r, op, i))
		if (-2048 <= c->bits.i && c->bits.i < 2048)
			break;
		r1 = newtmp("isel", k, fn);
		if (KBASE(k) == 1) {
			/* load floating points from memory
			 * slots, they can't be used as
			 * immediates
			 */
			SQ_ASSERT(c->type == CBits);
			n = stashbits(c->bits.i, KWIDE(k) ? 8 : 4);
			vgrow(&fn->con, ++fn->ncon);
			c = &fn->con[fn->ncon-1];
			snprintf(buf, sizeof(buf), "\"%sfp%d\"", GC(T).asloc, n);
			*c = (Con){.type = CAddr};
			c->sym.id = intern(buf);
			emit(Oload, k, r1, CON(c-fn->con), NULL_R);
			break;
		}
		emit(Ocopy, k, r1, r0, NULL_R);
		break;
	case RTmp:
		if (isreg(r0))
			break;
		s = fn->tmp[r0.val].slot;
		if (s != -1) {
			/* aggregate passed by value on
			 * stack, or fast local address,
			 * replace with slot if we can
			 */
			if (qbe_rv64_isel_memarg(r, op, i)) {
				r1 = SLOT(s);
				break;
			}
			r1 = newtmp("isel", k, fn);
			emit(Oaddr, k, r1, SLOT(s), NULL_R);
			break;
		}
		if (k == Kw && fn->tmp[r0.val].cls == Kl) {
			/* TODO: this sign extension isn't needed
			 * for 32-bit arithmetic instructions
			 */
			r1 = newtmp("isel", k, fn);
			emit(Oextsw, Kl, r1, r0, NULL_R);
		} else {
			SQ_ASSERT(k == fn->tmp[r0.val].cls);
		}
		break;
	}
	*r = r1;
}

static void
qbe_rv64_isel_negate(Ref *pr, Fn *fn)
{
	Ref r;

	r = newtmp("isel", Kw, fn);
	emit(Oxor, Kw, *pr, r, getcon(1, fn));
	*pr = r;
}

static void
qbe_rv64_isel_selcmp(Ins i, int k, int op, Fn *fn)
{
	Ins *icmp;
	Ref r, r0, r1;
	int sign, swap, neg;

	switch (op) {
	case Cieq:
		r = newtmp("isel", k, fn);
		emit(Oreqz, i.cls, i.to, r, NULL_R);
		emit(Oxor, k, r, i.arg[0], i.arg[1]);
		icmp = GC(curi);
		qbe_rv64_isel_fixarg(&icmp->arg[0], k, icmp, fn);
		qbe_rv64_isel_fixarg(&icmp->arg[1], k, icmp, fn);
		return;
	case Cine:
		r = newtmp("isel", k, fn);
		emit(Ornez, i.cls, i.to, r, NULL_R);
		emit(Oxor, k, r, i.arg[0], i.arg[1]);
		icmp = GC(curi);
		qbe_rv64_isel_fixarg(&icmp->arg[0], k, icmp, fn);
		qbe_rv64_isel_fixarg(&icmp->arg[1], k, icmp, fn);
		return;
	case Cisge: sign = 1, swap = 0, neg = 1; break;
	case Cisgt: sign = 1, swap = 1, neg = 0; break;
	case Cisle: sign = 1, swap = 1, neg = 1; break;
	case Cislt: sign = 1, swap = 0, neg = 0; break;
	case Ciuge: sign = 0, swap = 0, neg = 1; break;
	case Ciugt: sign = 0, swap = 1, neg = 0; break;
	case Ciule: sign = 0, swap = 1, neg = 1; break;
	case Ciult: sign = 0, swap = 0, neg = 0; break;
	case NCmpI+Cfeq:
	case NCmpI+Cfge:
	case NCmpI+Cfgt:
	case NCmpI+Cfle:
	case NCmpI+Cflt:
		swap = 0, neg = 0;
		break;
	case NCmpI+Cfuo:
		qbe_rv64_isel_negate(&i.to, fn);
		/* fall through */
	case NCmpI+Cfo:
		r0 = newtmp("isel", i.cls, fn);
		r1 = newtmp("isel", i.cls, fn);
		emit(Oand, i.cls, i.to, r0, r1);
		op = KWIDE(k) ? Oceqd : Oceqs;
		emit(op, i.cls, r0, i.arg[0], i.arg[0]);
		icmp = GC(curi);
		qbe_rv64_isel_fixarg(&icmp->arg[0], k, icmp, fn);
		qbe_rv64_isel_fixarg(&icmp->arg[1], k, icmp, fn);
		emit(op, i.cls, r1, i.arg[1], i.arg[1]);
		icmp = GC(curi);
		qbe_rv64_isel_fixarg(&icmp->arg[0], k, icmp, fn);
		qbe_rv64_isel_fixarg(&icmp->arg[1], k, icmp, fn);
		return;
	case NCmpI+Cfne:
		swap = 0, neg = 1;
		i.op = KWIDE(k) ? Oceqd : Oceqs;
		break;
	default:
		die("unknown comparison");
	}
	if (op < NCmpI)
		i.op = sign ? Ocsltl : Ocultl;
	if (swap) {
		r = i.arg[0];
		i.arg[0] = i.arg[1];
		i.arg[1] = r;
	}
	if (neg)
		qbe_rv64_isel_negate(&i.to, fn);
	emiti(i);
	icmp = GC(curi);
	qbe_rv64_isel_fixarg(&icmp->arg[0], k, icmp, fn);
	qbe_rv64_isel_fixarg(&icmp->arg[1], k, icmp, fn);
}

static void
qbe_rv64_isel_sel(Ins i, Fn *fn)
{
	Ins *i0;
	int ck, cc;

	if (INRANGE(i.op, Oalloc, Oalloc1)) {
		i0 = GC(curi) - 1;
		salloc(i.to, i.arg[0], fn);
		ret_on_err();
		qbe_rv64_isel_fixarg(&i0->arg[0], Kl, i0, fn);
		return;
	}
	if (iscmp(i.op, &ck, &cc)) {
		qbe_rv64_isel_selcmp(i, ck, cc, fn);
		return;
	}
	if (i.op != Onop) {
		emiti(i);
		i0 = GC(curi); /* qbe_rv64_isel_fixarg() can change curi */
		qbe_rv64_isel_fixarg(&i0->arg[0], argcls(&i, 0), i0, fn);
		qbe_rv64_isel_fixarg(&i0->arg[1], argcls(&i, 1), i0, fn);
	}
}

static void
qbe_rv64_isel_seljmp(Blk *b, Fn *fn)
{
	/* TODO: replace cmp+jnz with beq/bne/blt[u]/bge[u] */
	if (b->jmp.type == Jjnz)
		qbe_rv64_isel_fixarg(&b->jmp.arg, Kw, 0, fn);
}

void
rv64_isel(Fn *fn)
{
	Blk *b, **sb;
	Ins *i;
	Phi *p;
	uint n;
	int al;
	int64_t sz;

	/* assign slots to fast allocs */
	b = fn->start;
	/* specific to NAlign == 3 */ /* or change n=4 and sz /= 4 below */
	for (al=Oalloc, n=4; al<=Oalloc1; al++, n*=2)
		for (i=b->ins; i<&b->ins[b->nins]; i++)
			if (i->op == al) {
				if (rtype(i->arg[0]) != RCon)
					break;
				sz = fn->con[i->arg[0].val].bits.i;
				if (sz < 0 || sz >= INT_MAX-15)
					err("invalid alloc size %"PRId64, sz);
				sz = (sz + n-1) & -n;
				sz /= 4;
				if (sz > INT_MAX - fn->slot)
					die("alloc too large");
				fn->tmp[i->to.val].slot = fn->slot;
				fn->slot += sz;
				*i = (Ins){.op = Onop};
			}

	for (b=fn->start; b; b=b->link) {
		GC(curi) = &GC(insb)[NIns];
		for (sb=(Blk*[3]){b->s1, b->s2, 0}; *sb; sb++)
			for (p=(*sb)->phi; p; p=p->link) {
				for (n=0; p->blk[n] != b; n++)
					SQ_ASSERT(n+1 < p->narg);
				qbe_rv64_isel_fixarg(&p->arg[n], p->cls, 0, fn);
			}
		qbe_rv64_isel_seljmp(b, fn);
		for (i=&b->ins[b->nins]; i!=b->ins;) {
			qbe_rv64_isel_sel(*--i, fn);
      ret_on_err();
    }
		idup(b, GC(curi), &GC(insb)[NIns]-GC(curi));
	}

	if (GC(debug)['I']) {
		fprintf(stderr, "\n> After instruction selection:\n");
		printfn(fn, stderr);
	}
}
#undef G
/*** END FILE: rv64/isel.c ***/
/*** START FILE: rv64/targ.c ***/
/* skipping all.h */

static Rv64Op rv64_op[NOp] = {
#define O(op, t, x) [O##op] =
#define V(imm) { imm },
/* ------------------------------------------------------------including ops.h */
#ifndef X /* amd64 */
	#define X(NMemArgs, SetsZeroFlag, LeavesFlags)
#endif

#ifndef V /* riscv64 */
	#define V(Imm)
#endif

#ifndef F
#define F(a,b,c,d,e,f,g,h,i,j)
#endif

#define T(a,b,c,d,e,f,g,h) {                          \
	{[Kw]=K##a, [Kl]=K##b, [Ks]=K##c, [Kd]=K##d}, \
	{[Kw]=K##e, [Kl]=K##f, [Ks]=K##g, [Kd]=K##h}  \
}

/*********************/
/* PUBLIC OPERATIONS */
/*********************/

/*                                can fold                        */
/*                                | has identity                  */
/*                                | | identity value for arg[1]   */
/*                                | | | commutative               */
/*                                | | | | associative             */
/*                                | | | | | idempotent            */
/*                                | | | | | | c{eq,ne}[wl]        */
/*                                | | | | | | | c[us][gl][et][wl] */
/*                                | | | | | | | | value if = args */
/*                                | | | | | | | | | pinned        */
/* Arithmetic and Bits            v v v v v v v v v v             */
O(add,     T(w,l,s,d, w,l,s,d), F(1,1,0,1,1,0,0,0,0,0)) X(2,1,0) V(1)
O(sub,     T(w,l,s,d, w,l,s,d), F(1,1,0,0,0,0,0,0,0,0)) X(2,1,0) V(0)
O(neg,     T(w,l,s,d, x,x,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(1,1,0) V(0)
O(div,     T(w,l,s,d, w,l,s,d), F(1,1,1,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(rem,     T(w,l,e,e, w,l,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(udiv,    T(w,l,e,e, w,l,e,e), F(1,1,1,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(urem,    T(w,l,e,e, w,l,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(mul,     T(w,l,s,d, w,l,s,d), F(1,1,1,1,0,0,0,0,0,0)) X(2,0,0) V(0)
O(and,     T(w,l,e,e, w,l,e,e), F(1,0,0,1,1,1,0,0,0,0)) X(2,1,0) V(1)
O(or,      T(w,l,e,e, w,l,e,e), F(1,1,0,1,1,1,0,0,0,0)) X(2,1,0) V(1)
O(xor,     T(w,l,e,e, w,l,e,e), F(1,1,0,1,1,0,0,0,0,0)) X(2,1,0) V(1)
O(sar,     T(w,l,e,e, w,w,e,e), F(1,1,0,0,0,0,0,0,0,0)) X(1,1,0) V(1)
O(shr,     T(w,l,e,e, w,w,e,e), F(1,1,0,0,0,0,0,0,0,0)) X(1,1,0) V(1)
O(shl,     T(w,l,e,e, w,w,e,e), F(1,1,0,0,0,0,0,0,0,0)) X(1,1,0) V(1)

/* Comparisons */
O(ceqw,    T(w,w,e,e, w,w,e,e), F(1,1,1,1,0,0,1,0,1,0)) X(0,1,0) V(0)
O(cnew,    T(w,w,e,e, w,w,e,e), F(1,1,0,1,0,0,1,0,0,0)) X(0,1,0) V(0)
O(csgew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csgtw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(cslew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csltw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)
O(cugew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cugtw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(culew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cultw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)

O(ceql,    T(l,l,e,e, l,l,e,e), F(1,0,0,1,0,0,1,0,1,0)) X(0,1,0) V(0)
O(cnel,    T(l,l,e,e, l,l,e,e), F(1,0,0,1,0,0,1,0,0,0)) X(0,1,0) V(0)
O(csgel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csgtl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(cslel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csltl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)
O(cugel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cugtl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(culel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cultl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)

O(ceqs,    T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cges,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cgts,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cles,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(clts,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cnes,    T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cos,     T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cuos,    T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)

O(ceqd,    T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cged,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cgtd,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cled,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cltd,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cned,    T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cod,     T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cuod,    T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)

/* Memory */
O(storeb,  T(w,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(storeh,  T(w,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(storew,  T(w,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(storel,  T(l,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(stores,  T(s,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(stored,  T(d,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

O(loadsb,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loadub,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loadsh,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loaduh,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loadsw,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loaduw,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(load,    T(m,m,m,m, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

/* Extensions and Truncations */
O(extsb,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extub,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extsh,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extuh,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extsw,   T(e,w,e,e, e,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extuw,   T(e,w,e,e, e,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

O(exts,    T(e,e,e,s, e,e,e,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(truncd,  T(e,e,d,e, e,e,x,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(stosi,   T(s,s,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(stoui,   T(s,s,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(dtosi,   T(d,d,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(dtoui,   T(d,d,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(swtof,   T(e,e,w,w, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(uwtof,   T(e,e,w,w, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(sltof,   T(e,e,l,l, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(ultof,   T(e,e,l,l, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(cast,    T(s,d,w,l, x,x,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

/* Stack Allocation */
O(alloc4,  T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(alloc8,  T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(alloc16, T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)

/* Variadic Function Helpers */
O(vaarg,   T(m,m,m,m, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(vastart, T(m,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)

O(copy,    T(w,l,s,d, x,x,x,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

/* Debug */
O(dbgloc,  T(w,e,e,e, w,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

/****************************************/
/* INTERNAL OPERATIONS (keep nop first) */
/****************************************/

/* Miscellaneous and Architecture-Specific Operations */
O(nop,     T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(addr,    T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(blit0,   T(m,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,1,0) V(0)
O(blit1,   T(w,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,1,0) V(0)
O(sel0,    T(w,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(sel1,    T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(swap,    T(w,l,s,d, w,l,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(1,0,0) V(0)
O(sign,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(salloc,  T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xidiv,   T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(1,0,0) V(0)
O(xdiv,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(1,0,0) V(0)
O(xcmp,    T(w,l,s,d, w,l,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(1,1,0) V(0)
O(xtest,   T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(1,1,0) V(0)
O(acmp,    T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(acmn,    T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(afcmp,   T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(reqz,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(rnez,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)

/* Arguments, Parameters, and Calls */
O(par,     T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parsb,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parub,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parsh,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(paruh,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parc,    T(e,x,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(pare,    T(e,x,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(arg,     T(w,l,s,d, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argsb,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argub,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argsh,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(arguh,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argc,    T(e,x,e,e, e,l,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(arge,    T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argv,    T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(call,    T(m,m,m,m, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)

/* Flags Setting */
O(flagieq,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagine,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagisge, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagisgt, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagisle, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagislt, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiuge, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiugt, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiule, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiult, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfeq,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfge,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfgt,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfle,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagflt,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfne,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfo,   T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfuo,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

/* Backend Flag Select (Condition Move) */
O(xselieq,  T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseline,  T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselisge, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselisgt, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselisle, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselislt, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliuge, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliugt, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliule, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliult, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfeq,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfge,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfgt,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfle,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselflt,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfne,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfo,   T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfuo,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)

#undef T
#undef X
#undef V
#undef O

/*
| column -t -o ' '
*/
/* ------------------------------------------------------------end of ops.h */
};

static int rv64_rsave[] = {
	QBE_RV64_T0, QBE_RV64_T1, QBE_RV64_T2, QBE_RV64_T3, QBE_RV64_T4, QBE_RV64_T5,
	QBE_RV64_A0, QBE_RV64_A1, QBE_RV64_A2, QBE_RV64_A3, QBE_RV64_A4, QBE_RV64_A5, QBE_RV64_A6, QBE_RV64_A7,
	QBE_RV64_FA0, QBE_RV64_FA1, QBE_RV64_FA2,  QBE_RV64_FA3,  QBE_RV64_FA4, QBE_RV64_FA5, QBE_RV64_FA6, QBE_RV64_FA7,
	QBE_RV64_FT0, QBE_RV64_FT1, QBE_RV64_FT2,  QBE_RV64_FT3,  QBE_RV64_FT4, QBE_RV64_FT5, QBE_RV64_FT6, QBE_RV64_FT7,
	QBE_RV64_FT8, QBE_RV64_FT9, QBE_RV64_FT10,
	-1
};
static int rv64_rclob[] = {
	     QBE_RV64_S1,  QBE_RV64_S2,   QBE_RV64_S3,   QBE_RV64_S4,  QBE_RV64_S5,  QBE_RV64_S6,  QBE_RV64_S7,
	QBE_RV64_S8,  QBE_RV64_S9,  QBE_RV64_S10,  QBE_RV64_S11,
	QBE_RV64_FS0, QBE_RV64_FS1, QBE_RV64_FS2,  QBE_RV64_FS3,  QBE_RV64_FS4, QBE_RV64_FS5, QBE_RV64_FS6, QBE_RV64_FS7,
	QBE_RV64_FS8, QBE_RV64_FS9, QBE_RV64_FS10, QBE_RV64_FS11,
	-1
};

#define QBE_RV64_RGLOB (BIT(QBE_RV64_FP) | BIT(QBE_RV64_SP) | BIT(QBE_RV64_GP) | BIT(QBE_RV64_TP) | BIT(QBE_RV64_RA))

static int
rv64_memargs(int op)
{
	(void)op;
	return 0;
}

static Target T_rv64 = {
	.name = "rv64",
	.gpr0 = QBE_RV64_T0,
	.ngpr = QBE_RV64_NGPR,
	.fpr0 = QBE_RV64_FT0,
	.nfpr = QBE_RV64_NFPR,
	.rglob = QBE_RV64_RGLOB,
	.nrglob = 5,
	.rsave = rv64_rsave,
	.nrsave = {QBE_RV64_NGPS, QBE_RV64_NFPS},
	.retregs = rv64_retregs,
	.argregs = rv64_argregs,
	.memargs = rv64_memargs,
	.abi0 = elimsb,
	.abi1 = rv64_abi,
	.isel = rv64_isel,
	.emitfn = rv64_emitfn,
	.emitfin = elf_emitfin,
	.asloc = ".L",
	.cansel = 0,
};

MAKESURE(rsave_size_ok, sizeof rv64_rsave == (QBE_RV64_NGPS+QBE_RV64_NFPS+1) * sizeof(int));
MAKESURE(rclob_size_ok, sizeof rv64_rclob == (QBE_RV64_NCLR+1) * sizeof(int));
#undef G
/*** END FILE: rv64/targ.c ***/
/*** START FILE: ../sqbe_impl.c ***/
#if defined(_WIN32)
#include <windows.h>
#else
#include <alloca.h>
#include <sys/mman.h>
#include <unistd.h>
#endif  // _WIN32

#define SQ_ERR_CHECK(ret) { if (GC(in_error)) return ret; }
#define SQ_ERR_CHECK_VOID() { if (GC(in_error)) return; }

typedef struct SqArena SqArena;

typedef enum SqInitStatus {
  SQIS_UNINITIALIZED = 0,
  SQIS_INITIALIZED_EMIT_FIN = 1,
  SQIS_INITIALIZED_NO_FIN = 2,
} SqInitStatus;

// These things are sqbe state that have to be saved/restored on entry/exit from
// functions (or data)s. Additionally, it has to save/restore a lot of
// GlobalContext (but not all), so we keep a GlobalContext here too.
typedef struct SqPerFuncState {
  // This is allocated for all of them during sq_init, and just reset when done.
  SqArena* fn_arena;

  PState ps;
  Blk* block_pool;  // This is allocated from fn_arena.
  unsigned int num_blocks;
  unsigned int max_blocks;

  Dat curd;
  Lnk curd_lnk;

  SqBlock start_block;

  // TODO: Should really make our code stop using parse__ versions of these,
  // it'd be less confusing.
  Ins* insb;
  Ins* curi;
  Fn* curf;
  Blk* curb;
  Blk** blink;
  int rcls;
} SqPerFuncState;

typedef struct SqContext {
  SqInitStatus initialized;

  SqArena* global_arena;

  //
  // Global lifetime starts here.
  //
  uint ntyp;  // This is how many elements are in GC(typ), very hokey.
  SqOutputFn output_func;

  Lnk* linkage_pool;  // This is allocated from global_arena.
  unsigned int num_linkages;
  unsigned int max_linkages;

  Typ* curty;
  int curty_build_n;
  uint64_t curty_build_sz;
  int curty_build_al;

  int dbg_name_counter;

  // Copied here during init so allocating a SqPerFuncState has it.
  unsigned int config_max_blocks;

  //
  // Per-func/data lifetime storage.
  //
  SqPerFuncState pfs;  // This is the one that's been sq_itemctx_activate()d,
                       // and is always a copy of something in
                       // saved_per_func_states.
  SqItemCtx current_itemctx;

#define SQ_MAX_FUNC_CONTEXTS 8
  SqPerFuncState saved_per_func_states[SQ_MAX_FUNC_CONTEXTS];
  bool saved_per_func_state_in_use[SQ_MAX_FUNC_CONTEXTS];
} SqContext;

#define SQC(x) g_sq_context.x
static SqContext g_sq_context;

static int sq_usermsgf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = SQC(output_func)(fmt, ap);
  va_end(ap);
  return ret;
}

#if defined(_WIN32)

static void* os_mem_reserve(uint64_t size) {
  return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
}

static bool os_mem_commit(void* ptr, uint64_t size) {
  return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != 0;
}

static void os_mem_release(void* ptr, uint64_t size) {
  (void)size;
  VirtualFree(ptr, 0, MEM_RELEASE);
}

static uint64_t os_page_size(void) {
  SYSTEM_INFO sysInfo;
  GetSystemInfo(&sysInfo);
  return sysInfo.dwPageSize;
}

#else  // ^^^ _WIN32 / else vvv

static void* os_mem_reserve(uint64_t size) {
  void* result = mmap(0, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (result == MAP_FAILED) {
    result = NULL;
  }
  return result;
}

static bool os_mem_commit(void* ptr, uint64_t size) {
  mprotect(ptr, size, PROT_READ|PROT_WRITE);
  return true;
}

static void os_mem_release(void* ptr, uint64_t size) {
  munmap(ptr, size);
}

static uint64_t os_page_size(void) {
  return sysconf(_SC_PAGE_SIZE);
}

#endif

#define SQ_ARENA_HEADER_SIZE 128
struct SqArena {
  uint64_t commit_chunk_size;
  uint64_t cur_pos;
  uint64_t cur_commit;
  uint64_t cur_reserve;
};

#if defined(__clang__)
#  define SQ_BRANCH_EXPECT(expr, val) __builtin_expect((expr), (val))
#else
#  define SQ_BRANCH_EXPECT(expr, val) (expr)
#endif

#if defined(_WIN32)
#  if defined(__SANITIZE_ADDRESS__)
#    define SQ_ASAN_ENABLED 1
#  endif
#elif defined(__clang__)
#  if defined(__has_feature)
#    if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#      define SQ_ASAN_ENABLED 1
#    endif
#  endif
#endif

#if defined(SQ_ASAN_ENABLED)
void __asan_poison_memory_region(void const volatile *addr, size_t size);
void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
#  define SQ_ASAN_POISON_REGION(addr, size) __asan_poison_memory_region((addr), (size))
#  define SQ_ASAN_UNPOISON_REGION(addr, size) __asan_unpoison_memory_region((addr), (size))
#else
#  define SQ_ASAN_POISON_REGION(addr, size) ((void)(addr), (void)(size))
#  define SQ_ASAN_UNPOISON_REGION(addr, size) ((void)(addr), (void)(size))
#endif

#define SQ_BRANCH_LIKELY(expr) SQ_BRANCH_EXPECT(expr, 1)
#define SQ_BRANCH_UNLIKELY(expr) SQ_BRANCH_EXPECT(expr, 0)
#define SQ_MIN(x, y) ((x) <= (y) ? (x) : (y))
#define SQ_MAX(x, y) ((x) >= (y) ? (x) : (y))
#define SQ_CLAMP_MAX(x, max) SQ_MIN(x, max)
#define SQ_CLAMP_MIN(x, min) SQ_MAX(x, min)
#define SQ_ALIGN_DOWN(n, a) ((n) & ~((a)-1))
#define SQ_ALIGN_UP(n, a) SQ_ALIGN_DOWN((n) + (a)-1, (a))
MAKESURE(sq_arena_struct_too_large, sizeof(SqArena) < SQ_ARENA_HEADER_SIZE);

static SqArena* _sq_arena_create(uint64_t provided_reserve_size, uint64_t provided_commit_size) {
  uint64_t commit_size = ALIGN_UP(provided_commit_size, os_page_size());
  uint64_t reserve_size = ALIGN_UP(provided_reserve_size, os_page_size());

  void* base = os_mem_reserve(reserve_size);
  SQ_ASSERT(base);
  if (!os_mem_commit(base, commit_size)) {
    die("couldn't commit %zu", commit_size);
  }

  SqArena* arena = base;
  arena->commit_chunk_size = commit_size;
  arena->cur_pos = SQ_ARENA_HEADER_SIZE;
  arena->cur_commit = commit_size;
  arena->cur_reserve = reserve_size;

  SQ_ASAN_POISON_REGION(base, commit_size);
  SQ_ASAN_UNPOISON_REGION(base, SQ_ARENA_HEADER_SIZE);

  return arena;
}

static void _sq_arena_destroy(SqArena* arena) {
  os_mem_release(arena, arena->cur_reserve);
}

static void* _sq_arena_push(SqArena* arena, size_t size, size_t align) {
  uint64_t pos_pre = ALIGN_UP(arena->cur_pos, align);
  uint64_t pos_post = pos_pre + size;

  // Extend commit, if necessary.
  if (arena->cur_commit < pos_post) {
    uint64_t commit_post_aligned = pos_post + arena->commit_chunk_size - 1;
    commit_post_aligned -= commit_post_aligned % arena->commit_chunk_size;
    uint64_t commit_post_clamped = SQ_CLAMP_MAX(commit_post_aligned, arena->cur_reserve);
    uint64_t commit_size = commit_post_clamped - arena->cur_commit;
    unsigned char* commit_ptr = (unsigned char*)arena + arena->cur_commit;
    os_mem_commit(commit_ptr, commit_size);
    arena->cur_commit = commit_post_clamped;
  }

  void* result = NULL;
  if (arena->cur_commit >= pos_post) {
    result = (unsigned char*)arena + pos_pre;
    arena->cur_pos = pos_post;
    SQ_ASAN_UNPOISON_REGION(result, size);
  }

  if (SQ_BRANCH_UNLIKELY(result == NULL)) {
    die("allocation failure");
  }

  return result;
}

#if 0
static uint64_t _sq_arena_pos(SqArena* arena) {
  return arena->cur_pos;
}
#endif

static void _sq_arena_pop_to(SqArena* arena, uint64_t pos) {
  uint64_t clamped_pos = SQ_CLAMP_MIN(SQ_ARENA_HEADER_SIZE, pos);

  SQ_ASAN_POISON_REGION((unsigned char*)arena + clamped_pos, arena->cur_pos - clamped_pos);

  arena->cur_pos = clamped_pos;
}

void* emalloc(size_t n) {
  void* p = _sq_arena_push(SQC(global_arena), n, 8);
  memset(p, 0, n);
  return p;
}

void* alloc(size_t n) {
  if (n == 0) {
    return NULL;
  }
  void* p = _sq_arena_push(SQC(pfs.fn_arena), n, 8);
  memset(p, 0, n);
  return p;
}

void freeall(void) {
  _sq_arena_pop_to(SQC(pfs.fn_arena), 0);
}

void qbe_free(void* ptr) {
  (void)ptr;  // Nothing until arena_destroy.
}

static void _sq_gen_dbg_name(char* into, size_t len, const char* prefix) {
  snprintf(into, len, "%s_%d", prefix ? prefix : "", SQC(dbg_name_counter)++);
}

#define SQ_NAMED_IF_DEBUG(into, provided)        \
  if (global_context.main__dbg) {                \
    _sq_gen_dbg_name(into, sizeof(into), provided); \
  }

#define SQ_COUNTOF(a) (sizeof(a) / sizeof(a[0]))
#define SQ_COUNTOFI(a) ((int)(sizeof(a) / sizeof(a[0])))

#ifdef _MSC_VER
#  define alloca _alloca
#endif

static void _clear_initialized_state(void) {
  for (int i = 0; i < SQ_MAX_FUNC_CONTEXTS; ++i) {
    _sq_arena_destroy(g_sq_context.saved_per_func_states[i].fn_arena);
  }
  _sq_arena_destroy(SQC(global_arena));
  memset(&g_sq_context, 0, sizeof(SqContext));

  // Mostly want to clear this on shutdown/error, but save the fact that we're
  // actually in an error. Re-initialization will clear the GlobalContext fully.
  int saved_in_error = GC(in_error);
  memset(&global_context, 0, sizeof(GlobalContext));
  GC(in_error) = saved_in_error;
}

static void err_(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  SQC(output_func)(fmt, ap);
  va_end(ap);
  sq_usermsgf("\n");
  GC(in_error) = true;
  _clear_initialized_state();
}

static void die_(char* file, char* fmt, ...) {
  sq_usermsgf("%s: internal error: ", file);
  va_list ap;
  va_start(ap, fmt);
  SQC(output_func)(fmt, ap);
  va_end(ap);
  sq_usermsgf("\n");
  SQ_TRAP();
}

SqLinkage sq_linkage_create(int alignment,
                            bool exported,
                            bool tls,
                            bool common,
                            const char* section_name,
                            const char* section_flags) {
  SQ_ERR_CHECK((SqLinkage){0});
  SQ_ASSERT(SQC(num_linkages) < SQC(max_linkages));
  SqLinkage ret = {SQC(num_linkages)++};
  SQC(linkage_pool)[ret.u] = (Lnk){
      .export = exported,
      .thread = tls,
      .common = common,
      .align = alignment,
      .sec = (char*)section_name,
      .secf = (char*)section_flags,
  };
  return ret;
}

static Lnk _sqlinkage_to_internal_lnk(SqLinkage linkage) {
  SQ_ASSERT(linkage.u < SQC(num_linkages));
  return SQC(linkage_pool)[linkage.u];
}

MAKESURE(ref_sizes_match, sizeof(SqRef) == sizeof(Ref));
MAKESURE(ref_has_expected_size, sizeof(uint32_t) == sizeof(Ref));

static Ref _sqref_to_internal_ref(SqRef ref) {
  Ref ret;
  memcpy(&ret, &ref, sizeof(Ref));
  return ret;
}

static SqRef _internal_ref_to_sqref(Ref ref) {
  SqRef ret;
  memcpy(&ret, &ref, sizeof(Ref));
  return ret;
}

static int _sqtype_to_cls_and_ty(SqType type, int* ty) {
  SQ_ASSERT(type.u != SQ_TYPE_C);
  if (type.u > SQ_TYPE_0) {
    *ty = type.u;
    return SQ_TYPE_C;
  } else {
    *ty = Kx;
    return type.u;
  }
}

static int _default_output_fn(const char* fmt, va_list ap) {
  int ret = vfprintf(stdout, fmt, ap);
  return ret;
}

static Blk* _sqblock_to_internal_blk(SqBlock block) {
  SQ_ASSERT(block.u < SQC(pfs.num_blocks));
  return &SQC(pfs.block_pool)[block.u];
}

void sq_init(SqConfiguration* config) {
  SQ_ASSERT(SQC(initialized) == SQIS_UNINITIALIZED);

  memset(&g_sq_context, 0, sizeof(SqContext));
  memset(&global_context, 0, sizeof(GlobalContext));

  SQC(current_itemctx).u = SQ_MAX_FUNC_CONTEXTS;  // Hack, mark as none-active.

  SQC(global_arena) =
      _sq_arena_create(config->max_compiler_global_reserve, config->global_commit_chunk_size);
  for (int i = 0; i < SQ_MAX_FUNC_CONTEXTS; ++i) {
    SQC(saved_per_func_states)[i].fn_arena =
        _sq_arena_create(config->max_compiler_function_reserve, config->function_commit_chunk_size);
  }
  SQC(config_max_blocks) = config->max_blocks_per_function;

  SQC(max_linkages) = config->max_linkage_definitions;
  SQC(linkage_pool) =
      _sq_arena_push(SQC(global_arena), sizeof(Lnk) * SQC(max_linkages), _Alignof(Lnk));

  SQC(output_func) = config->output_function ? config->output_function : _default_output_fn;

  // These have to match the header for sq_linkage_default/export.
  SqLinkage def = sq_linkage_create(8, false, false, false, NULL, NULL);
  SQ_ASSERT(def.u == 0);
  (void)def;
  SqLinkage exp = sq_linkage_create(8, true, false, false, NULL, NULL);
  SQ_ASSERT(exp.u == 1);
  (void)exp;

  (void)qbe_main_dbgfile;  // TODO

  switch (config->target) {
    case SQ_TARGET_AMD64_APPLE:
      GC(T) = T_amd64_apple;
      break;
    case SQ_TARGET_AMD64_SYSV:
      GC(T) = T_amd64_sysv;
      break;
    case SQ_TARGET_AMD64_WIN:
      GC(T) = T_amd64_win;
      break;
    case SQ_TARGET_ARM64:
      GC(T) = T_arm64;
      break;
    case SQ_TARGET_ARM64_APPLE:
      GC(T) = T_arm64_apple;
      break;
    case SQ_TARGET_RV64:
      GC(T) = T_rv64;
      break;
    default: {
#if defined(__APPLE__) && defined(__MACH__)
#  if defined(__aarch64__)
      GC(T) = T_arm64_apple;
#  else
      GC(T) = T_amd64_apple;
#  endif
#elif defined(_WIN32)
#  if defined(__aarch64__)
#    error port win arm64
#  else
      GC(T) = T_amd64_win;
#  endif
#else
#  if defined(__aarch64__)
      GC(T) = T_arm64;
#  elif defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)
      GC(T) = T_amd64_sysv;
#  elif defined(__riscv)
      GC(T) = T_rv64;
#  else
#    error port unknown
#  endif
#endif
    }
  }

  if (config->format == SQ_FORMAT_OBJ_MACHO && GC(T).apple) {
    global_context.main__objmode = 1;
    global_context.main__macho_ctx = macho_new();
  }

  global_context.main__outf = config->output;

  memset(GC(debug), 0, sizeof(GC(debug)));
  for (const char* d = config->debug_flags; *d; ++d) {
    GC(debug)[(int)*d] = 1;
  }

  SQC(ntyp) = 0;
  GC(typ) = vnew(0, sizeof(GC(typ)[0]), PHeap);
  // Reserve the ids of the basic types so the .u values can be used as TypeKinds.
  vgrow(&GC(typ), SQC(ntyp) + SQ_TYPE_0 + 1);
  SQC(ntyp) += SQ_TYPE_0 + 1;

  global_context.main__dbg = config->debug_flags[0] != 0;
  SQC(initialized) = global_context.main__dbg ? SQIS_INITIALIZED_NO_FIN : SQIS_INITIALIZED_EMIT_FIN;
}

bool sq_shutdown(void) {
  SQ_ERR_CHECK(false);

  SQ_ASSERT(SQC(initialized) != SQIS_UNINITIALIZED);
  if (SQC(initialized) == SQIS_INITIALIZED_EMIT_FIN) {
    if (global_context.main__objmode) {
      macho_emitfin_obj(global_context.main__macho_ctx);
      macho_write(global_context.main__macho_ctx, global_context.main__outf);
      macho_free(global_context.main__macho_ctx);
    } else {
      GC(T).emitfin(global_context.main__outf);
    }
  }

  _clear_initialized_state();
  return true;
}

static SqItemCtx _sq_allocate_and_activate_itemctx(void) {
  unsigned int i = 0;
  SqPerFuncState* alloc_pfs = NULL;
  for (i = 0; i < SQ_MAX_FUNC_CONTEXTS; ++i) {
    if (!SQC(saved_per_func_state_in_use)[i]) {
      SQC(saved_per_func_state_in_use)[i] = true;
      alloc_pfs = &SQC(saved_per_func_states)[i];
      break;
    }
  }
  if (i == SQ_MAX_FUNC_CONTEXTS) {
    die("too many func contexts");
  }
  SQ_ASSERT(alloc_pfs);

  _sq_arena_pop_to(alloc_pfs->fn_arena, 0);
  alloc_pfs->ps = 0;
  alloc_pfs->block_pool =
      _sq_arena_push(alloc_pfs->fn_arena, sizeof(Blk) * SQC(config_max_blocks), _Alignof(Blk));
  alloc_pfs->num_blocks = 0;
  alloc_pfs->max_blocks = SQC(config_max_blocks);
  alloc_pfs->curd = (Dat){0};
  alloc_pfs->curd_lnk = (Lnk){0};

  // TODO: probably more that has to be saved/restored from GlobalContext
  alloc_pfs->insb = _sq_arena_push(alloc_pfs->fn_arena, sizeof(Ins) * NIns, _Alignof(Ins));
  alloc_pfs->curi = NULL;
  alloc_pfs->curf = NULL;
  alloc_pfs->curb = NULL;
  alloc_pfs->blink = NULL;
  alloc_pfs->rcls = 0;

  SqItemCtx ctx = {i};
  sq_itemctx_activate(ctx);
  return ctx;
}

SqItemCtx sq_func_start(SqLinkage linkage, SqType return_type, const char* name) {
  SQ_ERR_CHECK((SqItemCtx){0});

  SqItemCtx ctx = _sq_allocate_and_activate_itemctx();

  Lnk lnk = _sqlinkage_to_internal_lnk(linkage);
  lnk.align = 16;

#define G(x) global_context.parse__##x

  G(curb) = 0;
  SQC(pfs.num_blocks) = 0;
  GC(curi) = GC(insb);
  G(curf) = alloc(sizeof *G(curf));
  G(curf)->ntmp = 0;
  G(curf)->ncon = 2;
  G(curf)->tmp = vnew(G(curf)->ntmp, sizeof G(curf)->tmp[0], PFn);
  G(curf)->con = vnew(G(curf)->ncon, sizeof G(curf)->con[0], PFn);
  for (int i = 0; i < Tmp0; ++i) {
    if (GC(T).fpr0 <= i && i < GC(T).fpr0 + GC(T).nfpr) {
      newtmp(0, Kd, G(curf));
    } else {
      newtmp(0, Kl, G(curf));
    }
  }
  G(curf)->con[0].type = CBits;
  G(curf)->con[0].bits.i = 0xdeaddead; /* UNDEF */
  G(curf)->con[1].type = CBits;
  G(curf)->lnk = lnk;
  G(curf)->leaf = 1;
  G(blink) = &G(curf)->start;
  G(rcls) = _sqtype_to_cls_and_ty(return_type, &G(curf)->retty);
  strncpy(G(curf)->name, name, NString - 1);
  SQC(pfs.ps) = PLbl;

  SQC(pfs.start_block) = sq_block_declare_and_start();
  return ctx;
}

SqBlock sq_func_get_entry_block(void) {
  SQ_ERR_CHECK((SqBlock){0});
  return SQC(pfs.start_block);
}

// TODO: return the old one to help push/pop?
void sq_itemctx_activate(SqItemCtx itemctx) {
  SQ_ERR_CHECK_VOID();

  // "write back" the current state to the saved state
  if (SQC(current_itemctx.u < SQ_MAX_FUNC_CONTEXTS)) {
    unsigned int i = SQC(current_itemctx).u;
    SQC(pfs.insb) = global_context.insb;
    SQC(pfs.curi) = global_context.curi;
    SQC(pfs.curf) = G(curf);
    SQC(pfs.curb) = G(curb);
    SQC(pfs.blink) = G(blink);
    SQC(pfs.rcls) = G(rcls);
    SQC(saved_per_func_states)[i] = SQC(pfs);
  }

  // And then set the current state to the provided state.
  SQ_ASSERT(itemctx.u < SQ_MAX_FUNC_CONTEXTS);
  SQC(pfs) = SQC(saved_per_func_states)[itemctx.u];
  // TODO: more?
  global_context.insb = SQC(pfs.insb);
  global_context.curi = SQC(pfs.curi);
  G(curf) = SQC(pfs.curf);
  G(curb) = SQC(pfs.curb);
  G(blink) = SQC(pfs.blink);
  G(rcls) = SQC(pfs.rcls);
  SQC(current_itemctx) = itemctx;
}

SqRef sq_func_param_named(SqType type, const char* name) {
  SQ_ERR_CHECK((SqRef){0});
  int ty;
  Ref r = newtmp(0, Kx, G(curf));
  SQ_NAMED_IF_DEBUG(G(curf)->tmp[r.val].name, name);
  if ((int32_t)type.u == SQ_TYPE_ENV) {
    *GC(curi) = (Ins){Opare, Kl, r, {NULL_R}};
  } else {
    int k = _sqtype_to_cls_and_ty(type, &ty);
    // TODO: varargs
    if (k == Kc) {
      *GC(curi) = (Ins){Oparc, Kl, r, {TYPE(ty)}};
    } else if (k >= Ksb) {
      *GC(curi) = (Ins){Oparsb + (k - Ksb), Kw, r, {NULL_R}};
    } else {
      *GC(curi) = (Ins){Opar, k, r, {NULL_R}};
    }
  }
  ++GC(curi);
  return _internal_ref_to_sqref(r);
}

SqRef sq_const_int(int64_t i) {
  SQ_ERR_CHECK((SqRef){0});
  Con c = {0};
  c.type = CBits;
  c.bits.i = i;
  return _internal_ref_to_sqref(newcon(&c, G(curf)));
}

SqRef sq_const_single(float f) {
  SQ_ERR_CHECK((SqRef){0});
  Con c = {0};
  c.type = CBits;
  c.bits.s = f;
  c.flt = 1;
  return _internal_ref_to_sqref(newcon(&c, G(curf)));
}

SqRef sq_const_double(double d) {
  SQ_ERR_CHECK((SqRef){0});
  Con c = {0};
  c.type = CBits;
  c.bits.d = d;
  c.flt = 2;
  return _internal_ref_to_sqref(newcon(&c, G(curf)));
}

// This has to return a SqSymbol that's the name that we re-lookup on use rather
// than directly a Ref because Con Refs are stored per function (so when calling
// the function, we need to create a new one in the caller).
SqSymbol sq_func_end(void) {
  SQ_ERR_CHECK((SqSymbol){0});
  if (!G(curb)) {
    err_("empty function");
    return (SqSymbol){0};
  }
  if (G(curb)->jmp.type == Jxxx) {
    err_("last block misses jump");
    return (SqSymbol){0};
  }
  G(curf)->mem = vnew(0, sizeof G(curf)->mem[0], PFn);
  G(curf)->nmem = 0;
  G(curf)->nblk = SQC(pfs.num_blocks);
	G(curf)->rpo = vnew(G(nblk), sizeof G(curf)->rpo[0], PFn);
  for (Blk* b = G(curf)->start; b; b = b->link) {
    SQ_ASSERT(b->dlink == 0);
  }
  qbe_parse_typecheck(G(curf));
  if (GC(in_error)) {
    return (SqSymbol){0};
  }

  SqSymbol ret = {intern(G(curf)->name)};

  qbe_main_func(G(curf));

  G(curf) = NULL;

  _sq_arena_pop_to(SQC(pfs.fn_arena), 0);

  SQC(saved_per_func_state_in_use)[SQC(current_itemctx).u] = false;

  return ret;
}

SqRef sq_ref_for_symbol(SqSymbol sym) {
  SQ_ERR_CHECK((SqRef){0});
  SQ_ASSERT(G(curf));
  SQ_ASSERT(sym.u);
  Con c = {0};
  c.type = CAddr;
  c.sym.id = sym.u;
  Ref ret = newcon(&c, G(curf));
  return _internal_ref_to_sqref(ret);
}

SqRef sq_ref_declare(void) {
  SQ_ERR_CHECK((SqRef){0});
  Ref tmp = newtmp(NULL, Kx, G(curf));
  SQ_NAMED_IF_DEBUG(G(curf)->tmp[tmp.val].name, NULL);
  return _internal_ref_to_sqref(tmp);
}

SqRef sq_ref_extern(const char* name) {
  SQ_ERR_CHECK((SqRef){0});
  Con c = {0};
  c.type = CAddr;
  c.sym.id = intern((char*)name);
  Ref ret = newcon(&c, G(curf));
  return _internal_ref_to_sqref(ret);
}

SqBlock sq_block_declare_named(const char* name) {
  SQ_ERR_CHECK((SqBlock){0});
  SQ_ASSERT(SQC(pfs.num_blocks) < SQC(pfs.max_blocks));
  SqBlock ret = {SQC(pfs.num_blocks)++};
  Blk* blk = _sqblock_to_internal_blk(ret);
  memset(blk, 0, sizeof(Blk));
  blk->id = ret.u;
	blk->ins = vnew(0, sizeof blk->ins[0], PFn);
	blk->pred = vnew(0, sizeof blk->pred[0], PFn);
  SQ_NAMED_IF_DEBUG(blk->name, name);
  return ret;
}

void sq_block_start(SqBlock block) {
  SQ_ERR_CHECK_VOID();
  Blk* b = _sqblock_to_internal_blk(block);
  if (G(curb) && G(curb)->jmp.type == Jxxx) {
    qbe_parse_closeblk();
    G(curb)->jmp.type = Jjmp;
    G(curb)->s1 = b;
  }
  if (b->jmp.type != Jxxx) {
    err("multiple definitions of block @%s", b->name);
  }
  *G(blink) = b;
  G(curb) = b;
  G(plink) = &G(curb)->phi;
  SQC(pfs.ps) = PPhi;
}

SqBlock sq_block_declare_and_start_named(const char* name) {
  SQ_ERR_CHECK((SqBlock){0});
  SqBlock new = sq_block_declare_named(name);
  sq_block_start(new);
  return new;
}

void sq_i_ret(SqRef val) {
  SQ_ERR_CHECK_VOID();
  SQ_ASSERT(SQC(pfs.ps) == PIns || SQC(pfs.ps) == PPhi);
  G(curb)->jmp.type = Jretw + G(rcls);
  if (val.u == 0) {
    G(curb)->jmp.type = Jret0;
  } else if (G(rcls) != K0) {
    Ref r = _sqref_to_internal_ref(val);
    if (req(r, NULL_R)) {
      err("invalid return value");
    }
    G(curb)->jmp.arg = r;
  }
  qbe_parse_closeblk();
  SQC(pfs.ps) = PLbl;
}

void sq_i_ret_void(void) {
  SQ_ERR_CHECK_VOID();
  sq_i_ret((SqRef){0});  // TODO: not sure if this is correct == {RTmp, 0}.
}

SqRef sq_i_calla(SqType result, SqRef func, int num_args, SqCallArg* cas) {
  SQ_ERR_CHECK((SqRef){0});
  SQ_ASSERT(SQC(pfs.ps) == PIns || SQC(pfs.ps) == PPhi);
  // args are inserted into instrs first, then the call
  for (int i = 0; i < num_args; ++i) {
    SQ_ASSERT(GC(curi) - GC(insb) < NIns);
    int ty;
    int k;
    Ref r = _sqref_to_internal_ref(cas[i].value);
    if ((int32_t)cas[i].type.u == SQ_TYPE_VARARGS) {
      *GC(curi) = (Ins){.op = Oargv};
    } else if ((int32_t)cas[i].type.u == SQ_TYPE_ENV) {
      *GC(curi) = (Ins){Oarge, Kl, NULL_R, {r}};
    } else {
      k = _sqtype_to_cls_and_ty(cas[i].type, &ty);
      if (k == K0 && req(r, NULL_R)) {
        // This is our hacky special case for where '...' would appear in the call.
      } else if (k == Kc) {
        *GC(curi) = (Ins){Oargc, Kl, NULL_R, {TYPE(ty), r}};
      } else if (k >= Ksb) {
        *GC(curi) = (Ins){Oargsb + (k - Ksb), Kw, NULL_R, {r}};
      } else {
        *GC(curi) = (Ins){Oarg, k, NULL_R, {r}};
      }
    }
    ++GC(curi);
  }

  Ref tmp;

  {
    SQ_ASSERT(GC(curi) - GC(insb) < NIns);
    int ty;
    int k = _sqtype_to_cls_and_ty(result, &ty);
    G(curf)->leaf = 0;
    *GC(curi) = (Ins){0};
    GC(curi)->op = Ocall;
    GC(curi)->arg[0] = _sqref_to_internal_ref(func);
    if (k == Kc) {
      k = Kl;
      GC(curi)->arg[1] = TYPE(ty);
    }
    if (k >= Ksb) {
      k = Kw;
    }
    GC(curi)->cls = k;
    tmp = newtmp(NULL, k, G(curf));
    SQ_NAMED_IF_DEBUG(G(curf)->tmp[tmp.val].name, NULL);
    GC(curi)->to = tmp;
    ++GC(curi);
  }
  SQC(pfs.ps) = PIns;
  return _internal_ref_to_sqref(tmp);
}

SqRef sq_i_call0(SqType result, SqRef func) {
  SQ_ERR_CHECK((SqRef){0});
  return sq_i_calla(result, func, 0, NULL);
}

SqRef sq_i_call1(SqType result, SqRef func, SqCallArg ca0) {
  SQ_ERR_CHECK((SqRef){0});
  return sq_i_calla(result, func, 1, &ca0);
}

SqRef sq_i_call2(SqType result, SqRef func, SqCallArg ca0, SqCallArg ca1) {
  SQ_ERR_CHECK((SqRef){0});
  SqCallArg cas[2] = {ca0, ca1};
  return sq_i_calla(result, func, 2, cas);
}

SqRef sq_i_call3(SqType result, SqRef func, SqCallArg ca0, SqCallArg ca1, SqCallArg ca2) {
  SQ_ERR_CHECK((SqRef){0});
  SqCallArg cas[3] = {ca0, ca1, ca2};
  return sq_i_calla(result, func, 3, cas);
}

SqRef sq_i_call4(SqType result,
                 SqRef func,
                 SqCallArg ca0,
                 SqCallArg ca1,
                 SqCallArg ca2,
                 SqCallArg ca3) {
  SQ_ERR_CHECK((SqRef){0});
  SqCallArg cas[4] = {ca0, ca1, ca2, ca3};
  return sq_i_calla(result, func, 4, cas);
}

SqRef sq_i_call5(SqType result,
                 SqRef func,
                 SqCallArg ca0,
                 SqCallArg ca1,
                 SqCallArg ca2,
                 SqCallArg ca3,
                 SqCallArg ca4) {
  SQ_ERR_CHECK((SqRef){0});
  SqCallArg cas[5] = {ca0, ca1, ca2, ca3, ca4};
  return sq_i_calla(result, func, 5, cas);
}

SqRef sq_i_call6(SqType result,
                 SqRef func,
                 SqCallArg ca0,
                 SqCallArg ca1,
                 SqCallArg ca2,
                 SqCallArg ca3,
                 SqCallArg ca4,
                 SqCallArg ca5) {
  SQ_ERR_CHECK((SqRef){0});
  SqCallArg cas[6] = {ca0, ca1, ca2, ca3, ca4, ca5};
  return sq_i_calla(result, func, 6, cas);
}

SqRef sq_i_call7(SqType result,
                 SqRef func,
                 SqCallArg ca0,
                 SqCallArg ca1,
                 SqCallArg ca2,
                 SqCallArg ca3,
                 SqCallArg ca4,
                 SqCallArg ca5,
                 SqCallArg ca6) {
  SQ_ERR_CHECK((SqRef){0});
  SqCallArg cas[7] = {ca0, ca1, ca2, ca3, ca4, ca5, ca6};
  return sq_i_calla(result, func, 7, cas);
}

SqRef sq_i_call8(SqType result,
                 SqRef func,
                 SqCallArg ca0,
                 SqCallArg ca1,
                 SqCallArg ca2,
                 SqCallArg ca3,
                 SqCallArg ca4,
                 SqCallArg ca5,
                 SqCallArg ca6,
                 SqCallArg ca7) {
  SQ_ERR_CHECK((SqRef){0});
  SqCallArg cas[8] = {ca0, ca1, ca2, ca3, ca4, ca5, ca6, ca7};
  return sq_i_calla(result, func, 8, cas);
}

void sq_i_jmp(SqBlock block) {
  SQ_ERR_CHECK_VOID();

  SQ_ASSERT(SQC(pfs.ps) == PIns || SQC(pfs.ps) == PPhi);
  G(curb)->jmp.type = Jjmp;
  G(curb)->s1 = _sqblock_to_internal_blk(block);
  qbe_parse_closeblk();
}

void sq_i_jnz(SqRef cond, SqBlock if_true, SqBlock if_false) {
  SQ_ERR_CHECK_VOID();
  Ref r = _sqref_to_internal_ref(cond);
  if (req(r, NULL_R)) {
    err("invalid argument for jnz jump");
  }
  G(curb)->jmp.type = Jjnz;
  G(curb)->jmp.arg = r;
  G(curb)->s1 = _sqblock_to_internal_blk(if_true);
  G(curb)->s2 = _sqblock_to_internal_blk(if_false);
  qbe_parse_closeblk();
}

SqRef sq_i_phi(SqType size_class, SqBlock block0, SqRef val0, SqBlock block1, SqRef val1) {
  SQ_ERR_CHECK((SqRef){0});
  if (SQC(pfs.ps) != PPhi || G(curb) == G(curf)->start) {
    err_("unexpected phi instruction");
    return (SqRef){0};
  }

  Ref tmp = newtmp(NULL, Kx, G(curf));
  SQ_NAMED_IF_DEBUG(G(curf)->tmp[tmp.val].name, NULL);

  Phi* phi = alloc(sizeof *phi);
  phi->to = tmp;
  phi->cls = size_class.u;
  int i = 2;  // TODO: variable if necessary
  phi->arg = vnew(i, sizeof(Ref), PFn);
  phi->arg[0] = _sqref_to_internal_ref(val0);
  phi->arg[1] = _sqref_to_internal_ref(val1);
  phi->blk = vnew(i, sizeof(Blk*), PFn);
  phi->blk[0] = _sqblock_to_internal_blk(block0);
  phi->blk[1] = _sqblock_to_internal_blk(block1);
  phi->narg = i;
  *G(plink) = phi;
  G(plink) = &phi->link;
  SQC(pfs.ps) = PPhi;
  return _internal_ref_to_sqref(tmp);
}

void sq_i_blit(SqRef from, SqRef to, int num_bytes) {
  SQ_ERR_CHECK_VOID();
  memset(GC(curi), 0, 2 * sizeof(Ins));
  GC(curi)->op = Oblit0;
  GC(curi)->arg[0] = _sqref_to_internal_ref(from);
  GC(curi)->arg[1] = _sqref_to_internal_ref(to);
  ++GC(curi);
  GC(curi)->op = Oblit1;
  Ref r = INT(num_bytes);
  if (rsval(r) < 0 || rsval(r) != num_bytes) {
    err_("invalid blit size");
  }
  GC(curi)->arg[0] = r;
  ++GC(curi);
  SQC(pfs.ps) = PIns;
}

static void _normal_two_op_instr_into(int op, Ref into, SqType size_class, SqRef arg0, SqRef arg1) {
  SQ_ERR_CHECK_VOID();
  SQ_ASSERT(/*size_class.u >= SQ_TYPE_W && */ size_class.u <= SQ_TYPE_D);
  SQ_ASSERT(SQC(pfs.ps) == PIns || SQC(pfs.ps) == PPhi);
  SQ_ASSERT(GC(curi) - GC(insb) < NIns);
  GC(curi)->op = op;
  GC(curi)->cls = size_class.u;
  GC(curi)->to = into;
  GC(curi)->arg[0] = _sqref_to_internal_ref(arg0);
  GC(curi)->arg[1] = _sqref_to_internal_ref(arg1);
  ++GC(curi);
  SQC(pfs.ps) = PIns;
}

static SqRef _normal_two_op_instr(int op, SqType size_class, SqRef arg0, SqRef arg1) {
  SQ_ERR_CHECK((SqRef){0});
  Ref tmp = newtmp(NULL, Kx, G(curf));
  SQ_NAMED_IF_DEBUG(G(curf)->tmp[tmp.val].name, NULL);
  _normal_two_op_instr_into(op, tmp, size_class, arg0, arg1);
  return _internal_ref_to_sqref(tmp);
}

static void _normal_two_op_void_instr(int op, SqRef arg0, SqRef arg1) {
  SQ_ERR_CHECK_VOID();
  SQ_ASSERT(SQC(pfs.ps) == PIns || SQC(pfs.ps) == PPhi);
  SQ_ASSERT(GC(curi) - GC(insb) < NIns);
  GC(curi)->op = op;
  GC(curi)->cls = SQ_TYPE_W;
  GC(curi)->to = NULL_R;
  GC(curi)->arg[0] = _sqref_to_internal_ref(arg0);
  GC(curi)->arg[1] = _sqref_to_internal_ref(arg1);
  ++GC(curi);
  SQC(pfs.ps) = PIns;
}

static void _normal_one_op_instr_into(int op, Ref into, SqType size_class, SqRef arg0) {
  SQ_ERR_CHECK_VOID();
  SQ_ASSERT(/*size_class.u >= SQ_TYPE_W && */ size_class.u <= SQ_TYPE_D);
  SQ_ASSERT(SQC(pfs.ps) == PIns || SQC(pfs.ps) == PPhi);
  SQ_ASSERT(GC(curi) - GC(insb) < NIns);
  GC(curi)->op = op;
  GC(curi)->cls = size_class.u;
  GC(curi)->to = into;
  GC(curi)->arg[0] = _sqref_to_internal_ref(arg0);
  GC(curi)->arg[1] = NULL_R;
  ++GC(curi);
  SQC(pfs.ps) = PIns;
}

static SqRef _normal_one_op_instr(int op, SqType size_class, SqRef arg0) {
  SQ_ERR_CHECK((SqRef){0});
  Ref tmp = newtmp(NULL, Kx, G(curf));
  SQ_NAMED_IF_DEBUG(G(curf)->tmp[tmp.val].name, NULL);
  _normal_one_op_instr_into(op, tmp, size_class, arg0);
  return _internal_ref_to_sqref(tmp);
}

SqItemCtx sq_data_start(SqLinkage linkage, const char* name) {
  SQ_ERR_CHECK((SqItemCtx){0});

  SqItemCtx ctx = _sq_allocate_and_activate_itemctx();

  SQC(pfs.curd_lnk) = _sqlinkage_to_internal_lnk(linkage);
  if (SQC(pfs.curd_lnk).align == 0) {
    SQC(pfs.curd_lnk).align = 8;
  }

  SQC(pfs.curd) = (Dat){0};
  SQC(pfs.curd).type = DStart;
  SQ_ASSERT(name);
  SQC(pfs.curd).name = (char*)name;
  SQC(pfs.curd).lnk = &SQC(pfs.curd_lnk);
  qbe_main_data(&SQC(pfs.curd));
  if (GC(in_error)) { return (SqItemCtx){0}; }

  return ctx;
}

void sq_data_byte(uint8_t val) {
  SQ_ERR_CHECK_VOID();
  SQC(pfs.curd).isref = 0;
  SQC(pfs.curd).isstr = 0;
  SQC(pfs.curd).type = DB;
  SQC(pfs.curd).u.num = val;
  qbe_main_data(&SQC(pfs.curd));
  // qbe_main_data need ret_on_err, but below here they're all tailcalls anyway.
}

void sq_data_half(uint16_t val) {
  SQ_ERR_CHECK_VOID();
  SQC(pfs.curd).isref = 0;
  SQC(pfs.curd).isstr = 0;
  SQC(pfs.curd).type = DH;
  SQC(pfs.curd).u.num = val;
  qbe_main_data(&SQC(pfs.curd));
}

void sq_data_word(uint32_t val) {
  SQ_ERR_CHECK_VOID();
  SQC(pfs.curd).isref = 0;
  SQC(pfs.curd).isstr = 0;
  SQC(pfs.curd).type = DW;
  SQC(pfs.curd).u.num = val;
  qbe_main_data(&SQC(pfs.curd));
}

void sq_data_long(uint64_t val) {
  SQ_ERR_CHECK_VOID();
  SQC(pfs.curd).isref = 0;
  SQC(pfs.curd).isstr = 0;
  SQC(pfs.curd).type = DL;
  SQC(pfs.curd).u.num = val;
  qbe_main_data(&SQC(pfs.curd));
}

void sq_data_single(float val) {
  SQ_ERR_CHECK_VOID();
  SQC(pfs.curd).isref = 0;
  SQC(pfs.curd).isstr = 0;
  SQC(pfs.curd).type = DW;
  SQC(pfs.curd).u.flts = val;
  qbe_main_data(&SQC(pfs.curd));
}

void sq_data_double(double val) {
  SQ_ERR_CHECK_VOID();
  SQC(pfs.curd).isref = 0;
  SQC(pfs.curd).isstr = 0;
  SQC(pfs.curd).type = DL;
  SQC(pfs.curd).u.fltd = val;
  qbe_main_data(&SQC(pfs.curd));
}

void sq_data_ref(SqSymbol ref, int64_t offset) {
  SQ_ERR_CHECK_VOID();
  SQC(pfs.curd).isref = 1;
  SQC(pfs.curd).isstr = 0;
  SQC(pfs.curd).type = DL;
  SQC(pfs.curd).u.ref.name = str(ref.u);
  SQC(pfs.curd).u.ref.off = offset;
  qbe_main_data(&SQC(pfs.curd));
}

static size_t _str_repr(const char* str, char* into) {
  size_t at = 0;

#define EMIT(x)     \
  do {              \
    if (into)       \
      into[at] = x; \
    ++at;           \
  } while (0)

  EMIT('"');

  for (const char* p = str; *p; ++p) {
    switch (*p) {
      case '"':
        EMIT('\\');
        EMIT('"');
        break;
      case '\\':
        EMIT('\\');
        EMIT('"');
        break;
      case '\r':
        EMIT('\\');
        EMIT('r');
        break;
      case '\n':
        EMIT('\\');
        EMIT('n');
        break;
      case '\t':
        EMIT('\\');
        EMIT('t');
        break;
      case '\0':
        EMIT('\\');
        EMIT('0');
        break;
      default:
        EMIT(*p);
        break;
    }
  }

  EMIT('"');
  EMIT(0);

  return at;
}

void sq_data_string(const char* str) {
  SQ_ERR_CHECK_VOID();
  SQC(pfs.curd).type = DB;
  SQC(pfs.curd).isstr = 1;
  // QBE sneakily avoids de-escaping in the tokenizer and re-escaping during
  // emission by just not handling escapes at all and relying on the input
  // format for string escapes being the same as the assembler's. Because we're
  // getting our strings from C here, we need to "repr" it.
  size_t len = _str_repr(str, NULL);
  char* escaped = alloca(len);
  size_t len2 = _str_repr(str, escaped);
  SQ_ASSERT(len == len2);
  (void)len2;
  SQC(pfs.curd).u.str = (char*)escaped;
  qbe_main_data(&SQC(pfs.curd));
}

SqSymbol sq_data_end(void) {
  SQ_ERR_CHECK((SqSymbol){0});
  SQC(pfs.curd).isref = 0;
  SQC(pfs.curd).isstr = 0;
  SQC(pfs.curd).type = DEnd;
  qbe_main_data(&SQC(pfs.curd));
  if (GC(in_error)) { return (SqSymbol){0}; }

  SqSymbol ret = {intern(SQC(pfs.curd).name)};
  SQC(pfs.curd) = (Dat){0};
  SQC(pfs.curd_lnk) = (Lnk){0};
  SQC(saved_per_func_state_in_use)[SQC(current_itemctx).u] = false;
  return ret;
}

// Types are a bit questionable (or at least "minimal") in QBE. The specific
// field details are required for determining how to pass at the ABI level
// properly, but in practice, the ABI's only need to know about the first 16 or
// 32 byte of the structure (for example, to determine if the structure should
// be passed in int regs, float regs, or on the stack as a pointer). So, while
// QBE appears to define an arbitrary number of fields, it just drops the
// details of fields beyond the 32nd (but still updates overall struct
// size/alignment for additional values).
void sq_type_struct_start(const char* name, int align) {
  SQ_ERR_CHECK_VOID();
  vgrow(&GC(typ), SQC(ntyp) + 1);
  SQC(curty) = &GC(typ)[SQC(ntyp)++];
  SQC(curty)->isdark = 0;
  SQC(curty)->isunion = 0;
  SQC(curty)->align = -1;
  SQC(curty_build_n) = 0;

  if (align > 0) {
    int al;
    for (al = 0; align /= 2; al++)
      ;
    SQC(curty)->align = al;
  }

  SQC(curty)->size = 0;
  strcpy(SQC(curty)->name, name);
  SQC(curty)->fields = vnew(1, sizeof SQC(curty)->fields[0], PHeap);
  SQC(curty)->nunion = 1;

  SQC(curty_build_sz) = 0;
  SQC(curty_build_al) = SQC(curty)->align;
}

void sq_type_add_field_with_count(SqType field, uint32_t count) {
  SQ_ERR_CHECK_VOID();
  Field* fld = SQC(curty)->fields[0];

  Typ* ty1;
  uint64_t s;
  int a;

  int type;
  int ty;
  int cls = _sqtype_to_cls_and_ty(field, &ty);
  switch (cls) {
    case SQ_TYPE_D:
      type = Fd;
      s = 8;
      a = 3;
      break;
    case SQ_TYPE_L:
      type = Fl;
      s = 8;
      a = 3;
      break;
    case SQ_TYPE_S:
      type = Fs;
      s = 4;
      a = 2;
      break;
    case SQ_TYPE_W:
      type = Fw;
      s = 4;
      a = 2;
      break;
    case SQ_TYPE_SH:
      type = Fh;
      s = 2;
      a = 1;
      break;
    case SQ_TYPE_UH:
      type = Fh;
      s = 2;
      a = 1;
      break;
    case SQ_TYPE_SB:
      type = Fb;
      s = 1;
      a = 0;
      break;
    case SQ_TYPE_UB:
      type = Fb;
      s = 1;
      a = 0;
      break;
    default:
      type = FTyp;
      ty1 = &GC(typ)[field.u];
      s = ty1->size;
      a = ty1->align;
      break;
  }

  if (a > SQC(curty_build_al)) {
    SQC(curty_build_al) = a;
  }
  a = (1 << a) - 1;
  a = ((SQC(curty_build_sz) + a) & ~a) - SQC(curty_build_sz);
  if (a) {
    if (SQC(curty_build_n) < NField) {
      fld[SQC(curty_build_n)].type = FPad;
      fld[SQC(curty_build_n)].len = a;
      SQC(curty_build_n)++;
    }
  }
  SQC(curty_build_sz) += a + count * s;
  if (type == FTyp) {
    s = field.u;
  }
  for (; count > 0 && SQC(curty_build_n) < NField; count--, SQC(curty_build_n)++) {
    fld[SQC(curty_build_n)].type = type;
    fld[SQC(curty_build_n)].len = s;
  }
}

void sq_type_add_field(SqType field) {
  SQ_ERR_CHECK_VOID();
  sq_type_add_field_with_count(field, 1);
}

SqType sq_type_struct_end(void) {
  SQ_ERR_CHECK((SqType){0});
  Field* fld = SQC(curty)->fields[0];
  fld[SQC(curty_build_n)].type = FEnd;
  int a = 1 << SQC(curty_build_al);
  if (SQC(curty_build_sz) < SQC(curty)->size) {
    SQC(curty_build_sz) = SQC(curty)->size;
  }
  SQC(curty)->size = (SQC(curty_build_sz) + a - 1) & -a;
  SQC(curty)->align = SQC(curty_build_al);
  if (GC(debug)['T']) {
    fprintf(stderr, "\n> Parsed type:\n");
    printtyp(SQC(curty), stderr);
  }
  SqType ret = {SQC(curty) - GC(typ)};
  SQC(curty) = NULL;
  SQC(curty_build_n) = 0;
  SQC(curty_build_sz) = 0;
  SQC(curty_build_al) = 0;
  return ret;
}
#undef G
/*** END FILE: ../sqbe_impl.c ***/
SqRef sq_i_add(SqType size_class, SqRef arg0 /*wlsd*/, SqRef arg1 /*wlsd*/) { return _normal_two_op_instr(Oadd, size_class, arg0, arg1); }
void sq_i_add_into(SqRef into, SqType size_class, SqRef arg0 /*wlsd*/, SqRef arg1 /*wlsd*/) { _normal_two_op_instr_into(Oadd, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_sub(SqType size_class, SqRef arg0 /*wlsd*/, SqRef arg1 /*wlsd*/) { return _normal_two_op_instr(Osub, size_class, arg0, arg1); }
void sq_i_sub_into(SqRef into, SqType size_class, SqRef arg0 /*wlsd*/, SqRef arg1 /*wlsd*/) { _normal_two_op_instr_into(Osub, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_neg(SqType size_class, SqRef arg0 /*wlsd*/) { return _normal_one_op_instr(Oneg, size_class, arg0); }
void sq_i_neg_into(SqRef into, SqType size_class, SqRef arg0 /*wlsd*/) { _normal_one_op_instr_into(Oneg, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_div(SqType size_class, SqRef arg0 /*wlsd*/, SqRef arg1 /*wlsd*/) { return _normal_two_op_instr(Odiv, size_class, arg0, arg1); }
void sq_i_div_into(SqRef into, SqType size_class, SqRef arg0 /*wlsd*/, SqRef arg1 /*wlsd*/) { _normal_two_op_instr_into(Odiv, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_rem(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/) { return _normal_two_op_instr(Orem, size_class, arg0, arg1); }
void sq_i_rem_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/) { _normal_two_op_instr_into(Orem, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_udiv(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/) { return _normal_two_op_instr(Oudiv, size_class, arg0, arg1); }
void sq_i_udiv_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/) { _normal_two_op_instr_into(Oudiv, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_urem(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/) { return _normal_two_op_instr(Ourem, size_class, arg0, arg1); }
void sq_i_urem_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/) { _normal_two_op_instr_into(Ourem, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_mul(SqType size_class, SqRef arg0 /*wlsd*/, SqRef arg1 /*wlsd*/) { return _normal_two_op_instr(Omul, size_class, arg0, arg1); }
void sq_i_mul_into(SqRef into, SqType size_class, SqRef arg0 /*wlsd*/, SqRef arg1 /*wlsd*/) { _normal_two_op_instr_into(Omul, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_and(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/) { return _normal_two_op_instr(Oand, size_class, arg0, arg1); }
void sq_i_and_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/) { _normal_two_op_instr_into(Oand, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_or(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/) { return _normal_two_op_instr(Oor, size_class, arg0, arg1); }
void sq_i_or_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/) { _normal_two_op_instr_into(Oor, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_xor(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/) { return _normal_two_op_instr(Oxor, size_class, arg0, arg1); }
void sq_i_xor_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wlee*/) { _normal_two_op_instr_into(Oxor, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_sar(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wwee*/) { return _normal_two_op_instr(Osar, size_class, arg0, arg1); }
void sq_i_sar_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wwee*/) { _normal_two_op_instr_into(Osar, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_shr(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wwee*/) { return _normal_two_op_instr(Oshr, size_class, arg0, arg1); }
void sq_i_shr_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wwee*/) { _normal_two_op_instr_into(Oshr, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_shl(SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wwee*/) { return _normal_two_op_instr(Oshl, size_class, arg0, arg1); }
void sq_i_shl_into(SqRef into, SqType size_class, SqRef arg0 /*wlee*/, SqRef arg1 /*wwee*/) { _normal_two_op_instr_into(Oshl, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_ceqw(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { return _normal_two_op_instr(Oceqw, size_class, arg0, arg1); }
void sq_i_ceqw_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { _normal_two_op_instr_into(Oceqw, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cnew(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { return _normal_two_op_instr(Ocnew, size_class, arg0, arg1); }
void sq_i_cnew_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { _normal_two_op_instr_into(Ocnew, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_csgew(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { return _normal_two_op_instr(Ocsgew, size_class, arg0, arg1); }
void sq_i_csgew_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { _normal_two_op_instr_into(Ocsgew, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_csgtw(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { return _normal_two_op_instr(Ocsgtw, size_class, arg0, arg1); }
void sq_i_csgtw_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { _normal_two_op_instr_into(Ocsgtw, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cslew(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { return _normal_two_op_instr(Ocslew, size_class, arg0, arg1); }
void sq_i_cslew_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { _normal_two_op_instr_into(Ocslew, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_csltw(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { return _normal_two_op_instr(Ocsltw, size_class, arg0, arg1); }
void sq_i_csltw_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { _normal_two_op_instr_into(Ocsltw, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cugew(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { return _normal_two_op_instr(Ocugew, size_class, arg0, arg1); }
void sq_i_cugew_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { _normal_two_op_instr_into(Ocugew, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cugtw(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { return _normal_two_op_instr(Ocugtw, size_class, arg0, arg1); }
void sq_i_cugtw_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { _normal_two_op_instr_into(Ocugtw, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_culew(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { return _normal_two_op_instr(Oculew, size_class, arg0, arg1); }
void sq_i_culew_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { _normal_two_op_instr_into(Oculew, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cultw(SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { return _normal_two_op_instr(Ocultw, size_class, arg0, arg1); }
void sq_i_cultw_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/, SqRef arg1 /*wwee*/) { _normal_two_op_instr_into(Ocultw, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_ceql(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { return _normal_two_op_instr(Oceql, size_class, arg0, arg1); }
void sq_i_ceql_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { _normal_two_op_instr_into(Oceql, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cnel(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { return _normal_two_op_instr(Ocnel, size_class, arg0, arg1); }
void sq_i_cnel_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { _normal_two_op_instr_into(Ocnel, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_csgel(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { return _normal_two_op_instr(Ocsgel, size_class, arg0, arg1); }
void sq_i_csgel_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { _normal_two_op_instr_into(Ocsgel, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_csgtl(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { return _normal_two_op_instr(Ocsgtl, size_class, arg0, arg1); }
void sq_i_csgtl_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { _normal_two_op_instr_into(Ocsgtl, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cslel(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { return _normal_two_op_instr(Ocslel, size_class, arg0, arg1); }
void sq_i_cslel_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { _normal_two_op_instr_into(Ocslel, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_csltl(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { return _normal_two_op_instr(Ocsltl, size_class, arg0, arg1); }
void sq_i_csltl_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { _normal_two_op_instr_into(Ocsltl, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cugel(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { return _normal_two_op_instr(Ocugel, size_class, arg0, arg1); }
void sq_i_cugel_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { _normal_two_op_instr_into(Ocugel, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cugtl(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { return _normal_two_op_instr(Ocugtl, size_class, arg0, arg1); }
void sq_i_cugtl_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { _normal_two_op_instr_into(Ocugtl, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_culel(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { return _normal_two_op_instr(Oculel, size_class, arg0, arg1); }
void sq_i_culel_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { _normal_two_op_instr_into(Oculel, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cultl(SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { return _normal_two_op_instr(Ocultl, size_class, arg0, arg1); }
void sq_i_cultl_into(SqRef into, SqType size_class, SqRef arg0 /*llee*/, SqRef arg1 /*llee*/) { _normal_two_op_instr_into(Ocultl, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_ceqs(SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/) { return _normal_two_op_instr(Oceqs, size_class, arg0, arg1); }
void sq_i_ceqs_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/) { _normal_two_op_instr_into(Oceqs, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cges(SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/) { return _normal_two_op_instr(Ocges, size_class, arg0, arg1); }
void sq_i_cges_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/) { _normal_two_op_instr_into(Ocges, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cgts(SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/) { return _normal_two_op_instr(Ocgts, size_class, arg0, arg1); }
void sq_i_cgts_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/) { _normal_two_op_instr_into(Ocgts, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cles(SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/) { return _normal_two_op_instr(Ocles, size_class, arg0, arg1); }
void sq_i_cles_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/) { _normal_two_op_instr_into(Ocles, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_clts(SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/) { return _normal_two_op_instr(Oclts, size_class, arg0, arg1); }
void sq_i_clts_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/) { _normal_two_op_instr_into(Oclts, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cnes(SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/) { return _normal_two_op_instr(Ocnes, size_class, arg0, arg1); }
void sq_i_cnes_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/) { _normal_two_op_instr_into(Ocnes, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cos(SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/) { return _normal_two_op_instr(Ocos, size_class, arg0, arg1); }
void sq_i_cos_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/) { _normal_two_op_instr_into(Ocos, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cuos(SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/) { return _normal_two_op_instr(Ocuos, size_class, arg0, arg1); }
void sq_i_cuos_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/, SqRef arg1 /*ssee*/) { _normal_two_op_instr_into(Ocuos, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_ceqd(SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/) { return _normal_two_op_instr(Oceqd, size_class, arg0, arg1); }
void sq_i_ceqd_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/) { _normal_two_op_instr_into(Oceqd, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cged(SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/) { return _normal_two_op_instr(Ocged, size_class, arg0, arg1); }
void sq_i_cged_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/) { _normal_two_op_instr_into(Ocged, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cgtd(SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/) { return _normal_two_op_instr(Ocgtd, size_class, arg0, arg1); }
void sq_i_cgtd_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/) { _normal_two_op_instr_into(Ocgtd, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cled(SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/) { return _normal_two_op_instr(Ocled, size_class, arg0, arg1); }
void sq_i_cled_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/) { _normal_two_op_instr_into(Ocled, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cltd(SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/) { return _normal_two_op_instr(Ocltd, size_class, arg0, arg1); }
void sq_i_cltd_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/) { _normal_two_op_instr_into(Ocltd, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cned(SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/) { return _normal_two_op_instr(Ocned, size_class, arg0, arg1); }
void sq_i_cned_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/) { _normal_two_op_instr_into(Ocned, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cod(SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/) { return _normal_two_op_instr(Ocod, size_class, arg0, arg1); }
void sq_i_cod_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/) { _normal_two_op_instr_into(Ocod, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
SqRef sq_i_cuod(SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/) { return _normal_two_op_instr(Ocuod, size_class, arg0, arg1); }
void sq_i_cuod_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/, SqRef arg1 /*ddee*/) { _normal_two_op_instr_into(Ocuod, _sqref_to_internal_ref(into), size_class, arg0, arg1); }
void sq_i_storeb(SqRef arg0 /*weee*/, SqRef arg1 /*meee*/) { _normal_two_op_void_instr(Ostoreb, arg0, arg1); }
void sq_i_storeh(SqRef arg0 /*weee*/, SqRef arg1 /*meee*/) { _normal_two_op_void_instr(Ostoreh, arg0, arg1); }
void sq_i_storew(SqRef arg0 /*weee*/, SqRef arg1 /*meee*/) { _normal_two_op_void_instr(Ostorew, arg0, arg1); }
void sq_i_storel(SqRef arg0 /*leee*/, SqRef arg1 /*meee*/) { _normal_two_op_void_instr(Ostorel, arg0, arg1); }
void sq_i_stores(SqRef arg0 /*seee*/, SqRef arg1 /*meee*/) { _normal_two_op_void_instr(Ostores, arg0, arg1); }
void sq_i_stored(SqRef arg0 /*deee*/, SqRef arg1 /*meee*/) { _normal_two_op_void_instr(Ostored, arg0, arg1); }
SqRef sq_i_loadsb(SqType size_class, SqRef arg0 /*mmee*/) { return _normal_one_op_instr(Oloadsb, size_class, arg0); }
void sq_i_loadsb_into(SqRef into, SqType size_class, SqRef arg0 /*mmee*/) { _normal_one_op_instr_into(Oloadsb, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_loadub(SqType size_class, SqRef arg0 /*mmee*/) { return _normal_one_op_instr(Oloadub, size_class, arg0); }
void sq_i_loadub_into(SqRef into, SqType size_class, SqRef arg0 /*mmee*/) { _normal_one_op_instr_into(Oloadub, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_loadsh(SqType size_class, SqRef arg0 /*mmee*/) { return _normal_one_op_instr(Oloadsh, size_class, arg0); }
void sq_i_loadsh_into(SqRef into, SqType size_class, SqRef arg0 /*mmee*/) { _normal_one_op_instr_into(Oloadsh, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_loaduh(SqType size_class, SqRef arg0 /*mmee*/) { return _normal_one_op_instr(Oloaduh, size_class, arg0); }
void sq_i_loaduh_into(SqRef into, SqType size_class, SqRef arg0 /*mmee*/) { _normal_one_op_instr_into(Oloaduh, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_loadsw(SqType size_class, SqRef arg0 /*mmee*/) { return _normal_one_op_instr(Oloadsw, size_class, arg0); }
void sq_i_loadsw_into(SqRef into, SqType size_class, SqRef arg0 /*mmee*/) { _normal_one_op_instr_into(Oloadsw, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_loaduw(SqType size_class, SqRef arg0 /*mmee*/) { return _normal_one_op_instr(Oloaduw, size_class, arg0); }
void sq_i_loaduw_into(SqRef into, SqType size_class, SqRef arg0 /*mmee*/) { _normal_one_op_instr_into(Oloaduw, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_load(SqType size_class, SqRef arg0 /*mmmm*/) { return _normal_one_op_instr(Oload, size_class, arg0); }
void sq_i_load_into(SqRef into, SqType size_class, SqRef arg0 /*mmmm*/) { _normal_one_op_instr_into(Oload, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_extsb(SqType size_class, SqRef arg0 /*wwee*/) { return _normal_one_op_instr(Oextsb, size_class, arg0); }
void sq_i_extsb_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/) { _normal_one_op_instr_into(Oextsb, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_extub(SqType size_class, SqRef arg0 /*wwee*/) { return _normal_one_op_instr(Oextub, size_class, arg0); }
void sq_i_extub_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/) { _normal_one_op_instr_into(Oextub, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_extsh(SqType size_class, SqRef arg0 /*wwee*/) { return _normal_one_op_instr(Oextsh, size_class, arg0); }
void sq_i_extsh_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/) { _normal_one_op_instr_into(Oextsh, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_extuh(SqType size_class, SqRef arg0 /*wwee*/) { return _normal_one_op_instr(Oextuh, size_class, arg0); }
void sq_i_extuh_into(SqRef into, SqType size_class, SqRef arg0 /*wwee*/) { _normal_one_op_instr_into(Oextuh, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_extsw(SqRef arg0 /*ewee*/) { return _normal_one_op_instr(Oextsw, sq_type_long, arg0); }
void sq_i_extsw_into(SqRef into, SqRef arg0 /*ewee*/) { _normal_one_op_instr_into(Oextsw, _sqref_to_internal_ref(into), sq_type_long, arg0); }
SqRef sq_i_extuw(SqRef arg0 /*ewee*/) { return _normal_one_op_instr(Oextuw, sq_type_long, arg0); }
void sq_i_extuw_into(SqRef into, SqRef arg0 /*ewee*/) { _normal_one_op_instr_into(Oextuw, _sqref_to_internal_ref(into), sq_type_long, arg0); }
SqRef sq_i_exts(SqRef arg0 /*eees*/) { return _normal_one_op_instr(Oexts, sq_type_double, arg0); }
void sq_i_exts_into(SqRef into, SqRef arg0 /*eees*/) { _normal_one_op_instr_into(Oexts, _sqref_to_internal_ref(into), sq_type_double, arg0); }
SqRef sq_i_truncd(SqRef arg0 /*eede*/) { return _normal_one_op_instr(Otruncd, sq_type_single, arg0); }
void sq_i_truncd_into(SqRef into, SqRef arg0 /*eede*/) { _normal_one_op_instr_into(Otruncd, _sqref_to_internal_ref(into), sq_type_single, arg0); }
SqRef sq_i_stosi(SqType size_class, SqRef arg0 /*ssee*/) { return _normal_one_op_instr(Ostosi, size_class, arg0); }
void sq_i_stosi_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/) { _normal_one_op_instr_into(Ostosi, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_stoui(SqType size_class, SqRef arg0 /*ssee*/) { return _normal_one_op_instr(Ostoui, size_class, arg0); }
void sq_i_stoui_into(SqRef into, SqType size_class, SqRef arg0 /*ssee*/) { _normal_one_op_instr_into(Ostoui, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_dtosi(SqType size_class, SqRef arg0 /*ddee*/) { return _normal_one_op_instr(Odtosi, size_class, arg0); }
void sq_i_dtosi_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/) { _normal_one_op_instr_into(Odtosi, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_dtoui(SqType size_class, SqRef arg0 /*ddee*/) { return _normal_one_op_instr(Odtoui, size_class, arg0); }
void sq_i_dtoui_into(SqRef into, SqType size_class, SqRef arg0 /*ddee*/) { _normal_one_op_instr_into(Odtoui, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_swtof(SqType size_class, SqRef arg0 /*eeww*/) { return _normal_one_op_instr(Oswtof, size_class, arg0); }
void sq_i_swtof_into(SqRef into, SqType size_class, SqRef arg0 /*eeww*/) { _normal_one_op_instr_into(Oswtof, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_uwtof(SqType size_class, SqRef arg0 /*eeww*/) { return _normal_one_op_instr(Ouwtof, size_class, arg0); }
void sq_i_uwtof_into(SqRef into, SqType size_class, SqRef arg0 /*eeww*/) { _normal_one_op_instr_into(Ouwtof, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_sltof(SqType size_class, SqRef arg0 /*eell*/) { return _normal_one_op_instr(Osltof, size_class, arg0); }
void sq_i_sltof_into(SqRef into, SqType size_class, SqRef arg0 /*eell*/) { _normal_one_op_instr_into(Osltof, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_ultof(SqType size_class, SqRef arg0 /*eell*/) { return _normal_one_op_instr(Oultof, size_class, arg0); }
void sq_i_ultof_into(SqRef into, SqType size_class, SqRef arg0 /*eell*/) { _normal_one_op_instr_into(Oultof, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_cast(SqType size_class, SqRef arg0 /*sdwl*/) { return _normal_one_op_instr(Ocast, size_class, arg0); }
void sq_i_cast_into(SqRef into, SqType size_class, SqRef arg0 /*sdwl*/) { _normal_one_op_instr_into(Ocast, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_alloc4(SqRef arg0 /*elee*/) { return _normal_one_op_instr(Oalloc4, sq_type_long, arg0); }
void sq_i_alloc4_into(SqRef into, SqRef arg0 /*elee*/) { _normal_one_op_instr_into(Oalloc4, _sqref_to_internal_ref(into), sq_type_long, arg0); }
SqRef sq_i_alloc8(SqRef arg0 /*elee*/) { return _normal_one_op_instr(Oalloc8, sq_type_long, arg0); }
void sq_i_alloc8_into(SqRef into, SqRef arg0 /*elee*/) { _normal_one_op_instr_into(Oalloc8, _sqref_to_internal_ref(into), sq_type_long, arg0); }
SqRef sq_i_alloc16(SqRef arg0 /*elee*/) { return _normal_one_op_instr(Oalloc16, sq_type_long, arg0); }
void sq_i_alloc16_into(SqRef into, SqRef arg0 /*elee*/) { _normal_one_op_instr_into(Oalloc16, _sqref_to_internal_ref(into), sq_type_long, arg0); }
SqRef sq_i_vaarg(SqType size_class, SqRef arg0 /*mmmm*/) { return _normal_one_op_instr(Ovaarg, size_class, arg0); }
void sq_i_vaarg_into(SqRef into, SqType size_class, SqRef arg0 /*mmmm*/) { _normal_one_op_instr_into(Ovaarg, _sqref_to_internal_ref(into), size_class, arg0); }
SqRef sq_i_vastart(SqRef arg0 /*meee*/) { return _normal_one_op_instr(Ovastart, sq_type_word, arg0); }
void sq_i_vastart_into(SqRef into, SqRef arg0 /*meee*/) { _normal_one_op_instr_into(Ovastart, _sqref_to_internal_ref(into), sq_type_word, arg0); }
SqRef sq_i_copy(SqType size_class, SqRef arg0 /*wlsd*/) { return _normal_one_op_instr(Ocopy, size_class, arg0); }
void sq_i_copy_into(SqRef into, SqType size_class, SqRef arg0 /*wlsd*/) { _normal_one_op_instr_into(Ocopy, _sqref_to_internal_ref(into), size_class, arg0); }
void sq_i_dbgloc(SqRef arg0 /*weee*/, SqRef arg1 /*weee*/) { _normal_two_op_void_instr(Odbgloc, arg0, arg1); }

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // SQBE_IMPLEMENTATION


#ifdef SQBE_NOOP
#undef SQBE_NOOP

// --------------------------
//    NO-OP IMPLEMENTATION
// --------------------------

#define sq_i_add(s, a0, a1) (SqRef){0}
#define sq_i_add_into(into, s, a0, a1)
#define sq_i_sub(s, a0, a1) (SqRef){0}
#define sq_i_sub_into(into, s, a0, a1)
#define sq_i_neg(s, a0) (SqRef){0}
#define sq_i_neg_into(into, s, a0)
#define sq_i_div(s, a0, a1) (SqRef){0}
#define sq_i_div_into(into, s, a0, a1)
#define sq_i_rem(s, a0, a1) (SqRef){0}
#define sq_i_rem_into(into, s, a0, a1)
#define sq_i_udiv(s, a0, a1) (SqRef){0}
#define sq_i_udiv_into(into, s, a0, a1)
#define sq_i_urem(s, a0, a1) (SqRef){0}
#define sq_i_urem_into(into, s, a0, a1)
#define sq_i_mul(s, a0, a1) (SqRef){0}
#define sq_i_mul_into(into, s, a0, a1)
#define sq_i_and(s, a0, a1) (SqRef){0}
#define sq_i_and_into(into, s, a0, a1)
#define sq_i_or(s, a0, a1) (SqRef){0}
#define sq_i_or_into(into, s, a0, a1)
#define sq_i_xor(s, a0, a1) (SqRef){0}
#define sq_i_xor_into(into, s, a0, a1)
#define sq_i_sar(s, a0, a1) (SqRef){0}
#define sq_i_sar_into(into, s, a0, a1)
#define sq_i_shr(s, a0, a1) (SqRef){0}
#define sq_i_shr_into(into, s, a0, a1)
#define sq_i_shl(s, a0, a1) (SqRef){0}
#define sq_i_shl_into(into, s, a0, a1)
#define sq_i_ceqw(s, a0, a1) (SqRef){0}
#define sq_i_ceqw_into(into, s, a0, a1)
#define sq_i_cnew(s, a0, a1) (SqRef){0}
#define sq_i_cnew_into(into, s, a0, a1)
#define sq_i_csgew(s, a0, a1) (SqRef){0}
#define sq_i_csgew_into(into, s, a0, a1)
#define sq_i_csgtw(s, a0, a1) (SqRef){0}
#define sq_i_csgtw_into(into, s, a0, a1)
#define sq_i_cslew(s, a0, a1) (SqRef){0}
#define sq_i_cslew_into(into, s, a0, a1)
#define sq_i_csltw(s, a0, a1) (SqRef){0}
#define sq_i_csltw_into(into, s, a0, a1)
#define sq_i_cugew(s, a0, a1) (SqRef){0}
#define sq_i_cugew_into(into, s, a0, a1)
#define sq_i_cugtw(s, a0, a1) (SqRef){0}
#define sq_i_cugtw_into(into, s, a0, a1)
#define sq_i_culew(s, a0, a1) (SqRef){0}
#define sq_i_culew_into(into, s, a0, a1)
#define sq_i_cultw(s, a0, a1) (SqRef){0}
#define sq_i_cultw_into(into, s, a0, a1)
#define sq_i_ceql(s, a0, a1) (SqRef){0}
#define sq_i_ceql_into(into, s, a0, a1)
#define sq_i_cnel(s, a0, a1) (SqRef){0}
#define sq_i_cnel_into(into, s, a0, a1)
#define sq_i_csgel(s, a0, a1) (SqRef){0}
#define sq_i_csgel_into(into, s, a0, a1)
#define sq_i_csgtl(s, a0, a1) (SqRef){0}
#define sq_i_csgtl_into(into, s, a0, a1)
#define sq_i_cslel(s, a0, a1) (SqRef){0}
#define sq_i_cslel_into(into, s, a0, a1)
#define sq_i_csltl(s, a0, a1) (SqRef){0}
#define sq_i_csltl_into(into, s, a0, a1)
#define sq_i_cugel(s, a0, a1) (SqRef){0}
#define sq_i_cugel_into(into, s, a0, a1)
#define sq_i_cugtl(s, a0, a1) (SqRef){0}
#define sq_i_cugtl_into(into, s, a0, a1)
#define sq_i_culel(s, a0, a1) (SqRef){0}
#define sq_i_culel_into(into, s, a0, a1)
#define sq_i_cultl(s, a0, a1) (SqRef){0}
#define sq_i_cultl_into(into, s, a0, a1)
#define sq_i_ceqs(s, a0, a1) (SqRef){0}
#define sq_i_ceqs_into(into, s, a0, a1)
#define sq_i_cges(s, a0, a1) (SqRef){0}
#define sq_i_cges_into(into, s, a0, a1)
#define sq_i_cgts(s, a0, a1) (SqRef){0}
#define sq_i_cgts_into(into, s, a0, a1)
#define sq_i_cles(s, a0, a1) (SqRef){0}
#define sq_i_cles_into(into, s, a0, a1)
#define sq_i_clts(s, a0, a1) (SqRef){0}
#define sq_i_clts_into(into, s, a0, a1)
#define sq_i_cnes(s, a0, a1) (SqRef){0}
#define sq_i_cnes_into(into, s, a0, a1)
#define sq_i_cos(s, a0, a1) (SqRef){0}
#define sq_i_cos_into(into, s, a0, a1)
#define sq_i_cuos(s, a0, a1) (SqRef){0}
#define sq_i_cuos_into(into, s, a0, a1)
#define sq_i_ceqd(s, a0, a1) (SqRef){0}
#define sq_i_ceqd_into(into, s, a0, a1)
#define sq_i_cged(s, a0, a1) (SqRef){0}
#define sq_i_cged_into(into, s, a0, a1)
#define sq_i_cgtd(s, a0, a1) (SqRef){0}
#define sq_i_cgtd_into(into, s, a0, a1)
#define sq_i_cled(s, a0, a1) (SqRef){0}
#define sq_i_cled_into(into, s, a0, a1)
#define sq_i_cltd(s, a0, a1) (SqRef){0}
#define sq_i_cltd_into(into, s, a0, a1)
#define sq_i_cned(s, a0, a1) (SqRef){0}
#define sq_i_cned_into(into, s, a0, a1)
#define sq_i_cod(s, a0, a1) (SqRef){0}
#define sq_i_cod_into(into, s, a0, a1)
#define sq_i_cuod(s, a0, a1) (SqRef){0}
#define sq_i_cuod_into(into, s, a0, a1)
#define sq_i_storeb(a0, a1)
#define sq_i_storeh(a0, a1)
#define sq_i_storew(a0, a1)
#define sq_i_storel(a0, a1)
#define sq_i_stores(a0, a1)
#define sq_i_stored(a0, a1)
#define sq_i_loadsb(s, a0) (SqRef){0}
#define sq_i_loadsb_into(into, s, a0)
#define sq_i_loadub(s, a0) (SqRef){0}
#define sq_i_loadub_into(into, s, a0)
#define sq_i_loadsh(s, a0) (SqRef){0}
#define sq_i_loadsh_into(into, s, a0)
#define sq_i_loaduh(s, a0) (SqRef){0}
#define sq_i_loaduh_into(into, s, a0)
#define sq_i_loadsw(s, a0) (SqRef){0}
#define sq_i_loadsw_into(into, s, a0)
#define sq_i_loaduw(s, a0) (SqRef){0}
#define sq_i_loaduw_into(into, s, a0)
#define sq_i_load(s, a0) (SqRef){0}
#define sq_i_load_into(into, s, a0)
#define sq_i_extsb(s, a0) (SqRef){0}
#define sq_i_extsb_into(into, s, a0)
#define sq_i_extub(s, a0) (SqRef){0}
#define sq_i_extub_into(into, s, a0)
#define sq_i_extsh(s, a0) (SqRef){0}
#define sq_i_extsh_into(into, s, a0)
#define sq_i_extuh(s, a0) (SqRef){0}
#define sq_i_extuh_into(into, s, a0)
#define sq_i_extsw(a0) (SqRef){0}
#define sq_i_extsw_into(into, a0)
#define sq_i_extuw(a0) (SqRef){0}
#define sq_i_extuw_into(into, a0)
#define sq_i_exts(a0) (SqRef){0}
#define sq_i_exts_into(into, a0)
#define sq_i_truncd(a0) (SqRef){0}
#define sq_i_truncd_into(into, a0)
#define sq_i_stosi(s, a0) (SqRef){0}
#define sq_i_stosi_into(into, s, a0)
#define sq_i_stoui(s, a0) (SqRef){0}
#define sq_i_stoui_into(into, s, a0)
#define sq_i_dtosi(s, a0) (SqRef){0}
#define sq_i_dtosi_into(into, s, a0)
#define sq_i_dtoui(s, a0) (SqRef){0}
#define sq_i_dtoui_into(into, s, a0)
#define sq_i_swtof(s, a0) (SqRef){0}
#define sq_i_swtof_into(into, s, a0)
#define sq_i_uwtof(s, a0) (SqRef){0}
#define sq_i_uwtof_into(into, s, a0)
#define sq_i_sltof(s, a0) (SqRef){0}
#define sq_i_sltof_into(into, s, a0)
#define sq_i_ultof(s, a0) (SqRef){0}
#define sq_i_ultof_into(into, s, a0)
#define sq_i_cast(s, a0) (SqRef){0}
#define sq_i_cast_into(into, s, a0)
#define sq_i_alloc4(a0) (SqRef){0}
#define sq_i_alloc4_into(into, a0)
#define sq_i_alloc8(a0) (SqRef){0}
#define sq_i_alloc8_into(into, a0)
#define sq_i_alloc16(a0) (SqRef){0}
#define sq_i_alloc16_into(into, a0)
#define sq_i_vaarg(s, a0) (SqRef){0}
#define sq_i_vaarg_into(into, s, a0)
#define sq_i_vastart(a0) (SqRef){0}
#define sq_i_vastart_into(into, a0)
#define sq_i_copy(s, a0) (SqRef){0}
#define sq_i_copy_into(into, s, a0)
#define sq_i_dbgloc(a0, a1)
#define sq_init(config)
#define sq_shutdown() true

#define sq_linkage_create(alignment, exported, tls, common, section_name, section_flags) (SqLinkage){0}

#define sq_type_struct_start(name, align)
#define sq_type_add_field(field)
#define sq_type_add_field_with_count(field, count)
#define sq_type_struct_end() (SqType){0}

#define sq_itemctx_activate(ctx)

#define sq_data_start(linkage, name) (SqItemCtx){0}
#define sq_data_byte(val)
#define sq_data_half(val)
#define sq_data_word(val)
#define sq_data_long(val)
#define sq_data_string(str)
#define sq_data_single(f)
#define sq_data_double(d)
#define sq_data_ref(ref, offset)
#define sq_data_end(void) (SqSymbol){0}

#define sq_func_start(linkage, return_type, name) (SqItemCtx){0}
#define sq_func_end() (SqSymbol){0}

#define sq_func_get_entry_block() (SqBlock){0}

#define sq_const_int(i) (SqRef){0}
#define sq_const_single(f) (SqRef){0}
#define sq_const_double(d) (SqRef){0}

#define sq_ref_for_symbol(sym) (SqRef){0}

#define sq_ref_declare() (SqRef){0}

#define sq_ref_extern(name) (SqRef){0}

#define sq_func_param_named(type, name) (SqRef){0}

#define sq_block_declare_named(name) (SqBlock){0}

#define sq_block_start(block)

#define sq_block_declare_and_start_named(name) (SqBlock){0}

#define sq_i_ret_void();
#define sq_i_ret(val);
#define sq_i_jmp(block);
#define sq_i_jnz(cond, if_true, if_false)

#define sq_i_phi(size_class, block0, val0, block1, val1) (SqRef){0}

#define sq_i_blit(from, to, num_bytes)

#define sq_i_calla(result, func, num_args, cas) (SqRef){0}

#define sq_i_call0 sq_i_call_noop
#define sq_i_call1 sq_i_call_noop
#define sq_i_call2 sq_i_call_noop
#define sq_i_call3 sq_i_call_noop
#define sq_i_call4 sq_i_call_noop
#define sq_i_call5 sq_i_call_noop
#define sq_i_call6 sq_i_call_noop
static inline SqRef sq_i_call_noop(SqType result, ...) { return (SqRef){0}; }

#endif // SQBE_NOOP

/*

QBE LICENSE:

© 2015-2026 Quentin Carbonneaux <quentin@c9x.me>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

---

All other sqbe code under the same license,
© 2026 Scott Graham <scott.sqbe@h4ck3r.net>

*/

