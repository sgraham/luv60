#ifndef LIBQBE_H_INCLUDED_
#define LIBQBE_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define LQ_OPAQUE_STRUCT_DEF(x) \
  typedef struct x {            \
    uint32_t u;                 \
  } x

LQ_OPAQUE_STRUCT_DEF(LqLinkage);
LQ_OPAQUE_STRUCT_DEF(LqType);
LQ_OPAQUE_STRUCT_DEF(LqBlock);
LQ_OPAQUE_STRUCT_DEF(LqRef);
LQ_OPAQUE_STRUCT_DEF(LqSymbol);

#undef LQ_OPAQUE_STRUCT_DEF

typedef enum LqTarget {
  LQ_TARGET_DEFAULT,        //
  LQ_TARGET_AMD64_APPLE,    //
  LQ_TARGET_AMD64_SYSV,     //
  LQ_TARGET_AMD64_WINDOWS,  //
  LQ_TARGET_ARM64,          //
  LQ_TARGET_ARM64_APPLE,    //
  LQ_TARGET_RV64,           //
} LqTarget;

/*
debug_flags string can contain the following characters to cause QBE to output
information to stderr while compiling. NOTE: no final assembly will be emitted
if string other than "" is specified.
- P: parsing
- M: memory optimization
- N: ssa construction
- C: copy elimination
- F: constant folding
- A: abi lowering
- I: instruction selection
- L: liveness
- S: spilling
- R: reg. allocation
- T: types
*/
void lq_init(LqTarget target /*=LQ_TARGET_DEFAULT*/,
             FILE* output /*=stdout*/,
             const char* debug_flags /*=""*/);
void lq_shutdown(void);

LqLinkage lq_linkage_create(int alignment,
                            bool exported,
                            bool tls,
                            bool common,
                            const char* section_name,
                            const char* section_flags);
#define lq_linkage_default ((LqLinkage){0})
#define lq_linkage_export ((LqLinkage){1})

// These values must match internal Kw, Kl, etc.
typedef enum LqTypeKind {
  LQ_TYPE_W = 0,
  LQ_TYPE_L = 1,
  LQ_TYPE_S = 2,
  LQ_TYPE_D = 3,
  LQ_TYPE_SB = 4,
  LQ_TYPE_UB = 5,
  LQ_TYPE_SH = 6,
  LQ_TYPE_UH = 7,
  LQ_TYPE_C = 8,          // User-defined class
  LQ_TYPE_0 = 9,          // void
  LQ_TYPE_E = -2,         // error
  LQ_TYPE_M = LQ_TYPE_L,  // memory
} LqTypeKind;

#define lq_type_void ((LqType){LQ_TYPE_0})
#define lq_type_word ((LqType){LQ_TYPE_W})
#define lq_type_long ((LqType){LQ_TYPE_L})
#define lq_type_single ((LqType){LQ_TYPE_S})
#define lq_type_double ((LqType){LQ_TYPE_D})
#define lq_type_byte ((LqType){LQ_TYPE_UB})
#define lq_type_half ((LqType){LQ_TYPE_UH})
#define lq_type_sbyte ((LqType){LQ_TYPE_SB})
#define lq_type_shalf ((LqType){LQ_TYPE_SH})
#define lq_type_ubyte ((LqType){LQ_TYPE_UB})
#define lq_type_uhalf ((LqType){LQ_TYPE_UH})

void lq_type_struct_start(const char* name, int align /*=0 for natural*/);
void lq_type_add_field(LqType field);
void lq_type_add_field_with_count(LqType field, uint32_t count);
LqType lq_type_struct_end(void);

LqRef lq_const_int(int64_t i);
LqRef lq_const_single(float f);
LqRef lq_const_double(double d);

void lq_data_start(LqLinkage linkage, const char* name);
void lq_data_byte(uint8_t val);
void lq_data_half(uint16_t val);
void lq_data_word(uint32_t val);
void lq_data_long(uint64_t val);
void lq_data_string(const char* str);
void lq_data_single(float f);
void lq_data_double(double d);
void lq_data_ref(LqSymbol ref, int64_t offset);
LqSymbol lq_data_end(void);

void lq_func_start(LqLinkage linkage, LqType return_type, const char* name);
LqSymbol lq_func_end(void);

// LqRef are function local, so the return value from cannot be cached across
// functions.
LqRef lq_ref_for_symbol(LqSymbol sym);

LqRef lq_extern(const char* name);

#define lq_func_param(type) lq_func_param_named(type, NULL)
LqRef lq_func_param_named(LqType type, const char* name);

#define lq_block_declare() lq_block_declare_named(NULL)
LqBlock lq_block_declare_named(const char* name);

void lq_block_start(LqBlock block);

#define lq_block_declare_and_start() lq_block_declare_and_start_named(NULL)
LqBlock lq_block_declare_and_start_named(const char* name);

void lq_i_ret_void(void);
void lq_i_ret(LqRef val);
void lq_i_jmp(LqBlock block);
void lq_i_jnz(LqRef cond, LqBlock if_true, LqBlock if_false);

// TODO: only 2-branch phi supported currently
LqRef lq_i_phi(LqType size_class, LqBlock block0, LqRef val0, LqBlock block1, LqRef val1);

typedef struct LqCallArg {
  LqType type;
  LqRef value;
} LqCallArg;

#define lq_varargs_begin (LqCallArg){lq_type_void, (LqRef){0}}

LqRef lq_i_calla(LqType result,
                 LqRef func,
                 int num_args,
                 LqCallArg* cas);

LqRef lq_i_call0(LqType result, LqRef func);
LqRef lq_i_call1(LqType result, LqRef func, LqCallArg ca0);
LqRef lq_i_call2(LqType result, LqRef func, LqCallArg ca0, LqCallArg ca1);
LqRef lq_i_call3(LqType result, LqRef func, LqCallArg ca0, LqCallArg ca1, LqCallArg ca2);
LqRef lq_i_call4(LqType result, LqRef func, LqCallArg ca0, LqCallArg ca1, LqCallArg ca2, LqCallArg ca3);
LqRef lq_i_call5(LqType result, LqRef func, LqCallArg ca0, LqCallArg ca1, LqCallArg ca2, LqCallArg ca3, LqCallArg ca4);
LqRef lq_i_call6(LqType result, LqRef func, LqCallArg ca0, LqCallArg ca1, LqCallArg ca2, LqCallArg ca3, LqCallArg ca4, LqCallArg ca5);

LqRef lq_i_add(LqType size_class, LqRef arg0 /*wlsd*/, LqRef arg1 /*wlsd*/);
LqRef lq_i_sub(LqType size_class, LqRef arg0 /*wlsd*/, LqRef arg1 /*wlsd*/);
LqRef lq_i_neg(LqType size_class, LqRef arg0 /*wlsd*/);
LqRef lq_i_div(LqType size_class, LqRef arg0 /*wlsd*/, LqRef arg1 /*wlsd*/);
LqRef lq_i_rem(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wlee*/);
LqRef lq_i_udiv(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wlee*/);
LqRef lq_i_urem(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wlee*/);
LqRef lq_i_mul(LqType size_class, LqRef arg0 /*wlsd*/, LqRef arg1 /*wlsd*/);
LqRef lq_i_and(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wlee*/);
LqRef lq_i_or(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wlee*/);
LqRef lq_i_xor(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wlee*/);
LqRef lq_i_sar(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wwee*/);
LqRef lq_i_shr(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wwee*/);
LqRef lq_i_shl(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wwee*/);
LqRef lq_i_ceqw(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/);
LqRef lq_i_cnew(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/);
LqRef lq_i_csgew(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/);
LqRef lq_i_csgtw(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/);
LqRef lq_i_cslew(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/);
LqRef lq_i_csltw(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/);
LqRef lq_i_cugew(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/);
LqRef lq_i_cugtw(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/);
LqRef lq_i_culew(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/);
LqRef lq_i_cultw(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/);
LqRef lq_i_ceql(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/);
LqRef lq_i_cnel(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/);
LqRef lq_i_csgel(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/);
LqRef lq_i_csgtl(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/);
LqRef lq_i_cslel(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/);
LqRef lq_i_csltl(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/);
LqRef lq_i_cugel(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/);
LqRef lq_i_cugtl(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/);
LqRef lq_i_culel(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/);
LqRef lq_i_cultl(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/);
LqRef lq_i_ceqs(LqType size_class, LqRef arg0 /*ssee*/, LqRef arg1 /*ssee*/);
LqRef lq_i_cges(LqType size_class, LqRef arg0 /*ssee*/, LqRef arg1 /*ssee*/);
LqRef lq_i_cgts(LqType size_class, LqRef arg0 /*ssee*/, LqRef arg1 /*ssee*/);
LqRef lq_i_cles(LqType size_class, LqRef arg0 /*ssee*/, LqRef arg1 /*ssee*/);
LqRef lq_i_clts(LqType size_class, LqRef arg0 /*ssee*/, LqRef arg1 /*ssee*/);
LqRef lq_i_cnes(LqType size_class, LqRef arg0 /*ssee*/, LqRef arg1 /*ssee*/);
LqRef lq_i_cos(LqType size_class, LqRef arg0 /*ssee*/, LqRef arg1 /*ssee*/);
LqRef lq_i_cuos(LqType size_class, LqRef arg0 /*ssee*/, LqRef arg1 /*ssee*/);
LqRef lq_i_ceqd(LqType size_class, LqRef arg0 /*ddee*/, LqRef arg1 /*ddee*/);
LqRef lq_i_cged(LqType size_class, LqRef arg0 /*ddee*/, LqRef arg1 /*ddee*/);
LqRef lq_i_cgtd(LqType size_class, LqRef arg0 /*ddee*/, LqRef arg1 /*ddee*/);
LqRef lq_i_cled(LqType size_class, LqRef arg0 /*ddee*/, LqRef arg1 /*ddee*/);
LqRef lq_i_cltd(LqType size_class, LqRef arg0 /*ddee*/, LqRef arg1 /*ddee*/);
LqRef lq_i_cned(LqType size_class, LqRef arg0 /*ddee*/, LqRef arg1 /*ddee*/);
LqRef lq_i_cod(LqType size_class, LqRef arg0 /*ddee*/, LqRef arg1 /*ddee*/);
LqRef lq_i_cuod(LqType size_class, LqRef arg0 /*ddee*/, LqRef arg1 /*ddee*/);
void lq_i_storeb(LqRef arg0 /*weee*/, LqRef arg1 /*meee*/);
void lq_i_storeh(LqRef arg0 /*weee*/, LqRef arg1 /*meee*/);
void lq_i_storew(LqRef arg0 /*weee*/, LqRef arg1 /*meee*/);
void lq_i_storel(LqRef arg0 /*leee*/, LqRef arg1 /*meee*/);
void lq_i_stores(LqRef arg0 /*seee*/, LqRef arg1 /*meee*/);
void lq_i_stored(LqRef arg0 /*deee*/, LqRef arg1 /*meee*/);
LqRef lq_i_loadsb(LqType size_class, LqRef arg0 /*mmee*/);
LqRef lq_i_loadub(LqType size_class, LqRef arg0 /*mmee*/);
LqRef lq_i_loadsh(LqType size_class, LqRef arg0 /*mmee*/);
LqRef lq_i_loaduh(LqType size_class, LqRef arg0 /*mmee*/);
LqRef lq_i_loadsw(LqType size_class, LqRef arg0 /*mmee*/);
LqRef lq_i_loaduw(LqType size_class, LqRef arg0 /*mmee*/);
LqRef lq_i_load(LqType size_class, LqRef arg0 /*mmmm*/);
LqRef lq_i_extsb(LqType size_class, LqRef arg0 /*wwee*/);
LqRef lq_i_extub(LqType size_class, LqRef arg0 /*wwee*/);
LqRef lq_i_extsh(LqType size_class, LqRef arg0 /*wwee*/);
LqRef lq_i_extuh(LqType size_class, LqRef arg0 /*wwee*/);
LqRef lq_i_extsw(LqRef arg0 /*ewee*/);
LqRef lq_i_extuw(LqRef arg0 /*ewee*/);
LqRef lq_i_exts(LqRef arg0 /*eees*/);
LqRef lq_i_truncd(LqRef arg0 /*eede*/);
LqRef lq_i_stosi(LqType size_class, LqRef arg0 /*ssee*/);
LqRef lq_i_stoui(LqType size_class, LqRef arg0 /*ssee*/);
LqRef lq_i_dtosi(LqType size_class, LqRef arg0 /*ddee*/);
LqRef lq_i_dtoui(LqType size_class, LqRef arg0 /*ddee*/);
LqRef lq_i_swtof(LqType size_class, LqRef arg0 /*eeww*/);
LqRef lq_i_uwtof(LqType size_class, LqRef arg0 /*eeww*/);
LqRef lq_i_sltof(LqType size_class, LqRef arg0 /*eell*/);
LqRef lq_i_ultof(LqType size_class, LqRef arg0 /*eell*/);
LqRef lq_i_cast(LqType size_class, LqRef arg0 /*sdwl*/);
LqRef lq_i_alloc4(LqRef arg0 /*elee*/);
LqRef lq_i_alloc8(LqRef arg0 /*elee*/);
LqRef lq_i_alloc16(LqRef arg0 /*elee*/);
LqRef lq_i_vaarg(LqType size_class, LqRef arg0 /*mmmm*/);
LqRef lq_i_vastart(LqRef arg0 /*meee*/);
LqRef lq_i_copy(LqType size_class, LqRef arg0 /*wlsd*/);
void lq_i_dbgloc(LqRef arg0 /*weee*/, LqRef arg1 /*weee*/);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* LIBQBE_H_INCLUDED_ */
