/* pl1dsp.c -- the DISPLAY statement.
 *
 * DISPLAY (message) is a WTO with route code 2, DISPLAY (message)
 * REPLY (c) a WTOR with route code 1 the task waits for, both with
 * descriptor code 7 (GC28-6594-7, "Communication with Program during
 * Execution").  As PL/I (F) does (#218, measured, JOB01915-01916):
 *   - the message and the reply are cut to 126 characters;
 *   - an empty message without REPLY writes nothing; with REPLY, an
 *     empty message or reply area is IHE025I and ERROR, ONCODE 1005;
 *   - the whole reply area is set to blanks before the WTOR, and a
 *     VARYING one gets its maximum length: the reply comes padded;
 *   - with EVENT the program goes on, and a WAIT on the event waits for
 *     the reply (#228). */
#include <string.h>
#include "pl1rt.h"
#include "pl1int.h"

#define DSP_MAX  126                    /* message and reply, as PL/I (F) */
#define MCS_RD   0x8000                 /* route and descriptor codes given */
#define DESC_7   0x0200                 /* descriptor code 7 */
#define ROUTE_1  0x8000
#define ROUTE_2  0x4000

static void svc35(void *list)
{
    __asm__ __volatile__("L\t1,%0\n\tSVC\t35" : : "m"(list) : "0", "1", "14", "15");
}

static void wait1(volatile unsigned *ecb)
{
    __asm__ __volatile__("LA\t0,1\n\tL\t1,%0\n\tSVC\t1" : : "m"(ecb) : "0", "1", "14", "15");
}

static void put_half(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
}

void pl1_display(const char *msg, int len, char *reply, int rmax, int varying,
                 unsigned char *event)
{
    /* WTOR prefix (8), LL and MCS flags (4), text, descriptor and route
     * codes (4) */
    unsigned list[(8 + 4 + DSP_MAX + 4 + 3) / 4];
    unsigned char *w = (unsigned char *)list;
    volatile unsigned ecb = 0;
    char *area = reply;
    int n;

    if (reply && (len <= 0 || rmax <= 0)) {
        pl1i_errmsgc(1005, "IHE025I DISPLAY - MESSAGE OR REPLY AREA LENGTH ZERO");
        return;
    }
    if (len <= 0)
        return;
    if (len > DSP_MAX)
        len = DSP_MAX;
    if (reply) {
        if (varying) {
            put_half((unsigned char *)reply, (unsigned)rmax);
            area = reply + 2;
        }
        memset(area, ' ', (size_t)rmax);
        n = rmax > DSP_MAX ? DSP_MAX : rmax;
        list[0] = ((unsigned)n << 24) | ((unsigned)(size_t)area & 0xFFFFFFu);
        if (event) {
            /* EVENT: the event's ECB takes the reply's post; the event is
             * active and incomplete until a WAIT sees it posted (rt/pl1ev.c) */
            *(volatile unsigned *)(event + PL1_EV_ECB) = 0;
            event[PL1_EV_COMPL] = 0;
            event[PL1_EV_ACTIVE] = 0x80;
            list[1] = (unsigned)(size_t)(event + PL1_EV_ECB);
        } else {
            list[1] = (unsigned)(size_t)&ecb;
        }
        w += 8;
    }
    put_half(w, (unsigned)len + 4);
    put_half(w + 2, MCS_RD);
    memcpy(w + 4, msg, (size_t)len);
    put_half(w + 4 + len, DESC_7);
    put_half(w + 6 + len, reply ? ROUTE_1 : ROUTE_2);
    svc35(list);
    if (reply && !event)
        wait1(&ecb);
}
