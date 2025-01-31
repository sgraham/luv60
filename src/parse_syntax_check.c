#include <stdint.h>
#include <inttypes.h>

#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"

#define ENABLE_CODE_GEN 0

typedef uint32_t ir_ref;
typedef uint32_t ir_op;
typedef struct _ir_fake_code_buffer {
  void* start;
  void* end;
  void* pos;
} ir_code_buffer;
typedef struct _ir_fake_ctx {
  ir_ref ret_type;
  struct _ir_fake_code_buffer* code_buffer;
} ir_ctx;
typedef uint32_t ir_type;
typedef struct _ir_val {
  uint64_t u64;
} ir_val;

#define IR_ADDR 0
#define IR_BOOL 0
#define IR_DOUBLE 0
#define IR_FLOAT 0
#define IR_I8 0
#define IR_I16 0
#define IR_I32 0
#define IR_I64 0
#define IR_U8 0
#define IR_U16 0
#define IR_U32 0
#define IR_U64 0
#define IR_VOID 0

#define IR_EQ 10
#define IR_NE 11
#define IR_LE 12
#define IR_LT 13
#define IR_GE 14
#define IR_GT 15
#define IR_ULE 16
#define IR_ULT 17
#define IR_UGE 18
#define IR_UGT 19
#define IR_ADD 20
#define IR_SUB 21
#define IR_MUL 22
#define IR_DIV 23
#define IR_MOD 24
#define IR_OR 25
#define IR_AND 26
#define IR_XOR 27
#define IR_SHL 28
#define IR_SHR 29
#define IR_SAR 30

#define IR_OPT_FOLDING 0
#define IR_OPT_MEM2SSA 0

#define ir_NOP() 0

#define ir_CONST_BOOL(_val) 0
#define ir_CONST_U8(_val) 0
#define ir_CONST_U16(_val) 0
#define ir_CONST_U32(_val) 0
#define ir_CONST_U64(_val) 0
#define ir_CONST_ADDR(_val) 0
#define ir_CONST_CHAR(_val) 0
#define ir_CONST_I8(_val) 0
#define ir_CONST_I16(_val) 0
#define ir_CONST_I32(_val) 0
#define ir_CONST_I64(_val) 0
#define ir_CONST_DOUBLE(_val) 0
#define ir_CONST_FLOAT(_val) 0

#define ir_CMP_OP(_op, _op1, _op2) 0

#define ir_UNARY_OP(_op, _type, _op1) 0
#define ir_UNARY_OP_B(_op, _op1) 0
#define ir_UNARY_OP_U8(_op, _op1) 0
#define ir_UNARY_OP_U16(_op, _op1) 0
#define ir_UNARY_OP_U32(_op, _op1) 0
#define ir_UNARY_OP_U64(_op, _op1) 0
#define ir_UNARY_OP_A(_op, _op1) 0
#define ir_UNARY_OP_C(_op, _op1) 0
#define ir_UNARY_OP_I8(_op, _op1) 0
#define ir_UNARY_OP_I16(_op, _op1) 0
#define ir_UNARY_OP_I32(_op, _op1) 0
#define ir_UNARY_OP_I64(_op, _op1) 0
#define ir_UNARY_OP_D(_op, _op1) 0
#define ir_UNARY_OP_F(_op, _op1) 0

#define ir_BINARY_OP(_op, _t, _op1, _op2) 0
#define ir_BINARY_OP_B(_op, _op1, _op2) 0
#define ir_BINARY_OP_U8(_op, _op1, _op2) 0
#define ir_BINARY_OP_U16(_op, _op1, _op2) 0
#define ir_BINARY_OP_U32(_op, _op1, _op2) 0
#define ir_BINARY_OP_U64(_op, _op1, _op2) 0
#define ir_BINARY_OP_A(_op, _op1, _op2) 0
#define ir_BINARY_OP_C(_op, _op1, _op2) 0
#define ir_BINARY_OP_I8(_op, _op1, _op2) 0
#define ir_BINARY_OP_I16(_op, _op1, _op2) 0
#define ir_BINARY_OP_I32(_op, _op1, _op2) 0
#define ir_BINARY_OP_I64(_op, _op1, _op2) 0
#define ir_BINARY_OP_D(_op, _op1, _op2) 0
#define ir_BINARY_OP_F(_op, _op1, _op2) 0

#define ir_EQ(_op1, _op2) 0
#define ir_NE(_op1, _op2) 0

#define ir_LT(_op1, _op2) 0
#define ir_GE(_op1, _op2) 0
#define ir_LE(_op1, _op2) 0
#define ir_GT(_op1, _op2) 0

#define ir_ULT(_op1, _op2) 0
#define ir_UGE(_op1, _op2) 0
#define ir_ULE(_op1, _op2) 0
#define ir_UGT(_op1, _op2) 0

#define ir_ADD(_type, _op1, _op2) 0
#define ir_ADD_U8(_op1, _op2) 0
#define ir_ADD_U16(_op1, _op2) 0
#define ir_ADD_U32(_op1, _op2) 0
#define ir_ADD_U64(_op1, _op2) 0
#define ir_ADD_A(_op1, _op2) 0
#define ir_ADD_C(_op1, _op2) 0
#define ir_ADD_I8(_op1, _op2) 0
#define ir_ADD_I16(_op1, _op2) 0
#define ir_ADD_I32(_op1, _op2) 0
#define ir_ADD_I64(_op1, _op2) 0
#define ir_ADD_D(_op1, _op2) 0
#define ir_ADD_F(_op1, _op2) 0

#define ir_SUB(_type, _op1, _op2) 0
#define ir_SUB_U8(_op1, _op2) 0
#define ir_SUB_U16(_op1, _op2) 0
#define ir_SUB_U32(_op1, _op2) 0
#define ir_SUB_U64(_op1, _op2) 0
#define ir_SUB_A(_op1, _op2) 0
#define ir_SUB_C(_op1, _op2) 0
#define ir_SUB_I8(_op1, _op2) 0
#define ir_SUB_I16(_op1, _op2) 0
#define ir_SUB_I32(_op1, _op2) 0
#define ir_SUB_I64(_op1, _op2) 0
#define ir_SUB_D(_op1, _op2) 0
#define ir_SUB_F(_op1, _op2) 0

#define ir_MUL(_type, _op1, _op2) 0
#define ir_MUL_U8(_op1, _op2) 0
#define ir_MUL_U16(_op1, _op2) 0
#define ir_MUL_U32(_op1, _op2) 0
#define ir_MUL_U64(_op1, _op2) 0
#define ir_MUL_A(_op1, _op2) 0
#define ir_MUL_C(_op1, _op2) 0
#define ir_NUL_I8(_op1, _op2) 0
#define ir_MUL_I16(_op1, _op2) 0
#define ir_MUL_I32(_op1, _op2) 0
#define ir_MUL_I64(_op1, _op2) 0
#define ir_MUL_D(_op1, _op2) 0
#define ir_MUL_F(_op1, _op2) 0

#define ir_DIV(_type, _op1, _op2) 0
#define ir_DIV_U8(_op1, _op2) 0
#define ir_DIV_U16(_op1, _op2) 0
#define ir_DIV_U32(_op1, _op2) 0
#define ir_DIV_U64(_op1, _op2) 0
#define ir_DIV_A(_op1, _op2) 0
#define ir_DIV_C(_op1, _op2) 0
#define ir_DIV_I8(_op1, _op2) 0
#define ir_DIV_I16(_op1, _op2) 0
#define ir_DIV_I32(_op1, _op2) 0
#define ir_DIV_I64(_op1, _op2) 0
#define ir_DIV_D(_op1, _op2) 0
#define ir_DIV_F(_op1, _op2) 0

#define ir_MOD(_type, _op1, _op2) 0
#define ir_MOD_U8(_op1, _op2) 0
#define ir_MOD_U16(_op1, _op2) 0
#define ir_MOD_U32(_op1, _op2) 0
#define ir_MOD_U64(_op1, _op2) 0
#define ir_MOD_A(_op1, _op2) 0
#define ir_MOD_C(_op1, _op2) 0
#define ir_MOD_I8(_op1, _op2) 0
#define ir_MOD_I16(_op1, _op2) 0
#define ir_MOD_I32(_op1, _op2) 0
#define ir_MOD_I64(_op1, _op2) 0

#define ir_NEG(_type, _op1) 0
#define ir_NEG_C(_op1) 0
#define ir_NEG_I8(_op1) 0
#define ir_NEG_I16(_op1) 0
#define ir_NEG_I32(_op1) 0
#define ir_NEG_I64(_op1) 0
#define ir_NEG_D(_op1) 0
#define ir_NEG_F(_op1) 0

#define ir_ABS(_type, _op1) 0
#define ir_ABS_C(_op1) 0
#define ir_ABS_I8(_op1) 0
#define ir_ABS_I16(_op1) 0
#define ir_ABS_I32(_op1) 0
#define ir_ABS_I64(_op1) 0
#define ir_ABS_D(_op1) 0
#define ir_ABS_F(_op1) 0

#define ir_SEXT(_type, _op1) 0
#define ir_SEXT_U8(_op1) 0
#define ir_SEXT_U16(_op1) 0
#define ir_SEXT_U32(_op1) 0
#define ir_SEXT_U64(_op1) 0
#define ir_SEXT_A(_op1) 0
#define ir_SEXT_C(_op1) 0
#define ir_SEXT_I8(_op1) 0
#define ir_SEXT_I16(_op1) 0
#define ir_SEXT_I32(_op1) 0
#define ir_SEXT_I64(_op1) 0

#define ir_ZEXT(_type, _op1) 0
#define ir_ZEXT_U8(_op1) 0
#define ir_ZEXT_U16(_op1) 0
#define ir_ZEXT_U32(_op1) 0
#define ir_ZEXT_U64(_op1) 0
#define ir_ZEXT_A(_op1) 0
#define ir_ZEXT_C(_op1) 0
#define ir_ZEXT_I8(_op1) 0
#define ir_ZEXT_I16(_op1) 0
#define ir_ZEXT_I32(_op1) 0
#define ir_ZEXT_I64(_op1) 0

#define ir_TRUNC(_type, _op1) 0
#define ir_TRUNC_U8(_op1) 0
#define ir_TRUNC_U16(_op1) 0
#define ir_TRUNC_U32(_op1) 0
#define ir_TRUNC_U64(_op1) 0
#define ir_TRUNC_A(_op1) 0
#define ir_TRUNC_C(_op1) 0
#define ir_TRUNC_I8(_op1) 0
#define ir_TRUNC_I16(_op1) 0
#define ir_TRUNC_I32(_op1) 0
#define ir_TRUNC_I64(_op1) 0

#define ir_BITCAST(_type, _op1) 0
#define ir_BITCAST_U8(_op1) 0
#define ir_BITCAST_U16(_op1) 0
#define ir_BITCAST_U32(_op1) 0
#define ir_BITCAST_U64(_op1) 0
#define ir_BITCAST_A(_op1) 0
#define ir_BITCAST_C(_op1) 0
#define ir_BITCAST_I8(_op1) 0
#define ir_BITCAST_I16(_op1) 0
#define ir_BITCAST_I32(_op1) 0
#define ir_BITCAST_I64(_op1) 0
#define ir_BITCAST_D(_op1) 0
#define ir_BITCAST_F(_op1) 0

#define ir_INT2FP(_type, _op1) 0
#define ir_INT2D(_op1) 0
#define ir_INT2F(_op1) 0

#define ir_FP2INT(_type, _op1) 0
#define ir_FP2U8(_op1) 0
#define ir_FP2U16(_op1) 0
#define ir_FP2U32(_op1) 0
#define ir_FP2U64(_op1) 0
#define ir_FP2I8(_op1) 0
#define ir_FP2I16(_op1) 0
#define ir_FP2I32(_op1) 0
#define ir_FP2I64(_op1) 0

#define ir_FP2FP(_type, _op1) 0
#define ir_F2D(_op1) 0
#define ir_D2F(_op1) 0

#define ir_ADD_OV(_type, _op1, _op2) 0
#define ir_ADD_OV_U8(_op1, _op2) 0
#define ir_ADD_OV_U16(_op1, _op2) 0
#define ir_ADD_OV_U32(_op1, _op2) 0
#define ir_ADD_OV_U64(_op1, _op2) 0
#define ir_ADD_OV_A(_op1, _op2) 0
#define ir_ADD_OV_C(_op1, _op2) 0
#define ir_ADD_OV_I8(_op1, _op2) 0
#define ir_ADD_OV_I16(_op1, _op2) 0
#define ir_ADD_OV_I32(_op1, _op2) 0
#define ir_ADD_OV_I64(_op1, _op2) 0

#define ir_SUB_OV(_type, _op1, _op2) 0
#define ir_SUB_OV_U8(_op1, _op2) 0
#define ir_SUB_OV_U16(_op1, _op2) 0
#define ir_SUB_OV_U32(_op1, _op2) 0
#define ir_SUB_OV_U64(_op1, _op2) 0
#define ir_SUB_OV_A(_op1, _op2) 0
#define ir_SUB_OV_C(_op1, _op2) 0
#define ir_SUB_OV_I8(_op1, _op2) 0
#define ir_SUB_OV_I16(_op1, _op2) 0
#define ir_SUB_OV_I32(_op1, _op2) 0
#define ir_SUB_OV_I64(_op1, _op2) 0

#define ir_MUL_OV(_type, _op1, _op2) 0
#define ir_MUL_OV_U8(_op1, _op2) 0
#define ir_MUL_OV_U16(_op1, _op2) 0
#define ir_MUL_OV_U32(_op1, _op2) 0
#define ir_MUL_OV_U64(_op1, _op2) 0
#define ir_MUL_OV_A(_op1, _op2) 0
#define ir_MUL_OV_C(_op1, _op2) 0
#define ir_MUL_OV_I8(_op1, _op2) 0
#define ir_MUL_OV_I16(_op1, _op2) 0
#define ir_MUL_OV_I32(_op1, _op2) 0
#define ir_MUL_OV_I64(_op1, _op2) 0

#define ir_OVERFLOW(_op1) 0

#define ir_NOT(_type, _op1) 0
#define ir_NOT_B(_op1) 0
#define ir_NOT_U8(_op1) 0
#define ir_NOT_U16(_op1) 0
#define ir_NOT_U32(_op1) 0
#define ir_NOT_U64(_op1) 0
#define ir_NOT_A(_op1) 0
#define ir_NOT_C(_op1) 0
#define ir_NOT_I8(_op1) 0
#define ir_NOT_I16(_op1) 0
#define ir_NOT_I32(_op1) 0
#define ir_NOT_I64(_op1) 0

#define ir_OR(_type, _op1, _op2) 0
#define ir_OR_B(_op1, _op2) 0
#define ir_OR_U8(_op1, _op2) 0
#define ir_OR_U16(_op1, _op2) 0
#define ir_OR_U32(_op1, _op2) 0
#define ir_OR_U64(_op1, _op2) 0
#define ir_OR_A(_op1, _op2) 0
#define ir_OR_C(_op1, _op2) 0
#define ir_OR_I8(_op1, _op2) 0
#define ir_OR_I16(_op1, _op2) 0
#define ir_OR_I32(_op1, _op2) 0
#define ir_OR_I64(_op1, _op2) 0

#define ir_AND(_type, _op1, _op2) 0
#define ir_AND_B(_op1, _op2) 0
#define ir_AND_U8(_op1, _op2) 0
#define ir_AND_U16(_op1, _op2) 0
#define ir_AND_U32(_op1, _op2) 0
#define ir_AND_U64(_op1, _op2) 0
#define ir_AND_A(_op1, _op2) 0
#define ir_AND_C(_op1, _op2) 0
#define ir_AND_I8(_op1, _op2) 0
#define ir_AND_I16(_op1, _op2) 0
#define ir_AND_I32(_op1, _op2) 0
#define ir_AND_I64(_op1, _op2) 0

#define ir_XOR(_type, _op1, _op2) 0
#define ir_XOR_B(_op1, _op2) 0
#define ir_XOR_U8(_op1, _op2) 0
#define ir_XOR_U16(_op1, _op2) 0
#define ir_XOR_U32(_op1, _op2) 0
#define ir_XOR_U64(_op1, _op2) 0
#define ir_XOR_A(_op1, _op2) 0
#define ir_XOR_C(_op1, _op2) 0
#define ir_XOR_I8(_op1, _op2) 0
#define ir_XOR_I16(_op1, _op2) 0
#define ir_XOR_I32(_op1, _op2) 0
#define ir_XOR_I64(_op1, _op2) 0

#define ir_SHL(_type, _op1, _op2) 0
#define ir_SHL_U8(_op1, _op2) 0
#define ir_SHL_U16(_op1, _op2) 0
#define ir_SHL_U32(_op1, _op2) 0
#define ir_SHL_U64(_op1, _op2) 0
#define ir_SHL_A(_op1, _op2) 0
#define ir_SHL_C(_op1, _op2) 0
#define ir_SHL_I8(_op1, _op2) 0
#define ir_SHL_I16(_op1, _op2) 0
#define ir_SHL_I32(_op1, _op2) 0
#define ir_SHL_I64(_op1, _op2) 0

#define ir_SHR(_type, _op1, _op2) 0
#define ir_SHR_U8(_op1, _op2) 0
#define ir_SHR_U16(_op1, _op2) 0
#define ir_SHR_U32(_op1, _op2) 0
#define ir_SHR_U64(_op1, _op2) 0
#define ir_SHR_A(_op1, _op2) 0
#define ir_SHR_C(_op1, _op2) 0
#define ir_SHR_I8(_op1, _op2) 0
#define ir_SHR_I16(_op1, _op2) 0
#define ir_SHR_I32(_op1, _op2) 0
#define ir_SHR_I64(_op1, _op2) 0

#define ir_SAR(_type, _op1, _op2) 0
#define ir_SAR_U8(_op1, _op2) 0
#define ir_SAR_U16(_op1, _op2) 0
#define ir_SAR_U32(_op1, _op2) 0
#define ir_SAR_U64(_op1, _op2) 0
#define ir_SAR_A(_op1, _op2) 0
#define ir_SAR_C(_op1, _op2) 0
#define ir_SAR_I8(_op1, _op2) 0
#define ir_SAR_I16(_op1, _op2) 0
#define ir_SAR_I32(_op1, _op2) 0
#define ir_SAR_I64(_op1, _op2) 0

#define ir_ROL(_type, _op1, _op2) 0
#define ir_ROL_U8(_op1, _op2) 0
#define ir_ROL_U16(_op1, _op2) 0
#define ir_ROL_U32(_op1, _op2) 0
#define ir_ROL_U64(_op1, _op2) 0
#define ir_ROL_A(_op1, _op2) 0
#define ir_ROL_C(_op1, _op2) 0
#define ir_ROL_I8(_op1, _op2) 0
#define ir_ROL_I16(_op1, _op2) 0
#define ir_ROL_I32(_op1, _op2) 0
#define ir_ROL_I64(_op1, _op2) 0

#define ir_ROR(_type, _op1, _op2) 0
#define ir_ROR_U8(_op1, _op2) 0
#define ir_ROR_U16(_op1, _op2) 0
#define ir_ROR_U32(_op1, _op2) 0
#define ir_ROR_U64(_op1, _op2) 0
#define ir_ROR_A(_op1, _op2) 0
#define ir_ROR_C(_op1, _op2) 0
#define ir_ROR_I8(_op1, _op2) 0
#define ir_ROR_I16(_op1, _op2) 0
#define ir_ROR_I32(_op1, _op2) 0
#define ir_ROR_I64(_op1, _op2) 0

#define ir_BSWAP(_type, _op1) 0
#define ir_BSWAP_U16(_op1) 0
#define ir_BSWAP_U32(_op1) 0
#define ir_BSWAP_U64(_op1) 0
#define ir_BSWAP_A(_op1) 0
#define ir_BSWAP_I16(_op1) 0
#define ir_BSWAP_I32(_op1) 0
#define ir_BSWAP_I64(_op1) 0

#define ir_CTPOP(_type, _op1) 0
#define ir_CTPOP_8(_op1) 0
#define ir_CTPOP_U16(_op1) 0
#define ir_CTPOP_U32(_op1) 0
#define ir_CTPOP_U64(_op1) 0
#define ir_CTPOP_A(_op1) 0
#define ir_CTPOP_C(_op1) 0
#define ir_CTPOP_I8(_op1) 0
#define ir_CTPOP_I16(_op1) 0
#define ir_CTPOP_I32(_op1) 0
#define ir_CTPOP_I64(_op1) 0

#define ir_CTLZ(_type, _op1) 0
#define ir_CTLZ_8(_op1) 0
#define ir_CTLZ_U16(_op1) 0
#define ir_CTLZ_U32(_op1) 0
#define ir_CTLZ_U64(_op1) 0
#define ir_CTLZ_A(_op1) 0
#define ir_CTLZ_C(_op1) 0
#define ir_CTLZ_I8(_op1) 0
#define ir_CTLZ_I16(_op1) 0
#define ir_CTLZ_I32(_op1) 0
#define ir_CTLZ_I64(_op1) 0

#define ir_CTTZ(_type, _op1) 0
#define ir_CTTZ_8(_op1) 0
#define ir_CTTZ_U16(_op1) 0
#define ir_CTTZ_U32(_op1) 0
#define ir_CTTZ_U64(_op1) 0
#define ir_CTTZ_A(_op1) 0
#define ir_CTTZ_C(_op1) 0
#define ir_CTTZ_I8(_op1) 0
#define ir_CTTZ_I16(_op1) 0
#define ir_CTTZ_I32(_op1) 0
#define ir_CTTZ_I64(_op1) 0

#define ir_MIN(_type, _op1, _op2) 0
#define ir_MIN_U8(_op1, _op2) 0
#define ir_MIN_U16(_op1, _op2) 0
#define ir_MIN_U32(_op1, _op2) 0
#define ir_MIN_U64(_op1, _op2) 0
#define ir_MIN_A(_op1, _op2) 0
#define ir_MIN_C(_op1, _op2) 0
#define ir_MIN_I8(_op1, _op2) 0
#define ir_MIN_I16(_op1, _op2) 0
#define ir_MIN_I32(_op1, _op2) 0
#define ir_MIN_I64(_op1, _op2) 0
#define ir_MIN_D(_op1, _op2) 0
#define ir_MIN_F(_op1, _op2) 0

#define ir_MAX(_type, _op1, _op2) 0
#define ir_MAX_U8(_op1, _op2) 0
#define ir_MAX_U16(_op1, _op2) 0
#define ir_MAX_U32(_op1, _op2) 0
#define ir_MAX_U64(_op1, _op2) 0
#define ir_MAX_A(_op1, _op2) 0
#define ir_MAX_C(_op1, _op2) 0
#define ir_MAX_I8(_op1, _op2) 0
#define ir_MAX_I16(_op1, _op2) 0
#define ir_MAX_I32(_op1, _op2) 0
#define ir_MAX_I64(_op1, _op2) 0
#define ir_MAX_D(_op1, _op2) 0
#define ir_MAX_F(_op1, _op2) 0

#define ir_COND(_type, _op1, _op2, _op3) 0
#define ir_COND_U8(_op1, _op2, _op3) 0
#define ir_COND_U16(_op1, _op2, _op3) 0
#define ir_COND_U32(_op1, _op2, _op3) 0
#define ir_COND_U64(_op1, _op2, _op3) 0
#define ir_COND_A(_op1, _op2, _op3) 0
#define ir_COND_C(_op1, _op2, _op3) 0
#define ir_COND_I8(_op1, _op2, _op3) 0
#define ir_COND_I16(_op1, _op2, _op3) 0
#define ir_COND_I32(_op1, _op2, _op3) 0
#define ir_COND_I64(_op1, _op2, _op3) 0
#define ir_COND_D(_op1, _op2, _op3) 0
#define ir_COND_F(_op1, _op2, _op3) 0

#define ir_PHI_2(type, _src1, _src2) 0
#define ir_PHI_N(type, _n, _inputs) 0
#define ir_PHI_SET_OP(_ref, _pos, _src) 0

#define ir_COPY(_type, _op1) 0
#define ir_COPY_B(_op1) 0
#define ir_COPY_U8(_op1) 0
#define ir_COPY_U16(_op1) 0
#define ir_COPY_U32(_op1) 0
#define ir_COPY_U64(_op1) 0
#define ir_COPY_A(_op1) 0
#define ir_COPY_C(_op1) 0
#define ir_COPY_I8(_op1) 0
#define ir_COPY_I16(_op1) 0
#define ir_COPY_I32(_op1) 0
#define ir_COPY_I64(_op1) 0
#define ir_COPY_D(_op1) 0
#define ir_COPY_F(_op1) 0

/* Helper to add address with a constant offset */
#define ir_ADD_OFFSET(_addr, _offset) 0

/* Unfoldable variant of COPY */
#define ir_HARD_COPY(_type, _op1) 0
#define ir_HARD_COPY_B(_op1) 0
#define ir_HARD_COPY_U8(_op1) 0
#define ir_HARD_COPY_U16(_op1) 0
#define ir_HARD_COPY_U32(_op1) 0
#define ir_HARD_COPY_U64(_op1) 0
#define ir_HARD_COPY_A(_op1) 0
#define ir_HARD_COPY_C(_op1) 0
#define ir_HARD_COPY_I8(_op1) 0
#define ir_HARD_COPY_I16(_op1) 0
#define ir_HARD_COPY_I32(_op1) 0
#define ir_HARD_COPY_I64(_op1) 0
#define ir_HARD_COPY_D(_op1) 0
#define ir_HARD_COPY_F(_op1) 0

#define ir_PARAM(_type, _name, _num) 0
#define ir_VAR(_type, _name) 0

#define ir_CALL(type, func) 0
#define ir_CALL_1(type, func, a1) 0
#define ir_CALL_2(type, func, a1, a2) 0
#define ir_CALL_3(type, func, a1, a2, a3) 0
#define ir_CALL_4(type, func, a1, a2, a3, a4) 0
#define ir_CALL_5(type, func, a1, a2, a3, a4, a5) 0
#define ir_CALL_6(type, func, a, b, c, d, e, f) 0
#define ir_CALL_N(type, func, count, args) 0

#define ir_TAILCALL(type, func) 0
#define ir_TAILCALL_1(type, func, a1) 0
#define ir_TAILCALL_2(type, func, a1, a2) 0
#define ir_TAILCALL_3(type, func, a1, a2, a3) 0
#define ir_TAILCALL_4(type, func, a1, a2, a3, a4) 0
#define ir_TAILCALL_5(type, func, a1, a2, a3, a4, a5) 0
#define ir_TAILCALL_6(type, func, a, b, c, d, e, f) 0
#define ir_TAILCALL_N(type, func, count, args) 0

#define ir_ALLOCA(_size) 0
#define ir_AFREE(_size) 0
#define ir_VADDR(_var) 0
#define ir_VLOAD(_type, _var) 0
#define ir_VLOAD_B(_var) 0
#define ir_VLOAD_U8(_var) 0
#define ir_VLOAD_U16(_var) 0
#define ir_VLOAD_U32(_var) 0
#define ir_VLOAD_U64(_var) 0
#define ir_VLOAD_A(_var) 0
#define ir_VLOAD_C(_var) 0
#define ir_VLOAD_I8(_var) 0
#define ir_VLOAD_I16(_var) 0
#define ir_VLOAD_I32(_var) 0
#define ir_VLOAD_I64(_var) 0
#define ir_VLOAD_D(_var) 0
#define ir_VLOAD_F(_var) 0
#define ir_VSTORE(_var, _val) 0
#define ir_RLOAD(_type, _reg) 0
#define ir_RLOAD_B(_reg) 0
#define ir_RLOAD_U8(_reg) 0
#define ir_RLOAD_U16(_reg) 0
#define ir_RLOAD_U32(_reg) 0
#define ir_RLOAD_U64(_reg) 0
#define ir_RLOAD_A(_reg) 0
#define ir_RLOAD_C(_reg) 0
#define ir_RLOAD_I8(_reg) 0
#define ir_RLOAD_I16(_reg) 0
#define ir_RLOAD_I32(_reg) 0
#define ir_RLOAD_I64(_reg) 0
#define ir_RLOAD_D(_reg) 0
#define ir_RLOAD_F(_reg) 0
#define ir_RSTORE(_reg, _val) 0
#define ir_LOAD(_type, _addr) 0
#define ir_LOAD_B(_addr) 0
#define ir_LOAD_U8(_addr) 0
#define ir_LOAD_U16(_addr) 0
#define ir_LOAD_U32(_addr) 0
#define ir_LOAD_U64(_addr) 0
#define ir_LOAD_A(_addr) 0
#define ir_LOAD_C(_addr) 0
#define ir_LOAD_I8(_addr) 0
#define ir_LOAD_I16(_addr) 0
#define ir_LOAD_I32(_addr) 0
#define ir_LOAD_I64(_addr) 0
#define ir_LOAD_D(_addr) 0
#define ir_LOAD_F(_addr) 0
#define ir_STORE(_addr, _val) 0
#define ir_TLS(_index, _offset) 0
#define ir_TRAP() 0

#define ir_FRAME_ADDR() 0

#define ir_BLOCK_BEGIN() 0
#define ir_BLOCK_END(_val) 0

#define ir_VA_START(_list) 0
#define ir_VA_END(_list) 0
#define ir_VA_COPY(_dst, _src) 0
#define ir_VA_ARG(_list, _type) 0

#define ir_START() 0
#define ir_ENTRY(_src, _num) 0
#define ir_BEGIN(_src) 0
#define ir_IF(_condition) 0
#define ir_IF_TRUE(_if) 0
#define ir_IF_TRUE_cold(_if) 0
#define ir_IF_FALSE(_if) 0
#define ir_IF_FALSE_cold(_if) 0
#define ir_END() 0
#define ir_MERGE_2(_src1, _src2) 0
#define ir_MERGE_N(_n, _inputs) 0
#define ir_MERGE_SET_OP(_ref, _pos, _src) 0
#define ir_LOOP_BEGIN(_src1) 0
#define ir_LOOP_END() 0
#define ir_SWITCH(_val) 0
#define ir_CASE_VAL(_switch, _val) 0
#define ir_CASE_DEFAULT(_switch) 0
#define ir_RETURN(_val) 0
#define ir_IJMP(_addr) 0
#define ir_UNREACHABLE() 0

#define ir_GUARD(_condition, _addr) 0
#define ir_GUARD_NOT(_condition, _addr) 0

#define ir_SNAPSHOT(_n) 0
#define ir_SNAPSHOT_SET_OP(_s, _pos, _v) 0

#define ir_EXITCALL(_func) 0

#define ir_END_list(_list) 0
#define ir_MERGE_list(_list) 0

#define ir_END_PHI_list(_list, _val) 0
#define ir_PHI_list(_list) 0

#define ir_MERGE_WITH(_src2) 0
#define ir_MERGE_WITH_EMPTY_TRUE(_if) 0
#define ir_MERGE_WITH_EMPTY_FALSE(_if) 0

#define ir_init(ctx, flag, consts_limit, insns_limt) 0
#define ir_consistency_check() 0
#define ir_const(ctx, val, type) 0
#define ir_dump(ctx, file)
#define ir_save(ctx, flags, file)
#define ir_dump_dot(ctx, name, file)
#define ir_check(ctx) 1
#define ir_jit_compile(ctx, opt, size) 0
#define ir_free(ctx)
#define ir_mem_mmap(size) 0

#undef _ir_CTX

#include "parse.c"

void* parse_syntax_check(Arena* main_arena,
                         Arena* temp_arena,
                         const char* filename,
                         ReadFileResult file,
                         void* (*get_extern)(StrView),
                         int verbose,
                         bool ir_only,
                         int opt_level) {
  return parse_impl(main_arena, temp_arena, filename, file, get_extern, verbose, ir_only,
                    opt_level);
}
