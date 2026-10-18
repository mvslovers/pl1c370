/* pl1abi.h -- what the compiler and the run time must agree on: the
 * layouts the generated code builds and the run time reads.  Included by
 * both sides (src/pl1c.h, rt/pl1rt.h, rt/pl1int.h), so that each value
 * is written down once.
 *
 * PL1_ABI_STAMP names a symbol the compiler references in every external
 * procedure (DC V(...)) and the run time defines (rt/pl1cond.c): a
 * compiler and a run time from different levels of this file then fail
 * at the link with an unresolved name.  Change the number with every
 * change of a layout below.  (#8) */
#ifndef PL1ABI_H
#define PL1ABI_H

#define PL1_ABI_STAMP "PL1RT002"

/* A procedure's frame (gen.c gen_proc): the save area first, as PDPPRLG
 * makes it (NAB at 76), then the words the run time reads. */
#define PL1_FR_ARGS    88       /* argument words for the run time's calls */
#define PL1_FR_SLINK  112       /* the frame of the enclosing block */
#define PL1_FR_ALIST  116       /* this invocation's argument list (R1 at entry) */
#define PL1_FR_CURF   120       /* the run time's current frame at entry (PL1CURF) */
#define PL1_FR_PINFO  124       /* the procedure's descriptor (struct pinfo) */
#define PL1_FR_STUB14 128       /* R14 while a raise stub runs */
#define PL1_FR_NOCV   132       /* the caller's PL1NOCV flags, and (133) the block's own */
#define PL1_FR_VARS   136       /* the block's own variables from here */

/* PL1NOCV, the conditions the run time tests itself: a bit set is the
 * condition disabled */
#define PL1_NOCV_CONV 1         /* NOCONVERSION */
#define PL1_NOCV_OFL  2         /* NOOVERFLOW */
#define PL1_NOCV_STRZ 4         /* NOSTRINGSIZE */

/* A procedure's descriptor (gen.c gen_pinfo): A(entry), the length of
 * the name with these flags, the name */
#define PL1_PI_BLOCK  0x80      /* a BEGIN block or on-unit */
#define PL1_PI_MAIN   0x40      /* the main procedure */
#define PL1_PI_LEN    0x3F      /* the name's length */

/* A file's attributes, in its descriptor (gen.c; rt/pl1iof.h struct
 * desc: CL8 name, F attributes, A(0), V(PL1KHOK) or 0, and from
 * PL1RT002 the DCB its ENVIRONMENT gives: F RECFM (the DCBRECFM bits
 * below, 0 none), F LRECL, F BLKSIZE) */
#define PL1_RECFM_F 0x80
#define PL1_RECFM_V 0x40
#define PL1_RECFM_U 0xC0
#define PL1_RECFM_B 0x10
#define PL1_RECFM_S 0x08
#define PL1_RECFM_A 0x04
#define PL1_RECFM_M 0x02
#define FL_INPUT    1
#define FL_OUTPUT   2
#define FL_PRINT    4
#define FL_STREAM   8
#define FL_RECORD   16
#define FL_UPDATE   64          /* RECORD UPDATE: READ, then REWRITE */
#define FL_KEYED    128         /* KEY, KEYFROM, KEYTO */
#define FL_DIRECT   256
#define FL_REGIONAL 512         /* ENVIRONMENT(REGIONAL(n)) */
#define FL_INDEXED  1024        /* ENVIRONMENT(INDEXED) */
#define FL_REG2     2048        /* ENVIRONMENT(REGIONAL(2)), with FL_REGIONAL */
#define FL_REG3     4096        /* ENVIRONMENT(REGIONAL(3)), with FL_REGIONAL */

/* The conditions, as the generated code passes them to the run time */
enum cond { C_NONE, C_ENDFILE, C_ZERODIVIDE, C_FIXEDOVERFLOW, C_CONVERSION,
            C_ERROR, C_FINISH, C_CONDITION, C_UNDERFLOW, C_SIZE, C_SUBRG, C_STRG,
            C_OVERFLOW, C_ENDPAGE, C_UNDF, C_RECORD, C_AREA, C_KEY, C_NAME, C_CHECK, C_TRANSMIT,
            C_STRINGSIZE, C_STORAGE /* ANSI (Tier 70) */,
            C_ATTENTION /* the Optimizer's (Tier 106) */ };

/* A program that names ATTENTION references PL1ATTN, the run time's
 * attention flag (rt/pl1cond.c): with a run time older than the
 * condition the link fails, where a raise would have gone wrong. */

/* An EVENT variable (#3): 32 bytes, fullword aligned, as PL/I (F)'s
 * (its PLM, p. 239), so that a structure holding one has PL/I (F)'s
 * mapping.  A new one is incomplete with status 0: AUTOMATIC ones are
 * cleared at their block's entry, STATIC ones are in zeroed storage. */
#define PL1_EV_SIZE    32
#define PL1_EV_ECB      0       /* an ECB, for what completes it (DISPLAY ... EVENT) */
#define PL1_EV_COMPL    4       /* COMPLETION: X'80' complete */
#define PL1_EV_STATUS   8       /* STATUS, FIXED BINARY(31) (t228: its PUT LIST field) */
#define PL1_EV_ACTIVE  12       /* X'80' active: an operation will complete it */
#define PL1_EV_COND    13       /* EVENT I/O: a condition the WAIT raises, or 0 */
#define PL1_EV_ONCODE  14       /* ... its ONCODE, a halfword */
#define PL1_EV_DESC    16       /* ... A(the file's descriptor) */

/* CALL PLISRTA to PLISRTD (Tier 107): the argument block whose address
 * the generated code passes to PL1SORT (rt/pl1sort.c).  A string is
 * A(data) and its length; an exit A(its entry value: A(procedure),
 * A(parent frame)), 0 when there is none; the prefix's length -1 when
 * it is omitted.  A new entry point and a new layout, so no stamp:
 * a run time without PL1SORT fails at the link. */
#define PL1_SA_SORT     0       /* the SORT statement */
#define PL1_SA_RECORD   8       /* the RECORD statement */
#define PL1_SA_STORAGE 16       /* the main storage for the sort */
#define PL1_SA_RETCODE 20       /* A(the return code variable) */
#define PL1_SA_E15     24       /* A(E15's entry value), or 0 */
#define PL1_SA_E35     28       /* A(E35's entry value), or 0 */
#define PL1_SA_PREFIX  32       /* the ddname prefix */
#define PL1_SA_E15RET  40       /* E15's RETURNS: its length, PL1_SA_VARYING */
#define PL1_SA_E35PAR  44       /* E35's parameter: PL1_SA_BYDESC and the
                                   longest record it takes (times 256), or 0 */
#define PL1_SA_SIZE    48
#define PL1_SA_VARYING 0x10000  /* RETURNS(CHARACTER(n) VARYING) */
#define PL1_SA_BYDESC  1        /* by a descriptor, A(data) and length: CHAR(*),
                                   a parameter of an external procedure */

#endif
