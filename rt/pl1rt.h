/* pl1rt.h -- run time of PL1C/370: the routines generated code calls.
 *
 * Generated code calls these with the linkage of CC/370: R1 addresses a
 * list of argument VALUES, R13 is a frame of the C stack.  The external
 * names are fixed by the asm labels, eight characters at most; the
 * compiler emits exactly these names.
 */
#ifndef PL1RT_H
#define PL1RT_H

#include "pl1abi.h"             /* what the compiler and the run time agree on */

/* Stream output to SYSPRINT, PUT LIST semantics of PL/I (F) V5.5. */
void pl1_skip(int n)                                  asm("PL1SKIP");
void pl1_page(void)                                   asm("PL1PAGE");
void pl1_list_char(const char *p, int len)            asm("PL1LCHR");
void pl1_list_varchar(const void *v)                  asm("PL1LVCH");
void pl1_list_number(const char *p, int len)          asm("PL1LNUM");
/* a complex value's characters from its parts' (rt/pl1cx.c) */
int  pl1_cpx_join(char *dst, const char *re, int rl, const char *im, int il)
                                                      asm("PL1CPXJ");
void pl1_list_fixbin(int value, int prec)             asm("PL1LFXB");
void pl1_list_fixdec(const unsigned char *packed, int p, int q)
                                                      asm("PL1LFXD");

/* FIXED BINARY(p,q): converted to FIXED DECIMAL(1+CEIL(p/3.32),
 * CEIL(q/3.32)), the binary fraction truncated. */
void pl1_list_fixbin_scaled(int n, int p, int q)      asm("PL1LFXS");
void pl1_list_bit(unsigned int v, int n)              asm("PL1LBIT");

/* A number as a string (the PUT LIST field, p+3 characters): written to
 * dst, the length returned. */
int  pl1_format_fixbin(char *dst, int n, int p, int q)
                                                      asm("PL1FFXB");
int  pl1_format_fixdec(char *dst, const unsigned char *packed, int p, int q)
                                                      asm("PL1FFXD");
/* Scaled binary n/2**q to an 8-byte packed decimal with qd fraction
 * digits, truncated. */
void pl1_bin_to_dec(int n, int q, int qd, unsigned char *dst)
                                                      asm("PL1BTOD");
/* a || b into dst; returns the length. */
int  pl1_concat(char *dst, const char *a, int al, const char *b, int bl)
                                                      asm("PL1CAT");
/* INDEX(s,t): position of t in s, 0 if absent (and for an empty t). */
int  pl1_index(const char *s, int sl, const char *t, int tl)
                                                      asm("PL1INDX");

/* CHARACTER assignment: pad with blanks or truncate. */
void pl1_assign_char(char *dst, int dlen, const char *src, int slen)
                                                      asm("PL1ACHR");
/* CHARACTER VARYING assignment: the length becomes min(slen, maxlen). */
void pl1_assign_varchar(void *dst, int maxlen, const char *src, int slen)
                                                      asm("PL1AVCH");
/* String comparison, the shorter operand padded with blanks: -1, 0, 1. */
int  pl1_compare_char(const char *a, int alen, const char *b, int blen)
                                                      asm("PL1CMPC");

/* Files: a descriptor in the program (DD name, flags, a state word). */
void pl1_select(void *desc, int mode)                 asm("PL1SEL");
int  pl1_count(void *desc)                            asm("PL1COUN");
int  pl1_read_ignore(void *desc, int n)                asm("PL1RIGN");
void pl1_get_copy(void *desc)                         asm("PL1GCPY");
int  pl1_lineno(void *desc)                           asm("PL1LINE");
extern int pl1_icount                                 asm("PL1ICNT");
void pl1_open(void *desc, int attrs, const char *title, int tlen)
                                                      asm("PL1OPEN");
void *pl1_main_parm(int varying)                     asm("PL1MPRM");
void *pl1_main_plist(int n)                          asm("PL1MPLS");
int __premain(char *parm, char *pgmname, void **pgmr1);
void pl1_open_opts(void *desc, int ls, int ps, const int *tabs, int ntabs)
                                                      asm("PL1OPTS");
void pl1_close(void *desc)                            asm("PL1CLOS");
/* PUT EDIT / GET EDIT: the format list of the statement, then the items
 * (the PL1L... routines above, which follow the format in EDIT mode). */
void pl1_format_begin(const short *list)              asm("PL1FBEG");
void pl1_format_end(void)                             asm("PL1FEND");
/* GET: each returns 0, or 1 at end of file. */
void pl1_get_skip(int n)                              asm("PL1GSKP");
int  pl1_get_fixbin(void *target, int p)              asm("PL1GFXB");
int  pl1_get_fixdec(void *target, int p, int q)       asm("PL1GFXD");
int  pl1_get_char(char *target, int len)              asm("PL1GCHR");
int  pl1_get_varchar(void *target, int maxlen)        asm("PL1GVCH");
int  pl1_get_bit(void *target, int nbits)             asm("PL1GBIT");
int  pl1_get_vbit(void *target, int maxbits)          asm("PL1GVBT");

/* A string to FIXED DECIMAL(15,q), truncated, in 8 packed bytes. */
void pl1_char_to_dec(char *s, int len, int q, unsigned char *dst)
                                                      asm("PL1CTOD");
/* RECORD files: READ returns 0, or 1 at end of file. */
int  pl1_read(void *desc, void *target, int len)      asm("PL1READ");
void pl1_rewrite(void *desc, const void *source, int len)       asm("PL1RWRT");
/* QSAM in update mode (rt/pl1qsam.asm) */
int  pl1_qopen(void *dcb, const char *dd8)            asm("PL1QOPN");
int  pl1_qget(void *dcb, unsigned char **rec)         asm("PL1QGET");
void pl1_qputx(void *dcb)                             asm("PL1QPTX");
void pl1_qclose(void *dcb)                            asm("PL1QCLS");
void pl1_rewrite_varying(void *desc, const void *source, int maxlen)
                                                      asm("PL1RWRV");
int  pl1_read_varying(void *desc, void *target, int maxlen)
                                                      asm("PL1READV");
void pl1_write(void *desc, const void *source, int len)
                                                      asm("PL1WRIT");
void pl1_write_varying(void *desc, const void *source, int maxlen)
                                                      asm("PL1WRTV");
/* READ ... SET(p): p is set to the record (until the next READ);
 * returns 1 at end of file.  REWRITE without FROM: the record read so,
 * changed in place.  LOCATE x FILE(f): a buffer for the next output
 * record of len bytes, p set to it (+ phase), written by the next WRITE,
 * LOCATE or CLOSE. */
int  pl1_read_set(void *desc, void **p)               asm("PL1RSET");
void pl1_rewrite_set(void *desc)                      asm("PL1RWST");
void pl1_locate(void *desc, int len, int phase, void **p) asm("PL1LOCT");
/* LOCATE KEYFROM (INDEXED, SEQUENTIAL OUTPUT); READ SET with KEY and
 * with KEYTO (pl1iokey.c) */
void pl1_locate_key(void *desc, int len, int phase, void **p, const char *key, int klen)
                                                      asm("PL1LOCK");
void pl1_read_set_key(void *desc, void **p, const char *key, int klen) asm("PL1RSKY");
int  pl1_read_set_keyto(void *desc, void **p, char *kt, int ktlen)    asm("PL1RSKT");
/* KEYED files: REGIONAL(1) (ENVIRONMENT(REGIONAL(1)), DIRECT access):
 * key, klen the source key as a character string; kt the KEYTO
 * variable.  pl1_read_keyto returns 0, or 1 at end of file. */
void pl1_read_key(void *desc, void *target, int len, const char *key, int klen)
                                                      asm("PL1RDKY");
int  pl1_read_keyto(void *desc, void *target, int len, char *kt, int ktlen)
                                                      asm("PL1RDKT");
void pl1_write_key(void *desc, const void *source, int len, const char *key, int klen)
                                                      asm("PL1WRKY");
void pl1_rewrite_key(void *desc, const void *source, int len, const char *key, int klen)
                                                      asm("PL1RWKY");
void pl1_delete_key(void *desc, const char *key, int klen)
                                                      asm("PL1DLKY");
/* BSAM and BDAM for REGIONAL(1) (rt/pl1bdam.asm), QSAM input */
int  pl1_bopen(void *a, const char *dd8, int mode)    asm("PL1BOPN");
int  pl1_bwrite(void *a, void *buf, int dummy)        asm("PL1BWRT");
int  pl1_bdread(void *a, void *buf, int block)        asm("PL1BDRD");
int  pl1_bdwrite(void *a, void *buf, int block)       asm("PL1BDWR");
void pl1_bclose(void *a)                              asm("PL1BCLS");
int  pl1_qopeni(void *dcb, const char *dd8)           asm("PL1QOPI");
/* REGIONAL(2) and (3): BDAM with recorded keys (rt/pl1bdk.asm) */
int  pl1_kopen(void *a, const char *dd8, int mode, int limct) asm("PL1KOPN");
int  pl1_kio(void *a, int op, void *key, void *data, int *ref) asm("PL1KIO");
/* INDEXED (ENVIRONMENT(INDEXED)): QISAM and BISAM (rt/pl1isam.asm) */
void pl1_delete_seq(void *desc)                       asm("PL1DLSQ");
int  pl1_iopen(void *a, const char *dd8, int mode)    asm("PL1IOPN");
void pl1_iinfo(void *a, int *v)                       asm("PL1IINF");
int  pl1_iput(void *a, void *rec)                     asm("PL1IPUT");
int  pl1_iget(void *a, unsigned char **rec)           asm("PL1IGET");  /* rec[2] */
void pl1_iputx(void *a)                               asm("PL1IPTX");
int  pl1_ibisam(void *a, int op, void *area, void *key, unsigned char **rec)
                                                      asm("PL1IBIS");
int  pl1_isetk(void *a, void *key)                    asm("PL1ISTK");
void pl1_iclose(void *a)                              asm("PL1ICLS");

/* Program end: write the last line. */
void pl1_finish(void)                                 asm("PL1FINI");

/* FLOAT (pl1rt.c, pl1io.c): a float is 4 or 8 bytes (longf), p the
 * decimal digits PUT LIST writes. */
void pl1_bin_to_float(int v, int q, int longf, unsigned char *dst)   asm("PL1BTOF");
void pl1_dec_to_float(const unsigned char *packed, int q, int longf, unsigned char *dst)
                                                      asm("PL1DTOF");
void pl1_char_to_float(char *s, int len, int longf, unsigned char *dst)
                                                      asm("PL1CTOF");
int  pl1_float_to_bin(const unsigned char *src, int longf, int q)  asm("PL1FTOB");
void pl1_float_to_dec(const unsigned char *src, int longf, int q, unsigned char *dst)
                                                      asm("PL1FTOD");
int  pl1_float_to_char(char *dst, const unsigned char *src, int longf, int p)
                                                      asm("PL1FTOC");
void pl1_list_float(const unsigned char *src, int longf, int p)    asm("PL1LFLT");
int  pl1_get_float(void *target, int longf)                        asm("PL1GFLT");

/* The STRING option and data-directed I/O (pl1io.c). */
void pl1_select_string(void *s, int len, int kind, int mode)       asm("PL1SSEL");
void pl1_string_end(void)                                          asm("PL1SEND");
void pl1_data_name(const char *name, int len, int nsubs, int s1, int s2, int s3)
                                                      asm("PL1DNAM");
void pl1_data_end(void)                                            asm("PL1DEND");
int  pl1_get_data(const int *table, int n)                         asm("PL1GDAT");

/* The DISPLAY statement (pl1dsp.c). */
void pl1_display(const char *msg, int len, char *reply, int rmax, int varying,
                 unsigned char *event)
                                                      asm("PL1DSP");

/* INTERRUPT (-interrupt): the attention exit, established once; an
 * attention pending raises ATTENTION (pl1cond.c) */
void pl1_attn_on(void)                                asm("PL1ATON");
void pl1_attn_poll(void)                              asm("PL1APOL");

/* CALL PLIDUMP (pl1dump.c): an argument omitted has the length -1. */
void pl1_plidump(const char *opt, int olen, const char *id, int idlen)
                                                      asm("PL1DUMP");
/* CALL PLISRTA to PLISRTD: the argument block of rt/pl1abi.h (PL1_SA_...) */
void pl1_plisrt(const char *args)                         asm("PL1SORT");
/* EVENT I/O around a READ, WRITE or REWRITE (rt/pl1cond.c) */
void pl1_evio_begin(unsigned char *ev)                    asm("PL1EVIO");
void pl1_evio_end(void)                                   asm("PL1EVEN");
/* WAIT and DELAY (rt/pl1ev.c, rt/pl1dly.asm) */
void pl1_wait(const void *list, int n, int count)         asm("PL1WAIT");
void pl1_delay(int ms)                                    asm("PL1DLAY");
/* PLICKPT, PLIREST, PLICANC (rt/pl1ckpt.asm) */
void pl1_plickpt(const void *block)                       asm("PL1CKPT");
void pl1_plirest(void)                                    asm("PL1REST");
void pl1_plicanc(void)                                    asm("PL1CANC");

/* CURRENTSTORAGE of an area (pl1area.c). */
int pl1_area_cstg(const void *area)                                asm("PL1ACST");

/* Built-in functions (pl1rt.c). */
int  pl1_verify(const char *s, int sl, const char *c, int cl)      asm("PL1VERI");
void pl1_translate(char *s, int sl, const char *r, int rl, const char *p, int pl)
                                                      asm("PL1TRAN");
int  pl1_repeat(char *dst, const char *s, int sl, int n)            asm("PL1REPT");
/* pl1ansi.c: ANSI's string functions (Tier 65) */
int  pl1_after(char *dst, const char *s, int sl, const char *c, int cl)   asm("PL1AFTR");
int  pl1_before(char *dst, const char *s, int sl, const char *c, int cl)  asm("PL1BEFR");
int  pl1_decat(char *dst, const char *s, int sl, const char *c, int cl,
               const unsigned char *pa)                                 asm("PL1DCAT");
int  pl1_reverse(char *dst, const char *s, int sl)                      asm("PL1REVR");
int  pl1_copy(char *dst, const char *s, int sl, int n, int max)         asm("PL1COPY");
char *pl1_collate(void)                                                 asm("PL1COLL");
int  pl1_after_b(unsigned char *dst, const unsigned char *s, int sl,
                 const unsigned char *c, int cl)                        asm("PL1AFTB");
int  pl1_before_b(unsigned char *dst, const unsigned char *s, int sl,
                  const unsigned char *c, int cl)                       asm("PL1BEFB");
int  pl1_decat_b(unsigned char *dst, const unsigned char *s, int sl,
                 const unsigned char *c, int cl, const unsigned char *pa) asm("PL1DCAB");
int  pl1_reverse_b(unsigned char *dst, const unsigned char *s, int sl)    asm("PL1REVB");
int  pl1_index_b(const unsigned char *s, int sl, const unsigned char *c, int cl) asm("PL1INDB");
int  pl1_copy_b(unsigned char *dst, const unsigned char *s, int sl, int n, int max)
                                                                        asm("PL1COPB");
void pl1_valid(unsigned char *dst, const char *s, int sl, const char *pic, int pn,
               int numeric)                                             asm("PL1VALD");
/* pl1io.c: PAGENO(f), and PAGENO(f) = n (ANSI 9.4.4.62, 7.5.4.4) */
int  pl1_pageno(void *desc)                                             asm("PL1PGNO");
void pl1_set_pageno(void *desc, int n)                                  asm("PL1SPGN");
void pl1_fill(char *dst, int n, int byte)                           asm("PL1FILL");
void pl1_float_power(const unsigned char *x, int longf, int n, unsigned char *dst)
                                                      asm("PL1PWRF");
void pl1_float_int(const unsigned char *x, int longf, int mode, unsigned char *dst)
                                                      asm("PL1FINT");

/* Storage (pl1rt.c): ALLOCATE and FREE of BASED variables, doubleword
 * aligned, zeroed. */
void *pl1_allocate(int size)                          asm("PL1ALOC");
void pl1_free(void *p)                                asm("PL1FREE");
/* Areas (pl1area.c): n the length of the based variable plus its offset
 * from a doubleword.  pl1_area_alloc returns 0 after the AREA on-unit
 * returned normally: the allocation is made again. */
void *pl1_area_alloc(void *area, int n)               asm("PL1AALC");
void pl1_area_free(void *area, void *elem, int n)     asm("PL1AFRE");
void pl1_area_assign(void *target, const void *source) asm("PL1AASG");

/* Conditions (pl1cond.c).  An establishment is 24 bytes of the
 * establishing frame; cond is the compiler's enum cond, qual the file
 * descriptor (ENDFILE) or AL1(n),C'name' (CONDITION), unit the on-unit's
 * entry or 0 for SYSTEM. */
void pl1_on(void *est, int cond, const void *qual, void *unit, void *frame)
                                                      asm("PL1ON");
void pl1_revert(void *est)                            asm("PL1REVT");
void pl1_block_exit(void *frame)                      asm("PL1BEXT");
void pl1_goto(void *frame)                            asm("PL1GOTO");
void pl1_signal(int cond, const void *qual, int oncode)
                                                      asm("PL1SIG");
void pl1_end_main(void)                               asm("PL1EMAIN");
void pl1_math_error(int code)                         asm("PL1MER2");
void pl1_program_check(int code, unsigned addr)       asm("PL1PICK");
/* PICTURE: the editing is rt/pl1pic.h's */
void pl1_list_pic(const char *chars, int len, const unsigned char *packed, int q)
                                                      asm("PL1LPIC");
void pl1_pic_assign(char *t, const char *pic, int n, const char *src, int len)
                                                      asm("PL1PASG");
/* bit strings in storage: lengths in bits */
int  pl1_bit_op(unsigned char *d, int op, const unsigned char *a, int la,
                const unsigned char *b, int lb)       asm("PL1BOP");
void pl1_bit_not(unsigned char *d, const unsigned char *a, int la)   asm("PL1BNOT");
int  pl1_bit_cat(unsigned char *d, const unsigned char *a, int la,
                 const unsigned char *b, int lb)      asm("PL1BCAT");
int  pl1_bit_sub(unsigned char *d, const unsigned char *a, int la, int i, int j)
                                                      asm("PL1BSUB");
int  pl1_bit_cmp(const unsigned char *a, int la, const unsigned char *b, int lb)
                                                      asm("PL1BCMP");
int  pl1_bit_any(const unsigned char *a, int la)      asm("PL1BANY");
void pl1_bit_assign(unsigned char *d, int dmax, int varying, const unsigned char *s, int ls)
                                                      asm("PL1BASG");
/* packed bits (an unaligned bit string at a bit offset): the n bits at
 * bit off of s into d, aligned; and s (ls bits) assigned to the n bits
 * at bit off of d, the bits around them kept */
void pl1_bit_get(unsigned char *d, const unsigned char *s, int off, int n) asm("PL1BGET");
int  pl1_bit_bool(unsigned char *d, const unsigned char *a, const unsigned char *b, int lab,
                  const unsigned char *z, int lz)            asm("PL1BBOL");
void pl1_bit_put(unsigned char *d, int off, int n, const unsigned char *s, int ls)
                                                      asm("PL1BPUT");
void pl1_bit_to_char(char *d, const unsigned char *a, int la)        asm("PL1BTOC");
int  pl1_bit_to_int(const unsigned char *a, int la)   asm("PL1BTOI");
void pl1_list_bits(const unsigned char *a, int la)    asm("PL1LBTS");
void pl1_bit_setsub(unsigned char *b, int bl, int i, int j, const unsigned char *s, int sl)
                                                      asm("PL1BSET");
void pl1_int_to_bit(unsigned char *d, int v, int n)  asm("PL1ITOB");
void pl1_char_to_bit(unsigned char *d, const char *s, int len)      asm("PL1CTOB");
void pl1_pie_on(void)                                 asm("PL1PION");
void pl1_pie_off(void)                                asm("PL1PIOF");
void *pl1_fetch(const char *name)                     asm("PL1FTCH");
void pl1_release(const char *name)                    asm("PL1RLSE");
void pl1_recovery_on(void)                            asm("PL1STON");
void pl1_subscript_range(int cond, const void *qual, int oncode)   asm("PL1SUBR");
void pl1_substr_range(int len, int i, int j, int hasj, int *out)   asm("PL1SBSR");
/* STOP and EXIT: FINISH, then the end with step code 1000. */
void pl1_stop(void)                                   asm("PL1STOP");
int  pl1_oncode(void)                                 asm("PL1ONCD");
char *pl1_onsource_addr(void)                         asm("PL1ONSA");
int  pl1_onsource_len(void)                           asm("PL1ONSL");
char *pl1_onchar_addr(void)                           asm("PL1ONCA");
char *pl1_onfile_addr(void)                           asm("PL1ONFA");
int  pl1_onfile_len(void)                             asm("PL1ONFL");
/* ONKEY: the key of a KEY or RECORD condition in its on-unit, else a
 * null string; the keyed run time gives it before the condition */
char *pl1_onkey_addr(void)                            asm("PL1ONKA");
int  pl1_onkey_len(void)                              asm("PL1ONKL");
/* pl1date.c: DATE (6 characters), TIME (9); pl1cond.c: ONLOC, the
 * entry point of the block that raised the condition of the on-unit
 * running */
char *pl1_date(void)                                  asm("PL1DATE");
char *pl1_datetime(void)                              asm("PL1DTTM");
void pl1_pliretc(int n)                               asm("PL1RETC");
char *pl1_time(void)                                  asm("PL1TIME");
char *pl1_onloc_addr(void)                            asm("PL1ONLA");
char *pl1_datafield_addr(void)                        asm("PL1DFLA");
int  pl1_check(const unsigned char *qual, int oncode)  asm("PL1CHK");
/* pl1io.c: around the PUT DATA of CHECK's system action, as a message */
void pl1_check_begin(void)                            asm("PL1CKB");
void pl1_check_end(void)                              asm("PL1CKE");
int  pl1_datafield_len(void)                          asm("PL1DFLL");
int  pl1_onloc_len(void)                              asm("PL1ONLL");
void pl1_setkey(const char *key, int len)             asm("PL1STKY");
void pl1_set_onsource(const char *s, int len)         asm("PL1SONS");
void pl1_set_onchar(const char *s, int len)           asm("PL1SONC");

#endif
