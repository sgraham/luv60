/*

QBE LICENSE:

© 2015-2024 Quentin Carbonneaux <quentin@c9x.me>

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

All other libqbe code under the same license,
© 2025 Scott Graham <scott.libqbe@h4ck3r.net>

*/

#ifdef _MSC_VER
#define LQ_NO_RETURN __declspec(noreturn)
#pragma warning(push)
#pragma warning(disable: 4146)
#pragma warning(disable: 4200)
#pragma warning(disable: 4244)
#pragma warning(disable: 4245)
#pragma warning(disable: 4389)
#pragma warning(disable: 4459)
#pragma warning(disable: 4701)
#else
#define LQ_NO_RETURN __attribute__((noreturn))
#endif

#ifndef LQ_ASSERT
#include <assert.h>
#define LQ_ASSERT(x) assert(x)
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

enum {
	NString = 80,
	NIns    = 1 << 20,
	NAlign  = 3,
	NField  = 32,
	NBit    = CHAR_BIT * sizeof(bits),
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

#define R        (Ref){RTmp, 0}
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
	if (req(r, R))
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

#ifndef P
	#define P(CanFold, HasId, IdVal)
#endif


#define T(a,b,c,d,e,f,g,h) {                          \
	{[Kw]=K##a, [Kl]=K##b, [Ks]=K##c, [Kd]=K##d}, \
	{[Kw]=K##e, [Kl]=K##f, [Ks]=K##g, [Kd]=K##h}  \
}


/*********************/
/* PUBLIC OPERATIONS */
/*********************/

/* Arithmetic and Bits */
O(add,     T(w,l,s,d, w,l,s,d), P(1,1,0)) X(2,1,0) V(1)
O(sub,     T(w,l,s,d, w,l,s,d), P(1,1,0)) X(2,1,0) V(0)
O(neg,     T(w,l,s,d, x,x,x,x), P(1,0,0)) X(1,1,0) V(0)
O(div,     T(w,l,s,d, w,l,s,d), P(1,1,1)) X(0,0,0) V(0)
O(rem,     T(w,l,e,e, w,l,e,e), P(1,0,0)) X(0,0,0) V(0)
O(udiv,    T(w,l,e,e, w,l,e,e), P(1,1,1)) X(0,0,0) V(0)
O(urem,    T(w,l,e,e, w,l,e,e), P(1,0,0)) X(0,0,0) V(0)
O(mul,     T(w,l,s,d, w,l,s,d), P(1,1,1)) X(2,0,0) V(0)
O(and,     T(w,l,e,e, w,l,e,e), P(1,0,0)) X(2,1,0) V(1)
O(or,      T(w,l,e,e, w,l,e,e), P(1,1,0)) X(2,1,0) V(1)
O(xor,     T(w,l,e,e, w,l,e,e), P(1,1,0)) X(2,1,0) V(1)
O(sar,     T(w,l,e,e, w,w,e,e), P(1,1,0)) X(1,1,0) V(1)
O(shr,     T(w,l,e,e, w,w,e,e), P(1,1,0)) X(1,1,0) V(1)
O(shl,     T(w,l,e,e, w,w,e,e), P(1,1,0)) X(1,1,0) V(1)

/* Comparisons */
O(ceqw,    T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cnew,    T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csgew,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csgtw,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cslew,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csltw,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(1)
O(cugew,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cugtw,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(culew,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cultw,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(1)

O(ceql,    T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cnel,    T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csgel,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csgtl,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cslel,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csltl,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(1)
O(cugel,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cugtl,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(culel,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cultl,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(1)

O(ceqs,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cges,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cgts,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cles,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(clts,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cnes,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cos,     T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cuos,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)

O(ceqd,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cged,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cgtd,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cled,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cltd,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cned,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cod,     T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cuod,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)

/* Memory */
O(storeb,  T(w,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(storeh,  T(w,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(storew,  T(w,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(storel,  T(l,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(stores,  T(s,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(stored,  T(d,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)

O(loadsb,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loadub,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loadsh,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loaduh,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loadsw,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loaduw,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(load,    T(m,m,m,m, x,x,x,x), P(0,0,0)) X(0,0,1) V(0)

/* Extensions and Truncations */
O(extsb,   T(w,w,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extub,   T(w,w,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extsh,   T(w,w,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extuh,   T(w,w,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extsw,   T(e,w,e,e, e,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extuw,   T(e,w,e,e, e,x,e,e), P(1,0,0)) X(0,0,1) V(0)

O(exts,    T(e,e,e,s, e,e,e,x), P(1,0,0)) X(0,0,1) V(0)
O(truncd,  T(e,e,d,e, e,e,x,e), P(1,0,0)) X(0,0,1) V(0)
O(stosi,   T(s,s,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(stoui,   T(s,s,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(dtosi,   T(d,d,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(dtoui,   T(d,d,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(swtof,   T(e,e,w,w, e,e,x,x), P(1,0,0)) X(0,0,1) V(0)
O(uwtof,   T(e,e,w,w, e,e,x,x), P(1,0,0)) X(0,0,1) V(0)
O(sltof,   T(e,e,l,l, e,e,x,x), P(1,0,0)) X(0,0,1) V(0)
O(ultof,   T(e,e,l,l, e,e,x,x), P(1,0,0)) X(0,0,1) V(0)
O(cast,    T(s,d,w,l, x,x,x,x), P(1,0,0)) X(0,0,1) V(0)

/* Stack Allocation */
O(alloc4,  T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(alloc8,  T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(alloc16, T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)

/* Variadic Function Helpers */
O(vaarg,   T(m,m,m,m, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(vastart, T(m,e,e,e, x,e,e,e), P(0,0,0)) X(0,0,0) V(0)

O(copy,    T(w,l,s,d, x,x,x,x), P(0,0,0)) X(0,0,1) V(0)

/* Debug */
O(dbgloc,  T(w,e,e,e, w,e,e,e), P(0,0,0)) X(0,0,1) V(0)

/****************************************/
/* INTERNAL OPERATIONS (keep nop first) */
/****************************************/

/* Miscellaneous and Architecture-Specific Operations */
O(nop,     T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,1) V(0)
O(addr,    T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(blit0,   T(m,e,e,e, m,e,e,e), P(0,0,0)) X(0,1,0) V(0)
O(blit1,   T(w,e,e,e, x,e,e,e), P(0,0,0)) X(0,1,0) V(0)
O(swap,    T(w,l,s,d, w,l,s,d), P(0,0,0)) X(1,0,0) V(0)
O(sign,    T(w,l,e,e, x,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(salloc,  T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(xidiv,   T(w,l,e,e, x,x,e,e), P(0,0,0)) X(1,0,0) V(0)
O(xdiv,    T(w,l,e,e, x,x,e,e), P(0,0,0)) X(1,0,0) V(0)
O(xcmp,    T(w,l,s,d, w,l,s,d), P(0,0,0)) X(1,1,0) V(0)
O(xtest,   T(w,l,e,e, w,l,e,e), P(0,0,0)) X(1,1,0) V(0)
O(acmp,    T(w,l,e,e, w,l,e,e), P(0,0,0)) X(0,0,0) V(0)
O(acmn,    T(w,l,e,e, w,l,e,e), P(0,0,0)) X(0,0,0) V(0)
O(afcmp,   T(e,e,s,d, e,e,s,d), P(0,0,0)) X(0,0,0) V(0)
O(reqz,    T(w,l,e,e, x,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(rnez,    T(w,l,e,e, x,x,e,e), P(0,0,0)) X(0,0,0) V(0)

/* Arguments, Parameters, and Calls */
O(par,     T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(parsb,   T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(parub,   T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(parsh,   T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(paruh,   T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(parc,    T(e,x,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(pare,    T(e,x,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(arg,     T(w,l,s,d, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(argsb,   T(w,e,e,e, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(argub,   T(w,e,e,e, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(argsh,   T(w,e,e,e, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(arguh,   T(w,e,e,e, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(argc,    T(e,x,e,e, e,l,e,e), P(0,0,0)) X(0,0,0) V(0)
O(arge,    T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(argv,    T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(call,    T(m,m,m,m, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)

/* Flags Setting */
O(flagieq,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagine,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagisge, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagisgt, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagisle, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagislt, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagiuge, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagiugt, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagiule, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagiult, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfeq,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfge,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfgt,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfle,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagflt,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfne,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfo,   T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfuo,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)


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
	NPubOp = Onop,
	Jjf = Jjfieq,
	Jjf1 = Jjffuo,
};

#define INRANGE(x, l, u) ((unsigned)(x) - l <= u - l) /* linear in x */
#define isstore(o) INRANGE(o, Ostoreb, Ostored)
#define isload(o) INRANGE(o, Oloadsb, Oload)
#define isext(o) INRANGE(o, Oextsb, Oextuw)
#define ispar(o) INRANGE(o, Opar, Opare)
#define isarg(o) INRANGE(o, Oarg, Oargv)
#define isret(j) INRANGE(j, Jretw, Jret0)
#define isparbh(o) INRANGE(o, Oparsb, Oparuh)
#define isargbh(o) INRANGE(o, Oargsb, Oarguh)
#define isretbh(j) INRANGE(j, Jretsb, Jretuh)

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
	uint hasid:1;
	uint idval:1; /* identity value 0/1 */
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
	int cls;
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

/* main.c */
static Target T;
static char debug['Z'+1];

/* util.c */
typedef enum {
	PHeap, /* free() necessary */
	PFn, /* discarded after processing the function */
} Pool;

static Typ *typ;
static Ins insb[NIns], *curi;
static uint32_t hash(char *);
LQ_NO_RETURN static void die_(char *, char *, ...) ;
static void *emalloc(size_t);
static void *alloc(size_t);
static void freeall(void);
static void *vnew(ulong, size_t, Pool);
static void vfree(void *);
static void vgrow(void *, ulong);
static void strf(char[NString], char *, ...);
static uint32_t intern(char *);
static char *str(uint32_t);
static int argcls(Ins *, int);
static int isreg(Ref);
static int iscmp(int, int *, int *);
static void emit(int, int, Ref, Ref, Ref);
static void emiti(Ins);
static void idup(Ins **, Ins *, ulong);
static Ins *icpy(Ins *, Ins *, ulong);
static int cmpop(int);
static int cmpneg(int);
static int clsmerge(short *, short);
static int phicls(int, Tmp *);
static Ref newtmp(char *, int, Fn *);
static void chuse(Ref, int, Fn *);
static int symeq(Sym, Sym);
static Ref newcon(Con *, Fn *);
static Ref getcon(int64_t, Fn *);
static int addcon(Con *, Con *, int);
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
	assert(elt < bs->nt * NBit);
	return (bs->t[elt/NBit] & BIT(elt%NBit)) != 0;
}

/* parse.c */
static Op optab[NOp];
static void printfn(Fn *, FILE *);
static void printtyp(Typ*, FILE *);
static void printref(Ref, Fn *, FILE *);
LQ_NO_RETURN static void err(char *, ...) ;

/* abi.c */
static void elimsb(Fn *);

/* cfg.c */
static Blk *newblk(void);
static void edgedel(Blk *, Blk **);
static void fillpreds(Fn *);
static void fillrpo(Fn *);
static void filldom(Fn *);
static int sdom(Blk *, Blk *);
static int dom(Blk *, Blk *);
static void fillfron(Fn *);
static void loopiter(Fn *, void (*)(Blk *, Blk *));
static void fillloop(Fn *);
static void simpljmp(Fn *);

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
static void filluse(Fn *);
static void ssa(Fn *);
static void ssacheck(Fn *);

/* copy.c */
static void copy(Fn *);

/* fold.c */
static void fold(Fn *);

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
static int stashbits(void *, int);
static void elf_emitfnfin(char *, FILE *);
static void elf_emitfin(FILE *);
static void win_emitfin(FILE *);
static void macho_emitfin(FILE *);
/*** END FILE: all.h ***/
/*** START FILE: amd64/all.h ***/
/* skipping ../all.h */

typedef struct Amd64Op Amd64Op;

enum Amd64Reg {
	/* |Register    | SysV             | WinABI           | */
	/* +------------+------------------+------------------+ */
	QBE_AMD64_RAX = RXX+1, /* | caller-save (v)  | caller-save (v)  | */
	QBE_AMD64_RCX,         /* | caller-save (v)  | caller-save (v)  | */
	QBE_AMD64_RDX,         /* | caller-save (v)  | caller-save (v)  | */
	QBE_AMD64_RSI,         /* | caller-save (v)  | callee-save (nv) | */
	QBE_AMD64_RDI,         /* | caller-save (v)  | callee-save (nv) | */
	QBE_AMD64_R8,          /* | caller-save (v)  | caller-save (v)  | */
	QBE_AMD64_R9,          /* | caller-save (v)  | caller-save (v)  | */
	QBE_AMD64_R10,         /* | caller-save (v)  | caller-save (v)  | */
	QBE_AMD64_R11,         /* | caller-save (v)  | caller-save (v)  | */
	             /* +------------------+------------------+ */
	QBE_AMD64_RBX,         /* | callee-save (nv) | callee-save (nv) | */
	QBE_AMD64_R12,         /* | callee-save (nv) | callee-save (nv) | */
	QBE_AMD64_R13,         /* | callee-save (nv) | callee-save (nv) | */
	QBE_AMD64_R14,         /* | callee-save (nv) | callee-save (nv) | */
	QBE_AMD64_R15,         /* | callee-save (nv) | callee-save (nv) | */
				 /* +------------------+------------------+ */
	QBE_AMD64_RBP,         /* | globally live    | callee-save (nv) | */
	QBE_AMD64_RSP,         /* | globally live    | callee-save (nv) | */

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
	QBE_AMD64_NGPS = QBE_AMD64_R11 - QBE_AMD64_RAX + 1,
	QBE_AMD64_NFPS = QBE_AMD64_NFPR,
	QBE_AMD64_NCLR = QBE_AMD64_R15 - QBE_AMD64_RBX + 1,
};
MAKESURE(reg_not_tmp, QBE_AMD64_XMM15 < (int)Tmp0);

struct Amd64Op {
	char nmem;
	char zflag;
	char lflag;
};

/* targ.c */
static Amd64Op amd64_op[138];

/* sysv.c (abi) */
static int amd64_sysv_rsave[25];
static int amd64_sysv_rclob[6];
static bits amd64_sysv_retregs(Ref, int[2]);
static bits amd64_sysv_argregs(Ref, int[2]);
static void amd64_sysv_abi(Fn *);

/* winabi.c (abi) */
static int amd64_winabi_rsave[23];
static int amd64_winabi_rclob[8];
static bits amd64_winabi_retregs(Ref, int[2]);
static bits amd64_winabi_argregs(Ref, int[2]);
static void amd64_winabi_abi(Fn *);

/* isel.c */
static void amd64_isel(Fn *);

/* emit.c */
static void amd64_emitfn(Fn *, FILE *);
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
/*** END FILE: arm64/all.h ***/
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
static Rv64Op rv64_op[138];

/* abi.c */
static bits rv64_retregs(Ref, int[2]);
static bits rv64_argregs(Ref, int[2]);
static void rv64_abi(Fn *);

/* isel.c */
static void rv64_isel(Fn *);

/* emit.c */
static void rv64_emitfn(Fn *, FILE *);
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
		LQ_ASSERT(a->type != ABot);
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
		LQ_ASSERT(ap.type == ACon || ap.type == AUnk);
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

	LQ_ASSERT(rtype(r) <= RType);
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
			LQ_ASSERT(astack(a->type));
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
			LQ_ASSERT(rtype(p->to) == RTmp);
			a = &fn->tmp[p->to.val].alias;
			LQ_ASSERT(a->type == ABot);
			a->type = AUnk;
			a->base = p->to.val;
			a->offset = 0;
			a->slot = 0;
		}
		for (i=b->ins; i<&b->ins[b->nins]; ++i) {
			a = 0;
			if (!req(i->to, R)) {
				LQ_ASSERT(rtype(i->to) == RTmp);
				a = &fn->tmp[i->to.val].alias;
				LQ_ASSERT(a->type == ABot);
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
				LQ_ASSERT(a);
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
			if (req(i->to, R) || a->type == AUnk)
			if (i->op != Oblit0) {
				if (!isload(i->op))
					qbe_alias_esc(i->arg[0], fn);
				if (!isstore(i->op))
				if (i->op != Oargc)
					qbe_alias_esc(i->arg[1], fn);
			}
			if (i->op == Oblit0) {
				++i;
				LQ_ASSERT(i->op == Oblit1);
				LQ_ASSERT(rtype(i->arg[0]) == RInt);
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
/*** END FILE: alias.c ***/
/*** START FILE: cfg.c ***/
/* skipping all.h */

Blk *
newblk(void)
{
	static Blk z;
	Blk *b;

	b = alloc(sizeof *b);
	*b = z;
	return b;
}

void
edgedel(Blk *bs, Blk **pbd)
{
	Blk *bd;
	Phi *p;
	uint a;
	int mult;

	bd = *pbd;
	mult = 1 + (bs->s1 == bs->s2);
	*pbd = 0;
	if (!bd || mult > 1)
		return;
	for (p=bd->phi; p; p=p->link) {
		for (a=0; p->blk[a]!=bs; a++)
			LQ_ASSERT(a+1<p->narg);
		p->narg--;
		memmove(&p->blk[a], &p->blk[a+1],
			sizeof p->blk[0] * (p->narg-a));
		memmove(&p->arg[a], &p->arg[a+1],
			sizeof p->arg[0] * (p->narg-a));
	}
	if (bd->npred != 0) {
		for (a=0; bd->pred[a]!=bs; a++)
			LQ_ASSERT(a+1<bd->npred);
		bd->npred--;
		memmove(&bd->pred[a], &bd->pred[a+1],
			sizeof bd->pred[0] * (bd->npred-a));
	}
}

static void
qbe_cfg_addpred(Blk *bp, Blk *bc)
{
	if (!bc->pred) {
		bc->pred = alloc(bc->npred * sizeof bc->pred[0]);
		bc->visit = 0;
	}
	bc->pred[bc->visit++] = bp;
}

/* fill predecessors information in blocks */
void
fillpreds(Fn *f)
{
	Blk *b;

	for (b=f->start; b; b=b->link) {
		b->npred = 0;
		b->pred = 0;
	}
	for (b=f->start; b; b=b->link) {
		if (b->s1)
			b->s1->npred++;
		if (b->s2 && b->s2 != b->s1)
			b->s2->npred++;
	}
	for (b=f->start; b; b=b->link) {
		if (b->s1)
			qbe_cfg_addpred(b, b->s1);
		if (b->s2 && b->s2 != b->s1)
			qbe_cfg_addpred(b, b->s2);
	}
}

static int
qbe_cfg_rporec(Blk *b, uint x)
{
	Blk *s1, *s2;

	if (!b || b->id != -1u)
		return x;
	b->id = 1;
	s1 = b->s1;
	s2 = b->s2;
	if (s1 && s2 && s1->loop > s2->loop) {
		s1 = b->s2;
		s2 = b->s1;
	}
	x = qbe_cfg_rporec(s1, x);
	x = qbe_cfg_rporec(s2, x);
	b->id = x;
	LQ_ASSERT(x != -1u);
	return x - 1;
}

/* fill the rpo information */
void
fillrpo(Fn *f)
{
	uint n;
	Blk *b, **p;

	for (b=f->start; b; b=b->link)
		b->id = -1u;
	n = 1 + qbe_cfg_rporec(f->start, f->nblk-1);
	f->nblk -= n;
	f->rpo = alloc(f->nblk * sizeof f->rpo[0]);
	for (p=&f->start; (b=*p);) {
		if (b->id == -1u) {
			edgedel(b, &b->s1);
			edgedel(b, &b->s2);
			*p = b->link;
		} else {
			b->id -= n;
			f->rpo[b->id] = b;
			p = &b->link;
		}
	}
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
			LQ_ASSERT(b1);
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
			LQ_ASSERT(d != b);
			b->dlink = d->dom;
			d->dom = b;
		}
}

int
sdom(Blk *b1, Blk *b2)
{
	LQ_ASSERT(b1 && b2);
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
		LQ_ASSERT(!b->phi);
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
	free(uf);
}
/*** END FILE: cfg.c ***/
/*** START FILE: copy.c ***/
/* skipping all.h */

static int
qbe_copy_iscon(Ref r, int64_t bits, Fn *fn)
{
	return rtype(r) == RCon
		&& fn->con[r.val].type == CBits
		&& fn->con[r.val].bits.i == bits;
}

static int
qbe_copy_iscopy(Ins *i, Ref r, Fn *fn)
{
	static bits extcpy[] = {
		[WFull] = 0,
		[Wsb] = BIT(Wsb) | BIT(Wsh) | BIT(Wsw),
		[Wub] = BIT(Wub) | BIT(Wuh) | BIT(Wuw),
		[Wsh] = BIT(Wsh) | BIT(Wsw),
		[Wuh] = BIT(Wuh) | BIT(Wuw),
		[Wsw] = BIT(Wsw),
		[Wuw] = BIT(Wuw),
	};
	Op *op;
	bits b;
	Tmp *t;

	if (i->op == Ocopy)
		return 1;
	op = &optab[i->op];
	if (op->hasid && KBASE(i->cls) == 0)
		return qbe_copy_iscon(i->arg[1], op->idval, fn);
	if (!isext(i->op) || rtype(r) != RTmp)
		return 0;
	if (i->op == Oextsw || i->op == Oextuw)
	if (i->cls == Kw)
		return 1;

	t = &fn->tmp[r.val];
	LQ_ASSERT(KBASE(t->cls) == 0);
	if (i->cls == Kl && t->cls == Kw)
		return 0;
	b = extcpy[t->width];
	return (BIT(Wsb + (i->op-Oextsb)) & b) != 0;
}

static Ref
qbe_copy_copyof(Ref r, Ref *cpy)
{
	if (rtype(r) == RTmp && !req(cpy[r.val], R))
		return cpy[r.val];
	return r;
}

/* detects a cluster of phis/copies redundant with 'r';
 * the algorithm is inspired by Section 3.2 of "Simple
 * and Efficient SSA Construction" by Braun M. et al.
 */
static void
qbe_copy_phisimpl(Phi *p, Ref r, Ref *cpy, Use ***pstk, BSet *ts, BSet *as, Fn *fn)
{
	Use **stk, *u, *u1;
	uint nstk, a;
	int t;
	Ref r1;
	Phi *p0;

	bszero(ts);
	bszero(as);
	p0 = &(Phi){.narg = 0};
	stk = *pstk;
	nstk = 1;
	stk[0] = &(Use){.type = UPhi, .u.phi = p};
	while (nstk) {
		u = stk[--nstk];
		if (u->type == UIns && qbe_copy_iscopy(u->u.ins, r, fn)) {
			p = p0;
			t = u->u.ins->to.val;
		}
		else if (u->type == UPhi) {
			p = u->u.phi;
			t = p->to.val;
		}
		else
			continue;
		if (bshas(ts, t))
			continue;
		bsset(ts, t);
		for (a=0; a<p->narg; a++) {
			r1 = qbe_copy_copyof(p->arg[a], cpy);
			if (req(r1, r))
				continue;
			if (rtype(r1) != RTmp)
				return;
			bsset(as, r1.val);
		}
		u = fn->tmp[t].use;
		u1 = &u[fn->tmp[t].nuse];
		vgrow(pstk, nstk+(u1-u));
		stk = *pstk;
		for (; u<u1; u++)
			stk[nstk++] = u;
	}
	bsdiff(as, ts);
	if (!bscount(as))
		for (t=0; bsiter(ts, &t); t++)
			cpy[t] = r;
}

static void
qbe_copy_subst(Ref *pr, Ref *cpy)
{
	LQ_ASSERT(rtype(*pr) != RTmp || !req(cpy[pr->val], R));
	*pr = qbe_copy_copyof(*pr, cpy);
}

/* requires use and dom, breaks use */
void
copy(Fn *fn)
{
	BSet ts[1], as[1];
	Use **stk;
	Phi *p, **pp;
	Ins *i;
	Blk *b;
	uint n, a, eq;
	Ref *cpy, r, r1;
	int t;

	bsinit(ts, fn->ntmp);
	bsinit(as, fn->ntmp);
	cpy = emalloc(fn->ntmp * sizeof cpy[0]);
	stk = vnew(10, sizeof stk[0], PHeap);

	/* 1. build the copy-of map */
	for (n=0; n<fn->nblk; n++) {
		b = fn->rpo[n];
		for (p=b->phi; p; p=p->link) {
			LQ_ASSERT(rtype(p->to) == RTmp);
			if (!req(cpy[p->to.val], R))
				continue;
			eq = 0;
			r = R;
			for (a=0; a<p->narg; a++)
				if (p->blk[a]->id < n) {
					r1 = qbe_copy_copyof(p->arg[a], cpy);
					if (req(r, R) || req(r, UNDEF))
						r = r1;
					if (req(r1, r) || req(r1, UNDEF))
						eq++;
				}
			LQ_ASSERT(!req(r, R));
			if (rtype(r) == RTmp
			&& !dom(fn->rpo[fn->tmp[r.val].bid], b))
				cpy[p->to.val] = p->to;
			else if (eq == p->narg)
				cpy[p->to.val] = r;
			else {
				cpy[p->to.val] = p->to;
				qbe_copy_phisimpl(p, r, cpy, &stk, ts, as, fn);
			}
		}
		for (i=b->ins; i<&b->ins[b->nins]; i++) {
			LQ_ASSERT(rtype(i->to) <= RTmp);
			if (!req(cpy[i->to.val], R))
				continue;
			r = qbe_copy_copyof(i->arg[0], cpy);
			if (qbe_copy_iscopy(i, r, fn))
				cpy[i->to.val] = r;
			else
				cpy[i->to.val] = i->to;
		}
	}

	/* 2. remove redundant phis/copies
	 * and rewrite their uses */
	for (b=fn->start; b; b=b->link) {
		for (pp=&b->phi; (p=*pp);) {
			r = cpy[p->to.val];
			if (!req(r, p->to)) {
				*pp = p->link;
				continue;
			}
			for (a=0; a<p->narg; a++)
				qbe_copy_subst(&p->arg[a], cpy);
			pp=&p->link;
		}
		for (i=b->ins; i<&b->ins[b->nins]; i++) {
			r = cpy[i->to.val];
			if (!req(r, i->to)) {
				*i = (Ins){.op = Onop};
				continue;
			}
			qbe_copy_subst(&i->arg[0], cpy);
			qbe_copy_subst(&i->arg[1], cpy);
		}
		qbe_copy_subst(&b->jmp.arg, cpy);
	}

	if (debug['C']) {
		fprintf(stderr, "\n> Copy information:");
		for (t=Tmp0; t<fn->ntmp; t++) {
			if (req(cpy[t], R)) {
				fprintf(stderr, "\n%10s not seen!",
					fn->tmp[t].name);
			}
			else if (!req(cpy[t], TMP(t))) {
				fprintf(stderr, "\n%10s copy of ",
					fn->tmp[t].name);
				printref(cpy[t], fn, stderr);
			}
		}
		fprintf(stderr, "\n\n> After copy elimination:\n");
		printfn(fn, stderr);
	}
	vfree(stk);
	free(cpy);
}
/*** END FILE: copy.c ***/
/*** START FILE: emit.c ***/
/* skipping all.h */

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

	pfx = n[0] == '"' ? "" : T.assym;
	sfx = "";
	if (T.apple && l->thread) {
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
	static char *dtoa[] = {
		[DB] = "\t.byte",
		[DH] = "\t.short",
		[DW] = "\t.int",
		[DL] = "\t.quad"
	};
	static int64_t zero;
	char *p;

	switch (d->type) {
	case DStart:
		zero = 0;
		break;
	case DEnd:
		if (d->lnk->common) {
			if (zero == -1)
				die("invalid common data definition");
			p = d->name[0] == '"' ? "" : T.assym;
			fprintf(f, ".comm %s%s,%"PRId64,
				p, d->name, zero);
			if (d->lnk->align)
				fprintf(f, ",%d", d->lnk->align);
			fputc('\n', f);
		}
		else if (zero != -1) {
			emitlnk(d->name, d->lnk, SecBss, f);
			fprintf(f, "\t.fill %"PRId64",1,0\n", zero);
		}
		break;
	case DZ:
		if (zero != -1)
			zero += d->u.num;
		else
			fprintf(f, "\t.fill %"PRId64",1,0\n", d->u.num);
		break;
	default:
		if (zero != -1) {
			emitlnk(d->name, d->lnk, SecData, f);
			if (zero > 0)
				fprintf(f, "\t.fill %"PRId64",1,0\n", zero);
			zero = -1;
		}
		if (d->isstr) {
			if (d->type != DB)
				err("strings only supported for 'b' currently");
			fprintf(f, "\t.ascii %s\n", d->u.str);
		}
		else if (d->isref) {
			p = d->u.ref.name[0] == '"' ? "" : T.assym;
			fprintf(f, "%s %s%s%+"PRId64"\n",
				dtoa[d->type], p, d->u.ref.name,
				d->u.ref.off);
		}
		else {
			fprintf(f, "%s %"PRId64"\n",
				dtoa[d->type], d->u.num);
		}
		break;
	}
}

typedef struct Asmbits Asmbits;

struct Asmbits {
	char bits[16];
	int size;
	Asmbits *link;
};

static Asmbits *stash;

int
stashbits(void *bits, int size)
{
	Asmbits **pb, *b;
	int i;

	LQ_ASSERT(size == 4 || size == 8 || size == 16);
	for (pb=&stash, i=0; (b=*pb); pb=&b->link, i++)
		if (size <= b->size)
		if (memcmp(bits, b->bits, size) == 0)
			return i;
	b = emalloc(sizeof *b);
	memcpy(b->bits, bits, size);
	b->size = size;
	b->link = 0;
	*pb = b;
	return i;
}

static void
qbe_emit_emitfin(FILE *f, char *sec[3])
{
	Asmbits *b;
	char *p;
	int lg, i;
	double d;

	if (!stash)
		return;
	fprintf(f, "/* floating point constants */\n");
	for (lg=4; lg>=2; lg--)
		for (b=stash, i=0; b; b=b->link, i++) {
			if (b->size == (1<<lg)) {
				fprintf(f,
					".section %s\n"
					".p2align %d\n"
					"%sfp%d:",
					sec[lg-2], lg, T.asloc, i
				);
				for (p=b->bits; p<&b->bits[b->size]; p+=4)
					fprintf(f, "\n\t.int %"PRId32,
						*(int32_t *)p);
				if (lg <= 3) {
					if (lg == 2)
						d = *(float *)b->bits;
					else
						d = *(double *)b->bits;
					fprintf(f, " /* %f */\n\n", d);
				} else
					fprintf(f, "\n\n");
			}
		}
	while ((b=stash)) {
		stash = b->link;
		free(b);
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
win_emitfin(FILE *f)
{
	static char *sec[3] = { ".rodata", ".rodata", ".rodata" };

	qbe_emit_emitfin(f ,sec);
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

static uint32_t *file;
static uint nfile;
static uint curfile;

void
emitdbgfile(char *fn, FILE *f)
{
	uint32_t id;
	uint n;

	id = intern(fn);
	for (n=0; n<nfile; n++)
		if (file[n] == id) {
			/* gas requires positive
			 * file numbers */
			curfile = n + 1;
			return;
		}
	if (!file)
		file = vnew(0, sizeof *file, PHeap);
	vgrow(&file, ++nfile);
	file[nfile-1] = id;
	curfile = nfile;
	fprintf(f, ".file %u %s\n", curfile, fn);
}

void
emitdbgloc(uint line, uint col, FILE *f)
{
	if (col != 0)
		fprintf(f, "\t.loc %u %u %u\n", curfile, line, col);
	else
		fprintf(f, "\t.loc %u %u\n", curfile, line);
}
/*** END FILE: emit.c ***/
/*** START FILE: fold.c ***/
/* skipping all.h */

enum {
	Bot = -1, /* lattice bottom */
	Top = 0,  /* lattice top (matches UNDEF) */
};

typedef struct Edge Edge;

struct Edge {
	int dest;
	int dead;
	Edge *work;
};

static int *val;
static Edge *flowrk, (*edge)[2];
static Use **usewrk;
static uint nuse;

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

static int
qbe_fold_latval(Ref r)
{
	switch (rtype(r)) {
	case RTmp:
		return val[r.val];
	case RCon:
		return r.val;
	default:
		die("unreachable");
	}
}

static int
qbe_fold_latmerge(int v, int m)
{
	return m == Top ? v : (v == Top || v == m) ? m : Bot;
}

static void
qbe_fold_update(int t, int m, Fn *fn)
{
	Tmp *tmp;
	uint u;

	m = qbe_fold_latmerge(val[t], m);
	if (m != val[t]) {
		tmp = &fn->tmp[t];
		for (u=0; u<tmp->nuse; u++) {
			vgrow(&usewrk, ++nuse);
			usewrk[nuse-1] = &tmp->use[u];
		}
		val[t] = m;
	}
}

static int
qbe_fold_deadedge(int s, int d)
{
	Edge *e;

	e = edge[s];
	if (e[0].dest == d && !e[0].dead)
		return 0;
	if (e[1].dest == d && !e[1].dead)
		return 0;
	return 1;
}

static void
qbe_fold_visitphi(Phi *p, int n, Fn *fn)
{
	int v;
	uint a;

	v = Top;
	for (a=0; a<p->narg; a++)
		if (!qbe_fold_deadedge(p->blk[a]->id, n))
			v = qbe_fold_latmerge(v, qbe_fold_latval(p->arg[a]));
	qbe_fold_update(p->to.val, v, fn);
}

static int qbe_fold_opfold(int, int, Con *, Con *, Fn *);

static void
qbe_fold_visitins(Ins *i, Fn *fn)
{
	int v, l, r;

	if (rtype(i->to) != RTmp)
		return;
	if (optab[i->op].canfold) {
		l = qbe_fold_latval(i->arg[0]);
		if (!req(i->arg[1], R))
			r = qbe_fold_latval(i->arg[1]);
		else
			r = CON_Z.val;
		if (l == Bot || r == Bot)
			v = Bot;
		else if (l == Top || r == Top)
			v = Top;
		else
			v = qbe_fold_opfold(i->op, i->cls, &fn->con[l], &fn->con[r], fn);
	} else
		v = Bot;
	/* fprintf(stderr, "\nvisiting %s (%p)", optab[i->op].name, (void *)i); */
	qbe_fold_update(i->to.val, v, fn);
}

static void
qbe_fold_visitjmp(Blk *b, int n, Fn *fn)
{
	int l;

	switch (b->jmp.type) {
	case Jjnz:
		l = qbe_fold_latval(b->jmp.arg);
		if (l == Bot) {
			edge[n][1].work = flowrk;
			edge[n][0].work = &edge[n][1];
			flowrk = &edge[n][0];
		}
		else if (qbe_fold_iscon(&fn->con[l], 0, 0)) {
			LQ_ASSERT(edge[n][0].dead);
			edge[n][1].work = flowrk;
			flowrk = &edge[n][1];
		}
		else {
			LQ_ASSERT(edge[n][1].dead);
			edge[n][0].work = flowrk;
			flowrk = &edge[n][0];
		}
		break;
	case Jjmp:
		edge[n][0].work = flowrk;
		flowrk = &edge[n][0];
		break;
	case Jhlt:
		break;
	default:
		if (isret(b->jmp.type))
			break;
		die("unreachable");
	}
}

static void
qbe_fold_initedge(Edge *e, Blk *s)
{
	if (s)
		e->dest = s->id;
	else
		e->dest = -1;
	e->dead = 1;
	e->work = 0;
}

static int
qbe_fold_renref(Ref *r)
{
	int l;

	if (rtype(*r) == RTmp)
		if ((l=val[r->val]) != Bot) {
			*r = CON(l);
			return 1;
		}
	return 0;
}

/* require rpo, use, pred */
void
fold(Fn *fn)
{
	Edge *e, start;
	Use *u;
	Blk *b, **pb;
	Phi *p, **pp;
	Ins *i;
	int t, d;
	uint n, a;

	val = emalloc(fn->ntmp * sizeof val[0]);
	edge = emalloc(fn->nblk * sizeof edge[0]);
	usewrk = vnew(0, sizeof usewrk[0], PHeap);

	for (t=0; t<fn->ntmp; t++)
		val[t] = Top;
	for (n=0; n<fn->nblk; n++) {
		b = fn->rpo[n];
		b->visit = 0;
		qbe_fold_initedge(&edge[n][0], b->s1);
		qbe_fold_initedge(&edge[n][1], b->s2);
	}
	qbe_fold_initedge(&start, fn->start);
	flowrk = &start;
	nuse = 0;

	/* 1. find out constants and dead cfg edges */
	for (;;) {
		e = flowrk;
		if (e) {
			flowrk = e->work;
			e->work = 0;
			if (e->dest == -1 || !e->dead)
				continue;
			e->dead = 0;
			n = e->dest;
			b = fn->rpo[n];
			for (p=b->phi; p; p=p->link)
				qbe_fold_visitphi(p, n, fn);
			if (b->visit == 0) {
				for (i=b->ins; i<&b->ins[b->nins]; i++)
					qbe_fold_visitins(i, fn);
				qbe_fold_visitjmp(b, n, fn);
			}
			b->visit++;
			LQ_ASSERT(b->jmp.type != Jjmp
				|| !edge[n][0].dead
				|| flowrk == &edge[n][0]);
		}
		else if (nuse) {
			u = usewrk[--nuse];
			n = u->bid;
			b = fn->rpo[n];
			if (b->visit == 0)
				continue;
			switch (u->type) {
			case UPhi:
				qbe_fold_visitphi(u->u.phi, u->bid, fn);
				break;
			case UIns:
				qbe_fold_visitins(u->u.ins, fn);
				break;
			case UJmp:
				qbe_fold_visitjmp(b, n, fn);
				break;
			default:
				die("unreachable");
			}
		}
		else
			break;
	}

	if (debug['F']) {
		fprintf(stderr, "\n> SCCP findings:");
		for (t=Tmp0; t<fn->ntmp; t++) {
			if (val[t] == Bot)
				continue;
			fprintf(stderr, "\n%10s: ", fn->tmp[t].name);
			if (val[t] == Top)
				fprintf(stderr, "Top");
			else
				printref(CON(val[t]), fn, stderr);
		}
		fprintf(stderr, "\n dead code: ");
	}

	/* 2. trim dead code, replace constants */
	d = 0;
	for (pb=&fn->start; (b=*pb);) {
		if (b->visit == 0) {
			d = 1;
			if (debug['F'])
				fprintf(stderr, "%s ", b->name);
			edgedel(b, &b->s1);
			edgedel(b, &b->s2);
			*pb = b->link;
			continue;
		}
		for (pp=&b->phi; (p=*pp);)
			if (val[p->to.val] != Bot)
				*pp = p->link;
			else {
				for (a=0; a<p->narg; a++)
					if (!qbe_fold_deadedge(p->blk[a]->id, b->id))
						qbe_fold_renref(&p->arg[a]);
				pp = &p->link;
			}
		for (i=b->ins; i<&b->ins[b->nins]; i++)
			if (qbe_fold_renref(&i->to))
				*i = (Ins){.op = Onop};
			else {
				for (n=0; n<2; n++)
					qbe_fold_renref(&i->arg[n]);
				if (isstore(i->op))
				if (req(i->arg[0], UNDEF))
					*i = (Ins){.op = Onop};
			}
		qbe_fold_renref(&b->jmp.arg);
		if (b->jmp.type == Jjnz && rtype(b->jmp.arg) == RCon) {
				if (qbe_fold_iscon(&fn->con[b->jmp.arg.val], 0, 0)) {
					edgedel(b, &b->s1);
					b->s1 = b->s2;
					b->s2 = 0;
				} else
					edgedel(b, &b->s2);
				b->jmp.type = Jjmp;
				b->jmp.arg = R;
		}
		pb = &b->link;
	}

	if (debug['F']) {
		if (!d)
			fprintf(stderr, "(none)");
		fprintf(stderr, "\n\n> After constant folding:\n");
		printfn(fn, stderr);
	}

	free(val);
	free(edge);
	vfree(usewrk);
}

/* boring folding code */

static int
qbe_fold_foldint(Con *res, int op, int w, Con *cl, Con *cr)
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

static int
qbe_fold_opfold(int op, int cls, Con *cl, Con *cr, Fn *fn)
{
	Ref r;
	Con c;

	if (cls == Kw || cls == Kl) {
		if (qbe_fold_foldint(&c, op, cls == Kl, cl, cr))
			return Bot;
	} else
		qbe_fold_foldflt(&c, op, cls == Kd, cl, cr);
	if (!KWIDE(cls))
		c.bits.i &= 0xffffffff;
	r = newcon(&c, fn);
	LQ_ASSERT(!(cls == Ks || cls == Kd) || c.flt);
	return r.val;
}
/*** END FILE: fold.c ***/
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
		b->out->t[0] |= T.rglob;
		bscopy(b->in, b->out);
		for (t=0; bsiter(b->in, &t); t++)
			nlv[KBASE(f->tmp[t].cls)]++;
		if (rtype(b->jmp.arg) == RCall) {
			LQ_ASSERT((int)bscount(b->in) == T.nrglob &&
				b->in->t[0] == T.rglob);
			b->in->t[0] |= T.retregs(b->jmp.arg, nlv);
		} else
			qbe_live_bset(b->jmp.arg, b, nlv, f->tmp);
		for (k=0; k<2; k++)
			b->nlive[k] = nlv[k];
		for (i=&b->ins[b->nins]; i!=b->ins;) {
			if ((--i)->op == Ocall && rtype(i->arg[1]) == RCall) {
				b->in->t[0] &= ~T.retregs(i->arg[1], m);
				for (k=0; k<2; k++) {
					nlv[k] -= m[k];
					/* caller-save registers are used
					 * by the callee, in that sense,
					 * right in the middle of the call,
					 * they are live: */
					nlv[k] += T.nrsave[k];
					if (nlv[k] > b->nlive[k])
						b->nlive[k] = nlv[k];
				}
				b->in->t[0] |= T.argregs(i->arg[1], m);
				for (k=0; k<2; k++) {
					nlv[k] -= T.nrsave[k];
					nlv[k] += m[k];
				}
			}
			if (!req(i->to, R)) {
				LQ_ASSERT(rtype(i->to) == RTmp);
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

	if (debug['L']) {
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
/*** END FILE: live.c ***/
/*** START FILE: load.c ***/
/* skipping all.h */

#define MASK(w) (BIT(8*(w)-1)*2-1) /* must work when w==8 */

typedef struct Loc Loc;
typedef struct Slice Slice;
typedef struct Insert Insert;

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

static Fn *curf;
static uint inum;    /* current insertion number */
static Insert *ilog; /* global insertion log */
static uint nlog;    /* number of entries in the log */

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

	vgrow(&ilog, ++nlog);
	ist = &ilog[nlog-1];
	ist->isphi = 0;
	ist->num = inum++;
	ist->bid = l->blk->id;
	ist->off = l->off;
	ist->new.ins = (Ins){op, cls, R, {a0, a1}};
	return ist->new.ins.to = newtmp("ld", cls, curf);
}

static void
qbe_load_cast(Ref *r, int cls, Loc *l)
{
	int cls0;

	if (rtype(*r) == RCon)
		return;
	LQ_ASSERT(rtype(*r) == RTmp);
	cls0 = curf->tmp[r->val].cls;
	if (cls0 == cls || (cls == Kw && cls0 == Kl))
		return;
	if (KWIDE(cls0) < KWIDE(cls)) {
		if (cls0 == Ks)
			*r = qbe_load_iins(Kw, Ocast, *r, R, l);
		*r = qbe_load_iins(Kl, Oextuw, *r, R, l);
		if (cls == Kd)
			*r = qbe_load_iins(Kd, Ocast, *r, R, l);
	} else {
		if (cls0 == Kd && cls != Kl)
			*r = qbe_load_iins(Kl, Ocast, *r, R, l);
		if (cls0 != Kd || cls != Kw)
			*r = qbe_load_iins(cls, Ocast, *r, R, l);
	}
}

static inline void
qbe_load_mask(int cls, Ref *r, bits msk, Loc *l)
{
	qbe_load_cast(r, cls, l);
	*r = qbe_load_iins(cls, Oand, *r, getcon(msk, curf), l);
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
		a = &curf->tmp[r.val].alias;
		switch (a->type) {
		default:
			die("unreachable");
		case ALoc:
		case AEsc:
		case AUnk:
			r = TMP(a->base);
			if (!a->offset)
				break;
			r1 = getcon(a->offset, curf);
			r = qbe_load_iins(Kl, Oadd, r, r1, l);
			break;
		case ACon:
		case ASym:
			memset(&c, 0, sizeof c);
			c.type = CAddr;
			c.sym = a->u.sym;
			c.bits.i = a->offset;
			r = newcon(&c, curf);
			break;
		}
	}
	r = qbe_load_iins(cls, ld, r, R, l);
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
	a = &curf->tmp[sl.ref.val].alias;
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
	LQ_ASSERT(dom(b, il->blk));
	oldl = nlog;
	oldt = curf->ntmp;
	if (0) {
	Load:
		curf->ntmp = oldt;
		nlog = oldl;
		if (il->type != LLoad)
			return R;
		return qbe_load_load(sl, msk, il);
	}

	if (!i)
		i = &b->ins[b->nins];
	cls = sl.sz > 4 ? Kl : Kw;
	msks = MASK(sl.sz);

	while (i > b->ins) {
		--i;
		if (qbe_load_killsl(i->to, sl)
		|| (i->op == Ocall && escapes(sl.ref, curf)))
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
			LQ_ASSERT(rtype(i->arg[0]) == RInt);
			sz = abs(rsval(i->arg[0]));
			LQ_ASSERT(i > b->ins);
			--i;
			LQ_ASSERT(i->op == Oblit0);
			r1 = i->arg[1];
		} else
			continue;
		switch (alias(sl.ref, sl.off, sl.sz, r1, sz, &off, curf)) {
		case MustAlias:
			if (i->op == Oblit0) {
				sl1 = sl;
				sl1.ref = i->arg[0];
				if (off >= 0) {
					LQ_ASSERT(off < sz);
					sl1.off = off;
					sz -= off;
					off = 0;
				} else {
					sl1.off = 0;
					sl1.sz += off;
				}
				if (sz > sl1.sz)
					sz = sl1.sz;
				LQ_ASSERT(sz <= 8);
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
				if (req(r, R))
					goto Load;
			}
			if (off) {
				cls1 = cls;
				if (op == Oshr && off + sl.sz > 4)
					cls1 = Kl;
				qbe_load_cast(&r, cls1, il);
				r1 = getcon(8*off, curf);
				r = qbe_load_iins(cls1, op, r, r1, il);
			}
			if ((msk1 & msk) != msk1 || off + sz < sl.sz)
				qbe_load_mask(cls, &r, msk1 & msk, il);
			if ((msk & ~msk1) != 0) {
				r1 = qbe_load_def(sl, msk & ~msk1, b, i, il);
				if (req(r1, R))
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

	for (ist=ilog; ist<&ilog[nlog]; ++ist)
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
		LQ_ASSERT(bp->loop >= il->blk->loop);
		l = *il;
		if (bp->s2)
			l.type = LNoLoad;
		r1 = qbe_load_def(sl, msk, bp, 0, &l);
		if (req(r1, R))
			goto Load;
		return r1;
	}

	r = newtmp("ld", sl.cls, curf);
	p = alloc(sizeof *p);
	vgrow(&ilog, ++nlog);
	ist = &ilog[nlog-1];
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
		if (req(r1, R))
			goto Load;
		p->arg[np] = r1;
		p->blk[np] = bp;
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

	curf = fn;
	ilog = vnew(0, sizeof ilog[0], PHeap);
	nlog = 0;
	inum = 0;
	for (b=fn->start; b; b=b->link)
		for (i=b->ins; i<&b->ins[b->nins]; ++i) {
			if (!isload(i->op))
				continue;
			sz = loadsz(i);
			sl = (Slice){i->arg[0], 0, sz, i->cls};
			l = (Loc){LRoot, i-b->ins, b};
			i->arg[1] = qbe_load_def(sl, MASK(sz), b, i, &l);
		}
	qsort(ilog, nlog, sizeof ilog[0], qbe_load_icmp);
	vgrow(&ilog, nlog+1);
	ilog[nlog].bid = fn->nblk; /* add a sentinel */
	ib = vnew(0, sizeof(Ins), PHeap);
	for (ist=ilog, n=0; n<fn->nblk; ++n) {
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
				&& !req(i->arg[1], R)) {
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
					i->arg[1] = R;
				}
			}
			vgrow(&ib, ++nt);
			ib[nt-1] = *i;
		}
		b->nins = nt;
		idup(&b->ins, ib, nt);
	}
	vfree(ib);
	vfree(ilog);
	if (debug['M']) {
		fprintf(stderr, "\n> After load elimination:\n");
		printfn(fn, stderr);
	}
}
/*** END FILE: load.c ***/
/*** START FILE: main.c ***/
/* skipping all.h */
/* skipping config.h */
#include <ctype.h>
/* skipping getopt.h */

static Target T;

static char debug['Z'+1] = {
	['P'] = 0, /* parsing */
	['M'] = 0, /* memory optimization */
	['N'] = 0, /* ssa construction */
	['C'] = 0, /* copy elimination */
	['F'] = 0, /* constant folding */
	['A'] = 0, /* abi lowering */
	['I'] = 0, /* instruction selection */
	['L'] = 0, /* liveness */
	['S'] = 0, /* spilling */
	['R'] = 0, /* reg. allocation */
	['T'] = 0, /* types */
};

static Target T_amd64_sysv;
static Target T_amd64_apple;
static Target T_amd64_win;
static Target T_arm64;
static Target T_arm64_apple;
static Target T_rv64;

static FILE *outf;
static int dbg;

static void
qbe_main_data(Dat *d)
{
	if (dbg)
		return;
	emitdat(d, outf);
	if (d->type == DEnd) {
		fputs("/* end data */\n\n", outf);
		freeall();
	}
}

static void
qbe_main_func(Fn *fn)
{
	uint n;

	if (dbg)
		fprintf(stderr, "**** Function %s ****", fn->name);
	if (debug['P']) {
		fprintf(stderr, "\n> After parsing:\n");
		printfn(fn, stderr);
	}
	T.abi0(fn);
	fillrpo(fn);
	fillpreds(fn);
	filluse(fn);
	promote(fn);
	filluse(fn);
	ssa(fn);
	filluse(fn);
	ssacheck(fn);
	fillalias(fn);
	loadopt(fn);
	filluse(fn);
	fillalias(fn);
	coalesce(fn);
	filluse(fn);
	ssacheck(fn);
	copy(fn);
	filluse(fn);
	fold(fn);
	T.abi1(fn);
	simpl(fn);
	fillpreds(fn);
	filluse(fn);
	T.isel(fn);
	fillrpo(fn);
	filllive(fn);
	fillloop(fn);
	fillcost(fn);
	spill(fn);
	rega(fn);
	fillrpo(fn);
	simpljmp(fn);
	fillpreds(fn);
	fillrpo(fn);
	LQ_ASSERT(fn->rpo[0] == fn->start);
	for (n=0;; n++)
		if (n == fn->nblk-1) {
			fn->rpo[n]->link = 0;
			break;
		} else
			fn->rpo[n]->link = fn->rpo[n+1];
	if (!dbg) {
		T.emitfn(fn, outf);
		fprintf(outf, "/* end function %s */\n\n", fn->name);
	} else
		fprintf(stderr, "\n");
	freeall();
}

static void
qbe_main_dbgfile(char *fn)
{
	emitdbgfile(fn, outf);
}
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
		LQ_ASSERT(rtype(i->to) == RTmp);
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
				l->arg[1] = R;
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
	if (debug['M']) {
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
				LQ_ASSERT((i+1)->op == Oblit1);
				LQ_ASSERT(rtype((i+1)->arg[0]) == RInt);
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
					LQ_ASSERT(b->loop >= n);
					qbe_mem_radd(&s->r, br[b->loop].b - 1);
				}
			}
		br[n].a = ip;
	}
	free(br);

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
	if (debug['M']) {
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
		LQ_ASSERT(t->ndef == 1 && t->def);
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
				LQ_ASSERT(isret(b->jmp.type));
				b->jmp.type = Jret0;
				b->jmp.arg = R;
				continue;
			}
			LQ_ASSERT(u->type == UIns);
			i = u->u.ins;
			if (!req(i->to, R)) {
				LQ_ASSERT(rtype(i->to) == RTmp);
				vgrow(&stk, ++n);
				stk[n-1] = i->to.val;
			} else if (isarg(i->op)) {
				LQ_ASSERT(i->op == Oargc);
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
		LQ_ASSERT(t->ndef == 1 && t->def);
		if (s->s == s)
			continue;
		*t->def = (Ins){.op = Onop};
		ts = &fn->tmp[s->s->t];
		LQ_ASSERT(t->bid == ts->bid);
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
			LQ_ASSERT(u->type == UIns);
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
				LQ_ASSERT(sz >= 0);
				(i+1)->arg[0] = INT(-sz);
			} else if (off0 == off1) {
				*i = (Ins){.op = Onop};
				*(i+1) = (Ins){.op = Onop};
			}
		}
	}
	vfree(bl);

	if (debug['M']) {
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
/*** END FILE: mem.c ***/
/*** START FILE: parse.c ***/
/* skipping all.h */
#include <ctype.h>
#include <stdarg.h>

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
#undef P
#define P(cf, hi, id) .canfold = cf, .hasid = hi, .idval = id
#define O(op, t, p) [O##op]={.name = #op, .argcls = t, p},
/* ------------------------------------------------------------including ops.h */
#ifndef X /* amd64 */
	#define X(NMemArgs, SetsZeroFlag, LeavesFlags)
#endif

#ifndef V /* riscv64 */
	#define V(Imm)
#endif

#ifndef P
	#define P(CanFold, HasId, IdVal)
#endif


#define T(a,b,c,d,e,f,g,h) {                          \
	{[Kw]=K##a, [Kl]=K##b, [Ks]=K##c, [Kd]=K##d}, \
	{[Kw]=K##e, [Kl]=K##f, [Ks]=K##g, [Kd]=K##h}  \
}


/*********************/
/* PUBLIC OPERATIONS */
/*********************/

/* Arithmetic and Bits */
O(add,     T(w,l,s,d, w,l,s,d), P(1,1,0)) X(2,1,0) V(1)
O(sub,     T(w,l,s,d, w,l,s,d), P(1,1,0)) X(2,1,0) V(0)
O(neg,     T(w,l,s,d, x,x,x,x), P(1,0,0)) X(1,1,0) V(0)
O(div,     T(w,l,s,d, w,l,s,d), P(1,1,1)) X(0,0,0) V(0)
O(rem,     T(w,l,e,e, w,l,e,e), P(1,0,0)) X(0,0,0) V(0)
O(udiv,    T(w,l,e,e, w,l,e,e), P(1,1,1)) X(0,0,0) V(0)
O(urem,    T(w,l,e,e, w,l,e,e), P(1,0,0)) X(0,0,0) V(0)
O(mul,     T(w,l,s,d, w,l,s,d), P(1,1,1)) X(2,0,0) V(0)
O(and,     T(w,l,e,e, w,l,e,e), P(1,0,0)) X(2,1,0) V(1)
O(or,      T(w,l,e,e, w,l,e,e), P(1,1,0)) X(2,1,0) V(1)
O(xor,     T(w,l,e,e, w,l,e,e), P(1,1,0)) X(2,1,0) V(1)
O(sar,     T(w,l,e,e, w,w,e,e), P(1,1,0)) X(1,1,0) V(1)
O(shr,     T(w,l,e,e, w,w,e,e), P(1,1,0)) X(1,1,0) V(1)
O(shl,     T(w,l,e,e, w,w,e,e), P(1,1,0)) X(1,1,0) V(1)

/* Comparisons */
O(ceqw,    T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cnew,    T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csgew,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csgtw,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cslew,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csltw,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(1)
O(cugew,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cugtw,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(culew,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cultw,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(1)

O(ceql,    T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cnel,    T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csgel,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csgtl,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cslel,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csltl,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(1)
O(cugel,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cugtl,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(culel,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cultl,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(1)

O(ceqs,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cges,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cgts,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cles,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(clts,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cnes,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cos,     T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cuos,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)

O(ceqd,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cged,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cgtd,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cled,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cltd,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cned,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cod,     T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cuod,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)

/* Memory */
O(storeb,  T(w,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(storeh,  T(w,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(storew,  T(w,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(storel,  T(l,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(stores,  T(s,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(stored,  T(d,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)

O(loadsb,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loadub,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loadsh,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loaduh,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loadsw,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loaduw,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(load,    T(m,m,m,m, x,x,x,x), P(0,0,0)) X(0,0,1) V(0)

/* Extensions and Truncations */
O(extsb,   T(w,w,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extub,   T(w,w,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extsh,   T(w,w,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extuh,   T(w,w,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extsw,   T(e,w,e,e, e,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extuw,   T(e,w,e,e, e,x,e,e), P(1,0,0)) X(0,0,1) V(0)

O(exts,    T(e,e,e,s, e,e,e,x), P(1,0,0)) X(0,0,1) V(0)
O(truncd,  T(e,e,d,e, e,e,x,e), P(1,0,0)) X(0,0,1) V(0)
O(stosi,   T(s,s,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(stoui,   T(s,s,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(dtosi,   T(d,d,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(dtoui,   T(d,d,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(swtof,   T(e,e,w,w, e,e,x,x), P(1,0,0)) X(0,0,1) V(0)
O(uwtof,   T(e,e,w,w, e,e,x,x), P(1,0,0)) X(0,0,1) V(0)
O(sltof,   T(e,e,l,l, e,e,x,x), P(1,0,0)) X(0,0,1) V(0)
O(ultof,   T(e,e,l,l, e,e,x,x), P(1,0,0)) X(0,0,1) V(0)
O(cast,    T(s,d,w,l, x,x,x,x), P(1,0,0)) X(0,0,1) V(0)

/* Stack Allocation */
O(alloc4,  T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(alloc8,  T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(alloc16, T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)

/* Variadic Function Helpers */
O(vaarg,   T(m,m,m,m, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(vastart, T(m,e,e,e, x,e,e,e), P(0,0,0)) X(0,0,0) V(0)

O(copy,    T(w,l,s,d, x,x,x,x), P(0,0,0)) X(0,0,1) V(0)

/* Debug */
O(dbgloc,  T(w,e,e,e, w,e,e,e), P(0,0,0)) X(0,0,1) V(0)

/****************************************/
/* INTERNAL OPERATIONS (keep nop first) */
/****************************************/

/* Miscellaneous and Architecture-Specific Operations */
O(nop,     T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,1) V(0)
O(addr,    T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(blit0,   T(m,e,e,e, m,e,e,e), P(0,0,0)) X(0,1,0) V(0)
O(blit1,   T(w,e,e,e, x,e,e,e), P(0,0,0)) X(0,1,0) V(0)
O(swap,    T(w,l,s,d, w,l,s,d), P(0,0,0)) X(1,0,0) V(0)
O(sign,    T(w,l,e,e, x,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(salloc,  T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(xidiv,   T(w,l,e,e, x,x,e,e), P(0,0,0)) X(1,0,0) V(0)
O(xdiv,    T(w,l,e,e, x,x,e,e), P(0,0,0)) X(1,0,0) V(0)
O(xcmp,    T(w,l,s,d, w,l,s,d), P(0,0,0)) X(1,1,0) V(0)
O(xtest,   T(w,l,e,e, w,l,e,e), P(0,0,0)) X(1,1,0) V(0)
O(acmp,    T(w,l,e,e, w,l,e,e), P(0,0,0)) X(0,0,0) V(0)
O(acmn,    T(w,l,e,e, w,l,e,e), P(0,0,0)) X(0,0,0) V(0)
O(afcmp,   T(e,e,s,d, e,e,s,d), P(0,0,0)) X(0,0,0) V(0)
O(reqz,    T(w,l,e,e, x,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(rnez,    T(w,l,e,e, x,x,e,e), P(0,0,0)) X(0,0,0) V(0)

/* Arguments, Parameters, and Calls */
O(par,     T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(parsb,   T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(parub,   T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(parsh,   T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(paruh,   T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(parc,    T(e,x,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(pare,    T(e,x,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(arg,     T(w,l,s,d, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(argsb,   T(w,e,e,e, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(argub,   T(w,e,e,e, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(argsh,   T(w,e,e,e, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(arguh,   T(w,e,e,e, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(argc,    T(e,x,e,e, e,l,e,e), P(0,0,0)) X(0,0,0) V(0)
O(arge,    T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(argv,    T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(call,    T(m,m,m,m, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)

/* Flags Setting */
O(flagieq,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagine,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagisge, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagisgt, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagisle, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagislt, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagiuge, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagiugt, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagiule, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagiult, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfeq,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfge,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfgt,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfle,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagflt,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfne,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfo,   T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfuo,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)


#undef T
#undef X
#undef V
#undef O

/*
| column -t -o ' '
*/
/* ------------------------------------------------------------end of ops.h */
#undef P
};

typedef enum {
	PXXX,
	PLbl,
	PPhi,
	PIns,
	PEnd,
} PState;

enum Token {
	Txxx = 0,

	/* aliases */
	Tloadw = NPubOp,
	Tloadl,
	Tloads,
	Tloadd,
	Talloc1,
	Talloc2,

	Tblit,
	Tcall,
	Tenv,
	Tphi,
	Tjmp,
	Tjnz,
	Tret,
	Thlt,
	Texport,
	Tthread,
	Tcommon,
	Tfunc,
	Ttype,
	Tdata,
	Tsection,
	Talign,
	Tdbgfile,
	Tl,
	Tw,
	Tsh,
	Tuh,
	Th,
	Tsb,
	Tub,
	Tb,
	Td,
	Ts,
	Tz,

	Tint,
	Tflts,
	Tfltd,
	Ttmp,
	Tlbl,
	Tglo,
	Ttyp,
	Tstr,

	Tplus,
	Teq,
	Tcomma,
	Tlparen,
	Trparen,
	Tlbrace,
	Trbrace,
	Tnl,
	Tdots,
	Teof,

	Ntok
};


enum {
	NPred = 63,

	TMask = 16383, /* for temps hash */
	BMask = 8191, /* for blocks hash */

	K = 11183273, /* found using tools/lexh.c */
	M = 23,
};


static Fn *curf;
static Phi **plink;
static Blk *curb;
static Blk **blink;
static int rcls;















static void
qbe_parse_closeblk(void)
{
	curb->nins = curi - insb;
	idup(&curb->ins, insb, curb->nins);
	blink = &curb->link;
	curi = insb;
}


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
                typ[fld[j].len].name);
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
		fprintf(f, ":%s", typ[r.val].name);
		break;
	case RMem:
		i = 0;
		m = &fn->mem[r.val];
		fputc('[', f);
		if (m->offset.type != CUndef) {
			qbe_parse_printcon(&m->offset, f);
			i = 1;
		}
		if (!req(m->base, R)) {
			if (i)
				fprintf(f, " + ");
			printref(m->base, fn, f);
			i = 1;
		}
		if (!req(m->index, R)) {
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
			LQ_ASSERT(p->narg);
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
			if (!req(i->to, R)) {
				printref(i->to, fn, f);
				fprintf(f, " =%c ", ktoc[i->cls]);
			}
			LQ_ASSERT(optab[i->op].name);
			fprintf(f, "%s", optab[i->op].name);
			if (req(i->to, R))
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
			if (!req(i->arg[0], R)) {
				fprintf(f, " ");
				printref(i->arg[0], fn, f);
			}
			if (!req(i->arg[1], R)) {
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
			if (b->jmp.type != Jret0 || !req(b->jmp.arg, R)) {
				fprintf(f, " ");
				printref(b->jmp.arg, fn, f);
			}
			if (b->jmp.type == Jretc)
				fprintf(f, ", :%s", typ[fn->retty].name);
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
			LQ_ASSERT(b->s1 && b->s2);
			fprintf(f, "@%s, @%s\n", b->s1->name, b->s2->name);
			break;
		}
	}
	fprintf(f, "}\n");
}
/*** END FILE: parse.c ***/
/*** START FILE: rega.c ***/
/* skipping all.h */

#ifdef TEST_PMOV
	#undef assert
	#define LQ_ASSERT(x) assert_test(#x, x)
#endif

typedef struct RMap RMap;

struct RMap {
	int t[Tmp0];
	int r[Tmp0];
	int w[Tmp0];   /* wait list, for unmatched hints */
	BSet b[1];
	int n;
};

static bits regu;      /* registers used */
static Tmp *tmp;       /* function temporaries */
static Mem *mem;       /* function mem references */
static struct {
	Ref src, dst;
	int cls;
} pm[Tmp0];            /* parallel move constructed */
static int npm;        /* size of pm */
static int loop;       /* current loop level */

static uint stmov;     /* stats: added moves */
static uint stblk;     /* stats: added blocks */

static int *
qbe_rega_hint(int t)
{
	return &tmp[phicls(t, tmp)].hint.r;
}

static void
qbe_rega_sethint(int t, int r)
{
	Tmp *p;

	p = &tmp[phicls(t, tmp)];
	if (p->hint.r == -1 || p->hint.w > loop) {
		p->hint.r = r;
		p->hint.w = loop;
		tmp[t].visit = -1;
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
		s = tmp[t].slot;
		LQ_ASSERT(s != -1 && "should have spilled");
		return SLOT(s);
	} else
		return TMP(r);
}

static void
qbe_rega_radd(RMap *m, int t, int r)
{
	LQ_ASSERT((t >= Tmp0 || t == r) && "invalid temporary");
	LQ_ASSERT(((T.gpr0 <= r && r < T.gpr0 + T.ngpr)
		|| (T.fpr0 <= r && r < T.fpr0 + T.nfpr))
		&& "invalid register");
	LQ_ASSERT(!bshas(m->b, t) && "temporary has mapping");
	LQ_ASSERT(!bshas(m->b, r) && "register already allocated");
	LQ_ASSERT(m->n <= T.ngpr+T.nfpr && "too many mappings");
	bsset(m->b, t);
	bsset(m->b, r);
	m->t[m->n] = t;
	m->r[m->n] = r;
	m->n++;
	regu |= BIT(r);
}

static Ref
qbe_rega_ralloctry(RMap *m, int t, int try)
{
	bits regs;
	int h, r, r0, r1;

	if (t < Tmp0) {
		LQ_ASSERT(bshas(m->b, t));
		return TMP(t);
	}
	if (bshas(m->b, t)) {
		r = qbe_rega_rfind(m, t);
		LQ_ASSERT(r != -1);
		return TMP(r);
	}
	r = tmp[t].visit;
	if (r == -1 || bshas(m->b, r))
		r = *qbe_rega_hint(t);
	if (r == -1 || bshas(m->b, r)) {
		if (try)
			return R;
		regs = tmp[phicls(t, tmp)].hint.m;
		regs |= m->b->t[0];
		if (KBASE(tmp[t].cls) == 0) {
			r0 = T.gpr0;
			r1 = r0 + T.ngpr;
		} else {
			r0 = T.fpr0;
			r1 = r0 + T.nfpr;
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
	tmp[t].visit = r;
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

	LQ_ASSERT(t >= Tmp0 || !(BIT(t) & T.rglob));
	if (!bshas(m->b, t))
		return -1;
	for (i=0; m->t[i] != t; i++)
		LQ_ASSERT(i+1 < m->n);
	r = m->r[i];
	bsclr(m->b, t);
	bsclr(m->b, r);
	m->n--;
	memmove(&m->t[i], &m->t[i+1], (m->n-i) * sizeof m->t[0]);
	memmove(&m->r[i], &m->r[i+1], (m->n-i) * sizeof m->r[0]);
	LQ_ASSERT(t >= Tmp0 || t == r);
	return r;
}

static void
qbe_rega_mdump(RMap *m)
{
	int i;

	for (i=0; i<m->n; i++)
		if (m->t[i] >= Tmp0)
			fprintf(stderr, " (%s, R%d)",
				tmp[m->t[i]].name,
				m->r[i]);
	fprintf(stderr, "\n");
}

static void
qbe_rega_pmadd(Ref src, Ref dst, int k)
{
	if (npm == Tmp0)
		die("cannot have more moves than registers");
	pm[npm].src = src;
	pm[npm].dst = dst;
	pm[npm].cls = k;
	npm++;
}

enum PMStat { ToMove, Moving, Moved };

static int
qbe_rega_pmrec(enum PMStat *status, int i, int *k)
{
	int j, c;

	/* note, this routine might emit
	 * too many large instructions
	 */
	if (req(pm[i].src, pm[i].dst)) {
		status[i] = Moved;
		return -1;
	}
	LQ_ASSERT(KBASE(pm[i].cls) == KBASE(*k));
	LQ_ASSERT((Kw|Kl) == Kl && (Ks|Kd) == Kd);
	*k |= pm[i].cls;
	for (j=0; j<npm; j++)
		if (req(pm[j].dst, pm[i].src))
			break;
	switch (j == npm ? Moved : status[j]) {
	case Moving:
		c = j; /* start of cycle */
		emit(Oswap, *k, R, pm[i].src, pm[i].dst);
		break;
	case ToMove:
		status[i] = Moving;
		c = qbe_rega_pmrec(status, j, k);
		if (c == i) {
			c = -1; /* end of cycle */
			break;
		}
		if (c != -1) {
			emit(Oswap, *k, R, pm[i].src, pm[i].dst);
			break;
		}
		/* fall through */
	case Moved:
		c = -1;
		emit(Ocopy, pm[i].cls, pm[i].dst, pm[i].src, R);
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

	status = alloc(npm * sizeof status[0]);
	LQ_ASSERT(!npm || status[npm-1] == ToMove);
	for (i=0; i<npm; i++)
		if (status[i] == ToMove)
			qbe_rega_pmrec(status, i, (int[]){pm[i].cls});
}

static void
qbe_rega_move(int r, Ref to, RMap *m)
{
	int n, t, r1;

	r1 = req(to, R) ? -1 : qbe_rega_rfree(m, to.val);
	if (bshas(m->b, r)) {
		/* r is used and not by to */
		LQ_ASSERT(r1 != r);
		for (n=0; m->r[n] != r; n++)
			LQ_ASSERT(n+1 < m->n);
		t = m->t[n];
		qbe_rega_rfree(m, t);
		bsset(m->b, r);
		qbe_rega_ralloc(m, t);
		bsclr(m->b, r);
	}
	t = req(to, R) ? r : to.val;
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
	LQ_ASSERT(m0.n <= m->n);
	if (i != b->ins && (i-1)->op == Ocall) {
		def = T.retregs((i-1)->arg[1], 0) | T.rglob;
		for (r=0; T.rsave[r]>=0; r++)
			if (!(BIT(T.rsave[r]) & def))
				qbe_rega_move(T.rsave[r], R, m);
	}
	for (npm=0, n=0; n<m->n; n++) {
		t = m->t[n];
		s = tmp[t].slot;
		r1 = m->r[n];
		r = qbe_rega_rfind(&m0, t);
		if (r != -1)
			qbe_rega_pmadd(TMP(r1), TMP(r), tmp[t].cls);
		else if (s != -1)
			qbe_rega_pmadd(TMP(r1), SLOT(s), tmp[t].cls);
	}
	for (ip=i; ip<i1; ip++) {
		if (!req(ip->to, R))
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
	curi = &insb[NIns];
	for (i1=&b->ins[b->nins]; i1!=b->ins;) {
		emiti(*--i1);
		i = curi;
		rf = -1;
		switch (i->op) {
		case Ocall:
			rs = T.argregs(i->arg[1], 0) | T.rglob;
			for (r=0; T.rsave[r]>=0; r++)
				if (!(BIT(T.rsave[r]) & rs))
					qbe_rega_rfree(cur, T.rsave[r]);
			break;
		case Ocopy:
			if (qbe_rega_regcpy(i)) {
				curi++;
				i1 = qbe_rega_dopm(b, i1, cur);
				stmov += i+1 - curi;
				continue;
			}
			if (isreg(i->to))
			if (rtype(i->arg[0]) == RTmp)
				qbe_rega_sethint(i->arg[0].val, i->to.val);
			/* fall through */
		default:
			if (!req(i->to, R)) {
				LQ_ASSERT(rtype(i->to) == RTmp);
				r = i->to.val;
				if (r < Tmp0 && (BIT(r) & T.rglob))
					break;
				rf = qbe_rega_rfree(cur, r);
				if (rf == -1) {
					LQ_ASSERT(!isreg(i->to));
					curi++;
					continue;
				}
				i->to = TMP(rf);
			}
			break;
		}
		for (x=0, nr=0; x<2; x++)
			switch (rtype(i->arg[x])) {
			case RMem:
				m = &mem[i->arg[x].val];
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
			curi++;

		/* try to change the register of a hinted
		 * temporary if rf is available */
		if (rf != -1 && (t = cur->w[rf]) != 0)
		if (!bshas(cur->b, rf) && *qbe_rega_hint(t) == rf
		&& (rt = qbe_rega_rfree(cur, t)) != -1) {
			tmp[t].visit = -1;
			qbe_rega_ralloc(cur, t);
			LQ_ASSERT(bshas(cur->b, rf));
			emit(Ocopy, tmp[t].cls, TMP(rt), TMP(rf), R);
			stmov += 1;
			cur->w[rf] = 0;
			for (r=0; r<nr; r++)
				if (req(*ra[r], TMP(rt)))
					*ra[r] = TMP(rf);
			/* one could iterate this logic with
			 * the newly freed rt, but in this case
			 * the above loop must be changed */
		}
	}
	b->nins = &insb[NIns] - curi;
	idup(&b->ins, curi, b->nins);
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
	if ((tmp[t1].visit ^ tmp[t2].visit) < 0)  /* != signs */
		return tmp[t1].visit != -1 ? +1 : -1;
	if ((*qbe_rega_hint(t1) ^ *qbe_rega_hint(t2)) < 0)
		return *qbe_rega_hint(t1) != -1 ? +1 : -1;
	return tmp[t1].cost - tmp[t2].cost;
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
	stmov = 0;
	stblk = 0;
	regu = 0;
	tmp = fn->tmp;
	mem = fn->mem;
	blk = alloc(fn->nblk * sizeof blk[0]);
	end = alloc(fn->nblk * sizeof end[0]);
	beg = alloc(fn->nblk * sizeof beg[0]);
	for (n=0; n<fn->nblk; n++) {
		bsinit(end[n].b, fn->ntmp);
		bsinit(beg[n].b, fn->ntmp);
	}
	bsinit(cur.b, fn->ntmp);
	bsinit(old.b, fn->ntmp);

	loop = INT_MAX;
	for (t=0; t<fn->ntmp; t++) {
		tmp[t].hint.r = t < Tmp0 ? t : -1;
		tmp[t].hint.w = loop;
		tmp[t].visit = -1;
	}
	for (bp=blk, b=fn->start; b; b=b->link)
		*bp++ = b;
	qsort(blk, fn->nblk, sizeof blk[0], qbe_rega_carve);
	for (b=fn->start, i=b->ins; i<&b->ins[b->nins]; i++)
		if (i->op != Ocopy || !isreg(i->arg[0]))
			break;
		else {
			LQ_ASSERT(rtype(i->to) == RTmp);
			qbe_rega_sethint(i->to.val, i->arg[0].val);
		}

	/* 2. assign registers */
	for (bp=blk; bp<&blk[fn->nblk]; bp++) {
		b = *bp;
		n = b->id;
		loop = b->loop;
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

		npm = 0;
		for (j=0; j<m->n; j++) {
			t = m->t[j];
			r = m->r[j];
			x = rl[r];
			LQ_ASSERT(x != 0 || t < Tmp0 /* todo, ditto */);
			if (x > 0 && !bshas(m->b, x)) {
				qbe_rega_pmadd(TMP(x), TMP(r), tmp[t].cls);
				m->r[j] = x;
				bsset(m->b, x);
			}
		}
		curi = &insb[NIns];
		qbe_rega_pmgen();
		j = &insb[NIns] - curi;
		if (j == 0)
			continue;
		stmov += j;
		s->nins += j;
		i = alloc(s->nins * sizeof(Ins));
		icpy(icpy(i, curi, j), s->ins, s->nins-j);
		s->ins = i;
	}

	if (debug['R'])  {
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
			npm = 0;
			for (p=s->phi; p; p=p->link) {
				dst = p->to;
				LQ_ASSERT(rtype(dst)==RSlot || rtype(dst)==RTmp);
				if (rtype(dst) == RTmp) {
					r = qbe_rega_rfind(&beg[s->id], dst.val);
					if (r == -1)
						continue;
					dst = TMP(r);
				}
				for (u=0; p->blk[u]!=b; u++)
					LQ_ASSERT(u+1 < p->narg);
				src = p->arg[u];
				if (rtype(src) == RTmp)
					src = qbe_rega_rref(&end[b->id], src.val);
				qbe_rega_pmadd(src, dst, p->cls);
			}
			for (t=Tmp0; bsiter(s->in, &t); t++) {
				src = qbe_rega_rref(&end[b->id], t);
				dst = qbe_rega_rref(&beg[s->id], t);
				qbe_rega_pmadd(src, dst, tmp[t].cls);
			}
			curi = &insb[NIns];
			qbe_rega_pmgen();
			if (curi == &insb[NIns])
				continue;
			b1 = newblk();
			b1->loop = (b->loop+s->loop) / 2;
			b1->link = blist;
			blist = b1;
			fn->nblk++;
			strf(b1->name, "%s_%s", b->name, s->name);
			b1->nins = &insb[NIns] - curi;
			stmov += b1->nins;
			stblk += 1;
			idup(&b1->ins, curi, b1->nins);
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
	fn->reg = regu;

	if (debug['R']) {
		fprintf(stderr, "\n> Register allocation statistics:\n");
		fprintf(stderr, "\tnew moves:  %d\n", stmov);
		fprintf(stderr, "\tnew blocks: %d\n", stblk);
		fprintf(stderr, "\n> After register allocation:\n");
		printfn(fn, stderr);
	}
}
/*** END FILE: rega.c ***/
/*** START FILE: simpl.c ***/
/* skipping all.h */

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
			emit(p->st, 0, R, r, r1);
			emit(Oadd, Kl, r1, sd[1], ro);
			r1 = newtmp("blt", Kl, fn);
			emit(p->ld, p->cls, r, r1, R);
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
		LQ_ASSERT(i > b->ins);
		LQ_ASSERT((i-1)->op == Oblit0);
		if (!*new) {
			curi = &insb[NIns];
			ni = &b->ins[b->nins] - (i+1);
			curi -= ni;
			icpy(curi, i+1, ni);
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
		if (new) {
			b->nins = &insb[NIns] - curi;
			idup(&b->ins, curi, b->nins);
		}
	}
}
/*** END FILE: simpl.c ***/
/*** START FILE: spill.c ***/
/* skipping all.h */

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
	if (debug['S']) {
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
	if (debug['S']) {
		fprintf(stderr, "\n> Spill costs:\n");
		for (n=Tmp0; n<fn->ntmp; n++)
			fprintf(stderr, "\t%-10s %d\n",
				fn->tmp[n].name,
				fn->tmp[n].cost);
		fprintf(stderr, "\n");
	}
}

static BSet *fst; /* temps to prioritize in registers (for tcmp1) */
static Tmp *tmp;  /* current temporaries (for tcmpX) */
static int ntmp;  /* current # of temps (for limit) */
static int locs;  /* stack size used by locals */
static int slot4; /* next slot of 4 bytes */
static int slot8; /* ditto, 8 bytes */
static BSet mask[2][1]; /* class masks */

static int
qbe_spill_tcmp0(const void *pa, const void *pb)
{
	uint ca, cb;

	ca = tmp[*(int *)pa].cost;
	cb = tmp[*(int *)pb].cost;
	return (cb < ca) ? -1 : (cb > ca);
}

static int
qbe_spill_tcmp1(const void *pa, const void *pb)
{
	int c;

	c = bshas(fst, *(int *)pb) - bshas(fst, *(int *)pa);
	return c ? c : qbe_spill_tcmp0(pa, pb);
}

static Ref
qbe_spill_slot(int t)
{
	int s;

	LQ_ASSERT(t >= Tmp0 && "cannot spill register");
	s = tmp[t].slot;
	if (s == -1) {
		/* specific to NAlign == 3 */
		/* nice logic to pack stack slots
		 * on demand, there can be only
		 * one hole and slot4 points to it
		 *
		 * invariant: slot4 <= slot8
		 */
		if (KWIDE(tmp[t].cls)) {
			s = slot8;
			if (slot4 == slot8)
				slot4 += 2;
			slot8 += 2;
		} else {
			s = slot4;
			if (slot4 == slot8) {
				slot8 += 2;
				slot4 += 1;
			} else
				slot4 = slot8;
		}
		s += locs;
		tmp[t].slot = s;
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
	static int *tarr, maxt;
	int i, t, nt;

	nt = bscount(b);
	if (nt <= k)
		return;
	if (nt > maxt) {
		free(tarr);
		tarr = emalloc(nt * sizeof tarr[0]);
		maxt = nt;
	}
	for (i=0, t=0; bsiter(b, &t); t++) {
		bsclr(b, t);
		tarr[i++] = t;
	}
	if (nt > 1) {
		if (!f)
			qsort(tarr, nt, sizeof tarr[0], qbe_spill_tcmp0);
		else {
			fst = f;
			qsort(tarr, nt, sizeof tarr[0], qbe_spill_tcmp1);
		}
	}
	for (i=0; i<k && i<nt; i++)
		bsset(b, tarr[i]);
	for (; i<nt; i++)
		qbe_spill_slot(tarr[i]);
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

	bsinit(b2, ntmp); /* todo, free those */
	bscopy(b2, b1);
	bsinter(b1, mask[0]);
	bsinter(b2, mask[1]);
	qbe_spill_limit(b1, T.ngpr - k1, f);
	qbe_spill_limit(b2, T.nfpr - k2, f);
	bsunion(b1, b2);
}

static void
qbe_spill_sethint(BSet *u, bits r)
{
	int t;

	for (t=Tmp0; bsiter(u, &t); t++)
		tmp[phicls(t, tmp)].hint.m |= r;
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
			emit(Oload, tmp[t].cls, TMP(t), qbe_spill_slot(t), R);
}

static void
qbe_spill_store(Ref r, int s)
{
	if (s != -1)
		emit(Ostorew + tmp[r.val].cls, 0, R, r, SLOT(s));
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

	bsinit(u, ntmp); /* todo, free those */
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
		if (!req(i->to, R))
		if (bshas(v, t)) {
			bsclr(v, t);
			qbe_spill_store(i->to, tmp[t].slot);
		}
		bsset(v, i->arg[0].val);
	} while (i != b->ins && qbe_spill_regcpy(i-1));
	bscopy(u, v);
	if (i != b->ins && (i-1)->op == Ocall) {
		v->t[0] &= ~T.retregs((i-1)->arg[1], 0);
		qbe_spill_limit2(v, T.nrsave[0], T.nrsave[1], 0);
		for (n=0, r=0; T.rsave[n]>=0; n++)
			r |= BIT(T.rsave[n]);
		v->t[0] |= T.argregs((i-1)->arg[1], 0);
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
			if (tmp[t].slot == -1)
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

	tmp = fn->tmp;
	ntmp = fn->ntmp;
	bsinit(u, ntmp);
	bsinit(v, ntmp);
	bsinit(w, ntmp);
	bsinit(mask[0], ntmp);
	bsinit(mask[1], ntmp);
	locs = fn->slot;
	slot4 = 0;
	slot8 = 0;
	for (t=0; t<ntmp; t++) {
		k = 0;
		if (t >= T.fpr0 && t < T.fpr0 + T.nfpr)
			k = 1;
		if (t >= Tmp0)
			k = KBASE(tmp[t].cls);
		bsset(mask[k], t);
	}

	for (bp=&fn->rpo[fn->nblk]; bp!=fn->rpo;) {
		b = *--bp;
		/* invariant: all blocks with bigger rpo got
		 * their in,out updated. */

		/* 1. find temporaries in registers at
		 * the end of the block (put them in v) */
		curi = 0;
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
			hd->gen->t[0] |= T.rglob; /* don't spill registers */
			for (k=0; k<2; k++) {
				n = k == 0 ? T.ngpr : T.nfpr;
				bscopy(u, b->out);
				bsinter(u, mask[k]);
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
				v->t[0] |= T.retregs(b->jmp.arg, 0);
		}
		for (t=Tmp0; bsiter(b->out, &t); t++)
			if (!bshas(v, t))
				qbe_spill_slot(t);
		bscopy(b->out, v);

		/* 2. process the block instructions */
		if (rtype(b->jmp.arg) == RTmp) {
			t = b->jmp.arg.val;
			LQ_ASSERT(KBASE(tmp[t].cls) == 0);
			lvarg[0] = bshas(v, t);
			bsset(v, t);
			bscopy(u, v);
			qbe_spill_limit2(v, 0, 0, NULL);
			if (!bshas(v, t)) {
				if (!lvarg[0])
					bsclr(u, t);
				b->jmp.arg = qbe_spill_slot(t);
			}
			qbe_spill_reloads(u, v);
		}
		curi = &insb[NIns];
		for (i=&b->ins[b->nins]; i!=b->ins;) {
			i--;
			if (qbe_spill_regcpy(i)) {
				i = qbe_spill_dopm(b, i, v);
				continue;
			}
			bszero(w);
			if (!req(i->to, R)) {
				LQ_ASSERT(rtype(i->to) == RTmp);
				t = i->to.val;
				if (bshas(v, t))
					bsclr(v, t);
				else {
					/* make sure we have a reg
					 * for the result */
					LQ_ASSERT(t >= Tmp0 && "dead reg");
					bsset(v, t);
					bsset(w, t);
				}
			}
			j = T.memargs(i->op);
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
			if (!req(i->to, R)) {
				t = i->to.val;
				qbe_spill_store(i->to, tmp[t].slot);
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
			LQ_ASSERT(v->t[0] == (T.rglob | fn->reg));
		else
			LQ_ASSERT(v->t[0] == T.rglob);

		for (p=b->phi; p; p=p->link) {
			LQ_ASSERT(rtype(p->to) == RTmp);
			t = p->to.val;
			if (bshas(v, t)) {
				bsclr(v, t);
				qbe_spill_store(p->to, tmp[t].slot);
			} else if (bshas(b->in, t))
				/* only if the phi is live */
				p->to = qbe_spill_slot(p->to.val);
		}
		bscopy(b->in, v);
		b->nins = &insb[NIns] - curi;
		idup(&b->ins, curi, b->nins);
	}

	/* align the locals to a 16 byte boundary */
	/* specific to NAlign == 3 */
	slot8 += slot8 & 3;
	fn->slot += slot8;

	if (debug['S']) {
		fprintf(stderr, "\n> Block information:\n");
		for (b=fn->start; b; b=b->link) {
			fprintf(stderr, "\t%-10s (% 5d) ", b->name, b->loop);
			dumpts(b->out, fn->tmp, stderr);
		}
		fprintf(stderr, "\n> After spilling:\n");
		printfn(fn, stderr);
	}
}
/*** END FILE: spill.c ***/
/*** START FILE: ssa.c ***/
/* skipping all.h */
#include <stdarg.h>

static void
qbe_ssa_adduse(Tmp *tmp, int ty, Blk *b, ...)
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

	/* todo, is this the correct file? */
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
			LQ_ASSERT(rtype(p->to) == RTmp);
			tp = p->to.val;
			tmp[tp].bid = b->id;
			tmp[tp].ndef++;
			tmp[tp].cls = p->cls;
			tp = phicls(tp, fn->tmp);
			for (a=0; a<p->narg; a++)
				if (rtype(p->arg[a]) == RTmp) {
					t = p->arg[a].val;
					qbe_ssa_adduse(&tmp[t], UPhi, b, p);
					t = phicls(t, fn->tmp);
					if (t != tp)
						tmp[t].phi = tp;
				}
		}
		for (i=b->ins; i<&b->ins[b->nins]; i++) {
			if (!req(i->to, R)) {
				LQ_ASSERT(rtype(i->to) == RTmp);
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
					qbe_ssa_adduse(&tmp[t], UIns, b, i);
				}
		}
		if (rtype(b->jmp.arg) == RTmp)
			qbe_ssa_adduse(&tmp[b->jmp.arg.val], UJmp, b);
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
		k = -1;
		bp = be;
		for (b=fn->start; b; b=b->link) {
			b->visit = 0;
			r = R;
			for (i=b->ins; i<&b->ins[b->nins]; i++) {
				if (!req(r, R)) {
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
			if (!req(r, R) && req(b->jmp.arg, TMP(t)))
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
	free(blist);
}

typedef struct Name Name;
struct Name {
	Ref r;
	Blk *b;
	Name *up;
};

static Name *namel;

static Name *
qbe_ssa_nnew(Ref r, Blk *b, Name *up)
{
	Name *n;

	if (namel) {
		n = namel;
		namel = n->up;
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
	n->up = namel;
	namel = n;
}

static void
qbe_ssa_rendef(Ref *r, Blk *b, Name **stk, Fn *fn)
{
	Ref r1;
	int t;

	t = r->val;
	if (req(*r, R) || !fn->tmp[t].visit)
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
	d = debug['L'];
	debug['L'] = 0;
	filldom(fn);
	if (debug['N']) {
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
	debug['L'] = d;
	free(stk);
	if (debug['N']) {
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
/*** END FILE: ssa.c ***/
/*** START FILE: util.c ***/
/* skipping all.h */
#include <stdarg.h>

typedef struct Bitset Bitset;
typedef struct Vec Vec;
typedef struct Bucket Bucket;

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

struct Bucket {
	uint nstr;
	char **str;
};

enum {
	VMin = 2,
	VMag = 0xcabba9e,
	NPtr = 256,
	IBits = 12,
	IMask = (1<<IBits) - 1,
};

static Typ *typ;
static Ins insb[NIns], *curi;

static void *ptr[NPtr];
static void **pool = ptr;
static int nptr = 1;

static Bucket itbl[IMask+1]; /* string interning table */

uint32_t
hash(char *s)
{
	uint32_t h;

	for (h=0; *s; ++s)
		h = *s + 17*h;
	return h;
}

void
die_(char *file, char *s, ...)
{
	va_list ap;

	fprintf(stderr, "%s: dying: ", file);
	va_start(ap, s);
	vfprintf(stderr, s, ap);
	va_end(ap);
	fputc('\n', stderr);
	abort();
}

void *
emalloc(size_t n)
{
	void *p;

	p = calloc(1, n);
	if (!p)
		die("emalloc, out of memory");
	return p;
}

void *
alloc(size_t n)
{
	void **pp;

	if (n == 0)
		return 0;
	if (nptr >= NPtr) {
		pp = emalloc(NPtr * sizeof(void *));
		pp[0] = pool;
		pool = pp;
		nptr = 1;
	}
	return pool[nptr++] = emalloc(n);
}

void
freeall(void)
{
	void **pp;

	for (;;) {
		for (pp = &pool[1]; pp < &pool[nptr]; pp++)
			free(*pp);
		pp = pool[0];
		if (!pp)
			break;
		free(pool);
		pool = pp;
		nptr = NPtr;
	}
	nptr = 1;
}

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
	LQ_ASSERT(v->mag == VMag);
	if (v->pool == PHeap) {
		v->mag = 0;
		free(v);
	}
}

void
vgrow(void *vp, ulong len)
{
	Vec *v;
	void *v1;

	v = *(Vec **)vp - 1;
	LQ_ASSERT(v+1 && v->mag == VMag);
	if (v->cap >= len)
		return;
	v1 = vnew(len, v->esz, v->pool);
	memcpy(v1, v+1, v->cap * v->esz);
	vfree(v+1);
	*(Vec **)vp = v1;
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

	h = hash(s) & IMask;
	b = &itbl[h];
	n = b->nstr;

	for (i=0; i<n; i++)
		if (strcmp(s, b->str[i]) == 0)
			return h + (i<<IBits);

	if (n == 1<<(32-IBits))
		die("interning table overflow");
	if (n == 0)
		b->str = vnew(1, sizeof b->str[0], PHeap);
	else if ((n & (n-1)) == 0)
		vgrow(&b->str, n+n);

	b->str[n] = emalloc(strlen(s)+1);
	b->nstr = n + 1;
	strcpy(b->str[n], s);
	return h + (n<<IBits);
}

char *
str(uint32_t id)
{
	LQ_ASSERT(id>>IBits < itbl[id&IMask].nstr);
	return itbl[id&IMask].str[id>>IBits];
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

int
argcls(Ins *i, int n)
{
	return optab[i->op].argcls[n][i->cls];
}

void
emit(int op, int k, Ref to, Ref arg0, Ref arg1)
{
	if (curi == insb)
		die("emit, too many instructions");
	*--curi = (Ins){
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
idup(Ins **pd, Ins *s, ulong n)
{
	*pd = alloc(n * sizeof(Ins));
	if (n)
		memcpy(*pd, s, n * sizeof(Ins));
}

Ins *
icpy(Ins *d, Ins *s, ulong n)
{
	if (n)
		memcpy(d, s, n * sizeof(Ins));
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
	LQ_ASSERT(0 <= c && c < NCmp);
	return cmptab[c][0];
}

int
cmpop(int c)
{
	LQ_ASSERT(0 <= c && c < NCmp);
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

Ref
newtmp(char *prfx, int k,  Fn *fn)
{
	static int n;
	int t;

	t = fn->ntmp++;
	vgrow(&fn->tmp, fn->ntmp);
	memset(&fn->tmp[t], 0, sizeof(Tmp));
	if (prfx)
		strf(fn->tmp[t].name, "%s.%d", prfx, ++n);
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
		emit(Osalloc, Kl, rt, getcon(sz, fn), R);
	} else {
		/* r0 = (r + 15) & -16 */
		r0 = newtmp("isel", Kl, fn);
		r1 = newtmp("isel", Kl, fn);
		emit(Osalloc, Kl, rt, r0, R);
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
	LQ_ASSERT(elt < qbe_util_bsmax(bs));
	bs->t[elt/NBit] |= BIT(elt%NBit);
}

void
bsclr(BSet *bs, uint elt)
{
	LQ_ASSERT(elt < qbe_util_bsmax(bs));
	bs->t[elt/NBit] &= ~BIT(elt%NBit);
}

#define BSOP(f, op)                           \
	void                                  \
	f(BSet *a, BSet *b)                   \
	{                                     \
		uint i;                       \
		                              \
		LQ_ASSERT(a->nt == b->nt);       \
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

	LQ_ASSERT(a->nt == b->nt);
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

	LQ_ASSERT(rtype(ref) == RTmp);
	stk = stkbuf;
	pc = code;
	while ((bc = *pc))
		switch (bc) {
		case 1: /* pushsym */
		case 2: /* push */
			LQ_ASSERT(stk < &stkbuf[20]);
			LQ_ASSERT(rtype(ref) == RTmp);
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
			LQ_ASSERT(stk > &stkbuf[0]);
			ref = *--stk;
			pc++;
			break;
		case 5: /* switch */
			LQ_ASSERT(rtype(ref) == RTmp);
			n = tn[ref.val].n;
			s = pc + 1;
			for (i=*s++; i>0; i--, s++)
				if (n == *s++)
					break;
			pc += *s;
			break;
		default: /* jump */
			LQ_ASSERT(bc >= 10);
			pc = code + (bc - 10);
			break;
		}
}
/*** END FILE: util.c ***/
/*** START FILE: amd64/emit.c ***/
/* skipping all.h */


typedef struct QBE_AMD64_EMIT_E QBE_AMD64_EMIT_E;

struct QBE_AMD64_EMIT_E {
	FILE *f;
	Fn *fn;
	int fp;
	uint64_t fsz;
	int nclob;
};

#define CMP(X) \
	X(Ciule,      "be") \
	X(Ciult,      "b")  \
	X(Cisle,      "le") \
	X(Cislt,      "l")  \
	X(Cisgt,      "g")  \
	X(Cisge,      "ge") \
	X(Ciugt,      "a")  \
	X(Ciuge,      "ae") \
	X(Cieq,       "z")  \
	X(Cine,       "nz") \
	X(NCmpI+Cfle, "be") \
	X(NCmpI+Cflt, "b")  \
	X(NCmpI+Cfgt, "a")  \
	X(NCmpI+Cfge, "ae") \
	X(NCmpI+Cfeq, "z")  \
	X(NCmpI+Cfne, "nz") \
	X(NCmpI+Cfo,  "np") \
	X(NCmpI+Cfuo, "p")

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
	{ Oadd,    QBE_AMD64_EMIT_Ka, "+add%k %1, %=" },
	{ Osub,    QBE_AMD64_EMIT_Ka, "-sub%k %1, %=" },
	{ Oand,    QBE_AMD64_EMIT_Ki, "+and%k %1, %=" },
	{ Oor,     QBE_AMD64_EMIT_Ki, "+or%k %1, %=" },
	{ Oxor,    QBE_AMD64_EMIT_Ki, "+xor%k %1, %=" },
	{ Osar,    QBE_AMD64_EMIT_Ki, "-sar%k %B1, %=" },
	{ Oshr,    QBE_AMD64_EMIT_Ki, "-shr%k %B1, %=" },
	{ Oshl,    QBE_AMD64_EMIT_Ki, "-shl%k %B1, %=" },
	{ Omul,    QBE_AMD64_EMIT_Ki, "+imul%k %1, %=" },
	{ Omul,    Ks, "+mulss %1, %=" },
	{ Omul,    Kd, "+mulsd %1, %=" },
	{ Odiv,    QBE_AMD64_EMIT_Ka, "-div%k %1, %=" },
	{ Ostorel, QBE_AMD64_EMIT_Ka, "movq %L0, %M1" },
	{ Ostorew, QBE_AMD64_EMIT_Ka, "movl %W0, %M1" },
	{ Ostoreh, QBE_AMD64_EMIT_Ka, "movw %H0, %M1" },
	{ Ostoreb, QBE_AMD64_EMIT_Ka, "movb %B0, %M1" },
	{ Ostores, QBE_AMD64_EMIT_Ka, "movss %S0, %M1" },
	{ Ostored, QBE_AMD64_EMIT_Ka, "movsd %D0, %M1" },
	{ Oload,   QBE_AMD64_EMIT_Ka, "mov%k %M0, %=" },
	{ Oloadsw, Kl, "movslq %M0, %L=" },
	{ Oloadsw, Kw, "movl %M0, %W=" },
	{ Oloaduw, QBE_AMD64_EMIT_Ki, "movl %M0, %W=" },
	{ Oloadsh, QBE_AMD64_EMIT_Ki, "movsw%k %M0, %=" },
	{ Oloaduh, QBE_AMD64_EMIT_Ki, "movzw%k %M0, %=" },
	{ Oloadsb, QBE_AMD64_EMIT_Ki, "movsb%k %M0, %=" },
	{ Oloadub, QBE_AMD64_EMIT_Ki, "movzb%k %M0, %=" },
	{ Oextsw,  Kl, "movslq %W0, %L=" },
	{ Oextuw,  Kl, "movl %W0, %W=" },
	{ Oextsh,  QBE_AMD64_EMIT_Ki, "movsw%k %H0, %=" },
	{ Oextuh,  QBE_AMD64_EMIT_Ki, "movzw%k %H0, %=" },
	{ Oextsb,  QBE_AMD64_EMIT_Ki, "movsb%k %B0, %=" },
	{ Oextub,  QBE_AMD64_EMIT_Ki, "movzb%k %B0, %=" },

	{ Oexts,   Kd, "cvtss2sd %0, %=" },
	{ Otruncd, Ks, "cvtsd2ss %0, %=" },
	{ Ostosi,  QBE_AMD64_EMIT_Ki, "cvttss2si%k %0, %=" },
	{ Odtosi,  QBE_AMD64_EMIT_Ki, "cvttsd2si%k %0, %=" },
	{ Oswtof,  QBE_AMD64_EMIT_Ka, "cvtsi2%k %W0, %=" },
	{ Osltof,  QBE_AMD64_EMIT_Ka, "cvtsi2%k %L0, %=" },
	{ Ocast,   QBE_AMD64_EMIT_Ki, "movq %D0, %L=" },
	{ Ocast,   QBE_AMD64_EMIT_Ka, "movq %L0, %D=" },

	{ Oaddr,   QBE_AMD64_EMIT_Ki, "lea%k %M0, %=" },
	{ Oswap,   QBE_AMD64_EMIT_Ki, "xchg%k %0, %1" },
	{ Osign,   Kl, "cqto" },
	{ Osign,   Kw, "cltd" },
	{ Oxdiv,   QBE_AMD64_EMIT_Ki, "div%k %0" },
	{ Oxidiv,  QBE_AMD64_EMIT_Ki, "idiv%k %0" },
	{ Oxcmp,   Ks, "ucomiss %S0, %S1" },
	{ Oxcmp,   Kd, "ucomisd %D0, %D1" },
	{ Oxcmp,   QBE_AMD64_EMIT_Ki, "cmp%k %0, %1" },
	{ Oxtest,  QBE_AMD64_EMIT_Ki, "test%k %0, %1" },
#define X(c, s) \
	{ Oflag+c, QBE_AMD64_EMIT_Ki, "set" s " %B=\n\tmovzb%k %B=, %=" },
	CMP(X)
#undef X
	{ NOp, 0, 0 }
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
	LQ_ASSERT(s <= e->fn->slot);
	/* specific to NAlign == 3 */
	if (s < 0) {
		if (e->fp == QBE_AMD64_RSP)
			return 4*-s - 8 + e->fsz + e->nclob*8;
		else
			return 4*-s;
	}
	else if (e->fp == QBE_AMD64_RSP)
		return 4*s + e->nclob*8;
	else if (e->fn->vararg)
		return -176 + -4 * (e->fn->slot - s);
	else
		return -4 * (e->fn->slot - s);
}

static void
qbe_amd64_emit_emitcon(Con *con, QBE_AMD64_EMIT_E *e)
{
	char *p, *l;

	switch (con->type) {
	case CAddr:
		l = str(con->sym.id);
		p = l[0] == '"' ? "" : T.assym;
		if (con->sym.type == SThr) {
			if (T.apple)
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

static char *
qbe_amd64_emit_regtoa(int reg, int sz)
{
	static char buf[6];

	LQ_ASSERT(reg <= QBE_AMD64_XMM15);
	if (reg >= QBE_AMD64_XMM0) {
		sprintf(buf, "xmm%d", reg-QBE_AMD64_XMM0);
		return buf;
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
		LQ_ASSERT((!req(i->arg[1], i->to) || req(i->arg[0], i->to)) &&
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
			LQ_ASSERT(isreg(ref));
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
			if (!req(m->base, R))
				fprintf(e->f, "%%%s",
					qbe_amd64_emit_regtoa(m->base.val, SLong)
				);
			else if (m->offset.type == CAddr)
				fprintf(e->f, "%%rip");
			if (!req(m->index, R))
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
			if (off.sym.type != SThr || T.apple)
				fprintf(e->f, "(%%rip)");
			break;
		case RTmp:
			LQ_ASSERT(isreg(ref));
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

static void *negmask[4] = {
	[Ks] = (uint32_t[4]){ 0x80000000 },
	[Kd] = (uint64_t[2]){ 0x8000000000000000 },
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
				T.asloc,
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
		LQ_ASSERT(rtype(i.to) != RMem);
		if (req(i.to, R) || req(i.arg[0], R))
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
		if (!T.apple
		&& rtype(i.arg[0]) == RCon
		&& e->fn->con[i.arg[0].val].sym.type == SThr) {
			/* derive the symbol address from the TCB
			 * address at offset 0 of %fs */
			LQ_ASSERT(isreg(i.to));
			con = &e->fn->con[i.arg[0].val];
			sym = str(con->sym.id);
			qbe_amd64_emit_emitf("movq %%fs:0, %L=", &i, e);
			fprintf(e->f, "\tleaq %s%s@tpoff",
				sym[0] == '"' ? "" : T.assym, sym);
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
		LQ_ASSERT(e->fp == QBE_AMD64_RBP);
		qbe_amd64_emit_emitf("subq %L0, %%rsp", &i, e);
		if (!req(i.to, R))
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
	}
}

static void
qbe_amd64_emit_framesz(QBE_AMD64_EMIT_E *e)
{
	uint64_t i, o, f;

	/* specific to NAlign == 3 */
	o = 0;
	if (!e->fn->leaf) {
		for (i=0, o=0; i<QBE_AMD64_NCLR; i++)
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
amd64_emitfn(Fn *fn, FILE *f)
{
	static char *ctoa[] = {
	#define X(c, s) [c] = s,
		CMP(X)
	#undef X
	};
	static int id0;
	Blk *b, *s;
	Ins *i, itmp;
	int *r, c, o, n, lbl;
	QBE_AMD64_EMIT_E *e;

	e = &(QBE_AMD64_EMIT_E){.f = f, .fn = fn};
	emitfnlnk(fn->name, &fn->lnk, f);
	fputs("\tendbr64\n", f);
	if (!fn->leaf || fn->vararg || fn->dynalloc) {
		e->fp = QBE_AMD64_RBP;
		fputs("\tpushq %rbp\n\tmovq %rsp, %rbp\n", f);
	} else
		e->fp = QBE_AMD64_RSP;
	qbe_amd64_emit_framesz(e);
	if (e->fsz)
		fprintf(f, "\tsubq $%"PRIu64", %%rsp\n", e->fsz);
	if (fn->vararg) {
		o = -176;
		for (r=amd64_sysv_rsave; r<&amd64_sysv_rsave[6]; r++, o+=8)
			fprintf(f, "\tmovq %%%s, %d(%%rbp)\n", qbe_amd64_emit_rname[*r][0], o);
		for (n=0; n<8; ++n, o+=16)
			fprintf(f, "\tmovaps %%xmm%d, %d(%%rbp)\n", n, o);
	}
	for (r=amd64_sysv_rclob; r<&amd64_sysv_rclob[QBE_AMD64_NCLR]; r++)
		if (fn->reg & BIT(*r)) {
			itmp.arg[0] = TMP(*r);
			qbe_amd64_emit_emitf("pushq %L0", &itmp, e);
			e->nclob++;
		}

	for (lbl=0, b=fn->start; b; b=b->link) {
		if (lbl || b->npred > 1)
			fprintf(f, "%sbb%d:\n", T.asloc, id0+b->id);
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
			for (r=&amd64_sysv_rclob[QBE_AMD64_NCLR]; r>amd64_sysv_rclob;)
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
		Jmp:
			if (b->s1 != b->link)
				fprintf(f, "\tjmp %sbb%d\n",
					T.asloc, id0+b->s1->id);
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
					T.asloc, id0+b->s2->id);
				goto Jmp;
			}
			die("unhandled jump %d", b->jmp.type);
		}
	}
	id0 += fn->nblk;
	if (!T.apple && !T.windows)
		elf_emitfnfin(fn->name, f);
}
#undef CMP
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
		n = stashbits(&fn->con[r0.val].bits, KWIDE(k) ? 8 : 4);
		/* quote the name so that we do not
		 * add symbol prefixes on the apple
		 * target variant
		 */
		sprintf(buf, "\"%sfp%d\"", T.asloc, n);
		a.offset.sym.id = intern(buf);
		fn->mem[fn->nmem-1] = a;
	}
	else if (op != Ocopy && k == Kl && qbe_amd64_isel_noimm(r0, fn)) {
		/* load constants that do not fit in
		 * a 32bit signed integer into a
		 * long temporary
		 */
		r1 = newtmp("isel", Kl, fn);
		emit(Ocopy, Kl, r1, r0, R);
	}
	else if (s != -1) {
		/* load fast locals' addresses into
		 * temporaries right before the
		 * instruction
		 */
		r1 = newtmp("isel", Kl, fn);
		emit(Oaddr, Kl, r1, SLOT(s), R);
	}
	else if (T.apple && qbe_amd64_isel_hascon(r0, &c, fn)
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
		emit(Ocopy, Kl, r2, TMP(QBE_AMD64_RAX), R);
		r2 = newtmp("isel", Kl, fn);
		r3 = newtmp("isel", Kl, fn);
		emit(Ocall, 0, R, r3, CALL(17));
		emit(Ocopy, Kl, TMP(QBE_AMD64_RDI), r2, R);
		emit(Oload, Kl, r3, r2, R);
		cc = *c;
		cc.bits.i = 0;
		r3 = newcon(&cc, fn);
		emit(Oload, Kl, r2, r3, R);
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
		emit(Oaddr, Kl, r1, r0, R);
	}
	else if (rtype(r0) == RMem) {
		/* eliminate memory operands of
		 * the form $foo(%rip, ...)
		 */
		m = &fn->mem[r0.val];
		if (req(m->base, R))
		if (m->offset.type == CAddr) {
			r0 = newtmp("isel", Kl, fn);
			emit(Oaddr, Kl, r0, newcon(&m->offset, fn), R);
			m->offset.type = CUndef;
			m->base = r0;
		}
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
		if (!req(a.base, R))
		if (a.offset.type == CAddr) {
			/* apple as does not support
			 * $foo(%r0, %r1, M); try to
			 * rewrite it or bail out if
			 * impossible
			 */
			if (!req(a.index, R) || rtype(a.base) != RTmp)
				return;
			else {
				a.index = a.base;
				a.scale = 1;
				a.base = R;
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
	emit(Oxcmp, k, R, arg[1], arg[0]);
	icmp = curi;
	if (rtype(arg[0]) == RCon) {
		LQ_ASSERT(k != Kw);
		icmp->arg[1] = newtmp("isel", k, fn);
		emit(Ocopy, k, icmp->arg[1], arg[0], R);
		qbe_amd64_isel_fixarg(&curi->arg[0], k, curi, fn);
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
	i0 = curi;
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
		emit(Ocopy, k, i.to, r0, R);
		emit(Ocopy, k, R, r1, R);
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
			emit(Oxidiv, k, R, r0, R);
			emit(Osign, k, TMP(QBE_AMD64_RDX), TMP(QBE_AMD64_RAX), R);
		} else {
			emit(Oxdiv, k, R, r0, R);
			emit(Ocopy, k, TMP(QBE_AMD64_RDX), CON_Z, R);
		}
		emit(Ocopy, k, TMP(QBE_AMD64_RAX), i.arg[0], R);
		qbe_amd64_isel_fixarg(&curi->arg[0], k, curi, fn);
		if (rtype(i.arg[1]) == RCon)
			emit(Ocopy, k, r0, i.arg[1], R);
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
		emit(Ocopy, Kw, R, TMP(QBE_AMD64_RCX), R);
		emiti(i);
		i1 = curi;
		emit(Ocopy, Kw, TMP(QBE_AMD64_RCX), r0, R);
		qbe_amd64_isel_fixarg(&i1->arg[0], argcls(&i, 0), i1, fn);
		break;
	case Ouwtof:
		r0 = newtmp("utof", Kl, fn);
		emit(Osltof, k, i.to, r0, R);
		emit(Oextuw, Kl, r0, i.arg[0], R);
		qbe_amd64_isel_fixarg(&curi->arg[0], k, curi, fn);
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
		emit(Ocast, k, i.to, tmp[6], R);
		emit(Oadd, kc, tmp[6], tmp[4], tmp[5]);
		emit(Oshl, kc, tmp[5], tmp[1], getcon(sh, fn));
		emit(Ocast, kc, tmp[4], r0, R);
		emit(Osltof, k, r0, tmp[3], R);
		emit(Oor, Kl, tmp[3], tmp[0], tmp[2]);
		emit(Oshr, Kl, tmp[2], i.arg[0], tmp[1]);
		qbe_amd64_isel_sel(*curi++, 0, fn);
		emit(Oshr, Kl, tmp[1], i.arg[0], getcon(63, fn));
		qbe_amd64_isel_fixarg(&curi->arg[0], Kl, curi, fn);
		emit(Oand, Kl, tmp[0], i.arg[0], getcon(1, fn));
		qbe_amd64_isel_fixarg(&curi->arg[0], Kl, curi, fn);
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
			emit(Ocopy, Kw, i.to, r0, R);
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
		emit(i.op, Kl, tmp[2], r0, R);
		emit(Oadd, kc, r0, tmp[4], i.arg[0]);
		i1 = curi; /* qbe_amd64_isel_fixarg() can change curi */
		qbe_amd64_isel_fixarg(&i1->arg[0], kc, i1, fn);
		qbe_amd64_isel_fixarg(&i1->arg[1], kc, i1, fn);
		emit(Osar, Kl, tmp[1], tmp[0], getcon(63, fn));
		emit(i.op, Kl, tmp[0], i.arg[0], R);
		qbe_amd64_isel_fixarg(&curi->arg[0], Kl, curi, fn);
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
	case_OExt:
Emit:
		emiti(i);
		i1 = curi; /* qbe_amd64_isel_fixarg() can change curi */
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
			goto case_OExt;
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
				emit(Oflagfo, k, r1, R, R);
				i.to = r0;
				break;
			case NCmpI+Cfne:
				r0 = newtmp("isel", Kw, fn);
				r1 = newtmp("isel", Kw, fn);
				emit(Oor, Kw, i.to, r0, r1);
				emit(Oflagfuo, k, r1, R, R);
				i.to = r0;
				break;
			}
			swap = qbe_amd64_isel_cmpswap(i.arg, x);
			if (swap)
				x = cmpop(x);
			emit(Oflag+x, k, i.to, R, R);
			qbe_amd64_isel_selcmp(i.arg, kc, swap, fn);
			break;
		}
		die("unknown instruction %s", optab[i.op].name);
	}

	while (i0>curi && --i0) {
		LQ_ASSERT(qbe_amd64_isel_rslot(i0->arg[0], fn) == -1);
		LQ_ASSERT(qbe_amd64_isel_rslot(i0->arg[1], fn) == -1);
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
	LQ_ASSERT(b->jmp.type == Jjnz);
	r = b->jmp.arg;
	t = &fn->tmp[r.val];
	b->jmp.arg = R;
	LQ_ASSERT(rtype(r) == RTmp);
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
	     && c != NCmpI+Cfeq /* see qbe_amd64_isel_sel() */
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
		fi->to = R;
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
			emit(Ocopy, Kw, R, r, R);
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

	while (!req(r, R)) {
		LQ_ASSERT(rtype(r) == RTmp);
		n = qbe_amd64_isel_refn(r, tn, fn->con);
		if (!(match[n] & BIT(Pob)))
			break;
		runmatch(matcher[Pob], tn, r, v);
		LQ_ASSERT(rtype(v[0]) == RCon);
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
	if (!req(ro, R)) {
		LQ_ASSERT(rtype(ro) == RCon);
		c = &fn->con[ro.val];
		if (!addcon(&co, c, 1))
			return 0;
	}
	if (!req(rs, R)) {
		LQ_ASSERT(rtype(rs) == RCon);
		c = &fn->con[rs.val];
		LQ_ASSERT(c->type == CBits);
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
	if (!req(a->base, R)) {
		LQ_ASSERT(rtype(a->base) == RTmp);
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
		curi = &insb[NIns];
		for (sb=(Blk*[3]){b->s1, b->s2, 0}; *sb; sb++)
			for (p=(*sb)->phi; p; p=p->link) {
				for (a=0; p->blk[a] != b; a++)
					LQ_ASSERT(a+1 < p->narg);
				qbe_amd64_isel_fixarg(&p->arg[a], p->cls, 0, fn);
			}
		memset(num, 0, n * sizeof num[0]);
		qbe_amd64_isel_anumber(num, b, fn->con);
		qbe_amd64_isel_seljmp(b, fn);
		for (i=&b->ins[b->nins]; i!=b->ins;)
			qbe_amd64_isel_sel(*--i, num, fn);
		b->nins = &insb[NIns] - curi;
		idup(&b->ins, curi, b->nins);
	}
	free(num);

	if (debug['I']) {
		fprintf(stderr, "\n> After instruction selection:\n");
		printfn(fn, stderr);
	}
}
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
			LQ_ASSERT(s <= 16);
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
				qbe_amd64_sysv_classify(a, &typ[f->len], s);
				s += typ[f->len].size;
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
		qbe_amd64_sysv_typclass(&aret, &typ[fn->retty]);
		if (aret.inmem) {
			LQ_ASSERT(rtype(fn->retr) == RTmp);
			emit(Ocopy, Kl, TMP(QBE_AMD64_RAX), fn->retr, R);
			emit(Oblit1, 0, R, INT(aret.type->size), R);
			emit(Oblit0, 0, R, r0, fn->retr);
			ca = 1;
		} else {
			ca = qbe_amd64_sysv_retr(reg, &aret);
			if (aret.size > 8) {
				r = newtmp("abi", Kl, fn);
				emit(Oload, Kl, reg[1], r, R);
				emit(Oadd, Kl, r, r0, getcon(8, fn));
			}
			emit(Oload, Kl, reg[0], r0, R);
		}
	} else {
		k = j - Jretw;
		if (KBASE(k) == 0) {
			emit(Ocopy, k, TMP(QBE_AMD64_RAX), r0, R);
			ca = 1;
		} else {
			emit(Ocopy, k, TMP(QBE_AMD64_XMM0), r0, R);
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
			qbe_amd64_sysv_typclass(a, &typ[n]);
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
		err("sysv abi does not support variadic env calls");

	return ((varc|envc) << 12) | ((6-nint) << 4) | ((8-nsse) << 8);
}

static int amd64_sysv_rsave[] = {
	QBE_AMD64_RDI, QBE_AMD64_RSI, QBE_AMD64_RDX, QBE_AMD64_RCX, QBE_AMD64_R8, QBE_AMD64_R9, QBE_AMD64_R10, QBE_AMD64_R11, QBE_AMD64_RAX,
	QBE_AMD64_XMM0, QBE_AMD64_XMM1, QBE_AMD64_XMM2, QBE_AMD64_XMM3, QBE_AMD64_XMM4, QBE_AMD64_XMM5, QBE_AMD64_XMM6, QBE_AMD64_XMM7,
	QBE_AMD64_XMM8, QBE_AMD64_XMM9, QBE_AMD64_XMM10, QBE_AMD64_XMM11, QBE_AMD64_XMM12, QBE_AMD64_XMM13, QBE_AMD64_XMM14, -1
};
static int amd64_sysv_rclob[] = {QBE_AMD64_RBX, QBE_AMD64_R12, QBE_AMD64_R13, QBE_AMD64_R14, QBE_AMD64_R15, -1};

MAKESURE(sysv_arrays_ok,
	sizeof amd64_sysv_rsave == (QBE_AMD64_NGPS+QBE_AMD64_NFPS+1) * sizeof(int) &&
	sizeof amd64_sysv_rclob == (QBE_AMD64_NCLR+1) * sizeof(int)
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

	LQ_ASSERT(rtype(r) == RCall);
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

	LQ_ASSERT(rtype(r) == RCall);
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

	env = R;
	ac = alloc((i1-i0) * sizeof ac[0]);

	if (!req(i1->arg[1], R)) {
		LQ_ASSERT(rtype(i1->arg[1]) == RType);
		qbe_amd64_sysv_typclass(&aret, &typ[i1->arg[1].val]);
		ca = qbe_amd64_sysv_argsclass(i0, i1, ac, Oarg, &aret, &env);
	} else
		ca = qbe_amd64_sysv_argsclass(i0, i1, ac, Oarg, 0, &env);

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
		emit(Osalloc, Kl, R, r, R);
	}

	if (!req(i1->arg[1], R)) {
		if (aret.inmem) {
			/* get the return location from eax
			 * it saves one callee-save reg */
			r1 = newtmp("abi", Kl, fn);
			emit(Ocopy, Kl, i1->to, TMP(QBE_AMD64_RAX), R);
			ca += 1;
		} else {
			/* todo, may read out of bounds.
			 * gcc did this up until 5.2, but
			 * this should still be fixed.
			 */
			if (aret.size > 8) {
				r = newtmp("abi", Kl, fn);
				aret.ref[1] = newtmp("abi", aret.cls[1], fn);
				emit(Ostorel, 0, R, aret.ref[1], r);
				emit(Oadd, Kl, r, i1->to, getcon(8, fn));
			}
			aret.ref[0] = newtmp("abi", aret.cls[0], fn);
			emit(Ostorel, 0, R, aret.ref[0], i1->to);
			ca += qbe_amd64_sysv_retr(reg, &aret);
			if (aret.size > 8)
				emit(Ocopy, aret.cls[1], aret.ref[1], reg[1], R);
			emit(Ocopy, aret.cls[0], aret.ref[0], reg[0], R);
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
			emit(Ocopy, i1->cls, i1->to, TMP(QBE_AMD64_RAX), R);
			ca += 1;
		} else {
			emit(Ocopy, i1->cls, i1->to, TMP(QBE_AMD64_XMM0), R);
			ca += 1 << 2;
		}
	}

	emit(Ocall, i1->cls, R, i1->arg[0], CALL(ca));

	if (!req(R, env))
		emit(Ocopy, Kl, TMP(QBE_AMD64_RAX), env, R);
	else if ((ca >> 12) & 1) /* vararg call */
		emit(Ocopy, Kw, TMP(QBE_AMD64_RAX), getcon((ca >> 8) & 15, fn), R);

	ni = ns = 0;
	if (ra && aret.inmem)
		emit(Ocopy, Kl, qbe_amd64_sysv_rarg(Kl, &ni, &ns), ra->i.to, R); /* pass hidden argument */

	for (i=i0, a=ac; i<i1; i++, a++) {
		if (i->op >= Oarge || a->inmem)
			continue;
		r1 = qbe_amd64_sysv_rarg(a->cls[0], &ni, &ns);
		if (i->op == Oargc) {
			if (a->size > 8) {
				r2 = qbe_amd64_sysv_rarg(a->cls[1], &ni, &ns);
				r = newtmp("abi", Kl, fn);
				emit(Oload, a->cls[1], r2, r, R);
				emit(Oadd, Kl, r, i->arg[1], getcon(8, fn));
			}
			emit(Oload, a->cls[0], r1, i->arg[1], R);
		} else
			emit(Ocopy, i->cls, r1, i->arg[0], R);
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
			emit(Oblit1, 0, R, INT(a->type->size), R);
			emit(Oblit0, 0, R, i->arg[1], r1);
		} else
			emit(Ostorel, 0, R, i->arg[0], r1);
		emit(Oadd, Kl, r1, r, getcon(off, fn));
		off += a->size;
	}
	emit(Osalloc, Kl, r, getcon(stk, fn), R);
}

static int
qbe_amd64_sysv_selpar(Fn *fn, Ins *i0, Ins *i1)
{
	QBE_AMD64_SYSV_AClass *ac, *a, aret;
	Ins *i;
	int ni, ns, s, al, fa;
	Ref r, env;

	env = R;
	ac = alloc((i1-i0) * sizeof ac[0]);
	curi = &insb[NIns];
	ni = ns = 0;

	if (fn->retty >= 0) {
		qbe_amd64_sysv_typclass(&aret, &typ[fn->retty]);
		fa = qbe_amd64_sysv_argsclass(i0, i1, ac, Opar, &aret, &env);
	} else
		fa = qbe_amd64_sysv_argsclass(i0, i1, ac, Opar, 0, &env);
	fn->reg = amd64_sysv_argregs(CALL(fa), 0);

	for (i=i0, a=ac; i<i1; i++, a++) {
		if (i->op != Oparc || a->inmem)
			continue;
		if (a->size > 8) {
			r = newtmp("abi", Kl, fn);
			a->ref[1] = newtmp("abi", Kl, fn);
			emit(Ostorel, 0, R, a->ref[1], r);
			emit(Oadd, Kl, r, i->to, getcon(8, fn));
		}
		a->ref[0] = newtmp("abi", Kl, fn);
		emit(Ostorel, 0, R, a->ref[0], i->to);
		/* specific to NAlign == 3 */
		al = a->align >= 2 ? a->align - 2 : 0;
		emit(Oalloc+al, Kl, i->to, getcon(a->size, fn), R);
	}

	if (fn->retty >= 0 && aret.inmem) {
		r = newtmp("abi", Kl, fn);
		emit(Ocopy, Kl, r, qbe_amd64_sysv_rarg(Kl, &ni, &ns), R);
		fn->retr = r;
	}

	for (i=i0, a=ac, s=4; i<i1; i++, a++) {
		switch (a->inmem) {
		case 1:
			if (a->align > 4)
				err("sysv abi requires alignments of 16 or less");
			if (a->align == 4)
				s = (s+3) & -4;
			fn->tmp[i->to.val].slot = -s;
			s += a->size / 4;
			continue;
		case 2:
			emit(Oload, i->cls, i->to, SLOT(-s), R);
			s += 2;
			continue;
		}
		if (i->op == Opare)
			continue;
		r = qbe_amd64_sysv_rarg(a->cls[0], &ni, &ns);
		if (i->op == Oparc) {
			emit(Ocopy, a->cls[0], a->ref[0], r, R);
			if (a->size > 8) {
				r = qbe_amd64_sysv_rarg(a->cls[1], &ni, &ns);
				emit(Ocopy, a->cls[1], a->ref[1], r, R);
			}
		} else
			emit(Ocopy, i->cls, i->to, r, R);
	}

	if (!req(R, env))
		emit(Ocopy, Kl, env, TMP(QBE_AMD64_RAX), R);

	return fa | (s*4)<<12;
}

static Blk *
qbe_amd64_sysv_split(Fn *fn, Blk *b)
{
	Blk *bn;

	++fn->nblk;
	bn = newblk();
	bn->nins = &insb[NIns] - curi;
	idup(&bn->ins, curi, bn->nins);
	curi = &insb[NIns];
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
			LQ_ASSERT(a+1<p->narg);
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
	emit(Oload, i->cls, i->to, loc, R);
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
	emit(Ostorew, Kw, R, r0, r1);
	emit(Oadd, Kl, r1, ap, isint ? CON_Z : c4);
	emit(Oadd, Kw, r0, nr, isint ? c8 : c16);
	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Oadd, Kl, lreg, r1, nr);
	emit(Oload, Kl, r1, r0, R);
	emit(Oadd, Kl, r0, ap, c16);
	breg = qbe_amd64_sysv_split(fn, b);
	breg->jmp.type = Jjmp;
	breg->s1 = b0;

	lstk = newtmp("abi", Kl, fn);
	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, R, r1, r0);
	emit(Oadd, Kl, r1, lstk, c8);
	emit(Oload, Kl, lstk, r0, R);
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
	emit(Oloadsw, Kl, nr, r0, R);
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
	emit(Ostorel, Kw, R, r1, r0);
	emit(Oadd, Kl, r1, TMP(QBE_AMD64_RBP), getcon(-176, fn));
	emit(Oadd, Kl, r0, ap, getcon(16, fn));
	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, R, r1, r0);
	emit(Oadd, Kl, r1, TMP(QBE_AMD64_RBP), getcon(sp, fn));
	emit(Oadd, Kl, r0, ap, getcon(8, fn));
	r0 = newtmp("abi", Kl, fn);
	emit(Ostorew, Kw, R, getcon(fp, fn), r0);
	emit(Oadd, Kl, r0, ap, getcon(4, fn));
	emit(Ostorew, Kw, R, getcon(gp, fn), ap);
}

void
amd64_sysv_abi(Fn *fn)
{
	Blk *b;
	Ins *i, *i0, *ip;
	QBE_AMD64_SYSV_RAlloc *ral;
	int n, fa;

	for (b=fn->start; b; b=b->link)
		b->visit = 0;

	/* lower parameters */
	for (b=fn->start, i=b->ins; i<&b->ins[b->nins]; i++)
		if (!ispar(i->op))
			break;
	fa = qbe_amd64_sysv_selpar(fn, b->ins, i);
	n = b->nins - (i - b->ins) + (&insb[NIns] - curi);
	i0 = alloc(n * sizeof(Ins));
	ip = icpy(ip = i0, curi, &insb[NIns] - curi);
	ip = icpy(ip, i, &b->ins[b->nins] - i);
	b->nins = n;
	b->ins = i0;

	/* lower calls, returns, and vararg instructions */
	ral = 0;
	b = fn->start;
	do {
		if (!(b = b->link))
			b = fn->start; /* do it last */
		if (b->visit)
			continue;
		curi = &insb[NIns];
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
		b->nins = &insb[NIns] - curi;
		idup(&b->ins, curi, b->nins);
	} while (b != fn->start);

	if (debug['A']) {
		fprintf(stderr, "\n> After ABI lowering:\n");
		printfn(fn, stderr);
	}
}
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

#ifndef P
	#define P(CanFold, HasId, IdVal)
#endif


#define T(a,b,c,d,e,f,g,h) {                          \
	{[Kw]=K##a, [Kl]=K##b, [Ks]=K##c, [Kd]=K##d}, \
	{[Kw]=K##e, [Kl]=K##f, [Ks]=K##g, [Kd]=K##h}  \
}


/*********************/
/* PUBLIC OPERATIONS */
/*********************/

/* Arithmetic and Bits */
O(add,     T(w,l,s,d, w,l,s,d), P(1,1,0)) X(2,1,0) V(1)
O(sub,     T(w,l,s,d, w,l,s,d), P(1,1,0)) X(2,1,0) V(0)
O(neg,     T(w,l,s,d, x,x,x,x), P(1,0,0)) X(1,1,0) V(0)
O(div,     T(w,l,s,d, w,l,s,d), P(1,1,1)) X(0,0,0) V(0)
O(rem,     T(w,l,e,e, w,l,e,e), P(1,0,0)) X(0,0,0) V(0)
O(udiv,    T(w,l,e,e, w,l,e,e), P(1,1,1)) X(0,0,0) V(0)
O(urem,    T(w,l,e,e, w,l,e,e), P(1,0,0)) X(0,0,0) V(0)
O(mul,     T(w,l,s,d, w,l,s,d), P(1,1,1)) X(2,0,0) V(0)
O(and,     T(w,l,e,e, w,l,e,e), P(1,0,0)) X(2,1,0) V(1)
O(or,      T(w,l,e,e, w,l,e,e), P(1,1,0)) X(2,1,0) V(1)
O(xor,     T(w,l,e,e, w,l,e,e), P(1,1,0)) X(2,1,0) V(1)
O(sar,     T(w,l,e,e, w,w,e,e), P(1,1,0)) X(1,1,0) V(1)
O(shr,     T(w,l,e,e, w,w,e,e), P(1,1,0)) X(1,1,0) V(1)
O(shl,     T(w,l,e,e, w,w,e,e), P(1,1,0)) X(1,1,0) V(1)

/* Comparisons */
O(ceqw,    T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cnew,    T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csgew,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csgtw,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cslew,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csltw,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(1)
O(cugew,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cugtw,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(culew,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cultw,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(1)

O(ceql,    T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cnel,    T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csgel,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csgtl,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cslel,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csltl,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(1)
O(cugel,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cugtl,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(culel,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cultl,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(1)

O(ceqs,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cges,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cgts,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cles,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(clts,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cnes,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cos,     T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cuos,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)

O(ceqd,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cged,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cgtd,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cled,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cltd,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cned,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cod,     T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cuod,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)

/* Memory */
O(storeb,  T(w,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(storeh,  T(w,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(storew,  T(w,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(storel,  T(l,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(stores,  T(s,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(stored,  T(d,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)

O(loadsb,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loadub,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loadsh,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loaduh,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loadsw,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loaduw,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(load,    T(m,m,m,m, x,x,x,x), P(0,0,0)) X(0,0,1) V(0)

/* Extensions and Truncations */
O(extsb,   T(w,w,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extub,   T(w,w,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extsh,   T(w,w,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extuh,   T(w,w,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extsw,   T(e,w,e,e, e,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extuw,   T(e,w,e,e, e,x,e,e), P(1,0,0)) X(0,0,1) V(0)

O(exts,    T(e,e,e,s, e,e,e,x), P(1,0,0)) X(0,0,1) V(0)
O(truncd,  T(e,e,d,e, e,e,x,e), P(1,0,0)) X(0,0,1) V(0)
O(stosi,   T(s,s,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(stoui,   T(s,s,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(dtosi,   T(d,d,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(dtoui,   T(d,d,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(swtof,   T(e,e,w,w, e,e,x,x), P(1,0,0)) X(0,0,1) V(0)
O(uwtof,   T(e,e,w,w, e,e,x,x), P(1,0,0)) X(0,0,1) V(0)
O(sltof,   T(e,e,l,l, e,e,x,x), P(1,0,0)) X(0,0,1) V(0)
O(ultof,   T(e,e,l,l, e,e,x,x), P(1,0,0)) X(0,0,1) V(0)
O(cast,    T(s,d,w,l, x,x,x,x), P(1,0,0)) X(0,0,1) V(0)

/* Stack Allocation */
O(alloc4,  T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(alloc8,  T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(alloc16, T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)

/* Variadic Function Helpers */
O(vaarg,   T(m,m,m,m, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(vastart, T(m,e,e,e, x,e,e,e), P(0,0,0)) X(0,0,0) V(0)

O(copy,    T(w,l,s,d, x,x,x,x), P(0,0,0)) X(0,0,1) V(0)

/* Debug */
O(dbgloc,  T(w,e,e,e, w,e,e,e), P(0,0,0)) X(0,0,1) V(0)

/****************************************/
/* INTERNAL OPERATIONS (keep nop first) */
/****************************************/

/* Miscellaneous and Architecture-Specific Operations */
O(nop,     T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,1) V(0)
O(addr,    T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(blit0,   T(m,e,e,e, m,e,e,e), P(0,0,0)) X(0,1,0) V(0)
O(blit1,   T(w,e,e,e, x,e,e,e), P(0,0,0)) X(0,1,0) V(0)
O(swap,    T(w,l,s,d, w,l,s,d), P(0,0,0)) X(1,0,0) V(0)
O(sign,    T(w,l,e,e, x,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(salloc,  T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(xidiv,   T(w,l,e,e, x,x,e,e), P(0,0,0)) X(1,0,0) V(0)
O(xdiv,    T(w,l,e,e, x,x,e,e), P(0,0,0)) X(1,0,0) V(0)
O(xcmp,    T(w,l,s,d, w,l,s,d), P(0,0,0)) X(1,1,0) V(0)
O(xtest,   T(w,l,e,e, w,l,e,e), P(0,0,0)) X(1,1,0) V(0)
O(acmp,    T(w,l,e,e, w,l,e,e), P(0,0,0)) X(0,0,0) V(0)
O(acmn,    T(w,l,e,e, w,l,e,e), P(0,0,0)) X(0,0,0) V(0)
O(afcmp,   T(e,e,s,d, e,e,s,d), P(0,0,0)) X(0,0,0) V(0)
O(reqz,    T(w,l,e,e, x,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(rnez,    T(w,l,e,e, x,x,e,e), P(0,0,0)) X(0,0,0) V(0)

/* Arguments, Parameters, and Calls */
O(par,     T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(parsb,   T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(parub,   T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(parsh,   T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(paruh,   T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(parc,    T(e,x,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(pare,    T(e,x,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(arg,     T(w,l,s,d, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(argsb,   T(w,e,e,e, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(argub,   T(w,e,e,e, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(argsh,   T(w,e,e,e, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(arguh,   T(w,e,e,e, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(argc,    T(e,x,e,e, e,l,e,e), P(0,0,0)) X(0,0,0) V(0)
O(arge,    T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(argv,    T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(call,    T(m,m,m,m, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)

/* Flags Setting */
O(flagieq,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagine,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagisge, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagisgt, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagisle, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagislt, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagiuge, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagiugt, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagiule, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagiult, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfeq,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfge,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfgt,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfle,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagflt,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfne,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfo,   T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfuo,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)


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
	.emitfn = amd64_emitfn, \

static Target T_amd64_sysv = {
	.name = "amd64_sysv",
	.emitfin = elf_emitfin,
	.asloc = ".L",
	.abi1 = amd64_sysv_abi,
	.retregs = amd64_sysv_retregs,
	.argregs = amd64_sysv_argregs,
	.rsave = amd64_sysv_rsave,
	.nrsave = {QBE_AMD64_NGPS, QBE_AMD64_NFPS},
	AMD64_COMMON
};

static Target T_amd64_apple = {
	.name = "amd64_apple",
	.apple = 1,
	.emitfin = macho_emitfin,
	.asloc = "L",
	.assym = "_",
	.abi1 = amd64_sysv_abi,
	.retregs = amd64_sysv_retregs,
	.argregs = amd64_sysv_argregs,
	.rsave = amd64_sysv_rsave,
	.nrsave = {QBE_AMD64_NGPS, QBE_AMD64_NFPS},
	AMD64_COMMON
};

static Target T_amd64_win = {
	.name = "amd64_win",
	.windows = 1,
	.emitfin = win_emitfin,
	.asloc = ".L",
	.abi1 = amd64_winabi_abi,
	.retregs = amd64_winabi_retregs,
	.argregs = amd64_winabi_argregs,
	.rsave = amd64_winabi_rsave,
	.nrsave = {QBE_AMD64_NGPS - 2, QBE_AMD64_XMM3 - QBE_AMD64_XMM0 + 1},
	AMD64_COMMON
};
/*** END FILE: amd64/targ.c ***/
/*** START FILE: amd64/winabi.c ***/
/* skipping all.h */

typedef struct QBE_AMD64_WINABI_AClass QBE_AMD64_WINABI_AClass;
typedef struct QBE_AMD64_WINABI_RAlloc QBE_AMD64_WINABI_RAlloc;

struct QBE_AMD64_WINABI_AClass {
	Typ *type;
	int inmem;
	int align;
	uint size;
	int cls[2];
	Ref ref[2];
};

struct QBE_AMD64_WINABI_RAlloc {
	Ins i;
	QBE_AMD64_WINABI_RAlloc *link;
};

static void
qbe_amd64_winabi_classify(QBE_AMD64_WINABI_AClass *a, Typ *t, uint s)
{
	Field *f;
	int *cls;
	uint n, s1;

	for (n=0, s1=s; n<t->nunion; n++, s=s1)
		for (f=t->fields[n]; f->type!=FEnd; f++) {
			LQ_ASSERT(s <= 16);
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
				qbe_amd64_winabi_classify(a, &typ[f->len], s);
				s += typ[f->len].size;
				break;
			}
		}
}

static void
qbe_amd64_winabi_typclass(QBE_AMD64_WINABI_AClass *a, Typ *t)
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
	qbe_amd64_winabi_classify(a, t, 0);
}

static int
qbe_amd64_winabi_retr(Ref *reg, QBE_AMD64_WINABI_AClass *aret)
{
	int k;

	LQ_ASSERT(aret->size <= 8);
	k = KBASE(aret->cls[0]);
	*reg = (k == 0 ? TMP(QBE_AMD64_RAX) : TMP(QBE_AMD64_XMM0));
	return 1 << (2 * k);
}

static void
qbe_amd64_winabi_selret(Blk *b, Fn *fn)
{
	int j, k, ca;
	Ref r0, reg;
	QBE_AMD64_WINABI_AClass aret;

	j = b->jmp.type;

	if (!isret(j) || j == Jret0)
		return;

	r0 = b->jmp.arg;
	b->jmp.type = Jret0;

	if (j == Jretc) {
		qbe_amd64_winabi_typclass(&aret, &typ[fn->retty]);
		if (aret.inmem) {
			LQ_ASSERT(rtype(fn->retr) == RTmp);
			emit(Ocopy, Kl, TMP(QBE_AMD64_RAX), fn->retr, R);
			emit(Oblit1, 0, R, INT(aret.type->size), R);
			emit(Oblit0, 0, R, r0, fn->retr);
			ca = 1;
		} else {
			ca = qbe_amd64_winabi_retr(&reg, &aret);
			LQ_ASSERT(aret.size <= 8);
			emit(Oload, Kl, reg, r0, R);
		}
	} else {
		k = j - Jretw;
		if (KBASE(k) == 0) {
			emit(Ocopy, k, TMP(QBE_AMD64_RAX), r0, R);
			ca = 1;
		} else {
			emit(Ocopy, k, TMP(QBE_AMD64_XMM0), r0, R);
			ca = 1 << 2;
		}
	}

	b->jmp.arg = CALL(ca);
}

static int
qbe_amd64_winabi_argsclass(Ins *i0, Ins *i1, QBE_AMD64_WINABI_AClass *ac, int op, QBE_AMD64_WINABI_AClass *aret, Ref *env)
{
	int varc, envc, nint, ni, nsse, ns, n, *pn;
	QBE_AMD64_WINABI_AClass *a;
	Ins *i;

	if (aret && aret->inmem)
		nint = 3; /* hidden argument */
	else
		nint = 4;
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
			qbe_amd64_winabi_typclass(a, &typ[n]);
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
		err("winabi abi does not support variadic env calls");

	return ((varc|envc) << 12) | ((4-nint) << 4) | ((8-nsse) << 8);
}

static int amd64_winabi_rsave[] = {
	QBE_AMD64_RCX, QBE_AMD64_RDX, QBE_AMD64_R8, QBE_AMD64_R9, QBE_AMD64_R10, QBE_AMD64_R11, QBE_AMD64_RAX,
	QBE_AMD64_XMM0, QBE_AMD64_XMM1, QBE_AMD64_XMM2, QBE_AMD64_XMM3, QBE_AMD64_XMM4, QBE_AMD64_XMM5, QBE_AMD64_XMM6, QBE_AMD64_XMM7,
	QBE_AMD64_XMM8, QBE_AMD64_XMM9, QBE_AMD64_XMM10, QBE_AMD64_XMM11, QBE_AMD64_XMM12, QBE_AMD64_XMM13, QBE_AMD64_XMM14, -1
};
static int amd64_winabi_rclob[] = {QBE_AMD64_RBX, QBE_AMD64_R12, QBE_AMD64_R13, QBE_AMD64_R14, QBE_AMD64_R15, QBE_AMD64_RSI, QBE_AMD64_RDI, -1};

MAKESURE(winabi_arrays_ok,
	sizeof amd64_winabi_rsave == (QBE_AMD64_NGPS+QBE_AMD64_NFPS+1-2) * sizeof(int) &&
	sizeof amd64_winabi_rclob == (QBE_AMD64_NCLR+2+1) * sizeof(int)
);

bits
amd64_winabi_retregs(Ref r, int p[2])
{
	bits b;
	int ni, nf;

	LQ_ASSERT(rtype(r) == RCall);
	b = 0;
	ni = r.val & 3;
	nf = (r.val >> 2) & 3;
	LQ_ASSERT(ni <= 1);
	LQ_ASSERT(nf <= 1);
	if (ni == 1)
		b |= BIT(QBE_AMD64_RAX);
	else
		b |= BIT(QBE_AMD64_XMM1);
	if (p) {
		p[0] = ni;
		p[1] = nf;
	}
	return b;
}

bits
amd64_winabi_argregs(Ref r, int p[2])
{
	bits b;
	int j, ni, nf, ra;

	LQ_ASSERT(rtype(r) == RCall);
	b = 0;
	ni = (r.val >> 4) & 15;
	nf = (r.val >> 8) & 15;
	ra = (r.val >> 12) & 1;
	for (j=0; j<ni; j++)
		b |= BIT(amd64_winabi_rsave[j]);
	for (j=0; j<nf; j++)
		b |= BIT(QBE_AMD64_XMM0+j);
	if (p) {
		p[0] = ni + ra;
		p[1] = nf;
	}
	return b | (ra ? BIT(QBE_AMD64_RAX) : 0);
}

static Ref
qbe_amd64_winabi_rarg(int ty, int *ni, int *ns)
{
	if (KBASE(ty) == 0)
		return TMP(amd64_winabi_rsave[(*ni)++]);
	else
		return TMP(QBE_AMD64_XMM0 + (*ns)++);
}

static void
qbe_amd64_winabi_selcall(Fn *fn, Ins *i0, Ins *i1, QBE_AMD64_WINABI_RAlloc **rap)
{
	Ins *i;
	QBE_AMD64_WINABI_AClass *ac, *a, aret;
	int ca, ni, ns, al;
	uint stk, off;
	Ref r, r1, r2, reg[2], env;
	QBE_AMD64_WINABI_RAlloc *ra;

	env = R;
	ac = alloc((i1-i0) * sizeof ac[0]);

	if (!req(i1->arg[1], R)) {
		LQ_ASSERT(rtype(i1->arg[1]) == RType);
		qbe_amd64_winabi_typclass(&aret, &typ[i1->arg[1].val]);
		ca = qbe_amd64_winabi_argsclass(i0, i1, ac, Oarg, &aret, &env);
	} else
		ca = qbe_amd64_winabi_argsclass(i0, i1, ac, Oarg, 0, &env);

	for (stk=0, a=&ac[i1-i0]; a>ac;)
		if ((--a)->inmem) {
			if (a->align > 4)
				err("winabi abi requires alignments of 16 or less");
			stk += a->size;
			if (a->align == 4)
				stk += stk & 15;
		}
	stk += stk & 15;
	if (stk) {
		r = getcon(-(int64_t)stk, fn);
		emit(Osalloc, Kl, R, r, R);
	}

	if (!req(i1->arg[1], R)) {
		if (aret.inmem) {
			/* get the return location from eax
			 * it saves one callee-save reg */
			r1 = newtmp("abi", Kl, fn);
			emit(Ocopy, Kl, i1->to, TMP(QBE_AMD64_RAX), R);
			ca += 1;
		} else {
			/* todo, may read out of bounds.
			 * gcc did this up until 5.2, but
			 * this should still be fixed.
			 */
			if (aret.size > 8) {
				r = newtmp("abi", Kl, fn);
				aret.ref[1] = newtmp("abi", aret.cls[1], fn);
				emit(Ostorel, 0, R, aret.ref[1], r);
				emit(Oadd, Kl, r, i1->to, getcon(8, fn));
			}
			aret.ref[0] = newtmp("abi", aret.cls[0], fn);
			emit(Ostorel, 0, R, aret.ref[0], i1->to);
			ca += qbe_amd64_winabi_retr(reg, &aret);
			if (aret.size > 8)
				emit(Ocopy, aret.cls[1], aret.ref[1], reg[1], R);
			emit(Ocopy, aret.cls[0], aret.ref[0], reg[0], R);
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
			emit(Ocopy, i1->cls, i1->to, TMP(QBE_AMD64_RAX), R);
			ca += 1;
		} else {
			emit(Ocopy, i1->cls, i1->to, TMP(QBE_AMD64_XMM0), R);
			ca += 1 << 2;
		}
	}

	emit(Ocall, i1->cls, R, i1->arg[0], CALL(ca));

	if (!req(R, env))
		emit(Ocopy, Kl, TMP(QBE_AMD64_RAX), env, R);
	else if ((ca >> 12) & 1) /* vararg call */
		emit(Ocopy, Kw, TMP(QBE_AMD64_RAX), getcon((ca >> 8) & 15, fn), R);

	ni = ns = 0;
	if (ra && aret.inmem)
		emit(Ocopy, Kl, qbe_amd64_winabi_rarg(Kl, &ni, &ns), ra->i.to, R); /* pass hidden argument */

	for (i=i0, a=ac; i<i1; i++, a++) {
		if (i->op >= Oarge || a->inmem)
			continue;
		r1 = qbe_amd64_winabi_rarg(a->cls[0], &ni, &ns);
		if (i->op == Oargc) {
			if (a->size > 8) {
				r2 = qbe_amd64_winabi_rarg(a->cls[1], &ni, &ns);
				r = newtmp("abi", Kl, fn);
				emit(Oload, a->cls[1], r2, r, R);
				emit(Oadd, Kl, r, i->arg[1], getcon(8, fn));
			}
			emit(Oload, a->cls[0], r1, i->arg[1], R);
		} else
			emit(Ocopy, i->cls, r1, i->arg[0], R);
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
			emit(Oblit1, 0, R, INT(a->type->size), R);
			emit(Oblit0, 0, R, i->arg[1], r1);
		} else
			emit(Ostorel, 0, R, i->arg[0], r1);
		emit(Oadd, Kl, r1, r, getcon(off, fn));
		off += a->size;
	}
	emit(Osalloc, Kl, r, getcon(stk, fn), R);
}

static int
qbe_amd64_winabi_selpar(Fn *fn, Ins *i0, Ins *i1)
{
	QBE_AMD64_WINABI_AClass *ac, *a, aret;
	Ins *i;
	int ni, ns, s, al, fa;
	Ref r, env;

	env = R;
	ac = alloc((i1-i0) * sizeof ac[0]);
	curi = &insb[NIns];
	ni = ns = 0;

	if (fn->retty >= 0) {
		qbe_amd64_winabi_typclass(&aret, &typ[fn->retty]);
		fa = qbe_amd64_winabi_argsclass(i0, i1, ac, Opar, &aret, &env);
	} else
		fa = qbe_amd64_winabi_argsclass(i0, i1, ac, Opar, 0, &env);
	fn->reg = amd64_winabi_argregs(CALL(fa), 0);

	for (i=i0, a=ac; i<i1; i++, a++) {
		if (i->op != Oparc || a->inmem)
			continue;
		if (a->size > 8) {
			r = newtmp("abi", Kl, fn);
			a->ref[1] = newtmp("abi", Kl, fn);
			emit(Ostorel, 0, R, a->ref[1], r);
			emit(Oadd, Kl, r, i->to, getcon(8, fn));
		}
		a->ref[0] = newtmp("abi", Kl, fn);
		emit(Ostorel, 0, R, a->ref[0], i->to);
		/* specific to NAlign == 3 */
		al = a->align >= 2 ? a->align - 2 : 0;
		emit(Oalloc+al, Kl, i->to, getcon(a->size, fn), R);
	}

	if (fn->retty >= 0 && aret.inmem) {
		r = newtmp("abi", Kl, fn);
		emit(Ocopy, Kl, r, qbe_amd64_winabi_rarg(Kl, &ni, &ns), R);
		fn->retr = r;
	}

	for (i=i0, a=ac, s=4; i<i1; i++, a++) {
		switch (a->inmem) {
		case 1:
			if (a->align > 4)
				err("winabi abi requires alignments of 16 or less");
			if (a->align == 4)
				s = (s+3) & -4;
			fn->tmp[i->to.val].slot = -s;
			s += a->size / 4;
			continue;
		case 2:
			emit(Oload, i->cls, i->to, SLOT(-s), R);
			s += 2;
			continue;
		}
		if (i->op == Opare)
			continue;
		r = qbe_amd64_winabi_rarg(a->cls[0], &ni, &ns);
		if (i->op == Oparc) {
			emit(Ocopy, a->cls[0], a->ref[0], r, R);
			if (a->size > 8) {
				r = qbe_amd64_winabi_rarg(a->cls[1], &ni, &ns);
				emit(Ocopy, a->cls[1], a->ref[1], r, R);
			}
		} else
			emit(Ocopy, i->cls, i->to, r, R);
	}

	if (!req(R, env))
		emit(Ocopy, Kl, env, TMP(QBE_AMD64_RAX), R);

	return fa | (s*4)<<12;
}

static Blk *
qbe_amd64_winabi_split(Fn *fn, Blk *b)
{
	Blk *bn;

	++fn->nblk;
	bn = newblk();
	bn->nins = &insb[NIns] - curi;
	idup(&bn->ins, curi, bn->nins);
	curi = &insb[NIns];
	bn->visit = ++b->visit;
	strf(bn->name, "%s.%d", b->name, b->visit);
	bn->loop = b->loop;
	bn->link = b->link;
	b->link = bn;
	return bn;
}

static void
qbe_amd64_winabi_chpred(Blk *b, Blk *bp, Blk *bp1)
{
	Phi *p;
	uint a;

	for (p=b->phi; p; p=p->link) {
		for (a=0; p->blk[a]!=bp; a++)
			LQ_ASSERT(a+1<p->narg);
		p->blk[a] = bp1;
	}
}

static void
qbe_amd64_winabi_selvaarg(Fn *fn, Blk *b, Ins *i)
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
	emit(Oload, i->cls, i->to, loc, R);
	b0 = qbe_amd64_winabi_split(fn, b);
	b0->jmp = b->jmp;
	b0->s1 = b->s1;
	b0->s2 = b->s2;
	if (b->s1)
		qbe_amd64_winabi_chpred(b->s1, b, b0);
	if (b->s2 && b->s2 != b->s1)
		qbe_amd64_winabi_chpred(b->s2, b, b0);

	lreg = newtmp("abi", Kl, fn);
	nr = newtmp("abi", Kl, fn);
	r0 = newtmp("abi", Kw, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Ostorew, Kw, R, r0, r1);
	emit(Oadd, Kl, r1, ap, isint ? CON_Z : c4);
	emit(Oadd, Kw, r0, nr, isint ? c8 : c16);
	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Oadd, Kl, lreg, r1, nr);
	emit(Oload, Kl, r1, r0, R);
	emit(Oadd, Kl, r0, ap, c16);
	breg = qbe_amd64_winabi_split(fn, b);
	breg->jmp.type = Jjmp;
	breg->s1 = b0;

	lstk = newtmp("abi", Kl, fn);
	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, R, r1, r0);
	emit(Oadd, Kl, r1, lstk, c8);
	emit(Oload, Kl, lstk, r0, R);
	emit(Oadd, Kl, r0, ap, c8);
	bstk = qbe_amd64_winabi_split(fn, b);
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
	emit(Oloadsw, Kl, nr, r0, R);
	emit(Oadd, Kl, r0, ap, isint ? CON_Z : c4);
}

static void
qbe_amd64_winabi_selvastart(Fn *fn, int fa, Ref ap)
{
	Ref r0, r1;
	int gp, fp, sp;

	gp = ((fa >> 4) & 15) * 8;
	fp = 48 + ((fa >> 8) & 15) * 16;
	sp = fa >> 12;
	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, R, r1, r0);
	emit(Oadd, Kl, r1, TMP(QBE_AMD64_RBP), getcon(-176, fn));
	emit(Oadd, Kl, r0, ap, getcon(16, fn));
	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, R, r1, r0);
	emit(Oadd, Kl, r1, TMP(QBE_AMD64_RBP), getcon(sp, fn));
	emit(Oadd, Kl, r0, ap, getcon(8, fn));
	r0 = newtmp("abi", Kl, fn);
	emit(Ostorew, Kw, R, getcon(fp, fn), r0);
	emit(Oadd, Kl, r0, ap, getcon(4, fn));
	emit(Ostorew, Kw, R, getcon(gp, fn), ap);
}

void
amd64_winabi_abi(Fn *fn)
{
	Blk *b;
	Ins *i, *i0, *ip;
	QBE_AMD64_WINABI_RAlloc *ral;
	int n, fa;

	for (b=fn->start; b; b=b->link)
		b->visit = 0;

	/* lower parameters */
	for (b=fn->start, i=b->ins; i<&b->ins[b->nins]; i++)
		if (!ispar(i->op))
			break;
	fa = qbe_amd64_winabi_selpar(fn, b->ins, i);
	n = b->nins - (i - b->ins) + (&insb[NIns] - curi);
	i0 = alloc(n * sizeof(Ins));
	ip = icpy(ip = i0, curi, &insb[NIns] - curi);
	ip = icpy(ip, i, &b->ins[b->nins] - i);
	b->nins = n;
	b->ins = i0;

	/* lower calls, returns, and vararg instructions */
	ral = 0;
	b = fn->start;
	do {
		if (!(b = b->link))
			b = fn->start; /* do it last */
		if (b->visit)
			continue;
		curi = &insb[NIns];
		qbe_amd64_winabi_selret(b, fn);
		for (i=&b->ins[b->nins]; i!=b->ins;)
			switch ((--i)->op) {
			default:
				emiti(*i);
				break;
			case Ocall:
				for (i0=i; i0>b->ins; i0--)
					if (!isarg((i0-1)->op))
						break;
				qbe_amd64_winabi_selcall(fn, i0, i, &ral);
				i = i0;
				break;
			case Ovastart:
				qbe_amd64_winabi_selvastart(fn, fa, i->arg[0]);
				break;
			case Ovaarg:
				qbe_amd64_winabi_selvaarg(fn, b, i);
				break;
			case Oarg:
			case Oargc:
				die("unreachable");
			}
		if (b == fn->start)
			for (; ral; ral=ral->link)
				emiti(ral->i);
		b->nins = &insb[NIns] - curi;
		idup(&b->ins, curi, b->nins);
	} while (b != fn->start);

	if (debug['A']) {
		fprintf(stderr, "\n> After ABI lowering:\n");
		printfn(fn, stderr);
	}
}
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
				if (qbe_arm64_abi_isfloatv(&typ[f->len], cls))
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

	LQ_ASSERT(nreg <= 4);
	off = 0;
	for (n=0; n<nreg; n++) {
		tmp[n] = newtmp("abi", cls[n], fn);
		r = newtmp("abi", Kl, fn);
		emit(store[cls[n]], 0, R, tmp[n], r);
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
		emit(Oload, cls[i], TMP(reg[i]), r, R);
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
		qbe_arm64_abi_typclass(&cr, &typ[fn->retty], qbe_arm64_abi_gpreg, qbe_arm64_abi_fpreg);
		if (cr.class & QBE_ARM64_ABI_Cptr) {
			LQ_ASSERT(rtype(fn->retr) == RTmp);
			emit(Oblit1, 0, R, INT(cr.t->size), R);
			emit(Oblit0, 0, R, r, fn->retr);
			cty = 0;
		} else {
			qbe_arm64_abi_ldregs(cr.reg, cr.cls, cr.nreg, r, fn);
			cty = (cr.nfp << 2) | cr.ngp;
		}
	} else {
		k = j - Jretw;
		if (KBASE(k) == 0) {
			emit(Ocopy, k, TMP(QBE_ARM64_R0), r, R);
			cty = 1;
		} else {
			emit(Ocopy, k, TMP(QBE_ARM64_V0), r, R);
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
			if (T.apple && !KWIDE(i->cls))
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
			qbe_arm64_abi_typclass(c, &typ[i->arg[0].val], gp, fp);
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
			va = T.apple != 0;
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

	LQ_ASSERT(rtype(r) == RCall);
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

	LQ_ASSERT(rtype(r) == RCall);
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

	if (!req(i1->arg[1], R)) {
		qbe_arm64_abi_typclass(&cr, &typ[i1->arg[1].val], qbe_arm64_abi_gpreg, qbe_arm64_abi_fpreg);
		qbe_arm64_abi_stkblob(i1->to, &cr, fn, ilp);
		cty |= (cr.nfp << 2) | cr.ngp;
		if (cr.class & QBE_ARM64_ABI_Cptr) {
			/* spill & rega expect calls to be
			 * followed by copies from regs,
			 * so we emit a dummy
			 */
			cty |= 1 << 13 | 1;
			emit(Ocopy, Kw, R, TMP(QBE_ARM64_R0), R);
		} else {
			qbe_arm64_abi_sttmps(tmp, cr.cls, cr.nreg, i1->to, fn);
			for (n=0; n<cr.nreg; n++) {
				r = TMP(cr.reg[n]);
				emit(Ocopy, cr.cls[n], tmp[n], r, R);
			}
		}
	} else {
		if (KBASE(i1->cls) == 0) {
			emit(Ocopy, i1->cls, i1->to, TMP(QBE_ARM64_R0), R);
			cty |= 1;
		} else {
			emit(Ocopy, i1->cls, i1->to, TMP(QBE_ARM64_V0), R);
			cty |= 1 << 2;
		}
	}

	emit(Ocall, 0, R, i1->arg[0], CALL(cty));

	if (cty & (1 << 13))
		/* struct return argument */
		emit(Ocopy, Kl, TMP(QBE_ARM64_R8), i1->to, R);

	for (i=i0, c=ca; i<i1; i++, c++) {
		if ((c->class & QBE_ARM64_ABI_Cstk) != 0)
			continue;
		if (i->op == Oarg || i->op == Oarge)
			emit(Ocopy, *c->cls, TMP(*c->reg), i->arg[0], R);
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
			emit(op, 0, R, i->arg[0], r);
		} else {
			LQ_ASSERT(i->op == Oargc);
			emit(Oblit1, 0, R, INT(c->size), R);
			emit(Oblit0, 0, R, i->arg[1], r);
		}
		emit(Oadd, Kl, r, TMP(QBE_ARM64_SP), getcon(off, fn));
		off += c->size;
	}
	if (stk)
		emit(Osub, Kl, TMP(QBE_ARM64_SP), TMP(QBE_ARM64_SP), rstk);

	for (i=i0, c=ca; i<i1; i++, c++)
		if (c->class & QBE_ARM64_ABI_Cptr) {
			emit(Oblit1, 0, R, INT(c->t->size), R);
			emit(Oblit0, 0, R, i->arg[1], i->arg[0]);
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
	curi = &insb[NIns];

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
		qbe_arm64_abi_typclass(&cr, &typ[fn->retty], qbe_arm64_abi_gpreg, qbe_arm64_abi_fpreg);
		if (cr.class & QBE_ARM64_ABI_Cptr) {
			fn->retr = newtmp("abi", Kl, fn);
			emit(Ocopy, Kl, fn->retr, TMP(QBE_ARM64_R8), R);
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
					emit(Ocopy, c->cls[n], *t++, r, R);
				}
		} else if (c->class & QBE_ARM64_ABI_Cstk) {
			off = qbe_arm64_abi_align(off, c->align);
			if (isparbh(i->op))
				op = Oloadsb + (i->op - Oparsb);
			else
				op = Oload;
			emit(op, *c->cls, i->to, SLOT(-(off+2)), R);
			off += c->size;
		} else {
			emit(Ocopy, *c->cls, i->to, TMP(*c->reg), R);
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
	bn->nins = &insb[NIns] - curi;
	idup(&bn->ins, curi, bn->nins);
	curi = &insb[NIns];
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
			LQ_ASSERT(a+1<p->narg);
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

	emit(Ostorel, 0, R, stk8, ap);
	emit(Oadd, Kl, stk8, stk, c8);
	emit(Oload, i->cls, i->to, stk, R);
	emit(Oload, Kl, stk, ap, R);
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
	emit(Oload, i->cls, i->to, loc, R);
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
	emit(Ostorew, Kw, R, r0, r1);
	emit(Oadd, Kl, r1, ap, isgp ? c24 : c28);
	emit(Oadd, Kw, r0, nr, isgp ? c8 : c16);
	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Oadd, Kl, lreg, r1, nr);
	emit(Oload, Kl, r1, r0, R);
	emit(Oadd, Kl, r0, ap, isgp ? c8 : c16);
	breg = qbe_arm64_abi_split(fn, b);
	breg->jmp.type = Jjmp;
	breg->s1 = b0;

	lstk = newtmp("abi", Kl, fn);
	r0 = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, R, r0, ap);
	emit(Oadd, Kl, r0, lstk, c8);
	emit(Oload, Kl, lstk, ap, R);
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
	emit(Oloadsw, Kl, nr, r0, R);
	emit(Oadd, Kl, r0, ap, isgp ? c24 : c28);
}

static void
qbe_arm64_abi_apple_selvastart(Fn *fn, QBE_ARM64_ABI_Params p, Ref ap)
{
	Ref off, stk, arg;

	off = getcon(p.stk, fn);
	stk = newtmp("abi", Kl, fn);
	arg = newtmp("abi", Kl, fn);

	emit(Ostorel, 0, R, arg, ap);
	emit(Oadd, Kl, arg, stk, off);
	emit(Oaddr, Kl, stk, SLOT(-1), R);
}

static void
qbe_arm64_abi_arm64_selvastart(Fn *fn, QBE_ARM64_ABI_Params p, Ref ap)
{
	Ref r0, r1, rsave;

	rsave = newtmp("abi", Kl, fn);

	r0 = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, R, r0, ap);
	emit(Oadd, Kl, r0, rsave, getcon(p.stk + 192, fn));

	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, R, r1, r0);
	emit(Oadd, Kl, r1, rsave, getcon(64, fn));
	emit(Oadd, Kl, r0, ap, getcon(8, fn));

	r0 = newtmp("abi", Kl, fn);
	r1 = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, R, r1, r0);
	emit(Oadd, Kl, r1, rsave, getcon(192, fn));
	emit(Oaddr, Kl, rsave, SLOT(-1), R);
	emit(Oadd, Kl, r0, ap, getcon(16, fn));

	r0 = newtmp("abi", Kl, fn);
	emit(Ostorew, Kw, R, getcon((p.ngp-8)*8, fn), r0);
	emit(Oadd, Kl, r0, ap, getcon(24, fn));

	r0 = newtmp("abi", Kl, fn);
	emit(Ostorew, Kw, R, getcon((p.nfp-8)*16, fn), r0);
	emit(Oadd, Kl, r0, ap, getcon(28, fn));
}

void
arm64_abi(Fn *fn)
{
	Blk *b;
	Ins *i, *i0, *ip;
	QBE_ARM64_ABI_Insl *il;
	int n;
	QBE_ARM64_ABI_Params p;

	for (b=fn->start; b; b=b->link)
		b->visit = 0;

	/* lower parameters */
	for (b=fn->start, i=b->ins; i<&b->ins[b->nins]; i++)
		if (!ispar(i->op))
			break;
	p = qbe_arm64_abi_selpar(fn, b->ins, i);
	n = b->nins - (i - b->ins) + (&insb[NIns] - curi);
	i0 = alloc(n * sizeof(Ins));
	ip = icpy(ip = i0, curi, &insb[NIns] - curi);
	ip = icpy(ip, i, &b->ins[b->nins] - i);
	b->nins = n;
	b->ins = i0;

	/* lower calls, returns, and vararg instructions */
	il = 0;
	b = fn->start;
	do {
		if (!(b = b->link))
			b = fn->start; /* do it last */
		if (b->visit)
			continue;
		curi = &insb[NIns];
		qbe_arm64_abi_selret(b, fn);
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
				if (T.apple)
					qbe_arm64_abi_apple_selvastart(fn, p, i->arg[0]);
				else
					qbe_arm64_abi_arm64_selvastart(fn, p, i->arg[0]);
				break;
			case Ovaarg:
				if (T.apple)
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
		b->nins = &insb[NIns] - curi;
		idup(&b->ins, curi, b->nins);
	} while (b != fn->start);

	if (debug['A']) {
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
		curi = &insb[NIns];
		j = b->jmp.type;
		if (isretbh(j)) {
			r = newtmp("abi", Kw, fn);
			op = Oextsb + (j - Jretsb);
			emit(op, Kw, r, b->jmp.arg, R);
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
					curi->arg[0] = i->to;
				}
			}
			for (i=i1; i>i0;)
				if (isargbh((--i)->op)) {
					op = Oextsb + (i->op - Oargsb);
					emit(op, Kw, i->to, i->arg[0], R);
				}
		}
		b->nins = &insb[NIns] - curi;
		idup(&b->ins, curi, b->nins);
	}

	if (debug['A']) {
		fprintf(stderr, "\n> After Apple pre-ABI:\n");
		printfn(fn, stderr);
	}
}
/*** END FILE: arm64/abi.c ***/
/*** START FILE: arm64/emit.c ***/
/* skipping all.h */

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

static char *
qbe_arm64_emit_rname(int r, int k)
{
	static char buf[4];

	if (r == QBE_ARM64_SP) {
		LQ_ASSERT(k == Kl);
		sprintf(buf, "sp");
	}
	else if (QBE_ARM64_R0 <= r && r <= QBE_ARM64_LR)
		switch (k) {
		default: die("invalid class");
		case Kw: sprintf(buf, "w%d", r-QBE_ARM64_R0); break;
		case Kx:
		case Kl: sprintf(buf, "x%d", r-QBE_ARM64_R0); break;
		}
	else if (QBE_ARM64_V0 <= r && r <= QBE_ARM64_V30)
		switch (k) {
		default: die("invalid class");
		case Ks: sprintf(buf, "s%d", r-QBE_ARM64_V0); break;
		case Kx:
		case Kd: sprintf(buf, "d%d", r-QBE_ARM64_V0); break;
		}
	else
		die("invalid register");
	return buf;
}

static uint64_t
qbe_arm64_emit_slot(Ref r, QBE_ARM64_EMIT_E *e)
{
	int s;

	s = rsval(r);
	if (s == -1)
		return 16 + e->frame;
	if (s < 0) {
		if (e->fn->vararg && !T.apple)
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
			} else if ( !c) {
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
				fputs(qbe_arm64_emit_rname(QBE_ARM64_R18, k), e->f);
			else
				fputs(k==Ks ? "s31" : "d31", e->f);
			break;
		case '=':
		case '0':
			r = c == '=' ? i->to : i->arg[0];
			LQ_ASSERT(isreg(r));
			fputs(qbe_arm64_emit_rname(r.val, k), e->f);
			break;
		case '1':
			r = i->arg[1];
			switch (rtype(r)) {
			default:
				die("invalid second argument");
			case RTmp:
				LQ_ASSERT(isreg(r));
				fputs(qbe_arm64_emit_rname(r.val, k), e->f);
				break;
			case RCon:
				pc = &e->fn->con[r.val];
				n = pc->bits.i;
				LQ_ASSERT(pc->type == CBits);
				if (n >> 24) {
					LQ_ASSERT(arm64_logimm(n, k));
					fprintf(e->f, "#%"PRIu64, n);
				} else if (n & 0xfff000) {
					LQ_ASSERT(!(n & ~0xfff000ull));
					fprintf(e->f, "#%"PRIu64", lsl #12",
						n>>12);
				} else {
					LQ_ASSERT(!(n & ~0xfffull));
					fprintf(e->f, "#%"PRIu64, n);
				}
				break;
			}
			break;
		case 'M':
			c = *s++;
			LQ_ASSERT(c == '0' || c == '1' || c == '=');
			r = c == '=' ? i->to : i->arg[c - '0'];
			switch (rtype(r)) {
			default:
				die("todo (arm emit): unhandled ref");
			case RTmp:
				LQ_ASSERT(isreg(r));
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
		if (T.apple)
			s = "\tadrp\tR, S@pageO\n"
			    "\tadd\tR, R, S@pageoffO\n";
		else
			s = "\tadrp\tR, SO\n"
			    "\tadd\tR, R, #:lo12:SO\n";
		break;
	case SThr:
		if (T.apple)
			s = "\tadrp\tR, S@tlvppage\n"
			    "\tldr\tR, [R, S@tlvppageoff]\n";
		else
			s = "\tmrs\tR, tpidr_el0\n"
			    "\tadd\tR, R, #:tprel_hi12:SO, lsl #12\n"
			    "\tadd\tR, R, #:tprel_lo12_nc:SO\n";
		break;
	}

	l = str(c->sym.id);
	p = l[0] == '"' ? "" : T.assym;
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
	LQ_ASSERT(c->type == CBits);
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

static void
qbe_arm64_emit_fixarg(Ref *pr, int sz, QBE_ARM64_EMIT_E *e)
{
	Ins *i;
	Ref r;
	uint64_t s;

	r = *pr;
	if (rtype(r) == RSlot) {
		s = qbe_arm64_emit_slot(r, e);
		if (s > sz * 4095u) {
			i = &(Ins){Oaddr, Kl, TMP(QBE_ARM64_IP0), {r}};
			qbe_arm64_emit_emitins(i, e);
			*pr = TMP(QBE_ARM64_IP0);
		}
	}
}

static void
qbe_arm64_emit_emitins(Ins *i, QBE_ARM64_EMIT_E *e)
{
	char *l, *p, *rn;
	uint64_t s;
	int o;
	Ref r;
	Con *c;

	switch (i->op) {
	default:
		if (isload(i->op))
			qbe_arm64_emit_fixarg(&i->arg[0], loadsz(i), e);
		if (isstore(i->op))
			qbe_arm64_emit_fixarg(&i->arg[1], storesz(i), e);
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
				i->to = TMP(QBE_ARM64_R18);
				qbe_arm64_emit_emitins(i, e);
				i->arg[0] = i->to;
			}
			i->op = Ostorew + i->cls;
			i->cls = Kw;
			i->arg[1] = r;
			qbe_arm64_emit_emitins(i, e);
			break;
		}
		LQ_ASSERT(isreg(i->to));
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
			LQ_ASSERT(i->to.val != QBE_ARM64_R18);
			goto Table;
		}
		break;
	case Oaddr:
		LQ_ASSERT(rtype(i->arg[0]) == RSlot);
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
		p = l[0] == '"' ? "" : T.assym;
		fprintf(e->f, "\tbl\t%s%s\n", p, l);
		break;
	case Osalloc:
		qbe_arm64_emit_emitf("sub sp, sp, %0", i, e);
		if (!req(i->to, R))
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
	static int id0;
	int s, n, c, lbl, *r;
	uint64_t o;
	Blk *b, *t;
	Ins *i;
	QBE_ARM64_EMIT_E *e;

	e = &(QBE_ARM64_EMIT_E){.f = out, .fn = fn};
	if (T.apple)
		e->fn->lnk.align = 4;
	emitfnlnk(e->fn->name, &e->fn->lnk, e->f);
	fputs("\thint\t#34\n", e->f);
	qbe_arm64_emit_framelayout(e);

	if (e->fn->vararg && !T.apple) {
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
			fprintf(e->f, "%s%d:\n", T.asloc, id0+b->id);
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
			if (e->fn->vararg && !T.apple)
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
		Jmp:
			if (b->s1 != b->link)
				fprintf(e->f,
					"\tb\t%s%d\n",
					T.asloc, id0+b->s1->id
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
			} else
				c = cmpneg(c);
			fprintf(e->f,
				"\tb%s\t%s%d\n",
				ctoa[c], T.asloc, id0+b->s2->id
			);
			goto Jmp;
		}
	}
	id0 += e->fn->nblk;
	if (!T.apple)
		elf_emitfnfin(fn->name, out);
}
#undef CMP
/*** END FILE: arm64/emit.c ***/
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
		if (T.apple
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
			emit(Ocopy, Kl, r1, TMP(QBE_ARM64_R0), R);
			r1 = newtmp("isel", Kl, fn);
			r2 = newtmp("isel", Kl, fn);
			emit(Ocall, 0, R, r1, CALL(33));
			emit(Ocopy, Kl, TMP(QBE_ARM64_R0), r2, R);
			emit(Oload, Kl, r1, r2, R);
			cc = *c;
			cc.bits.i = 0;
			r3 = newcon(&cc, fn);
			emit(Ocopy, Kl, r2, r3, R);
			break;
		}
		if (KBASE(k) == 0 && phi)
			return;
		r1 = newtmp("isel", k, fn);
		if (KBASE(k) == 0) {
			emit(Ocopy, k, r1, r0, R);
		} else {
			n = stashbits(&c->bits, KWIDE(k) ? 8 : 4);
			vgrow(&fn->con, ++fn->ncon);
			c = &fn->con[fn->ncon-1];
			sprintf(buf, "\"%sfp%d\"", T.asloc, n);
			*c = (Con){.type = CAddr};
			c->sym.id = intern(buf);
			r2 = newtmp("isel", Kl, fn);
			emit(Oload, k, r1, r2, R);
			emit(Ocopy, Kl, r2, CON(c-fn->con), R);
		}
		*pr = r1;
		break;
	case RTmp:
		s = fn->tmp[r0.val].slot;
		if (s == -1)
			break;
		r1 = newtmp("isel", Kl, fn);
		emit(Oaddr, Kl, r1, SLOT(s), R);
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
		emit(Oafcmp, k, R, arg[0], arg[1]);
		iarg = curi->arg;
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
	emit(cmp, k, R, arg[0], r);
	iarg = curi->arg;
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
		i0 = curi - 1;
		salloc(i.to, i.arg[0], fn);
		qbe_arm64_isel_fixarg(&i0->arg[0], Kl, 0, fn);
		return;
	}
	if (iscmp(i.op, &ck, &cc)) {
		emit(Oflag, i.cls, i.to, R, R);
		i0 = curi;
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
		iarg = curi->arg; /* qbe_arm64_isel_fixarg() can change curi */
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
	LQ_ASSERT(b->jmp.type == Jjnz);
	r = b->jmp.arg;
	use = -1;
	b->jmp.arg = R;
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
		curi = &insb[NIns];
		for (sb=(Blk*[3]){b->s1, b->s2, 0}; *sb; sb++)
			for (p=(*sb)->phi; p; p=p->link) {
				for (n=0; p->blk[n] != b; n++)
					LQ_ASSERT(n+1 < p->narg);
				qbe_arm64_isel_fixarg(&p->arg[n], p->cls, 1, fn);
			}
		qbe_arm64_isel_seljmp(b, fn);
		for (i=&b->ins[b->nins]; i!=b->ins;)
			qbe_arm64_isel_sel(*--i, fn);
		b->nins = &insb[NIns] - curi;
		idup(&b->ins, curi, b->nins);
	}

	if (debug['I']) {
		fprintf(stderr, "\n> After instruction selection:\n");
		printfn(fn, stderr);
	}
}
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

#define RGLOB (BIT(QBE_ARM64_FP) | BIT(QBE_ARM64_SP) | BIT(QBE_ARM64_R18))

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
	.nrglob = 3, \
	.rsave = arm64_rsave, \
	.nrsave = {QBE_ARM64_NGPS, QBE_ARM64_NFPS}, \
	.retregs = arm64_retregs, \
	.argregs = arm64_argregs, \
	.memargs = arm64_memargs, \
	.isel = arm64_isel, \
	.abi1 = arm64_abi, \
	.emitfn = arm64_emitfn, \

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

	LQ_ASSERT(rtype(r) == RCall);
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

	LQ_ASSERT(rtype(r) == RCall);
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
			if (qbe_rv64_abi_fpstruct(&typ[f->len], off, c) == -1)
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

	LQ_ASSERT(ntmp > 0);
	LQ_ASSERT(ntmp <= 2);
	for (i=0; i<ntmp; i++) {
		tmp[i] = newtmp("abi", c->cls[i], fn);
		r = newtmp("abi", Kl, fn);
		emit(st[c->cls[i]], 0, R, tmp[i], r);
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
		emit(Oload, c->cls[i], TMP(c->reg[i]), r, R);
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
		qbe_rv64_abi_typclass(&cr, &typ[fn->retty], 1, qbe_rv64_abi_gpreg, qbe_rv64_abi_fpreg);
		if (cr.class & QBE_RV64_ABI_Cptr) {
			LQ_ASSERT(rtype(fn->retr) == RTmp);
			emit(Oblit1, 0, R, INT(cr.type->size), R);
			emit(Oblit0, 0, R, r, fn->retr);
			cty = 0;
		} else {
			qbe_rv64_abi_ldregs(&cr, r, fn);
			cty = (cr.nfp << 2) | cr.ngp;
		}
	} else {
		k = j - Jretw;
		if (KBASE(k) == 0) {
			emit(Ocopy, k, TMP(QBE_RV64_A0), r, R);
			cty = 1;
		} else {
			emit(Ocopy, k, TMP(QBE_RV64_FA0), r, R);
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
			t = &typ[i->arg[0].val];
			qbe_rv64_abi_typclass(c, t, 1, gp, fp);
			if (c->nfp > 0)
			if (c->nfp >= nfp || c->ngp >= ngp)
				qbe_rv64_abi_typclass(c, t, 0, gp, fp);
			LQ_ASSERT(c->nfp <= nfp);
			if (c->ngp <= ngp) {
				ngp -= c->ngp;
				nfp -= c->nfp;
				gp += c->ngp;
				fp += c->nfp;
			} else if (ngp > 0) {
				LQ_ASSERT(c->ngp == 2);
				LQ_ASSERT(c->class == 0);
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

	if (!req(i1->arg[1], R))
		qbe_rv64_abi_typclass(&cr, &typ[i1->arg[1].val], 1, qbe_rv64_abi_gpreg, qbe_rv64_abi_fpreg);

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
		emit(Osalloc, Kl, R, getcon(-stk, fn), R);

	if (!req(i1->arg[1], R)) {
		qbe_rv64_abi_stkblob(i1->to, cr.type, fn, ilp);
		cty |= (cr.nfp << 2) | cr.ngp;
		if (cr.class & QBE_RV64_ABI_Cptr)
			/* spill & rega expect calls to be
			 * followed by copies from regs,
			 * so we emit a dummy
			 */
			emit(Ocopy, Kw, R, TMP(QBE_RV64_A0), R);
		else {
			qbe_rv64_abi_sttmps(tmp, cr.nreg, &cr, i1->to, fn);
			for (j=0; j<cr.nreg; j++) {
				r = TMP(cr.reg[j]);
				emit(Ocopy, cr.cls[j], tmp[j], r, R);
			}
		}
	} else if (KBASE(i1->cls) == 0) {
		emit(Ocopy, i1->cls, i1->to, TMP(QBE_RV64_A0), R);
		cty |= 1;
	} else {
		emit(Ocopy, i1->cls, i1->to, TMP(QBE_RV64_FA0), R);
		cty |= 1 << 2;
	}

	emit(Ocall, 0, R, i1->arg[0], CALL(cty));

	if (cr.class & QBE_RV64_ABI_Cptr)
		/* struct return argument */
		emit(Ocopy, Kl, TMP(QBE_RV64_A0), i1->to, R);

	/* move arguments into registers */
	for (i=i0, c=ca; i<i1; i++, c++) {
		if (i->op == Oargv || c->class & QBE_RV64_ABI_Cstk1)
			continue;
		if (i->op == Oargc) {
			qbe_rv64_abi_ldregs(c, i->arg[1], fn);
		} else if (c->class & QBE_RV64_ABI_Cfpint) {
			k = KWIDE(*c->cls) ? Kl : Kw;
			r = newtmp("abi", k, fn);
			emit(Ocopy, k, TMP(*c->reg), r, R);
			*c->reg = r.val;
		} else {
			emit(Ocopy, *c->cls, TMP(*c->reg), i->arg[0], R);
		}
	}

	for (i=i0, c=ca; i<i1; i++, c++) {
		if (c->class & QBE_RV64_ABI_Cfpint) {
			k = KWIDE(*c->cls) ? Kl : Kw;
			emit(Ocast, k, TMP(*c->reg), i->arg[0], R);
		}
		if (c->class & QBE_RV64_ABI_Cptr) {
			emit(Oblit1, 0, R, INT(c->type->size), R);
			emit(Oblit0, 0, R, i->arg[1], i->arg[0]);
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
			emit(Ostorew+i->cls, Kw, R, i->arg[0], r1);
			if (i->cls == Kw) {
				/* TODO: we only need this sign
				 * extension for l temps passed
				 * as w arguments
				 * (see rv64/isel.c:fixarg)
				 */
				curi->op = Ostorel;
				curi->arg[0] = newtmp("abi", Kl, fn);
				emit(Oextsw, Kl, curi->arg[0], i->arg[0], R);
			}
			emit(Oadd, Kl, r1, r, getcon(off, fn));
			off += 8;
		}
		if (i->op == Oargc) {
			if (c->class & QBE_RV64_ABI_Cstk1) {
				r1 = newtmp("abi", Kl, fn);
				r2 = newtmp("abi", Kl, fn);
				emit(Ostorel, 0, R, r2, r1);
				emit(Oadd, Kl, r1, r, getcon(off, fn));
				emit(Oload, Kl, r2, i->arg[1], R);
				off += 8;
			}
			if (c->class & QBE_RV64_ABI_Cstk2) {
				r1 = newtmp("abi", Kl, fn);
				r2 = newtmp("abi", Kl, fn);
				emit(Ostorel, 0, R, r2, r1);
				emit(Oadd, Kl, r1, r, getcon(off, fn));
				r1 = newtmp("abi", Kl, fn);
				emit(Oload, Kl, r2, r1, R);
				emit(Oadd, Kl, r1, i->arg[1], getcon(8, fn));
				off += 8;
			}
		}
	}
	emit(Osalloc, Kl, r, getcon(stk, fn), R);
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
	curi = &insb[NIns];

	if (fn->retty >= 0) {
		qbe_rv64_abi_typclass(&cr, &typ[fn->retty], 1, qbe_rv64_abi_gpreg, qbe_rv64_abi_fpreg);
		if (cr.class & QBE_RV64_ABI_Cptr) {
			fn->retr = newtmp("abi", Kl, fn);
			emit(Ocopy, Kl, fn->retr, TMP(QBE_RV64_A0), R);
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
			emit(Ocast, k, r, i->to, R);
		}
		if (i->op == Oparc)
		if (!(c->class & QBE_RV64_ABI_Cptr))
		if (c->nreg != 0) {
			nt = c->nreg;
			if (c->class & QBE_RV64_ABI_Cstk2) {
				c->cls[1] = Kl;
				c->off[1] = 8;
				LQ_ASSERT(nt == 1);
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
				emit(Ocopy, c->cls[j], *t++, r, R);
			}
			if (c->class & QBE_RV64_ABI_Cstk2) {
				emit(Oload, Kl, *t, SLOT(-s), R);
				t++, s++;
			}
		} else if (c->class & QBE_RV64_ABI_Cstk1) {
			emit(Oload, *c->cls, i->to, SLOT(-s), R);
			s++;
		} else {
			emit(Ocopy, *c->cls, i->to, TMP(*c->reg), R);
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
	emit(Ostorel, Kw, R, newloc, i->arg[0]);
	emit(Oadd, Kl, newloc, loc, getcon(8, fn));
	emit(Oload, i->cls, i->to, loc, R);
	emit(Oload, Kl, loc, i->arg[0], R);
}

static void
qbe_rv64_abi_selvastart(Fn *fn, QBE_RV64_ABI_Params p, Ref ap)
{
	Ref rsave;
	int s;

	rsave = newtmp("abi", Kl, fn);
	emit(Ostorel, Kw, R, rsave, ap);
	s = p.stk > 2 + 8 * fn->vararg ? p.stk : 2 + p.ngp;
	emit(Oaddr, Kl, rsave, SLOT(-s), R);
}

void
rv64_abi(Fn *fn)
{
	Blk *b;
	Ins *i, *i0, *ip;
	QBE_RV64_ABI_Insl *il;
	int n;
	QBE_RV64_ABI_Params p;

	for (b=fn->start; b; b=b->link)
		b->visit = 0;

	/* lower parameters */
	for (b=fn->start, i=b->ins; i<&b->ins[b->nins]; i++)
		if (!ispar(i->op))
			break;
	p = qbe_rv64_abi_selpar(fn, b->ins, i);
	n = b->nins - (i - b->ins) + (&insb[NIns] - curi);
	i0 = alloc(n * sizeof(Ins));
	ip = icpy(ip = i0, curi, &insb[NIns] - curi);
	ip = icpy(ip, i, &b->ins[b->nins] - i);
	b->nins = n;
	b->ins = i0;

	/* lower calls, returns, and vararg instructions */
	il = 0;
	b = fn->start;
	do {
		if (!(b = b->link))
			b = fn->start; /* do it last */
		if (b->visit)
			continue;
		curi = &insb[NIns];
		qbe_rv64_abi_selret(b, fn);
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
		b->nins = &insb[NIns] - curi;
		idup(&b->ins, curi, b->nins);
	} while (b != fn->start);

	if (debug['A']) {
		fprintf(stderr, "\n> After ABI lowering:\n");
		printfn(fn, stderr);
	}
}
/*** END FILE: rv64/abi.c ***/
/*** START FILE: rv64/emit.c ***/
/* skipping all.h */

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
	LQ_ASSERT(s <= fn->slot);
	if (s < 0)
		return 8 * -s;
	else
		return -4 * (fn->slot - s);
}

static void
qbe_rv64_emit_emitaddr(Con *c, FILE *f)
{
	LQ_ASSERT(c->sym.type == SGlo);
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
			LQ_ASSERT(isreg(r));
			fputs(qbe_rv64_emit_rname[r.val], f);
			break;
		case '1':
			r = i->arg[1];
			switch (rtype(r)) {
			default:
				die("invalid second argument");
			case RTmp:
				LQ_ASSERT(isreg(r));
				fputs(qbe_rv64_emit_rname[r.val], f);
				break;
			case RCon:
				pc = &fn->con[r.val];
				LQ_ASSERT(pc->type == CBits);
				LQ_ASSERT(pc->bits.i >= -2048 && pc->bits.i < 2048);
				fprintf(f, "%d", (int)pc->bits.i);
				break;
			}
			break;
		case 'M':
			c = *s++;
			LQ_ASSERT(c == '0' || c == '1');
			r = i->arg[c - '0'];
			switch (rtype(r)) {
			default:
				die("invalid address argument");
			case RTmp:
				fprintf(f, "0(%s)", qbe_rv64_emit_rname[r.val]);
				break;
			case RCon:
				pc = &fn->con[r.val];
				LQ_ASSERT(pc->type == CAddr);
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
				LQ_ASSERT(offset >= -2048 && offset <= 2047);
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
			sprintf(off, "+%"PRIi64, c->bits.i);
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
				LQ_ASSERT(isreg(i->arg[0]));
				i->arg[1] = i->to;
				i->to = R;
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
		LQ_ASSERT(isreg(i->to));
		switch (rtype(i->arg[0])) {
		case RCon:
			qbe_rv64_emit_loadcon(&fn->con[i->arg[0].val], i->to.val, i->cls, f);
			break;
		case RSlot:
			i->op = Oload;
			qbe_rv64_emit_fixmem(&i->arg[0], fn, f);
			goto Table;
		default:
			LQ_ASSERT(isreg(i->arg[0]));
			goto Table;
		}
		break;
	case Onop:
		break;
	case Oaddr:
		LQ_ASSERT(rtype(i->arg[0]) == RSlot);
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
		if (!req(i->to, R))
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
	static int id0;
	int lbl, neg, off, frame, *pr, r;
	Blk *b, *s;
	Ins *i;

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
			fprintf(f, ".L%d:\n", id0+b->id);
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
		Jmp:
			if (b->s1 != b->link)
				fprintf(f, "\tj .L%d\n", id0+b->s1->id);
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
			LQ_ASSERT(isreg(b->jmp.arg));
			fprintf(f,
				"\tb%sz %s, .L%d\n",
				neg ? "ne" : "eq",
				qbe_rv64_emit_rname[b->jmp.arg.val],
				id0+b->s2->id
			);
			goto Jmp;
		}
	}
	id0 += fn->nblk;
	elf_emitfnfin(fn->name, f);
}
#undef CMP
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
		if (c->type == CBits && qbe_rv64_isel_immarg(r, op, i))
		if (-2048 <= c->bits.i && c->bits.i < 2048)
			break;
		r1 = newtmp("isel", k, fn);
		if (KBASE(k) == 1) {
			/* load floating points from memory
			 * slots, they can't be used as
			 * immediates
			 */
			LQ_ASSERT(c->type == CBits);
			n = stashbits(&c->bits, KWIDE(k) ? 8 : 4);
			vgrow(&fn->con, ++fn->ncon);
			c = &fn->con[fn->ncon-1];
			sprintf(buf, "\"%sfp%d\"", T.asloc, n);
			*c = (Con){.type = CAddr};
			c->sym.id = intern(buf);
			emit(Oload, k, r1, CON(c-fn->con), R);
			break;
		}
		emit(Ocopy, k, r1, r0, R);
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
			emit(Oaddr, k, r1, SLOT(s), R);
			break;
		}
		if (k == Kw && fn->tmp[r0.val].cls == Kl) {
			/* TODO: this sign extension isn't needed
			 * for 32-bit arithmetic instructions
			 */
			r1 = newtmp("isel", k, fn);
			emit(Oextsw, Kl, r1, r0, R);
		} else {
			LQ_ASSERT(k == fn->tmp[r0.val].cls);
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
		emit(Oreqz, i.cls, i.to, r, R);
		emit(Oxor, k, r, i.arg[0], i.arg[1]);
		icmp = curi;
		qbe_rv64_isel_fixarg(&icmp->arg[0], k, icmp, fn);
		qbe_rv64_isel_fixarg(&icmp->arg[1], k, icmp, fn);
		return;
	case Cine:
		r = newtmp("isel", k, fn);
		emit(Ornez, i.cls, i.to, r, R);
		emit(Oxor, k, r, i.arg[0], i.arg[1]);
		icmp = curi;
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
		icmp = curi;
		qbe_rv64_isel_fixarg(&icmp->arg[0], k, icmp, fn);
		qbe_rv64_isel_fixarg(&icmp->arg[1], k, icmp, fn);
		emit(op, i.cls, r1, i.arg[1], i.arg[1]);
		icmp = curi;
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
	icmp = curi;
	qbe_rv64_isel_fixarg(&icmp->arg[0], k, icmp, fn);
	qbe_rv64_isel_fixarg(&icmp->arg[1], k, icmp, fn);
}

static void
qbe_rv64_isel_sel(Ins i, Fn *fn)
{
	Ins *i0;
	int ck, cc;

	if (INRANGE(i.op, Oalloc, Oalloc1)) {
		i0 = curi - 1;
		salloc(i.to, i.arg[0], fn);
		qbe_rv64_isel_fixarg(&i0->arg[0], Kl, i0, fn);
		return;
	}
	if (iscmp(i.op, &ck, &cc)) {
		qbe_rv64_isel_selcmp(i, ck, cc, fn);
		return;
	}
	if (i.op != Onop) {
		emiti(i);
		i0 = curi; /* qbe_rv64_isel_fixarg() can change curi */
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
		curi = &insb[NIns];
		for (sb=(Blk*[3]){b->s1, b->s2, 0}; *sb; sb++)
			for (p=(*sb)->phi; p; p=p->link) {
				for (n=0; p->blk[n] != b; n++)
					LQ_ASSERT(n+1 < p->narg);
				qbe_rv64_isel_fixarg(&p->arg[n], p->cls, 0, fn);
			}
		qbe_rv64_isel_seljmp(b, fn);
		for (i=&b->ins[b->nins]; i!=b->ins;)
			qbe_rv64_isel_sel(*--i, fn);
		b->nins = &insb[NIns] - curi;
		idup(&b->ins, curi, b->nins);
	}

	if (debug['I']) {
		fprintf(stderr, "\n> After instruction selection:\n");
		printfn(fn, stderr);
	}
}
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

#ifndef P
	#define P(CanFold, HasId, IdVal)
#endif


#define T(a,b,c,d,e,f,g,h) {                          \
	{[Kw]=K##a, [Kl]=K##b, [Ks]=K##c, [Kd]=K##d}, \
	{[Kw]=K##e, [Kl]=K##f, [Ks]=K##g, [Kd]=K##h}  \
}


/*********************/
/* PUBLIC OPERATIONS */
/*********************/

/* Arithmetic and Bits */
O(add,     T(w,l,s,d, w,l,s,d), P(1,1,0)) X(2,1,0) V(1)
O(sub,     T(w,l,s,d, w,l,s,d), P(1,1,0)) X(2,1,0) V(0)
O(neg,     T(w,l,s,d, x,x,x,x), P(1,0,0)) X(1,1,0) V(0)
O(div,     T(w,l,s,d, w,l,s,d), P(1,1,1)) X(0,0,0) V(0)
O(rem,     T(w,l,e,e, w,l,e,e), P(1,0,0)) X(0,0,0) V(0)
O(udiv,    T(w,l,e,e, w,l,e,e), P(1,1,1)) X(0,0,0) V(0)
O(urem,    T(w,l,e,e, w,l,e,e), P(1,0,0)) X(0,0,0) V(0)
O(mul,     T(w,l,s,d, w,l,s,d), P(1,1,1)) X(2,0,0) V(0)
O(and,     T(w,l,e,e, w,l,e,e), P(1,0,0)) X(2,1,0) V(1)
O(or,      T(w,l,e,e, w,l,e,e), P(1,1,0)) X(2,1,0) V(1)
O(xor,     T(w,l,e,e, w,l,e,e), P(1,1,0)) X(2,1,0) V(1)
O(sar,     T(w,l,e,e, w,w,e,e), P(1,1,0)) X(1,1,0) V(1)
O(shr,     T(w,l,e,e, w,w,e,e), P(1,1,0)) X(1,1,0) V(1)
O(shl,     T(w,l,e,e, w,w,e,e), P(1,1,0)) X(1,1,0) V(1)

/* Comparisons */
O(ceqw,    T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cnew,    T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csgew,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csgtw,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cslew,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csltw,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(1)
O(cugew,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cugtw,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(culew,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cultw,   T(w,w,e,e, w,w,e,e), P(1,0,0)) X(0,1,0) V(1)

O(ceql,    T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cnel,    T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csgel,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csgtl,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cslel,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(csltl,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(1)
O(cugel,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cugtl,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(culel,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cultl,   T(l,l,e,e, l,l,e,e), P(1,0,0)) X(0,1,0) V(1)

O(ceqs,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cges,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cgts,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cles,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(clts,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cnes,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cos,     T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cuos,    T(s,s,e,e, s,s,e,e), P(1,0,0)) X(0,1,0) V(0)

O(ceqd,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cged,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cgtd,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cled,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cltd,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cned,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cod,     T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)
O(cuod,    T(d,d,e,e, d,d,e,e), P(1,0,0)) X(0,1,0) V(0)

/* Memory */
O(storeb,  T(w,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(storeh,  T(w,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(storew,  T(w,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(storel,  T(l,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(stores,  T(s,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)
O(stored,  T(d,e,e,e, m,e,e,e), P(0,0,0)) X(0,0,1) V(0)

O(loadsb,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loadub,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loadsh,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loaduh,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loadsw,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(loaduw,  T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(load,    T(m,m,m,m, x,x,x,x), P(0,0,0)) X(0,0,1) V(0)

/* Extensions and Truncations */
O(extsb,   T(w,w,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extub,   T(w,w,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extsh,   T(w,w,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extuh,   T(w,w,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extsw,   T(e,w,e,e, e,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(extuw,   T(e,w,e,e, e,x,e,e), P(1,0,0)) X(0,0,1) V(0)

O(exts,    T(e,e,e,s, e,e,e,x), P(1,0,0)) X(0,0,1) V(0)
O(truncd,  T(e,e,d,e, e,e,x,e), P(1,0,0)) X(0,0,1) V(0)
O(stosi,   T(s,s,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(stoui,   T(s,s,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(dtosi,   T(d,d,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(dtoui,   T(d,d,e,e, x,x,e,e), P(1,0,0)) X(0,0,1) V(0)
O(swtof,   T(e,e,w,w, e,e,x,x), P(1,0,0)) X(0,0,1) V(0)
O(uwtof,   T(e,e,w,w, e,e,x,x), P(1,0,0)) X(0,0,1) V(0)
O(sltof,   T(e,e,l,l, e,e,x,x), P(1,0,0)) X(0,0,1) V(0)
O(ultof,   T(e,e,l,l, e,e,x,x), P(1,0,0)) X(0,0,1) V(0)
O(cast,    T(s,d,w,l, x,x,x,x), P(1,0,0)) X(0,0,1) V(0)

/* Stack Allocation */
O(alloc4,  T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(alloc8,  T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(alloc16, T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)

/* Variadic Function Helpers */
O(vaarg,   T(m,m,m,m, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(vastart, T(m,e,e,e, x,e,e,e), P(0,0,0)) X(0,0,0) V(0)

O(copy,    T(w,l,s,d, x,x,x,x), P(0,0,0)) X(0,0,1) V(0)

/* Debug */
O(dbgloc,  T(w,e,e,e, w,e,e,e), P(0,0,0)) X(0,0,1) V(0)

/****************************************/
/* INTERNAL OPERATIONS (keep nop first) */
/****************************************/

/* Miscellaneous and Architecture-Specific Operations */
O(nop,     T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,1) V(0)
O(addr,    T(m,m,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(blit0,   T(m,e,e,e, m,e,e,e), P(0,0,0)) X(0,1,0) V(0)
O(blit1,   T(w,e,e,e, x,e,e,e), P(0,0,0)) X(0,1,0) V(0)
O(swap,    T(w,l,s,d, w,l,s,d), P(0,0,0)) X(1,0,0) V(0)
O(sign,    T(w,l,e,e, x,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(salloc,  T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(xidiv,   T(w,l,e,e, x,x,e,e), P(0,0,0)) X(1,0,0) V(0)
O(xdiv,    T(w,l,e,e, x,x,e,e), P(0,0,0)) X(1,0,0) V(0)
O(xcmp,    T(w,l,s,d, w,l,s,d), P(0,0,0)) X(1,1,0) V(0)
O(xtest,   T(w,l,e,e, w,l,e,e), P(0,0,0)) X(1,1,0) V(0)
O(acmp,    T(w,l,e,e, w,l,e,e), P(0,0,0)) X(0,0,0) V(0)
O(acmn,    T(w,l,e,e, w,l,e,e), P(0,0,0)) X(0,0,0) V(0)
O(afcmp,   T(e,e,s,d, e,e,s,d), P(0,0,0)) X(0,0,0) V(0)
O(reqz,    T(w,l,e,e, x,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(rnez,    T(w,l,e,e, x,x,e,e), P(0,0,0)) X(0,0,0) V(0)

/* Arguments, Parameters, and Calls */
O(par,     T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(parsb,   T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(parub,   T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(parsh,   T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(paruh,   T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(parc,    T(e,x,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(pare,    T(e,x,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(arg,     T(w,l,s,d, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(argsb,   T(w,e,e,e, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(argub,   T(w,e,e,e, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(argsh,   T(w,e,e,e, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(arguh,   T(w,e,e,e, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(argc,    T(e,x,e,e, e,l,e,e), P(0,0,0)) X(0,0,0) V(0)
O(arge,    T(e,l,e,e, e,x,e,e), P(0,0,0)) X(0,0,0) V(0)
O(argv,    T(x,x,x,x, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)
O(call,    T(m,m,m,m, x,x,x,x), P(0,0,0)) X(0,0,0) V(0)

/* Flags Setting */
O(flagieq,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagine,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagisge, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagisgt, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagisle, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagislt, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagiuge, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagiugt, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagiule, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagiult, T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfeq,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfge,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfgt,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfle,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagflt,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfne,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfo,   T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)
O(flagfuo,  T(x,x,e,e, x,x,e,e), P(0,0,0)) X(0,0,1) V(0)


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
};

MAKESURE(rsave_size_ok, sizeof rv64_rsave == (QBE_RV64_NGPS+QBE_RV64_NFPS+1) * sizeof(int));
MAKESURE(rclob_size_ok, sizeof rv64_rclob == (QBE_RV64_NCLR+1) * sizeof(int));
/*** END FILE: rv64/targ.c ***/
/*** START FILE: ../libqbe_impl.c ***/
#include "libqbe.h"

static PState _ps;

// Blk lifetimes are per-func
// This array is ~2M, perhaps lq_init arg to size this?
static Blk _block_arena[8<<10];
static int _num_blocks;

// Lnk lifetimes are lq_init() scoped
static Lnk _linkage_arena[1<<10];
static int _num_linkages;

typedef enum LqInitStatus {
  LQIS_UNINITIALIZED = 0,
  LQIS_INITIALIZED_EMIT_FIN = 1,
  LQIS_INITIALIZED_NO_FIN = 2,
} LqInitStatus;
static LqInitStatus lq_initialized;

static uint _ntyp;
static Typ* _curty;
static int _curty_build_n;
static uint64_t _curty_build_sz;
static int _curty_build_al;

static int _dbg_name_counter;

static Dat _curd;
static Lnk _curd_lnk;

static void _gen_dbg_name(char* into, const  char* prefix) {
  sprintf(into, "%s_%d", prefix ? prefix : "", _dbg_name_counter++);
}

#define LQ_NAMED_IF_DEBUG(into, provided) \
  if (dbg) {                              \
    _gen_dbg_name(into, provided);        \
  }

#define LQ_COUNTOF(a) (sizeof(a)/sizeof(a[0]))
#define LQ_COUNTOFI(a) ((int)(sizeof(a)/sizeof(a[0])))

#ifdef _MSC_VER
#define alloca _alloca
#endif

static void err(char* s, ...) {
  va_list args;
  va_start(args, s);
  fprintf(stderr, "libqbe: ");
  vfprintf(stderr, s, args);
  fprintf(stderr, "\n");
  va_end(args);
  // TODO: setjmp/longjmp w/ clean up
  exit(1);
}

LqLinkage lq_linkage_create(int alignment,
                            bool exported,
                            bool tls,
                            bool common,
                            const char* section_name,
                            const char* section_flags) {
  LQ_ASSERT(_num_linkages < LQ_COUNTOFI(_linkage_arena));
  LqLinkage ret = {_num_linkages++};
  _linkage_arena[ret.u] = (Lnk){
      .export = exported,
      .thread = tls,
      .common = common,
      .align = alignment,
      .sec = (char*)section_name,
      .secf = (char*)section_flags,
  };
  return ret;
}

static Lnk _linkage_to_internal_lnk(LqLinkage linkage) {
  return _linkage_arena[linkage.u];
}

MAKESURE(ref_sizes_match, sizeof(LqRef) == sizeof(Ref));
MAKESURE(ref_has_expected_size, sizeof(uint32_t) == sizeof(Ref));

static Ref _lqref_to_internal_ref(LqRef ref) {
  Ref ret;
  memcpy(&ret, &ref, sizeof(Ref));
  return ret;
}

static LqRef _internal_ref_to_lqref(Ref ref) {
  LqRef ret;
  memcpy(&ret, &ref, sizeof(Ref));
  return ret;
}

static int _lqtype_to_cls_and_ty(LqType type, int* ty) {
  LQ_ASSERT(type.u != LQ_TYPE_C);
  if (type.u > LQ_TYPE_0) {
    *ty = type.u;
    return LQ_TYPE_C;
  } else {
    *ty = Kx;
    return type.u;
  }
}

static Blk* _lqblock_to_internal_blk(LqBlock block) {
  return &_block_arena[block.u];
}

void lq_init(LqTarget target, FILE* output, const char* debug_names) {
  LQ_ASSERT(lq_initialized == LQIS_UNINITIALIZED);

  _dbg_name_counter = 0;

  _num_linkages = 0;
  // These have to match the header for lq_linkage_default/export.
  LqLinkage def = lq_linkage_create(8, false, false, false, NULL, NULL);
  LQ_ASSERT(def.u == 0); (void)def;
  LqLinkage exp = lq_linkage_create(8, true, false, false, NULL, NULL);
  LQ_ASSERT(exp.u == 1); (void)exp;

  (void)qbe_main_dbgfile;  // TODO
  (void)amd64_winabi_rclob;

  switch (target) {
    case LQ_TARGET_AMD64_APPLE:
      T = T_amd64_apple;
      break;
    case LQ_TARGET_AMD64_SYSV:
      T = T_amd64_sysv;
      break;
    case LQ_TARGET_AMD64_WINDOWS:
      T = T_amd64_win;
      break;
    case LQ_TARGET_ARM64:
      T = T_arm64;
      break;
    case LQ_TARGET_ARM64_APPLE:
      T = T_arm64_apple;
      break;
    case LQ_TARGET_RV64:
      T = T_rv64;
      break;
    default: {
#if defined(__APPLE__) && defined(__MACH__)
#  if defined(__aarch64__)
      T = T_arm64_apple;
#  else
      T = T_amd64_apple;
#  endif
#elif defined(_WIN32)
#  if defined(__aarch64__)
#    error port win arm64
#  else
      T = T_amd64_win;
#  endif
#else
#  if defined(__aarch64__)
      T = T_arm64;
#  elif defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)
      T = T_amd64_sysv;
#  elif defined(__riscv)
      T = T_rv64;
#  else
#    error port unknown
#  endif
#endif
    }
  }

  outf = output;

  memset(debug, 0, sizeof(debug));
  for (const char* d = debug_names; *d; ++d) {
    debug[(int)*d] = 1;
  }

  _ntyp = 0;
  typ = vnew(0, sizeof(typ[0]), PHeap);
  // Reserve the ids of the basic types so the .u values can be used as TypeKinds.
  vgrow(&typ, _ntyp + LQ_TYPE_0 + 1);
  _ntyp += LQ_TYPE_0 + 1;

  dbg = debug_names[0] != 0;
  lq_initialized = dbg ? LQIS_INITIALIZED_NO_FIN : LQIS_INITIALIZED_EMIT_FIN;
}

void lq_shutdown(void) {
  LQ_ASSERT(lq_initialized != LQIS_UNINITIALIZED);
  if (lq_initialized == LQIS_INITIALIZED_EMIT_FIN) {
    T.emitfin(outf);
  }
  // TODO: pool flushes, etc
  lq_initialized = LQIS_UNINITIALIZED;
}

void lq_func_start(LqLinkage linkage, LqType return_type, const char* name) {
  Lnk lnk = _linkage_to_internal_lnk(linkage);
  lnk.align = 16;

  curb = 0;
  _num_blocks = 0;
  curi = insb;
  curf = alloc(sizeof *curf);
  curf->ntmp = 0;
  curf->ncon = 2;
  curf->tmp = vnew(curf->ntmp, sizeof curf->tmp[0], PFn);
  curf->con = vnew(curf->ncon, sizeof curf->con[0], PFn);
  for (int i = 0; i < Tmp0; ++i) {
    if (T.fpr0 <= i && i < T.fpr0 + T.nfpr) {
      newtmp(0, Kd, curf);
    } else {
      newtmp(0, Kl, curf);
    }
  }
  curf->con[0].type = CBits;
  curf->con[0].bits.i = 0xdeaddead; /* UNDEF */
  curf->con[1].type = CBits;
  curf->lnk = lnk;
  curf->leaf = 1;
  blink = &curf->start;
  rcls = _lqtype_to_cls_and_ty(return_type, &curf->retty);
  strncpy(curf->name, name, NString - 1);
  _ps = PLbl;

  lq_block_declare_and_start();
}

LqRef lq_func_param_named(LqType type, const char* name) {
  int ty;
  int k = _lqtype_to_cls_and_ty(type, &ty);
  Ref r = newtmp(0, Kx, curf);
  LQ_NAMED_IF_DEBUG(curf->tmp[r.val].name, name);
  // TODO: env ptr, varargs
  if (k == Kc) {
    *curi = (Ins){Oparc, Kl, r, {TYPE(ty)}};
  } else if (k >= Ksb) {
    *curi = (Ins){Oparsb + (k - Ksb), Kw, r, {R}};
  } else {
    *curi = (Ins){Opar, k, r, {R}};
  }
  ++curi;
  return _internal_ref_to_lqref(r);
}

LqRef lq_const_int(int64_t i) {
  Con c = {0};
  c.type = CBits;
  c.bits.i = i;
  return _internal_ref_to_lqref(newcon(&c, curf));
}

LqRef lq_const_single(float f) {
  Con c = {0};
  c.type = CBits;
  c.bits.s = f;
  c.flt = 1;
  return _internal_ref_to_lqref(newcon(&c, curf));
}

LqRef lq_const_double(double d) {
  Con c = {0};
  c.type = CBits;
  c.bits.d = d;
  c.flt = 2;
  return _internal_ref_to_lqref(newcon(&c, curf));
}

// This has to return a LqSymbol that's the name that we re-lookup on use rather
// than directly a Ref because Con Refs are stored per function (so when calling
// the function, we need to create a new one in the caller).
LqSymbol lq_func_end(void) {
  if (!curb) {
    err("empty function");
  }
  if (curb->jmp.type == Jxxx) {
    err("last block misses jump");
  }
  curf->mem = vnew(0, sizeof curf->mem[0], PFn);
  curf->nmem = 0;
  curf->nblk = _num_blocks;
  curf->rpo = 0;
  for (Blk* b = curf->start; b; b = b->link) {
    LQ_ASSERT(b->dlink == 0);
  }
  qbe_parse_typecheck(curf);

  LqSymbol ret = {intern(curf->name)};

  qbe_main_func(curf);

  curf = NULL;

  return ret;
}

void lq_func_dump_current(FILE* to) {
  printfn(curf, to);
}

LqRef lq_ref_for_symbol(LqSymbol sym) {
  LQ_ASSERT(curf);
  Con c = {0};
  c.type = CAddr;
  c.sym.id = sym.u;
  Ref ret = newcon(&c, curf);
  return _internal_ref_to_lqref(ret);
}

LqRef lq_extern(const char* name) {
  Con c = {0};
  c.type = CAddr;
  c.sym.id = intern((char*)name);
  Ref ret = newcon(&c, curf);
  return _internal_ref_to_lqref(ret);
}

LqBlock lq_block_declare_named(const char* name) {
  LQ_ASSERT(_num_blocks < LQ_COUNTOFI(_block_arena));
  LqBlock ret = {_num_blocks++};
  Blk* blk = _lqblock_to_internal_blk(ret);
  memset(blk, 0, sizeof(Blk));
  blk->id = ret.u;
  LQ_NAMED_IF_DEBUG(blk->name, name);
  return ret;
}

void lq_block_start(LqBlock block) {
  Blk* b = _lqblock_to_internal_blk(block);
  if (curb && curb->jmp.type == Jxxx) {
    qbe_parse_closeblk();
    curb->jmp.type = Jjmp;
    curb->s1 = b;
  }
  if (b->jmp.type != Jxxx) {
    err("multiple definitions of block @%s", b->name);
  }
  *blink = b;
  curb = b;
  plink = &curb->phi;
  _ps = PPhi;
}

LqBlock lq_block_declare_and_start_named(const char* name) {
  LqBlock new = lq_block_declare_named(name);
  lq_block_start(new);
  return new;
}

void lq_i_ret(LqRef val) {
  LQ_ASSERT(_ps == PIns || _ps == PPhi);
  curb->jmp.type = Jretw + rcls;
  if (val.u == 0) {
    curb->jmp.type = Jret0;
  } else if (rcls != K0) {
    Ref r = _lqref_to_internal_ref(val);
    if (req(r, R)) {
      err("invalid return value");
    }
    curb->jmp.arg = r;
  }
  qbe_parse_closeblk();
  _ps = PLbl;
}

void lq_i_ret_void(void) {
  lq_i_ret((LqRef){0});  // TODO: not sure if this is correct == {RTmp, 0}.
}

LqRef lq_i_calla(LqType result,
                 LqRef func,
                 int num_args,
                 LqCallArg* cas) {
  LQ_ASSERT(_ps == PIns || _ps == PPhi);
  // args are inserted into instrs first, then the call
  for (int i = 0; i < num_args; ++i) {
    LQ_ASSERT(curi - insb < NIns);
    int ty;
    int k = _lqtype_to_cls_and_ty(cas[i].type, &ty);
    Ref r = _lqref_to_internal_ref(cas[i].value);
    // TODO: env
    if (k == K0 && req(r, R)) {
      // This is our hacky special case for where '...' would appear in the call.
      *curi = (Ins){.op = Oargv};
    } else if (k == Kc) {
      *curi = (Ins){Oargc, Kl, R, {TYPE(ty), r}};
    } else if (k >= Ksb) {
      *curi = (Ins){Oargsb + (k - Ksb), Kw, R, {r}};
    } else {
      *curi = (Ins){Oarg, k, R, {r}};
    }
    ++curi;
  }

  Ref tmp;

  {
    LQ_ASSERT(curi - insb < NIns);
    int ty;
    int k = _lqtype_to_cls_and_ty(result, &ty);
    curf->leaf = 0;
    *curi = (Ins){0};
    curi->op = Ocall;
    curi->arg[0] = _lqref_to_internal_ref(func);
    if (k == Kc) {
      k = Kl;
      curi->arg[1] = TYPE(ty);
    }
    if (k >= Ksb) {
      k = Kw;
    }
    curi->cls = k;
    tmp = newtmp(NULL, k, curf);
    LQ_NAMED_IF_DEBUG(curf->tmp[tmp.val].name, NULL);
    curi->to = tmp;
    ++curi;
  }
  _ps = PIns;
  return _internal_ref_to_lqref(tmp);
}

LqRef lq_i_call0(LqType result, LqRef func) {
  return lq_i_calla(result, func, 0, NULL);
}

LqRef lq_i_call1(LqType result, LqRef func, LqCallArg ca0) {
  return lq_i_calla(result, func, 1, &ca0);
}

LqRef lq_i_call2(LqType result, LqRef func, LqCallArg ca0, LqCallArg ca1) {
  LqCallArg cas[2] = {ca0, ca1};
  return lq_i_calla(result, func, 2, cas);
}

LqRef lq_i_call3(LqType result, LqRef func, LqCallArg ca0, LqCallArg ca1, LqCallArg ca2) {
  LqCallArg cas[3] = {ca0, ca1, ca2};
  return lq_i_calla(result, func, 3, cas);
}

LqRef lq_i_call4(LqType result,
                 LqRef func,
                 LqCallArg ca0,
                 LqCallArg ca1,
                 LqCallArg ca2,
                 LqCallArg ca3) {
  LqCallArg cas[4] = {ca0, ca1, ca2, ca3};
  return lq_i_calla(result, func, 4, cas);
}

LqRef lq_i_call5(LqType result,
                 LqRef func,
                 LqCallArg ca0,
                 LqCallArg ca1,
                 LqCallArg ca2,
                 LqCallArg ca3,
                 LqCallArg ca4) {
  LqCallArg cas[5] = {ca0, ca1, ca2, ca3, ca4};
  return lq_i_calla(result, func, 5, cas);
}

LqRef lq_i_call6(LqType result,
                 LqRef func,
                 LqCallArg ca0,
                 LqCallArg ca1,
                 LqCallArg ca2,
                 LqCallArg ca3,
                 LqCallArg ca4,
                 LqCallArg ca5) {
  LqCallArg cas[6] = {ca0, ca1, ca2, ca3, ca4, ca5};
  return lq_i_calla(result, func, 6, cas);
}

void lq_i_jmp(LqBlock block) {
  LQ_ASSERT(_ps == PIns || _ps == PPhi);
  curb->jmp.type = Jjmp;
  curb->s1 = _lqblock_to_internal_blk(block);
  qbe_parse_closeblk();
}

void lq_i_jnz(LqRef cond, LqBlock if_true, LqBlock if_false) {
  Ref r = _lqref_to_internal_ref(cond);
  if (req(r, R))
    err("invalid argument for jnz jump");
  curb->jmp.type = Jjnz;
  curb->jmp.arg = r;
  curb->s1 = _lqblock_to_internal_blk(if_true);
  curb->s2 = _lqblock_to_internal_blk(if_false);
  qbe_parse_closeblk();
}

LqRef lq_i_phi(LqType size_class, LqBlock block0, LqRef val0, LqBlock block1, LqRef val1) {
  if (_ps != PPhi || curb == curf->start) {
    err("unexpected phi instruction");
  }

  Ref tmp = newtmp(NULL, Kx, curf);
  LQ_NAMED_IF_DEBUG(curf->tmp[tmp.val].name, NULL);

  Phi* phi = alloc(sizeof *phi);
  phi->to = tmp;
  phi->cls = size_class.u;
  int i = 2;  // TODO: variable if necessary
  phi->arg = vnew(i, sizeof(Ref), PFn);
  phi->arg[0] = _lqref_to_internal_ref(val0);
  phi->arg[1] = _lqref_to_internal_ref(val1);
  phi->blk = vnew(i, sizeof(Blk*), PFn);
  phi->blk[0] = _lqblock_to_internal_blk(block0);
  phi->blk[1] = _lqblock_to_internal_blk(block1);
  phi->narg = i;
  *plink = phi;
  plink = &phi->link;
  _ps = PPhi;
  return _internal_ref_to_lqref(tmp);
}

static LqRef _normal_two_op_instr(int op, LqType size_class, LqRef arg0, LqRef arg1) {
  LQ_ASSERT(/*size_class.u >= LQ_TYPE_W && */size_class.u <= LQ_TYPE_D);
  LQ_ASSERT(_ps == PIns || _ps == PPhi);
  LQ_ASSERT(curi - insb < NIns);
  Ref tmp = newtmp(NULL, Kx, curf);
  LQ_NAMED_IF_DEBUG(curf->tmp[tmp.val].name, NULL);
  curi->op = op;
  curi->cls = size_class.u;
  curi->to = tmp;
  curi->arg[0] = _lqref_to_internal_ref(arg0);
  curi->arg[1] = _lqref_to_internal_ref(arg1);
  ++curi;
  _ps = PIns;
  return _internal_ref_to_lqref(tmp);
}

static void _normal_two_op_void_instr(int op, LqRef arg0, LqRef arg1) {
  LQ_ASSERT(_ps == PIns || _ps == PPhi);
  LQ_ASSERT(curi - insb < NIns);
  curi->op = op;
  curi->cls = LQ_TYPE_W;
  curi->to = R;
  curi->arg[0] = _lqref_to_internal_ref(arg0);
  curi->arg[1] = _lqref_to_internal_ref(arg1);
  ++curi;
  _ps = PIns;
}

static LqRef _normal_one_op_instr(int op, LqType size_class, LqRef arg0) {
  LQ_ASSERT(/*size_class.u >= LQ_TYPE_W && */size_class.u <= LQ_TYPE_D);
  LQ_ASSERT(_ps == PIns || _ps == PPhi);
  LQ_ASSERT(curi - insb < NIns);
  Ref tmp = newtmp(NULL, Kx, curf);
  LQ_NAMED_IF_DEBUG(curf->tmp[tmp.val].name, NULL);
  curi->op = op;
  curi->cls = size_class.u;
  curi->to = tmp;
  curi->arg[0] = _lqref_to_internal_ref(arg0);
  curi->arg[1] = R;
  ++curi;
  _ps = PIns;
  return _internal_ref_to_lqref(tmp);
}

void lq_data_start(LqLinkage linkage, const char* name) {
  _curd_lnk = _linkage_to_internal_lnk(linkage);
  if (_curd_lnk.align == 0) {
    _curd_lnk.align = 8;
  }

  _curd = (Dat){0};
  _curd.type = DStart;
  _curd.name = (char*)name;
  _curd.lnk = &_curd_lnk;
  qbe_main_data(&_curd);
}

void lq_data_byte(uint8_t val) {
  _curd.isref = 0;
  _curd.isstr = 0;
  _curd.type = DB;
  _curd.u.num = val;
  qbe_main_data(&_curd);
}

void lq_data_half(uint16_t val) {
  _curd.isref = 0;
  _curd.isstr = 0;
  _curd.type = DH;
  _curd.u.num = val;
  qbe_main_data(&_curd);
}

void lq_data_word(uint32_t val) {
  _curd.isref = 0;
  _curd.isstr = 0;
  _curd.type = DW;
  _curd.u.num = val;
  qbe_main_data(&_curd);
}

void lq_data_long(uint64_t val) {
  _curd.isref = 0;
  _curd.isstr = 0;
  _curd.type = DL;
  _curd.u.num = val;
  qbe_main_data(&_curd);
}

void lq_data_single(float val) {
  _curd.isref = 0;
  _curd.isstr = 0;
  _curd.type = DW;
  _curd.u.flts = val;
  qbe_main_data(&_curd);
}

void lq_data_double(double val) {
  _curd.isref = 0;
  _curd.isstr = 0;
  _curd.type = DL;
  _curd.u.fltd = val;
  qbe_main_data(&_curd);
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

void lq_data_string(const char* str) {
  _curd.type = DB;
  _curd.isstr = 1;
  // QBE sneakily avoids de-escaping in the tokenizer and re-escaping during
  // emission by just not handling escapes at all and relying on the input
  // format for string escapes being the same as the assembler's. Because we're
  // getting our strings from C here, we need to "repr" it.
  size_t len = _str_repr(str, NULL);
  char* escaped = alloca(len);
  size_t len2 = _str_repr(str, escaped);
  LQ_ASSERT(len == len2); (void)len2;
  _curd.u.str = (char*)escaped;
  qbe_main_data(&_curd);
}

LqSymbol lq_data_end(void) {
  _curd.isref = 0;
  _curd.isstr = 0;
  _curd.type = DEnd;
  qbe_main_data(&_curd);

  LqSymbol ret = {intern(_curd.name)};
  _curd = (Dat){0};
  _curd_lnk = (Lnk){0};
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
void lq_type_struct_start(const char *name, int align) {
  vgrow(&typ, _ntyp + 1);
  _curty = &typ[_ntyp++];
  _curty->isdark = 0;
  _curty->isunion = 0;
  _curty->align = -1;
  _curty_build_n = 0;

  if (align > 0) {
    int al;
    for (al = 0; align /= 2; al++)
      ;
    _curty->align = al;
  }

  _curty->size = 0;
  strcpy(_curty->name, name);
  _curty->fields = vnew(1, sizeof _curty->fields[0], PHeap);
  _curty->nunion = 1;

  _curty_build_sz = 0;
  _curty_build_al = _curty->align;
}

void lq_type_add_field_with_count(LqType field, uint32_t count) {
  Field* fld = _curty->fields[0];

  Typ* ty1;
  uint64_t s;
  int a;

  int type;
  int ty;
  int cls = _lqtype_to_cls_and_ty(field, &ty);
  switch (cls) {
    case LQ_TYPE_D: type = Fd; s = 8; a = 3; break;
    case LQ_TYPE_L: type = Fl; s = 8; a = 3; break;
    case LQ_TYPE_S: type = Fs; s = 4; a = 2; break;
    case LQ_TYPE_W: type = Fw; s = 4; a = 2; break;
    case LQ_TYPE_SH: type = Fh; s = 2; a = 1; break;
    case LQ_TYPE_UH: type = Fh; s = 2; a = 1; break;
    case LQ_TYPE_SB: type = Fb; s = 1; a = 0; break;
    case LQ_TYPE_UB: type = Fb; s = 1; a = 0; break;
    default:
      type = FTyp;
      ty1 = &typ[field.u];
      s = ty1->size;
      a = ty1->align;
      break;
  }

  if (a > _curty_build_al) {
    _curty_build_al = a;
  }
  a = (1 << a) - 1;
  a = ((_curty_build_sz + a) & ~a) - _curty_build_sz;
  if (a) {
    if (_curty_build_n < NField) {
      fld[_curty_build_n].type = FPad;
      fld[_curty_build_n].len = a;
      _curty_build_n++;
    }
  }
  _curty_build_sz += a + count * s;
  if (type == FTyp) {
    s = field.u;
  }
  for (; count > 0 && _curty_build_n < NField; count--, _curty_build_n++) {
    fld[_curty_build_n].type = type;
    fld[_curty_build_n].len = s;
  }
}

void lq_type_add_field(LqType field) {
  lq_type_add_field_with_count(field, 1);
}

LqType lq_type_struct_end(void) {
  Field* fld = _curty->fields[0];
  fld[_curty_build_n].type = FEnd;
  int a = 1 << _curty_build_al;
  if (_curty_build_sz < _curty->size) {
    _curty_build_sz = _curty->size;
  }
  _curty->size = (_curty_build_sz + a - 1) & -a;
  _curty->align = _curty_build_al;
  if (debug['T']) {
    fprintf(stderr, "\n> Parsed type:\n");
    printtyp(_curty, stderr);
  }
  LqType ret = {_curty - typ};
  _curty = NULL;
  _curty_build_n = 0;
  _curty_build_sz = 0;
  _curty_build_al = 0;
  return ret;
}
/*** END FILE: ../libqbe_impl.c ***/
LqRef lq_i_add(LqType size_class, LqRef arg0 /*wlsd*/, LqRef arg1 /*wlsd*/) { return _normal_two_op_instr(Oadd, size_class, arg0, arg1); }
LqRef lq_i_sub(LqType size_class, LqRef arg0 /*wlsd*/, LqRef arg1 /*wlsd*/) { return _normal_two_op_instr(Osub, size_class, arg0, arg1); }
LqRef lq_i_neg(LqType size_class, LqRef arg0 /*wlsd*/) { return _normal_one_op_instr(Oneg, size_class, arg0); }
LqRef lq_i_div(LqType size_class, LqRef arg0 /*wlsd*/, LqRef arg1 /*wlsd*/) { return _normal_two_op_instr(Odiv, size_class, arg0, arg1); }
LqRef lq_i_rem(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wlee*/) { return _normal_two_op_instr(Orem, size_class, arg0, arg1); }
LqRef lq_i_udiv(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wlee*/) { return _normal_two_op_instr(Oudiv, size_class, arg0, arg1); }
LqRef lq_i_urem(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wlee*/) { return _normal_two_op_instr(Ourem, size_class, arg0, arg1); }
LqRef lq_i_mul(LqType size_class, LqRef arg0 /*wlsd*/, LqRef arg1 /*wlsd*/) { return _normal_two_op_instr(Omul, size_class, arg0, arg1); }
LqRef lq_i_and(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wlee*/) { return _normal_two_op_instr(Oand, size_class, arg0, arg1); }
LqRef lq_i_or(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wlee*/) { return _normal_two_op_instr(Oor, size_class, arg0, arg1); }
LqRef lq_i_xor(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wlee*/) { return _normal_two_op_instr(Oxor, size_class, arg0, arg1); }
LqRef lq_i_sar(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wwee*/) { return _normal_two_op_instr(Osar, size_class, arg0, arg1); }
LqRef lq_i_shr(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wwee*/) { return _normal_two_op_instr(Oshr, size_class, arg0, arg1); }
LqRef lq_i_shl(LqType size_class, LqRef arg0 /*wlee*/, LqRef arg1 /*wwee*/) { return _normal_two_op_instr(Oshl, size_class, arg0, arg1); }
LqRef lq_i_ceqw(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/) { return _normal_two_op_instr(Oceqw, size_class, arg0, arg1); }
LqRef lq_i_cnew(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/) { return _normal_two_op_instr(Ocnew, size_class, arg0, arg1); }
LqRef lq_i_csgew(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/) { return _normal_two_op_instr(Ocsgew, size_class, arg0, arg1); }
LqRef lq_i_csgtw(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/) { return _normal_two_op_instr(Ocsgtw, size_class, arg0, arg1); }
LqRef lq_i_cslew(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/) { return _normal_two_op_instr(Ocslew, size_class, arg0, arg1); }
LqRef lq_i_csltw(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/) { return _normal_two_op_instr(Ocsltw, size_class, arg0, arg1); }
LqRef lq_i_cugew(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/) { return _normal_two_op_instr(Ocugew, size_class, arg0, arg1); }
LqRef lq_i_cugtw(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/) { return _normal_two_op_instr(Ocugtw, size_class, arg0, arg1); }
LqRef lq_i_culew(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/) { return _normal_two_op_instr(Oculew, size_class, arg0, arg1); }
LqRef lq_i_cultw(LqType size_class, LqRef arg0 /*wwee*/, LqRef arg1 /*wwee*/) { return _normal_two_op_instr(Ocultw, size_class, arg0, arg1); }
LqRef lq_i_ceql(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/) { return _normal_two_op_instr(Oceql, size_class, arg0, arg1); }
LqRef lq_i_cnel(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/) { return _normal_two_op_instr(Ocnel, size_class, arg0, arg1); }
LqRef lq_i_csgel(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/) { return _normal_two_op_instr(Ocsgel, size_class, arg0, arg1); }
LqRef lq_i_csgtl(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/) { return _normal_two_op_instr(Ocsgtl, size_class, arg0, arg1); }
LqRef lq_i_cslel(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/) { return _normal_two_op_instr(Ocslel, size_class, arg0, arg1); }
LqRef lq_i_csltl(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/) { return _normal_two_op_instr(Ocsltl, size_class, arg0, arg1); }
LqRef lq_i_cugel(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/) { return _normal_two_op_instr(Ocugel, size_class, arg0, arg1); }
LqRef lq_i_cugtl(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/) { return _normal_two_op_instr(Ocugtl, size_class, arg0, arg1); }
LqRef lq_i_culel(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/) { return _normal_two_op_instr(Oculel, size_class, arg0, arg1); }
LqRef lq_i_cultl(LqType size_class, LqRef arg0 /*llee*/, LqRef arg1 /*llee*/) { return _normal_two_op_instr(Ocultl, size_class, arg0, arg1); }
LqRef lq_i_ceqs(LqType size_class, LqRef arg0 /*ssee*/, LqRef arg1 /*ssee*/) { return _normal_two_op_instr(Oceqs, size_class, arg0, arg1); }
LqRef lq_i_cges(LqType size_class, LqRef arg0 /*ssee*/, LqRef arg1 /*ssee*/) { return _normal_two_op_instr(Ocges, size_class, arg0, arg1); }
LqRef lq_i_cgts(LqType size_class, LqRef arg0 /*ssee*/, LqRef arg1 /*ssee*/) { return _normal_two_op_instr(Ocgts, size_class, arg0, arg1); }
LqRef lq_i_cles(LqType size_class, LqRef arg0 /*ssee*/, LqRef arg1 /*ssee*/) { return _normal_two_op_instr(Ocles, size_class, arg0, arg1); }
LqRef lq_i_clts(LqType size_class, LqRef arg0 /*ssee*/, LqRef arg1 /*ssee*/) { return _normal_two_op_instr(Oclts, size_class, arg0, arg1); }
LqRef lq_i_cnes(LqType size_class, LqRef arg0 /*ssee*/, LqRef arg1 /*ssee*/) { return _normal_two_op_instr(Ocnes, size_class, arg0, arg1); }
LqRef lq_i_cos(LqType size_class, LqRef arg0 /*ssee*/, LqRef arg1 /*ssee*/) { return _normal_two_op_instr(Ocos, size_class, arg0, arg1); }
LqRef lq_i_cuos(LqType size_class, LqRef arg0 /*ssee*/, LqRef arg1 /*ssee*/) { return _normal_two_op_instr(Ocuos, size_class, arg0, arg1); }
LqRef lq_i_ceqd(LqType size_class, LqRef arg0 /*ddee*/, LqRef arg1 /*ddee*/) { return _normal_two_op_instr(Oceqd, size_class, arg0, arg1); }
LqRef lq_i_cged(LqType size_class, LqRef arg0 /*ddee*/, LqRef arg1 /*ddee*/) { return _normal_two_op_instr(Ocged, size_class, arg0, arg1); }
LqRef lq_i_cgtd(LqType size_class, LqRef arg0 /*ddee*/, LqRef arg1 /*ddee*/) { return _normal_two_op_instr(Ocgtd, size_class, arg0, arg1); }
LqRef lq_i_cled(LqType size_class, LqRef arg0 /*ddee*/, LqRef arg1 /*ddee*/) { return _normal_two_op_instr(Ocled, size_class, arg0, arg1); }
LqRef lq_i_cltd(LqType size_class, LqRef arg0 /*ddee*/, LqRef arg1 /*ddee*/) { return _normal_two_op_instr(Ocltd, size_class, arg0, arg1); }
LqRef lq_i_cned(LqType size_class, LqRef arg0 /*ddee*/, LqRef arg1 /*ddee*/) { return _normal_two_op_instr(Ocned, size_class, arg0, arg1); }
LqRef lq_i_cod(LqType size_class, LqRef arg0 /*ddee*/, LqRef arg1 /*ddee*/) { return _normal_two_op_instr(Ocod, size_class, arg0, arg1); }
LqRef lq_i_cuod(LqType size_class, LqRef arg0 /*ddee*/, LqRef arg1 /*ddee*/) { return _normal_two_op_instr(Ocuod, size_class, arg0, arg1); }
void lq_i_storeb(LqRef arg0 /*weee*/, LqRef arg1 /*meee*/) { _normal_two_op_void_instr(Ostoreb, arg0, arg1); }
void lq_i_storeh(LqRef arg0 /*weee*/, LqRef arg1 /*meee*/) { _normal_two_op_void_instr(Ostoreh, arg0, arg1); }
void lq_i_storew(LqRef arg0 /*weee*/, LqRef arg1 /*meee*/) { _normal_two_op_void_instr(Ostorew, arg0, arg1); }
void lq_i_storel(LqRef arg0 /*leee*/, LqRef arg1 /*meee*/) { _normal_two_op_void_instr(Ostorel, arg0, arg1); }
void lq_i_stores(LqRef arg0 /*seee*/, LqRef arg1 /*meee*/) { _normal_two_op_void_instr(Ostores, arg0, arg1); }
void lq_i_stored(LqRef arg0 /*deee*/, LqRef arg1 /*meee*/) { _normal_two_op_void_instr(Ostored, arg0, arg1); }
LqRef lq_i_loadsb(LqType size_class, LqRef arg0 /*mmee*/) { return _normal_one_op_instr(Oloadsb, size_class, arg0); }
LqRef lq_i_loadub(LqType size_class, LqRef arg0 /*mmee*/) { return _normal_one_op_instr(Oloadub, size_class, arg0); }
LqRef lq_i_loadsh(LqType size_class, LqRef arg0 /*mmee*/) { return _normal_one_op_instr(Oloadsh, size_class, arg0); }
LqRef lq_i_loaduh(LqType size_class, LqRef arg0 /*mmee*/) { return _normal_one_op_instr(Oloaduh, size_class, arg0); }
LqRef lq_i_loadsw(LqType size_class, LqRef arg0 /*mmee*/) { return _normal_one_op_instr(Oloadsw, size_class, arg0); }
LqRef lq_i_loaduw(LqType size_class, LqRef arg0 /*mmee*/) { return _normal_one_op_instr(Oloaduw, size_class, arg0); }
LqRef lq_i_load(LqType size_class, LqRef arg0 /*mmmm*/) { return _normal_one_op_instr(Oload, size_class, arg0); }
LqRef lq_i_extsb(LqType size_class, LqRef arg0 /*wwee*/) { return _normal_one_op_instr(Oextsb, size_class, arg0); }
LqRef lq_i_extub(LqType size_class, LqRef arg0 /*wwee*/) { return _normal_one_op_instr(Oextub, size_class, arg0); }
LqRef lq_i_extsh(LqType size_class, LqRef arg0 /*wwee*/) { return _normal_one_op_instr(Oextsh, size_class, arg0); }
LqRef lq_i_extuh(LqType size_class, LqRef arg0 /*wwee*/) { return _normal_one_op_instr(Oextuh, size_class, arg0); }
LqRef lq_i_extsw(LqRef arg0 /*ewee*/) { return _normal_one_op_instr(Oextsw, lq_type_long, arg0); }
LqRef lq_i_extuw(LqRef arg0 /*ewee*/) { return _normal_one_op_instr(Oextuw, lq_type_long, arg0); }
LqRef lq_i_exts(LqRef arg0 /*eees*/) { return _normal_one_op_instr(Oexts, lq_type_double, arg0); }
LqRef lq_i_truncd(LqRef arg0 /*eede*/) { return _normal_one_op_instr(Otruncd, lq_type_single, arg0); }
LqRef lq_i_stosi(LqType size_class, LqRef arg0 /*ssee*/) { return _normal_one_op_instr(Ostosi, size_class, arg0); }
LqRef lq_i_stoui(LqType size_class, LqRef arg0 /*ssee*/) { return _normal_one_op_instr(Ostoui, size_class, arg0); }
LqRef lq_i_dtosi(LqType size_class, LqRef arg0 /*ddee*/) { return _normal_one_op_instr(Odtosi, size_class, arg0); }
LqRef lq_i_dtoui(LqType size_class, LqRef arg0 /*ddee*/) { return _normal_one_op_instr(Odtoui, size_class, arg0); }
LqRef lq_i_swtof(LqType size_class, LqRef arg0 /*eeww*/) { return _normal_one_op_instr(Oswtof, size_class, arg0); }
LqRef lq_i_uwtof(LqType size_class, LqRef arg0 /*eeww*/) { return _normal_one_op_instr(Ouwtof, size_class, arg0); }
LqRef lq_i_sltof(LqType size_class, LqRef arg0 /*eell*/) { return _normal_one_op_instr(Osltof, size_class, arg0); }
LqRef lq_i_ultof(LqType size_class, LqRef arg0 /*eell*/) { return _normal_one_op_instr(Oultof, size_class, arg0); }
LqRef lq_i_cast(LqType size_class, LqRef arg0 /*sdwl*/) { return _normal_one_op_instr(Ocast, size_class, arg0); }
LqRef lq_i_alloc4(LqRef arg0 /*elee*/) { return _normal_one_op_instr(Oalloc4, lq_type_long, arg0); }
LqRef lq_i_alloc8(LqRef arg0 /*elee*/) { return _normal_one_op_instr(Oalloc8, lq_type_long, arg0); }
LqRef lq_i_alloc16(LqRef arg0 /*elee*/) { return _normal_one_op_instr(Oalloc16, lq_type_long, arg0); }
LqRef lq_i_vaarg(LqType size_class, LqRef arg0 /*mmmm*/) { return _normal_one_op_instr(Ovaarg, size_class, arg0); }
LqRef lq_i_vastart(LqRef arg0 /*meee*/) { return _normal_one_op_instr(Ovastart, lq_type_word, arg0); }
LqRef lq_i_copy(LqType size_class, LqRef arg0 /*wlsd*/) { return _normal_one_op_instr(Ocopy, size_class, arg0); }
void lq_i_dbgloc(LqRef arg0 /*weee*/, LqRef arg1 /*weee*/) { _normal_two_op_void_instr(Odbgloc, arg0, arg1); }

#ifdef _MSC_VER
#pragma warning(pop)
#endif
