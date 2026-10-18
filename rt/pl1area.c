/* pl1area.c -- AREA variables: allocation and freeing of based
 * variables in an area, and area assignment.
 *
 * The layout and the allocation rules are PL/I (F)'s, as GY28-6801-6
 * ("The Area Variable") describes them and as measured on PL/I (F)
 * V5.5 (internals/research/pli/tier29-measurement.md).  An area is 16 bytes of control
 * information, then its size in bytes:
 *
 *   +0   a flag byte (X'80': there is a free list), the size (3 bytes)
 *   +4   the end of the extent, an offset from the start of the area;
 *        0 while nothing was ever allocated
 *   +8   the largest free element (an offset)
 *   +12  zero: the end of the free list, a free element of length 0
 *
 * An element is a multiple of 8 bytes.  A free element holds its
 * length and the offset of the next smaller one; offsets count from
 * the start of the area, the control information included, so the
 * first allocation is at offset 16.  The words are big-endian, as on
 * the machine; they are read byte by byte so that the host test runs
 * the same code.
 */
#include <string.h>

#include "pl1rt.h"
#include "pl1int.h"

static unsigned get(const unsigned char *a, unsigned off)
{
    const unsigned char *p = a + off;
    return (unsigned)p[0] << 24 | (unsigned)p[1] << 16 | (unsigned)p[2] << 8 | p[3];
}

static void put(unsigned char *a, unsigned off, unsigned v)
{
    unsigned char *p = a + off;
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

/* the length and the next smaller element of a free element; the
 * control word at +4 serves as the "next" of the list's head (+8) */
#define LEN(o)  get(a, (o))
#define NEXT(o) get(a, (o) + 4)
#define SETNEXT(o, v) put(a, (o) + 4, (v))

/* the size declared plus 16: what the extent may reach */
static unsigned capacity(const unsigned char *a)
{
    return (get(a, 0) & 0xFFFFFFu) + 16;
}

void *pl1_area_alloc(void *area, int n)
{
    unsigned char *a = area;
    unsigned size = ((unsigned)n + 7) & ~7u, ext = get(a, 4), cur, prev, nx;

    if (ext == 0) {
        if (size + 16 > capacity(a))
            goto full;
        put(a, 4, size + 16);
        return a + 16;
    }
    if (!(a[0] & 0x80))
        goto from_end;
    /* the smallest free element that holds it: the list runs from the
     * largest down */
    prev = 4;
    cur = get(a, 8);
    if (size > LEN(cur))
        goto from_end;
    if (size == LEN(cur)) {
        nx = NEXT(cur);
        put(a, 8, nx);
        if (LEN(nx) == 0)
            a[0] = 0;
        return a + cur;
    }
    for (;;) {
        nx = NEXT(cur);
        if (size > LEN(nx)) {
            /* this one, split: the allocation at its start, the rest a
             * free element put in its place by size */
            unsigned rem = LEN(cur) - size, r = cur + size, p = prev, q = cur;
            SETNEXT(prev, nx);
            put(a, r, rem);
            for (;;) {
                q = NEXT(q);
                if (rem >= LEN(q))
                    break;
                p = q;
            }
            SETNEXT(p, r);
            SETNEXT(r, q);
            return a + cur;
        }
        prev = cur;
        cur = nx;
        if (size == LEN(nx)) {
            SETNEXT(prev, NEXT(cur));
            return a + cur;
        }
    }
from_end:
    if (size > capacity(a) - ext)
        goto full;
    put(a, 4, ext + size);
    return a + ext;
full:
    /* AREA; after a normal return of the on-unit the compiled code
     * makes the allocation again */
    pl1i_areacond(360);
    return 0;
}

void pl1_area_free(void *area, void *elem, int n)
{
    unsigned char *a = area;
    unsigned size = ((unsigned)n + 7) & ~7u, off = (unsigned)((unsigned char *)elem - a),
             end = off + size, ext = get(a, 4);

    if (end == ext) {
        /* the last element: the extent ends before it, and before a free
         * element that ended where it began */
        unsigned cur, prev = 4, pp;
        ext -= size;
        put(a, 4, ext);
        if (!(a[0] & 0x80))
            return;
        cur = get(a, 8);
        for (;;) {
            if (LEN(cur) == 0)
                return;
            if (cur + LEN(cur) == ext)
                break;
            prev = cur;
            cur = NEXT(cur);
        }
        if (cur == 16) {
            a[0] = 0;
            put(a, 4, 0);
            return;
        }
        pp = prev;
        put(a, 4, cur);
        SETNEXT(pp, NEXT(cur));
        /* the flag stays set when the list became empty, as on PL/I
         * (F) (JOB00420, FREE 4) */
        return;
    }
    if (!(a[0] & 0x80)) {
        a[0] = 0x80;
        put(a, 12, 0);
        put(a, 8, off);
        put(a, off, size);
        SETNEXT(off, 12);
        return;
    }
    {
        unsigned p, q;
        /* merged with a free element right after it ... */
        for (p = 4, q = NEXT(p); q != 12; p = q, q = NEXT(q))
            if (q == end) {
                size += LEN(q);
                SETNEXT(p, NEXT(q));
                break;
            }
        /* ... and with one right before it */
        for (p = 4, q = NEXT(p); q != 12; p = q, q = NEXT(q))
            if (q + LEN(q) == off) {
                off = q;
                size += LEN(q);
                SETNEXT(p, NEXT(q));
                break;
            }
        put(a, off, size);
        for (p = 4, q = NEXT(p); size < LEN(q); p = q, q = NEXT(q))
            ;
        SETNEXT(p, off);
        SETNEXT(off, q);
    }
}

void pl1_area_assign(void *target, const void *source)
{
    unsigned char *t = target;
    const unsigned char *s = source;
    unsigned ext = get(s, 4), tsize = get(t, 0) & 0xFFFFFFu;

    if (tsize + 16 < ext) {
        pl1i_areacond(361);         /* the assignment is not made */
        return;
    }
    /* the extent with the control information, the target's size kept */
    if (ext == 0)
        memmove(t + 4, s + 4, 12);
    else
        memmove(t, s, ext);
    put(t, 0, tsize);
    t[0] = s[0];
}

/* CURRENTSTORAGE of an area: its control bytes and its current extent,
 * the end of what was ever allocated (SC26-3114-01, CURRENTSTORAGE). */
int pl1_area_cstg(const void *area)
{
    unsigned end = get(area, 4);
    return end ? (int)end : 16;
}
