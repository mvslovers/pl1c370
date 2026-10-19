/* pl1int.h -- internals shared by the run time's source files. */
#ifndef PL1INT_H
#define PL1INT_H

/* Right-justify nd decimal digits d (q after the point) in a field of
 * p+3 characters; returns the width. */
int pl1i_format_number(char *field, const char *d, int nd, int p, int q, int neg);
/* Digits of the scaled binary n/2**q, qd of them after the point. */
int pl1i_scaled_digits(char *d, int n, int q, int qd, int *neg);
/* Digits of a packed decimal of precision p (p of them). */
int pl1i_dec_digits(char *d, const unsigned char *packed, int p, int *neg);
int pl1i_ceil_div332(int x);
/* A number written as a float (pl1rt.c). */
void pl1i_parse_float(char *t, int n, int implied, int code, int blank_error, int longf,
                      unsigned char *out);
/* A float in the PUT LIST form (p+6 wide); returns the width. */
int pl1i_flt_field(char *field, const unsigned char *v, int longf, int p);

#include "pl1abi.h"             /* enum cond, as the compiler passes it */

/* CONVERSION for the character at src[idx]; returns after a normal return
 * of the on-unit, which has changed the field: convert it again. */
int pl1i_conversion(int oncode, char *src, int len, int idx);
/* the complex constant reader (rt/pl1cxsp.c), once a program with
 * COMPLEX data has called PL1CXON; NULL otherwise, so that a program
 * without complex values links none of it (rt/pl1rt.c) */
extern int (*pl1x_cxsplit)(const char *t, int n, int *rs, int *rn, int *is, int *in)
    asm("PL1XCSP");
int pl1i_cpx_real(char *s, int len, int code, int *rs, int *rn) asm("PL1CXRL");
int pl1i_endpage(void *desc);
void pl1i_filecond(int cond, int oncode, void *desc);
/* GET DATA: the data field read last, for DATAFIELD */
void pl1i_datafield(const char *p, int len);
/* ANSI's STRINGSIZE and STORAGE (pl1cond.c, Tier 70) */
void pl1i_strsize(void) asm("PL1SZCK");
int pl1i_storage(void) asm("PL1STOR");
/* EVENT I/O (pl1cond.c): complete a posted event, raising its condition */
void pl1i_evraise(unsigned char *ev) asm("PL1EVRS");
/* GET ... COPY (pl1io.c): on for the GET running; a field to SYSPRINT */
extern int pl1i_copy asm("PL1ICPY");
void pl1i_copyline(const char *p, int n) asm("PL1ICPL");
void pl1i_areacond(int oncode);

/* A system-action message with the AT OFFSET ... part, then going on or
 * raising ERROR (pl1cond.c). */
void pl1i_errmsg(const char *text);
void pl1i_errmsgc(int oncode, const char *text) asm("PL1IERRC");
void pl1i_fileerr(int oncode, int msgno, const char *text, void *desc) asm("PL1IFERR");

/* A message of the system action on SYSPRINT, as PL/I (F) places it. */
void pl1i_message(const char *text);

/* SYSPRINT has no DD (pl1io.c): the messages go to the console, as
 * pl1i_console() writes them (pl1dsp.c) */
extern int pl1i_nosysprint asm("PL1INOSP");
void pl1i_console(const char *text) asm("PL1ICONS");

/* PLIDUMP (pl1dump.c): the on-unit activation raise_cond made between
 * the frames lo and hi -- its own C frame lies between the raising
 * block's frame and the on-unit's (pl1cond.c); 0 if none.  The open
 * files (pl1io.c).  The end of the program without FINISH, the step
 * code thousands plus PLIRETC's (pl1cond.c). */
/* ATTENTION (pl1cond.c, rt/pl1attn.asm): the flag the attention exit
 * sets, the exit established and cancelled, the poll */
extern volatile unsigned char pl1_attn asm("PL1ATTN");
void pl1_stax_on(void) asm("PL1STXN");
void pl1_stax_off(void) asm("PL1STXF");

struct pl1i_unit {
    int cond, oncode;
    const char *src;             /* CONVERSION: ONSOURCE, ONCHAR at idx */
    int srclen, idx;
    const char *key;             /* KEY, RECORD: ONKEY */
    int keylen;
    const char *loc;             /* ONLOC */
    int loclen;
};
int pl1i_unit_at(const char *lo, const char *hi, struct pl1i_unit *u) asm("PL1IUNIT");
struct pfile *pl1i_files(void) asm("PL1IFILS");
void pl1i_quit(int thousands) asm("PL1IQUIT");

/* The state of the GET or PUT statement running, kept across an on-unit. */
struct pl1i_iostate { void *cur; int mode; const short *fmt; int fi; int rsp;
                      short rst[8][3]; };
void pl1i_iosave(struct pl1i_iostate *s);
void pl1i_iorest(const struct pl1i_iostate *s);

#endif
