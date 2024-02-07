/*
 *  FDPP - freedos port to modern C++
 *  Copyright (C) 2018  Stas Sergeev (stsp)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include "portab.h"
#include "smalloc.h"
#include "thunks_priv.h"
#include "dosobj.h"

static smpool pool;
static far_t base;
static int initialized;

static void err_printf(int prio, const char *fmt, ...) PRINTF(2);
static void err_printf(int prio, const char *fmt, ...)
{
    va_list vl;

    va_start(vl, fmt);
    fdvprintf(fmt, vl);
    va_end(vl);
}

void dosobj_init(far_t fa, int size)
{
    void *ptr = resolve_segoff(fa);

    if (initialized)
        smdestroy(&pool);
    sminit(&pool, ptr, size);
    smregister_error_notifier(&pool, err_printf);
    base = fa;
    initialized = 1;
}

static void do_free(void)
{
    int leaked;

    assert(initialized);
    leaked = smdestroy(&pool);
    assert(!leaked);
}

void dosobj_free(void)
{
    do_free();
    initialized = 0;
}

void dosobj_reinit(far_t fa, int size)
{
    void *ptr = resolve_segoff(fa);

    do_free();
    sminit(&pool, ptr, size);
    base = fa;
}

far_t mk_dosobj(UWORD len)
{
    void *ptr;
    uint16_t offs;
    far_t ret;

    ___assert(initialized);
    ptr = smalloc(&pool, len);
    if (!ptr) {
        fdprintf("dosobj: OOM! len=%i\n", len);
        _fail();
    }
    offs = (uintptr_t)ptr - (uintptr_t)smget_base_addr(&pool);
    ret = base;
    ret.off += offs;
    return ret;
}

void pr_dosobj(far_t fa, const void *data, UWORD len)
{
    void *ptr = resolve_segoff(fa);

    memcpy(ptr, data, len);
}

void cp_dosobj(void *data, far_t fa, UWORD len)
{
    void *ptr = resolve_segoff(fa);

    memcpy(data, ptr, len);
}

void rm_dosobj(far_t fa)
{
    void *ptr = resolve_segoff(fa);

    smfree(&pool, ptr);
}

uint16_t dosobj_seg(void)
{
    return base.seg;
}

void dosobj_dump(void)
{
    smdump(&pool);
}
