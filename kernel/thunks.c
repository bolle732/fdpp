#include <stdlib.h>
#include "../hdr/portab.h"
#include "globals.h"
#include "proto.h"
#include "thunks_priv.h"
#include "thunks.h"

static struct fdpp_api *fdpp;
static struct fdthunk_api *api_calls;

struct asm_dsc_s {
    UWORD num;
    UWORD off;
    UWORD seg;
};
static struct asm_dsc_s *asm_tab;
static int asm_tab_len;
static struct far_s *sym_tab;
static int sym_tab_len;
static void fdprintf(const char *format, ...);

void FdppSetAsmCalls(struct asm_dsc_s *tab, int size)
{
    asm_tab = tab;
    asm_tab_len = size / sizeof(struct asm_dsc_s);
}

#define SEMIC ;
#define __ASM(t, v) __ASMSYM(t) __##v
#define __ASM_FAR(t, v) __ASMFAR(t) __##v
#define __ASM_ARR(t, v, l) __ASMARSYM(t, __##v, l)
#define __ASM_ARRI(t, v) __ASMARISYM(t, __##v)
#define __ASM_ARRI_F(t, v) __ASMARIFSYM(t, __##v)
#define __ASM_FUNC(v) __ASMFSYM(void) __##v
#include "glob_asm.h"
#undef __ASM
#undef __ASM_FAR
#undef __ASM_ARR
#undef __ASM_ARRI
#undef __ASM_ARRI_F
#undef __ASM_FUNC

static union asm_thunks_u {
  struct _thunks {
#define __ASM(t, v) struct far_s * __##v
#define __ASM_FAR(t, v) struct far_s * __##v
#define __ASM_ARR(t, v, l) struct far_s * __##v
#define __ASM_ARRI(t, v) struct far_s * __##v
#define __ASM_ARRI_F(t, v) struct far_s * __##v
#define __ASM_FUNC(v) struct far_s * __##v
#include "glob_asm.h"
#undef __ASM
#undef __ASM_FAR
#undef __ASM_ARR
#undef __ASM_ARRI
#undef __ASM_ARRI_F
#undef __ASM_FUNC
  } thunks;
  struct far_s * arr[sizeof(struct _thunks) / sizeof(struct far_s *)];
} asm_thunks = {{
#undef SEMIC
#define SEMIC ,
#define __ASM(t, v) __ASMREF(__##v)
#define __ASM_FAR(t, v) __ASMREF(__##v)
#define __ASM_ARR(t, v, l) __ASMREF(__##v)
#define __ASM_ARRI(t, v) __ASMREF(__##v)
#define __ASM_ARRI_F(t, v) __ASMREF(__##v)
#define __ASM_FUNC(v) __ASMREF(__##v)
#include "glob_asm.h"
#undef __ASM
#undef __ASM_FAR
#undef __ASM_ARR
#undef __ASM_ARRI
#undef __ASM_ARRI_F
#undef __ASM_FUNC
#undef SEMIC
}};

static void *so2lin(uint16_t seg, uint16_t off)
{
    return fdpp->mem_base() + (seg << 4) + off;
}

void *resolve_segoff(struct far_s fa)
{
    return so2lin(fa.seg, fa.off);
}

int FdppSetAsmThunks(struct far_s *ptrs, int size)
{
#define _countof(a) (sizeof(a)/sizeof(*(a)))
    int i;
    int len = size / (sizeof(struct far_s));
    int exp = _countof(asm_thunks.arr);

    if (len != exp) {
        fdprintf("len=%i expected %i\n", len, exp);
        return -1;
    }
    for (i = 0; i < len; i++)
        *asm_thunks.arr[i] = ptrs[i];

    sym_tab = (struct far_s *)malloc(size);
    memcpy(sym_tab, ptrs, size);
    sym_tab_len = len;
    return 0;
}

#define _ARG(n, t, ap) (*(t *)(ap + n))
#define _ARG_PTR(n, t, ap) // unimplemented, will create syntax error
#define _ARG_PTR_FAR(n, t, ap)  ({ \
    UDWORD __d = *(UDWORD *)(ap + n); \
    FP_FROM_D(t, __d); \
})

UDWORD FdppThunkCall(int fn, UBYTE *sp, UBYTE *r_len)
{
    UDWORD ret = 0;
    UBYTE rsz = 0;
#define _RET ret
#define _RSZ rsz
#define _SP sp

    switch (fn) {
        #include "thunk_calls.h"
    }

    if (r_len)
        *r_len = rsz;
    return ret;
}

void do_abort(const char *file, int line)
{
    fdpp->abort_handler(file, line);
}

void FdppInit(struct fdpp_api *api)
{
    fdpp = api;
    api_calls = &api->thunks;
}

static void fdprintf(const char *format, ...)
{
    va_list vl;

    va_start(vl, format);
    fdpp->print_handler(format, vl);
    va_end(vl);
}

static uint32_t do_asm_call(int num, uint8_t *sp, uint8_t len)
{
    int i;

    for (i = 0; i < asm_tab_len; i++) {
        if (asm_tab[i].num == num)
            return fdpp->asm_call(asm_tab[i].seg, asm_tab[i].off, sp, len);
    }
    _assert(0);
    return -1;
}

#define __ARG(t) t
#define __ARG_PTR(t) t *
#define __ARG_PTR_FAR(t) t FAR *
#define __ARG_A(t) t
#define __ARG_PTR_A(t) PTR_MEMB(t)
#define __ARG_PTR_FAR_A(t) __DOSFAR(t)

#define PACKED __attribute__((packed))
#define __CNV_PTR_FAR(t, d, f) t d = (f)
// XXX: calculate size properly!
#define __CNV_PTR(t, d, f) _MK_FAR_SZ(__##d, f, 64); t d = __MK_NEAR(__##d)
#define __CNV_SIMPLE(t, d, f) t d = (f)

#define _CNV(c, at, n) c(at, _a##n, a##n)

#define _THUNK0_v(n, f) \
void f(void) \
{ \
    _assert(n < asm_tab_len); \
    do_asm_call(n, NULL, 0); \
}

#define _THUNK1_v(n, f, t1, at1, c1) \
void f(t1 a1) \
{ \
    _CNV(c1, at1, 1); \
    struct { \
	at1 a1; \
    } PACKED _args = { _a1 }; \
    _assert(n < asm_tab_len); \
    do_asm_call(n, (UBYTE *)&_args, sizeof(_args)); \
}

#define _THUNK2_v(n, f, t1, at1, c1, t2, at2, c2) \
void f(t1 a1, t2 a2) \
{ \
    _CNV(c1, at1, 1); \
    _CNV(c2, at2, 2); \
    struct { \
	at1 a1; \
	at2 a2; \
    } PACKED _args = { _a1, _a2 }; \
    _assert(n < asm_tab_len); \
    do_asm_call(n, (UBYTE *)&_args, sizeof(_args)); \
}

#define _THUNK3_v(n, f, t1, at1, c1, t2, at2, c2, t3, at3, c3) \
void f(t1 a1, t2 a2, t3 a3) \
{ \
    _CNV(c1, at1, 1); \
    _CNV(c2, at2, 2); \
    _CNV(c3, at3, 3); \
    struct { \
	at1 a1; \
	at2 a2; \
	at2 a3; \
    } PACKED _args = { _a1, _a2, _a3 }; \
    _assert(n < asm_tab_len); \
    do_asm_call(n, (UBYTE *)&_args, sizeof(_args)); \
}

#define _THUNK4(n, r, f, t1, at1, c1, t2, at2, c2, t3, at3, c3, t4, at4, c4) \
r f(t1 a1, t2 a2, t3 a3, t4 a4) \
{ \
    _CNV(c1, at1, 1); \
    _CNV(c2, at2, 2); \
    _CNV(c3, at3, 3); \
    _CNV(c4, at4, 4); \
    struct { \
	at1 a1; \
	at2 a2; \
	at3 a3; \
	at4 a4; \
    } PACKED _args = { _a1, _a2, _a3, _a4 }; \
    _assert(n < asm_tab_len); \
    return do_asm_call(n, (UBYTE *)&_args, sizeof(_args)); \
}

#define _THUNK_P_0_v(n, f) \
void f(void) \
{ \
    _assert(n < asm_tab_len); \
    do_asm_call(n, NULL, 0); \
}

#define _THUNK_P_0_vp(n, f) \
void FAR *f(void) \
{ \
    uint32_t __ret; \
    _assert(n < asm_tab_len); \
    __ret = do_asm_call(n, NULL, 0); \
    return FP_FROM_D(void, __ret); \
}

#define _THUNK_P_0(n, r, f) \
r f(void) \
{ \
    _assert(n < asm_tab_len); \
    return do_asm_call(n, NULL, 0); \
}

#define _THUNK_P_1_v(n, f, t1, at1, c1) \
void f(t1 a1) \
{ \
    _CNV(c1, at1, 1); \
    struct { \
	at1 a1; \
    } PACKED _args = { _a1 }; \
    _assert(n < asm_tab_len); \
    do_asm_call(n, (UBYTE *)&_args, sizeof(_args)); \
}

#define _THUNK_P_1(n, r, f, t1, at1, c1) \
r f(t1 a1) \
{ \
    _CNV(c1, at1, 1); \
    struct { \
	at1 a1; \
    } PACKED _args = { _a1 }; \
    _assert(n < asm_tab_len); \
    return do_asm_call(n, (UBYTE *)&_args, sizeof(_args)); \
}

#define _THUNK_P_2(n, r, f, t1, at1, c1, t2, at2, c2) \
r f(t1 a1, t2 a2) \
{ \
    _CNV(c1, at1, 1); \
    _CNV(c2, at2, 2); \
    struct { \
	at2 a2; \
	at1 a1; \
    } PACKED _args = { _a2, _a1 }; \
    _assert(n < asm_tab_len); \
    return do_asm_call(n, (UBYTE *)&_args, sizeof(_args)); \
}

#define _THUNK_P_3(n, r, f, t1, at1, c1, t2, at2, c2, t3, at3, c3) \
r f(t1 a1, t2 a2, t3 a3) \
{ \
    _CNV(c1, at1, 1); \
    _CNV(c2, at2, 2); \
    _CNV(c3, at3, 3); \
    struct { \
	at3 a3; \
	at2 a2; \
	at1 a1; \
    } PACKED _args = { _a3, _a2, _a1 }; \
    _assert(n < asm_tab_len); \
    return do_asm_call(n, (UBYTE *)&_args, sizeof(_args)); \
}

#define _THUNK_P_4_v(n, f, t1, at1, c1, t2, at2, c2, t3, at3, c3, t4, at4, c4) \
void f(t1 a1, t2 a2, t3 a3, t4 a4) \
{ \
    _CNV(c1, at1, 1); \
    _CNV(c2, at2, 2); \
    _CNV(c3, at3, 3); \
    _CNV(c4, at4, 4); \
    struct { \
	at4 a4; \
	at3 a3; \
	at2 a2; \
	at1 a1; \
    } PACKED _args = { _a4, _a3, _a2, _a1 }; \
    _assert(n < asm_tab_len); \
    do_asm_call(n, (UBYTE *)&_args, sizeof(_args)); \
}

#define _THUNK_P_4(n, r, f, t1, at1, c1, t2, at2, c2, t3, at3, c3, t4, at4, c4) \
r f(t1 a1, t2 a2, t3 a3, t4 a4) \
{ \
    _CNV(c1, at1, 1); \
    _CNV(c2, at2, 2); \
    _CNV(c3, at3, 3); \
    _CNV(c4, at4, 4); \
    struct { \
	at4 a4; \
	at3 a3; \
	at2 a2; \
	at1 a1; \
    } PACKED _args = { _a4, _a3, _a2, _a1 }; \
    _assert(n < asm_tab_len); \
    return do_asm_call(n, (UBYTE *)&_args, sizeof(_args)); \
}

#define _THUNK_P_5(n, r, f, t1, at1, c1, t2, at2, c2, t3, at3, c3, t4, at4, c4, t5, at5, c5) \
r f(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) \
{ \
    _CNV(c1, at1, 1); \
    _CNV(c2, at2, 2); \
    _CNV(c3, at3, 3); \
    _CNV(c4, at4, 4); \
    _CNV(c5, at5, 5); \
    struct { \
	at5 a5; \
	at4 a4; \
	at3 a3; \
	at2 a2; \
	at1 a1; \
    } PACKED _args = { _a5, _a4, _a3, _a2, _a1 }; \
    _assert(n < asm_tab_len); \
    return do_asm_call(n, (UBYTE *)&_args, sizeof(_args)); \
}

#define _THUNK_P_6(n, r, f, t1, at1, c1, t2, at2, c2, t3, at3, c3, t4, at4, c4, t5, at5, c5, t6, at6, c6) \
r f(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
    _CNV(c1, at1, 1); \
    _CNV(c2, at2, 2); \
    _CNV(c3, at3, 3); \
    _CNV(c4, at4, 4); \
    _CNV(c5, at5, 5); \
    _CNV(c6, at6, 6); \
    struct { \
	at6 a6; \
	at5 a5; \
	at4 a4; \
	at3 a3; \
	at2 a2; \
	at1 a1; \
    } PACKED _args = { _a6, _a5, _a4, _a3, _a2, _a1 }; \
    _assert(n < asm_tab_len); \
    return do_asm_call(n, (UBYTE *)&_args, sizeof(_args)); \
}

#include "thunk_asms.h"


#define _THUNK_API_v(n) \
void n(void) \
{ \
    api_calls->n(); \
}

#define _THUNK_API_0(r, n) \
r n(void) \
{ \
    return api_calls->n(); \
}

#include "thunkapi_tmpl.h"


UBYTE peekb(UWORD seg, UWORD ofs)
{
    return *(UBYTE *)so2lin(seg, ofs);
}

UWORD peekw(UWORD seg, UWORD ofs)
{
    return *(UWORD *)so2lin(seg, ofs);
}

UDWORD peekl(UWORD seg, UWORD ofs)
{
    return *(UDWORD *)so2lin(seg, ofs);
}

void pokeb(UWORD seg, UWORD ofs, UBYTE b)
{
    *(UBYTE *)so2lin(seg, ofs) = b;
}

void pokew(UWORD seg, UWORD ofs, UWORD w)
{
    *(UWORD *)so2lin(seg, ofs) = w;
}

void pokel(UWORD seg, UWORD ofs, UDWORD l)
{
    *(UDWORD *)so2lin(seg, ofs) = l;
}

/* never create far pointers.
 * only look them up in symtab. */
struct far_s lookup_far_st(void *ptr)
{
    int i;
    for (i = 0; i < sym_tab_len; i++) {
        if (resolve_segoff(sym_tab[i]) == ptr)
            return sym_tab[i];
    }
    _assert(0);
    return (struct far_s){0, 0};
}

uint32_t thunk_call_void(struct far_s fa)
{
    return fdpp->asm_call(fa.seg, fa.off, NULL, 0);
}
