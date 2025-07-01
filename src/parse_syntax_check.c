#include "luv60.h"

#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"

#define ENABLE_CODE_GEN 0

#define sq_init(config) (void)config
#define sq_shutdown() true

#define sq_linkage_create(alignment, exported, tls, common, section_name, section_flags)

#define sq_type_struct_start(name, align)
#define sq_type_add_field(field);
#define sq_type_add_field_with_count(field, count)
#define sq_type_struct_end() (SqType){0}

#define sq_itemctx_activate(ctx)

#define sq_data_start(linkage, name) (SqItemCtx){0}
#define sq_data_byte(val);
#define sq_data_half(val);
#define sq_data_word(val);
#define sq_data_long(val);
#define sq_data_string(str);
#define sq_data_single(f);
#define sq_data_double(d);
#define sq_data_ref(ref, offset)
#define sq_data_end() (SqSymbol){0}

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

#define sq_i_ret_void()
#define sq_i_ret(val)
#define sq_i_jmp( block);
#define sq_i_jnz(cond, if_true, if_false);

#define sq_i_phi(size_class, block0, val0, block1, val1) (SqRef){0}

#define sq_i_calla(result, func, num_args, cas) (SqRef){0}

//#define sq_i_call0(result, func) (SqRef){0}
//#define sq_i_call1(result, func, ca0) (SqRef){0}
//#define sq_i_call2(result, func, ca0, ca1) (SqRef){0}
//#define sq_i_call3(result, func, ca0, ca1, ca2) (SqRef){0}
//#define sq_i_call4(result, func, ca0, ca1, ca2, ca3) (SqRef){0}
//#define sq_i_call5(result, func, ca0, ca1, ca2, ca3, ca4) (SqRef){0}
//#define sq_i_call6(result, func, ca0, ca1, ca2, ca3, ca4, ca5) (SqRef){0}

#define sq_i_add(size_class, arg0 /*wlsd*/, arg1 /*wlsd*/) (SqRef){0}
#define sq_i_add_into(into, size_class, arg0 /*wlsd*/, arg1 /*wlsd*/)
#define sq_i_sub(size_class, arg0 /*wlsd*/, arg1 /*wlsd*/) (SqRef){0}
#define sq_i_sub_into(into, size_class, arg0 /*wlsd*/, arg1 /*wlsd*/)
#define sq_i_neg(size_class, arg0 /*wlsd*/) (SqRef){0}
#define sq_i_neg_into(into, size_class, arg0 /*wlsd*/)
#define sq_i_div(size_class, arg0 /*wlsd*/, arg1 /*wlsd*/) (SqRef){0}
#define sq_i_div_into(into, size_class, arg0 /*wlsd*/, arg1 /*wlsd*/)
#define sq_i_rem(size_class, arg0 /*wlee*/, arg1 /*wlee*/) (SqRef){0}
#define sq_i_rem_into(into, size_class, arg0 /*wlee*/, arg1 /*wlee*/)
#define sq_i_udiv(size_class, arg0 /*wlee*/, arg1 /*wlee*/) (SqRef){0}
#define sq_i_udiv_into(into, size_class, arg0 /*wlee*/, arg1 /*wlee*/)
#define sq_i_urem(size_class, arg0 /*wlee*/, arg1 /*wlee*/) (SqRef){0}
#define sq_i_urem_into(into, size_class, arg0 /*wlee*/, arg1 /*wlee*/)
#define sq_i_mul(size_class, arg0 /*wlsd*/, arg1 /*wlsd*/) (SqRef){0}
#define sq_i_mul_into(into, size_class, arg0 /*wlsd*/, arg1 /*wlsd*/)
#define sq_i_and(size_class, arg0 /*wlee*/, arg1 /*wlee*/) (SqRef){0}
#define sq_i_and_into(into, size_class, arg0 /*wlee*/, arg1 /*wlee*/)
#define sq_i_or(size_class, arg0 /*wlee*/, arg1 /*wlee*/) (SqRef){0}
#define sq_i_or_into(into, size_class, arg0 /*wlee*/, arg1 /*wlee*/)
#define sq_i_xor(size_class, arg0 /*wlee*/, arg1 /*wlee*/) (SqRef){0}
#define sq_i_xor_into(into, size_class, arg0 /*wlee*/, arg1 /*wlee*/)
#define sq_i_sar(size_class, arg0 /*wlee*/, arg1 /*wwee*/) (SqRef){0}
#define sq_i_sar_into(into, size_class, arg0 /*wlee*/, arg1 /*wwee*/)
#define sq_i_shr(size_class, arg0 /*wlee*/, arg1 /*wwee*/) (SqRef){0}
#define sq_i_shr_into(into, size_class, arg0 /*wlee*/, arg1 /*wwee*/)
#define sq_i_shl(size_class, arg0 /*wlee*/, arg1 /*wwee*/) (SqRef){0}
#define sq_i_shl_into(into, size_class, arg0 /*wlee*/, arg1 /*wwee*/)
#define sq_i_ceqw(size_class, arg0 /*wwee*/, arg1 /*wwee*/) (SqRef){0}
#define sq_i_ceqw_into(into, size_class, arg0 /*wwee*/, arg1 /*wwee*/)
#define sq_i_cnew(size_class, arg0 /*wwee*/, arg1 /*wwee*/) (SqRef){0}
#define sq_i_cnew_into(into, size_class, arg0 /*wwee*/, arg1 /*wwee*/)
#define sq_i_csgew(size_class, arg0 /*wwee*/, arg1 /*wwee*/) (SqRef){0}
#define sq_i_csgew_into(into, size_class, arg0 /*wwee*/, arg1 /*wwee*/)
#define sq_i_csgtw(size_class, arg0 /*wwee*/, arg1 /*wwee*/) (SqRef){0}
#define sq_i_csgtw_into(into, size_class, arg0 /*wwee*/, arg1 /*wwee*/)
#define sq_i_cslew(size_class, arg0 /*wwee*/, arg1 /*wwee*/) (SqRef){0}
#define sq_i_cslew_into(into, size_class, arg0 /*wwee*/, arg1 /*wwee*/)
#define sq_i_csltw(size_class, arg0 /*wwee*/, arg1 /*wwee*/) (SqRef){0}
#define sq_i_csltw_into(into, size_class, arg0 /*wwee*/, arg1 /*wwee*/)
#define sq_i_cugew(size_class, arg0 /*wwee*/, arg1 /*wwee*/) (SqRef){0}
#define sq_i_cugew_into(into, size_class, arg0 /*wwee*/, arg1 /*wwee*/)
#define sq_i_cugtw(size_class, arg0 /*wwee*/, arg1 /*wwee*/) (SqRef){0}
#define sq_i_cugtw_into(into, size_class, arg0 /*wwee*/, arg1 /*wwee*/)
#define sq_i_culew(size_class, arg0 /*wwee*/, arg1 /*wwee*/) (SqRef){0}
#define sq_i_culew_into(into, size_class, arg0 /*wwee*/, arg1 /*wwee*/)
#define sq_i_cultw(size_class, arg0 /*wwee*/, arg1 /*wwee*/) (SqRef){0}
#define sq_i_cultw_into(into, size_class, arg0 /*wwee*/, arg1 /*wwee*/)
#define sq_i_ceql(size_class, arg0 /*llee*/, arg1 /*llee*/) (SqRef){0}
#define sq_i_ceql_into(into, size_class, arg0 /*llee*/, arg1 /*llee*/)
#define sq_i_cnel(size_class, arg0 /*llee*/, arg1 /*llee*/) (SqRef){0}
#define sq_i_cnel_into(into, size_class, arg0 /*llee*/, arg1 /*llee*/)
#define sq_i_csgel(size_class, arg0 /*llee*/, arg1 /*llee*/) (SqRef){0}
#define sq_i_csgel_into(into, size_class, arg0 /*llee*/, arg1 /*llee*/)
#define sq_i_csgtl(size_class, arg0 /*llee*/, arg1 /*llee*/) (SqRef){0}
#define sq_i_csgtl_into(into, size_class, arg0 /*llee*/, arg1 /*llee*/)
#define sq_i_cslel(size_class, arg0 /*llee*/, arg1 /*llee*/) (SqRef){0}
#define sq_i_cslel_into(into, size_class, arg0 /*llee*/, arg1 /*llee*/)
#define sq_i_csltl(size_class, arg0 /*llee*/, arg1 /*llee*/) (SqRef){0}
#define sq_i_csltl_into(into, size_class, arg0 /*llee*/, arg1 /*llee*/)
#define sq_i_cugel(size_class, arg0 /*llee*/, arg1 /*llee*/) (SqRef){0}
#define sq_i_cugel_into(into, size_class, arg0 /*llee*/, arg1 /*llee*/)
#define sq_i_cugtl(size_class, arg0 /*llee*/, arg1 /*llee*/) (SqRef){0}
#define sq_i_cugtl_into(into, size_class, arg0 /*llee*/, arg1 /*llee*/)
#define sq_i_culel(size_class, arg0 /*llee*/, arg1 /*llee*/) (SqRef){0}
#define sq_i_culel_into(into, size_class, arg0 /*llee*/, arg1 /*llee*/)
#define sq_i_cultl(size_class, arg0 /*llee*/, arg1 /*llee*/) (SqRef){0}
#define sq_i_cultl_into(into, size_class, arg0 /*llee*/, arg1 /*llee*/)
#define sq_i_ceqs(size_class, arg0 /*ssee*/, arg1 /*ssee*/) (SqRef){0}
#define sq_i_ceqs_into(into, size_class, arg0 /*ssee*/, arg1 /*ssee*/)
#define sq_i_cges(size_class, arg0 /*ssee*/, arg1 /*ssee*/) (SqRef){0}
#define sq_i_cges_into(into, size_class, arg0 /*ssee*/, arg1 /*ssee*/)
#define sq_i_cgts(size_class, arg0 /*ssee*/, arg1 /*ssee*/) (SqRef){0}
#define sq_i_cgts_into(into, size_class, arg0 /*ssee*/, arg1 /*ssee*/)
#define sq_i_cles(size_class, arg0 /*ssee*/, arg1 /*ssee*/) (SqRef){0}
#define sq_i_cles_into(into, size_class, arg0 /*ssee*/, arg1 /*ssee*/)
#define sq_i_clts(size_class, arg0 /*ssee*/, arg1 /*ssee*/) (SqRef){0}
#define sq_i_clts_into(into, size_class, arg0 /*ssee*/, arg1 /*ssee*/)
#define sq_i_cnes(size_class, arg0 /*ssee*/, arg1 /*ssee*/) (SqRef){0}
#define sq_i_cnes_into(into, size_class, arg0 /*ssee*/, arg1 /*ssee*/)
#define sq_i_cos(size_class, arg0 /*ssee*/, arg1 /*ssee*/) (SqRef){0}
#define sq_i_cos_into(into, size_class, arg0 /*ssee*/, arg1 /*ssee*/)
#define sq_i_cuos(size_class, arg0 /*ssee*/, arg1 /*ssee*/) (SqRef){0}
#define sq_i_cuos_into(into, size_class, arg0 /*ssee*/, arg1 /*ssee*/)
#define sq_i_ceqd(size_class, arg0 /*ddee*/, arg1 /*ddee*/) (SqRef){0}
#define sq_i_ceqd_into(into, size_class, arg0 /*ddee*/, arg1 /*ddee*/)
#define sq_i_cged(size_class, arg0 /*ddee*/, arg1 /*ddee*/) (SqRef){0}
#define sq_i_cged_into(into, size_class, arg0 /*ddee*/, arg1 /*ddee*/)
#define sq_i_cgtd(size_class, arg0 /*ddee*/, arg1 /*ddee*/) (SqRef){0}
#define sq_i_cgtd_into(into, size_class, arg0 /*ddee*/, arg1 /*ddee*/)
#define sq_i_cled(size_class, arg0 /*ddee*/, arg1 /*ddee*/) (SqRef){0}
#define sq_i_cled_into(into, size_class, arg0 /*ddee*/, arg1 /*ddee*/)
#define sq_i_cltd(size_class, arg0 /*ddee*/, arg1 /*ddee*/) (SqRef){0}
#define sq_i_cltd_into(into, size_class, arg0 /*ddee*/, arg1 /*ddee*/)
#define sq_i_cned(size_class, arg0 /*ddee*/, arg1 /*ddee*/) (SqRef){0}
#define sq_i_cned_into(into, size_class, arg0 /*ddee*/, arg1 /*ddee*/)
#define sq_i_cod(size_class, arg0 /*ddee*/, arg1 /*ddee*/) (SqRef){0}
#define sq_i_cod_into(into, size_class, arg0 /*ddee*/, arg1 /*ddee*/)
#define sq_i_cuod(size_class, arg0 /*ddee*/, arg1 /*ddee*/) (SqRef){0}
#define sq_i_cuod_into(into, size_class, arg0 /*ddee*/, arg1 /*ddee*/)
#define sq_i_storeb(arg0 /*weee*/, arg1 /*meee*/) (SqRef){0}
#define sq_i_storeh(arg0 /*weee*/, arg1 /*meee*/) (SqRef){0}
#define sq_i_storew(arg0 /*weee*/, arg1 /*meee*/) (SqRef){0}
#define sq_i_storel(arg0 /*leee*/, arg1 /*meee*/) (SqRef){0}
#define sq_i_stores(arg0 /*seee*/, arg1 /*meee*/) (SqRef){0}
#define sq_i_stored(arg0 /*deee*/, arg1 /*meee*/) (SqRef){0}
#define sq_i_loadsb(size_class, arg0 /*mmee*/) (SqRef){0}
#define sq_i_loadsb_into(into, size_class, arg0 /*mmee*/)
#define sq_i_loadub(size_class, arg0 /*mmee*/) (SqRef){0}
#define sq_i_loadub_into(into, size_class, arg0 /*mmee*/)
#define sq_i_loadsh(size_class, arg0 /*mmee*/) (SqRef){0}
#define sq_i_loadsh_into(into, size_class, arg0 /*mmee*/)
#define sq_i_loaduh(size_class, arg0 /*mmee*/) (SqRef){0}
#define sq_i_loaduh_into(into, size_class, arg0 /*mmee*/)
#define sq_i_loadsw(size_class, arg0 /*mmee*/) (SqRef){0}
#define sq_i_loadsw_into(into, size_class, arg0 /*mmee*/)
#define sq_i_loaduw(size_class, arg0 /*mmee*/) (SqRef){0}
#define sq_i_loaduw_into(into, size_class, arg0 /*mmee*/)
#define sq_i_load(size_class, arg0 /*mmmm*/) (SqRef){0}
#define sq_i_load_into(into, size_class, arg0 /*mmmm*/)
#define sq_i_extsb(size_class, arg0 /*wwee*/) (SqRef){0}
#define sq_i_extsb_into(into, size_class, arg0 /*wwee*/)
#define sq_i_extub(size_class, arg0 /*wwee*/) (SqRef){0}
#define sq_i_extub_into(into, size_class, arg0 /*wwee*/)
#define sq_i_extsh(size_class, arg0 /*wwee*/) (SqRef){0}
#define sq_i_extsh_into(into, size_class, arg0 /*wwee*/)
#define sq_i_extuh(size_class, arg0 /*wwee*/) (SqRef){0}
#define sq_i_extuh_into(into, size_class, arg0 /*wwee*/)
#define sq_i_extsw(arg0 /*ewee*/) (SqRef){0}
#define sq_i_extsw_into(into, arg0 /*ewee*/)
#define sq_i_extuw(arg0 /*ewee*/) (SqRef){0}
#define sq_i_extuw_into(into, arg0 /*ewee*/)
#define sq_i_exts(arg0 /*eees*/) (SqRef){0}
#define sq_i_exts_into(into, arg0 /*eees*/)
#define sq_i_truncd(arg0 /*eede*/) (SqRef){0}
#define sq_i_truncd_into(into, arg0 /*eede*/)
#define sq_i_stosi(size_class, arg0 /*ssee*/) (SqRef){0}
#define sq_i_stosi_into(into, size_class, arg0 /*ssee*/)
#define sq_i_stoui(size_class, arg0 /*ssee*/) (SqRef){0}
#define sq_i_stoui_into(into, size_class, arg0 /*ssee*/)
#define sq_i_dtosi(size_class, arg0 /*ddee*/) (SqRef){0}
#define sq_i_dtosi_into(into, size_class, arg0 /*ddee*/)
#define sq_i_dtoui(size_class, arg0 /*ddee*/) (SqRef){0}
#define sq_i_dtoui_into(into, size_class, arg0 /*ddee*/)
#define sq_i_swtof(size_class, arg0 /*eeww*/) (SqRef){0}
#define sq_i_swtof_into(into, size_class, arg0 /*eeww*/)
#define sq_i_uwtof(size_class, arg0 /*eeww*/) (SqRef){0}
#define sq_i_uwtof_into(into, size_class, arg0 /*eeww*/)
#define sq_i_sltof(size_class, arg0 /*eell*/) (SqRef){0}
#define sq_i_sltof_into(into, size_class, arg0 /*eell*/)
#define sq_i_ultof(size_class, arg0 /*eell*/) (SqRef){0}
#define sq_i_ultof_into(into, size_class, arg0 /*eell*/)
#define sq_i_cast(size_class, arg0 /*sdwl*/) (SqRef){0}
#define sq_i_cast_into(into, size_class, arg0 /*sdwl*/)
#define sq_i_alloc4(arg0 /*elee*/) (SqRef){0}
#define sq_i_alloc4_into(into, arg0 /*elee*/)
#define sq_i_alloc8(arg0 /*elee*/) (SqRef){0}
#define sq_i_alloc8_into(into, arg0 /*elee*/)
#define sq_i_alloc16(arg0 /*elee*/) (SqRef){0}
#define sq_i_alloc16_into(into, arg0 /*elee*/)
#define sq_i_vaarg(size_class, arg0 /*mmmm*/) (SqRef){0}
#define sq_i_vaarg_into(into, size_class, arg0 /*mmmm*/)
#define sq_i_vastart(arg0 /*meee*/) (SqRef){0}
#define sq_i_vastart_into(into, arg0 /*meee*/)
#define sq_i_copy(size_class, arg0 /*wlsd*/) (SqRef){0}
#define sq_i_copy_into(into, size_class, arg0 /*wlsd*/)
#define sq_i_dbgloc(arg0 /*weee*/, arg1 /*weee*/)

#include "parse.c"

void parse_syntax_check(Arena* main_arena,
                        Arena* temp_arena,
                        const char* filename,
                        ReadFileResult file,
                        int verbose) {
  parse_impl(main_arena, temp_arena, filename, file, verbose, NULL);
}
