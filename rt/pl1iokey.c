/* pl1iokey.c -- KEYED files of PL1C/370's run time, REGIONAL(1) and
 * INDEXED, out of pl1io.c: reached through the file descriptor's hook,
 * which the compiler sets only for such files. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "pl1rt.h"
#include "pl1pic.h"
#include "pl1int.h"
#include "pl1flt.h"
#include "pl1iof.h"

/* ---- REGIONAL(1) ------------------------------------------------------- */

/* As PL/I (F) (internals/research/pli/tier30-measurement.md): a region
 * is a record, numbered from 0; a dummy record has X'FF' in its first
 * byte.  SEQUENTIAL OUTPUT creates the data set in ascending regions,
 * dummy records in the gaps -- the record written before them, its first
 * byte X'FF' -- and at CLOSE up to the end of the extent; DIRECT OUTPUT
 * fills the whole extent with dummy records at OPEN (X'FF', then zeros:
 * PL/I (F)'s are what its buffer held).  DELETE writes X'FF' and zeros.
 * SEQUENTIAL INPUT and UPDATE read every record, dummy ones included. */

static void dd8_of(struct pfile *f, char *dd)
{
    size_t n = strlen(f->dd);
    memset(dd, ' ', 8);
    memcpy(dd, f->dd, n > 8 ? 8 : n);
}

static void regional_fail(struct pfile *f)
{
    if (f->desc)
        pl1i_filecond(C_UNDF, 84, f->desc);
    fprintf(stderr, "PL1C/370: REGIONAL file %s cannot be opened (DD %s missing?)\n", f->dd,
            f->dd);
    exit(16);
}

/* the record length, BLKSIZE of the DCB opened, and a buffer for it */
static void regional_buffer(struct pfile *f, const unsigned char *dcb)
{
    int n = (dcb[62] << 8) | dcb[63];
    if (!f->rbuf || n > f->reglen) {
        free(f->rbuf);
        f->rbuf = calloc(1, (size_t)(n > 0 ? n : 1));
        if (!f->rbuf) {
            fputs("PL1C/370: out of storage for a record\n", stderr);
            exit(16);
        }
    }
    f->reglen = n;
}

static void indexed_open(struct pfile *f, int output);
static unsigned char *fit_key(struct pfile *f, const char *key, int klen);
static int isam_error(struct pfile *f, int exc, int notfound);
static int embedded_differs(struct pfile *f, const void *source, int len, const unsigned char *k);

static void keyed_open(struct pfile *f, int output);

static void regional_open(struct pfile *f, int output)
{
    char dd[8];
    if (f->flags & FL_INDEXED) {
        indexed_open(f, output);
        return;
    }
    if (f->flags & (FL_REG2 | FL_REG3)) {
        keyed_open(f, output);
        return;
    }
    dd8_of(f, dd);
    if (!f->rarea)
        f->rarea = calloc(1, 160);
    if (!f->qdcb)
        f->qdcb = calloc(1, 128);
    if (!f->rarea || !f->qdcb)
        regional_fail(f);
    f->eof = 0;
    f->trel = 0;
    f->qmode = 0;
    f->qrec = NULL;
    if (f->flags & FL_DIRECT) {
        if (output) {
            /* creation: every region a dummy record first */
            if (pl1_bopen(f->rarea, dd, 0) != 0)
                regional_fail(f);
            regional_buffer(f, f->rarea);
            memset(f->rbuf, 0, (size_t)f->reglen);
            f->rbuf[0] = 0xFF;
            while (pl1_bwrite(f->rarea, f->rbuf, 1) != 8)
                ;
            pl1_bclose(f->rarea);
        }
        if (pl1_bopen(f->rarea, dd, (f->flags & FL_INPUT) && !output ? 2 : 1) != 0)
            regional_fail(f);
        regional_buffer(f, f->rarea);
        f->rkind = R_DIRECT;
    } else if (output) {
        if (pl1_bopen(f->rarea, dd, 0) != 0)
            regional_fail(f);
        regional_buffer(f, f->rarea);
        f->rnext = 0;
        f->rkind = R_CREATE;
        f->flags |= FL_OUTPUT;
    } else {
        if ((f->flags & FL_UPDATE ? pl1_qopen(f->qdcb, dd) : pl1_qopeni(f->qdcb, dd)) != 0)
            regional_fail(f);
        f->qmode = f->flags & FL_UPDATE ? 1 : 2;
        f->rkind = R_SEQ;
        f->flags |= FL_INPUT;
    }
    f->open = 1;
}

static void regional_close(struct pfile *f)
{
    if (f->rkind == R_ICREATE || f->rkind == R_IDIRECT || f->rkind == R_ISEQ) {
        pl1_iclose(f->rarea);
        f->rkind = R_NONE;
        f->open = 0;
        return;
    }
    if (f->rkind == R_KCREATE || f->rkind == R_KDIRECT || f->rkind == R_KSEQ) {
        pl1_bclose(f->rarea);
        f->rkind = R_NONE;
        f->open = 0;
        return;
    }
    if (f->rkind == R_CREATE) {
        f->rbuf[0] = 0xFF;
        while (pl1_bwrite(f->rarea, f->rbuf, 1) != 8)
            ;
        pl1_bclose(f->rarea);
    } else if (f->rkind == R_DIRECT) {
        pl1_bclose(f->rarea);
    } else {
        pl1_qclose(f->qdcb);
    }
    f->rkind = R_NONE;
    f->qmode = 0;
    f->open = 0;
}

static void key_cond(struct pfile *f, int oncode)
{
    pl1i_filecond(C_KEY, oncode, f->desc);
}

/* The region of a source key: its last eight characters, digits and
 * blanks (zeros); -1 after KEY (54, key conversion error). */
static int region(struct pfile *f, const char *key, int klen)
{
    int r = 0;
    for (int i = klen > 8 ? klen - 8 : 0; i < klen; i++) {
        char c = key[i];
        if (c == ' ')
            c = '0';
        if (c < '0' || c > '9') {
            key_cond(f, 54);
            return -1;
        }
        r = r * 10 + (c - '0');
    }
    return r;
}

static struct pfile *keyed_file(void *d, int output)
{
    struct pfile *f = of(d);
    if (!f->open)
        open_file(f, output);
    if (f->rkind == R_NONE) {
        fprintf(stderr, "PL1C/370: file %s is not a REGIONAL(1) file open for keyed access\n",
                f->dd);
        exit(16);
    }
    return f;
}

static int is_kfile(struct pfile *f);
static void keyed_op(struct pfile *f, int op, void *target, const void *source, int len,
                     const char *key, int klen);
static int kseq_read(struct pfile *f, const unsigned char **data);
static void kseq_rewrite(struct pfile *f, const void *src, int len);

static void read_key(void *d, void *target, int len, const char *key, int klen)
{
    struct pfile *f = keyed_file(d, 0);
    int r;
    if (is_kfile(f)) {
        keyed_op(f, 0, target, NULL, len, key, klen);
        return;
    }
    if (f->rkind == R_IDIRECT) {
        /* blocked, UPDATE: READ KU, which a REWRITE needs right before it */
        int ku = f->iblk && (f->flags & FL_UPDATE);
        unsigned char *k = fit_key(f, key, klen);
        f->iread = 0;
        f->rbuf[BDATA] = 0;
        if (isam_error(f, pl1_ibisam(f->rarea, ku, f->rbuf, k, &f->irecp), 51))
            return;
        if (f->irecp[0] == 0xFF && !(f->iblk && f->irkp == 0)) {
            /* READ: a deleted record (OPTCD=L) is not found (Q1IDX); DELETE
             * and REWRITE take it like any other (t124idx2) */
            key_cond(f, 51);
            return;
        }
        if (ku) {
            f->iread = 1;
            memcpy(f->lastkey, k, (size_t)f->keylen);
        }
        memcpy(target, f->irecp + f->idoff, (size_t)(len < f->ilrecl ? len : f->ilrecl));
        if (f->ilrecl > len)
            pl1i_filecond(C_RECORD, 21, d);
        return;
    }
    if (f->rkind == R_ISEQ) {
        /* SEQUENTIAL: SETL K, then the record; a key not there is KEY 51
         * and the file starts again at its first record (Q3IXB) */
        if (pl1_isetk(f->rarea, fit_key(f, key, klen)) != 0) {
            f->eof = 0;
            key_cond(f, 51);
            return;
        }
        f->eof = 0;
        pl1_read(d, target, len);
        return;
    }
    r = region(f, key, klen);
    if (r < 0)
        return;
    if (f->rkind != R_DIRECT || pl1_bdread(f->rarea, f->rbuf, r) != 0) {
        key_cond(f, 56);           /* outside the data set */
        return;
    }
    memcpy(target, f->rbuf, (size_t)(len < f->reglen ? len : f->reglen));
    if (f->reglen > len)
        pl1i_filecond(C_RECORD, 21, d);
}

static void keyto_copy(void *d, char *kt, int ktlen);

int pl1_read_keyto(void *d, void *target, int len, char *kt, int ktlen)
{
    if (pl1_read(d, target, len) != 0)
        return 1;
    keyto_copy(d, kt, ktlen);
    return 0;
}

int pl1_read_set_keyto(void *d, void **p, char *kt, int ktlen)
{
    if (pl1_read_set(d, p) != 0)
        return 1;
    keyto_copy(d, kt, ktlen);
    return 0;
}

static void keyto_copy(void *d, char *kt, int ktlen)
{
    char k[12];                 /* "%8d" of any int */
    if (of(d)->rkind == R_ISEQ || of(d)->rkind == R_KSEQ) {
        /* INDEXED, REGIONAL(2), (3): the recorded key, as a string is
         * assigned (Q2IKEY, Q2R2) */
        struct pfile *f = of(d);
        int n = f->keylen < ktlen ? f->keylen : ktlen;
        memcpy(kt, f->irec, (size_t)n);
        memset(kt + n, ' ', (size_t)(ktlen - n));
        return;
    }
    /* the region number, right-aligned in eight characters: a longer
     * variable gets blanks on the left, a shorter one the right end */
    snprintf(k, sizeof k, "%8d", of(d)->trel - 1);
    if (ktlen >= 8) {
        memset(kt, ' ', (size_t)(ktlen - 8));
        memcpy(kt + ktlen - 8, k, 8);
    } else {
        memcpy(kt, k + 8 - ktlen, (size_t)ktlen);
    }
    return;
}

static void fill_record(struct pfile *f, const void *source, int len)
{
    memcpy(f->rbuf, source, (size_t)(len < f->reglen ? len : f->reglen));
}

static void write_key(void *d, const void *source, int len, const char *key, int klen)
{
    struct pfile *f = keyed_file(d, 1);
    int r;
    if (is_kfile(f)) {
        keyed_op(f, 1, NULL, source, len, key, klen);
        return;
    }
    if (f->rkind == R_ICREATE) {
        /* ascending keys, checked before the PUT as PL/I (F) does */
        unsigned char *k;
        pl1x_flushloc(f);              /* a LOCATE's record first */
        k = fit_key(f, key, klen);
        int c = f->haslast ? memcmp(k, f->lastkey, (size_t)f->keylen) : 1;
        if (c <= 0) {
            key_cond(f, c == 0 ? 52 : 53);
            return;
        }
        if (embedded_differs(f, source, len, k))
            return;
        memcpy(f->lastkey, k, (size_t)f->keylen);
        f->haslast = 1;
        if (f->irkp) {
            /* an embedded key: the record alone, its key in it */
            memcpy(f->rbuf, source, (size_t)(len < f->ilrecl ? len : f->ilrecl));
        } else {
            memcpy(f->rbuf, k, (size_t)f->keylen);
            memcpy(f->rbuf + f->keylen, source, (size_t)(len < f->ilrecl ? len : f->ilrecl));
        }
        pl1_iput(f->rarea, f->rbuf);
        return;
    }
    if (f->rkind == R_IDIRECT) {
        unsigned char *k = fit_key(f, key, klen), *rec;
        f->iread = 0;
        if (embedded_differs(f, source, len, k))
            return;
        /* the logical record at +BDATA: the key in front of the data when
         * blocked with RKP=0 */
        if (f->idoff)
            memcpy(f->rbuf + BDATA, k, (size_t)f->keylen);
        memcpy(f->rbuf + BDATA + f->idoff, source, (size_t)(len < f->ilrecl ? len : f->ilrecl));
        isam_error(f, pl1_ibisam(f->rarea, 3, f->rbuf, k, &rec), 0);
        return;
    }
    r = region(f, key, klen);
    if (r < 0)
        return;
    if (f->rkind == R_CREATE) {
        /* ascending regions: a duplicate (52) or a lower one (53) is not
         * written */
        if (r < f->rnext) {
            key_cond(f, r == f->rnext - 1 ? 52 : 53);
            return;
        }
        while (f->rnext < r) {
            f->rbuf[0] = 0xFF;
            pl1_bwrite(f->rarea, f->rbuf, 1);
            f->rnext++;
        }
        fill_record(f, source, len);
        pl1_bwrite(f->rarea, f->rbuf, 0);
        f->rnext++;
        return;
    }
    fill_record(f, source, len);
    if (f->rkind != R_DIRECT || pl1_bdwrite(f->rarea, f->rbuf, r) != 0)
        key_cond(f, 56);
}

static void rewrite_key(void *d, const void *source, int len, const char *key, int klen)
{
    struct pfile *f = keyed_file(d, 0);
    if (is_kfile(f)) {
        keyed_op(f, 2, NULL, source, len, key, klen);
        return;
    }
    if (f->rkind == R_IDIRECT) {
        unsigned char *k = fit_key(f, key, klen), *rec;
        if (f->iblk) {
            /* blocked records: WRITE K into the block READ KU brought,
             * right after it, with the same key (IHE030I, ONCODE 1007);
             * the embedded key is not checked (Q4IXB) */
            int ok = f->iread && !memcmp(f->lastkey, k, (size_t)f->keylen);
            f->iread = 0;
            if (!ok) {
                pl1i_fileerr(1007, 30, "REWRITE/DELETE NOT IMMEDIATELY PRECEDED BY READ", f->desc);
                return;
            }
            memcpy(f->irecp + f->idoff, source, (size_t)(len < f->ilrecl ? len : f->ilrecl));
            isam_error(f, pl1_ibisam(f->rarea, 4, f->rbuf, k, &rec), 51);
            return;
        }
        /* unblocked records: WRITE K, no READ before it (GY28-6801-6) */
        if (embedded_differs(f, source, len, k))
            return;
        memcpy(f->rbuf + BDATA, source, (size_t)(len < f->ilrecl ? len : f->ilrecl));
        isam_error(f, pl1_ibisam(f->rarea, 2, f->rbuf, k, &rec), 51);
        return;
    }
    write_key(d, source, len, key, klen);
}

static void delete_key(void *d, const char *key, int klen)
{
    struct pfile *f = keyed_file(d, 0);
    int r;
    if (is_kfile(f)) {
        keyed_op(f, 3, NULL, NULL, 0, key, klen);
        return;
    }
    if (f->rkind == R_IDIRECT) {
        /* READ KU, X'FF' in the first byte of the data, WRITE K: OPTCD=L
         * leaves the record out from then on */
        unsigned char *k = fit_key(f, key, klen), *rec;
        f->iread = 0;
        if (f->iblk && f->irkp == 0) {
            /* the first byte of a blocked record with RKP=0 is its key:
             * no room for the mark (Q2IXB) */
            pl1i_fileerr(1009, 29, "UNSUPPORTED FILE OPERATION", f->desc);
            return;
        }
        f->rbuf[BDATA] = 0;
        if (isam_error(f, pl1_ibisam(f->rarea, 1, f->rbuf, k, &f->irecp), 51))
            return;
        f->irecp[0] = 0xFF;
        isam_error(f, pl1_ibisam(f->rarea, 2, f->rbuf, k, &rec), 0);
        return;
    }
    r = region(f, key, klen);
    if (r < 0)
        return;
    memset(f->rbuf, 0, (size_t)f->reglen);
    f->rbuf[0] = 0xFF;
    if (f->rkind != R_DIRECT || pl1_bdwrite(f->rarea, f->rbuf, r) != 0)
        key_cond(f, 56);
}

/* ---- REGIONAL(2) and REGIONAL(3) ------------------------------------- */

/* As PL/I (F) (internals/research/pli/tier38-measurement.md): F records
 * with recorded keys, a dummy record has X'FF' in the first byte of its
 * key.  A source key's last eight characters are the region (blanks in
 * front of the digits), the KEYLEN characters at its left end the
 * recorded key (blanks after a short one).  REGIONAL(2)'s region is a
 * record, REGIONAL(3)'s a track.  A search for a key, and the search for
 * a dummy record of a WRITE, start at the beginning of the region's
 * track and go on over the whole data set (rt/pl1bdk.asm).  The data
 * set is created with dummy records only; SEQUENTIAL OUTPUT then puts
 * each record into its region (REGIONAL(2)) or the first dummy record
 * of its track (REGIONAL(3)), regions ascending -- the same region again
 * in REGIONAL(2) or a lower one is KEY 53.  SEQUENTIAL INPUT and UPDATE
 * read the records in order and leave the dummy ones out. */

static int kreg(struct pfile *f, const char *key, int klen)
{
    int i = klen > 8 ? klen - 8 : 0, r = 0, digits = 0;
    while (i < klen && key[i] == ' ')
        i++;
    for (; i < klen; i++) {
        if (key[i] < '0' || key[i] > '9') {
            key_cond(f, 54);
            return -1;
        }
        r = r * 10 + (key[i] - '0');
        digits++;
    }
    if (!digits) {
        key_cond(f, 54);
        return -1;
    }
    if (r >= f->rel) {
        key_cond(f, 56);
        return -1;
    }
    return r;
}

/* the block reference of a region: a relative block, or the track as
 * TTR with R 0 */
static int kref(struct pfile *f, int r)
{
    return f->flags & FL_REG3 ? r << 8 : r;
}

/* BDAM's exception code: X'80' no record with the key, X'20' no dummy
 * record left; KEY and 1 on an error */
static int kerror(struct pfile *f, int exc)
{
    if (!exc)
        return 0;
    if (exc & 0x8000)
        key_cond(f, 51);
    else if (exc & 0x2000)
        key_cond(f, 57);
    else {
        fprintf(stderr, "PL1C/370: file %s: BDAM exception code X'%04X'\n", f->dd, exc);
        key_cond(f, 56);
    }
    return 1;
}

static void keyed_open(struct pfile *f, int output)
{
    char dd[8];
    int r3 = f->flags & FL_REG3, mode, n;
    dd8_of(f, dd);
    if (!f->rarea)
        f->rarea = calloc(1, 160);
    if (!f->rarea)
        regional_fail(f);
    if (output) {
        /* every record a dummy one first, as DIRECT OUTPUT does */
        unsigned char *b;
        if (pl1_bopen(f->rarea, dd, 0) != 0)
            regional_fail(f);
        n = f->rarea[16] + ((f->rarea[62] << 8) | f->rarea[63]);
        b = calloc(1, (size_t)(n > 0 ? n : 1));
        if (!b)
            regional_fail(f);
        b[0] = 0xFF;
        while (pl1_bwrite(f->rarea, b, 1) != 8)
            ;
        pl1_bclose(f->rarea);
        free(b);
    }
    if (f->flags & FL_DIRECT || output) {
        mode = (f->flags & FL_INPUT) && !output ? 2 : 1;
        if (r3)
            mode += 2;
        f->rkind = output && !(f->flags & FL_DIRECT) ? R_KCREATE : R_KDIRECT;
    } else {
        mode = f->flags & FL_UPDATE ? 1 : 2;
        f->rkind = R_KSEQ;
        f->flags |= FL_INPUT;
    }
    if (output && !(f->flags & FL_DIRECT))
        f->flags |= FL_OUTPUT;
    /* the size of the data set (DCBREL) is the limit of the search; in
     * tracks (TTR) and in records (relative blocks) it gives the records
     * per track */
    if (pl1_kopen(f->rarea, dd, (mode - 1) % 2 + 3, 1) != 0)
        regional_fail(f);
    n = (f->rarea[17] << 16) | (f->rarea[18] << 8) | f->rarea[19];
    pl1_bclose(f->rarea);
    if (pl1_kopen(f->rarea, dd, (mode - 1) % 2 + 1, 1) != 0)
        regional_fail(f);
    f->rel = (f->rarea[17] << 16) | (f->rarea[18] << 8) | f->rarea[19];
    f->rpt = n > 0 ? f->rel / n : 1;
    if (f->rkind != R_KSEQ) {
        if (r3)
            f->rel = n;
        pl1_bclose(f->rarea);
        if (pl1_kopen(f->rarea, dd, mode, f->rel) != 0)
            regional_fail(f);
    }
    f->keylen = f->rarea[16];
    f->reglen = (f->rarea[62] << 8) | f->rarea[63];
    free(f->rbuf);
    free(f->kbuf);
    f->rbuf = calloc(1, (size_t)f->reglen + 1);
    f->kbuf = calloc(1, (size_t)f->keylen + 1);
    if (!f->rbuf || !f->kbuf)
        regional_fail(f);
    f->rnext = f->rkind == R_KCREATE ? -1 : 0;
    f->eof = 0;
    f->qrec = NULL;
    f->qmode = 0;
    f->open = 1;
}

/* SEQUENTIAL: the next record that is not a dummy one, -1 at the end */
static int kseq_read(struct pfile *f, const unsigned char **data)
{
    for (;;) {
        int ref = f->rnext;
        if (f->eof || f->rnext >= f->rel) {
            f->eof = 1;
            return -1;
        }
        f->rnext++;
        if (kerror(f, pl1_kio(f->rarea, 4, f->kbuf, f->rbuf, &ref)))
            return -1;
        if (f->kbuf[0] != 0xFF)
            break;
    }
    f->irec = f->kbuf;
    f->qrec = f->rbuf;
    f->qlen = f->reglen;
    *data = f->qrec;
    return f->qlen;
}

static void kseq_rewrite(struct pfile *f, const void *src, int len)
{
    int ref = f->rnext - 1;
    if (src != f->rbuf)
        memcpy(f->rbuf, src, (size_t)(len < f->reglen ? len : f->reglen));
    kerror(f, pl1_kio(f->rarea, 3, f->kbuf, f->rbuf, &ref));
    f->qrec = NULL;
}

static void kfill(struct pfile *f, const void *source, int len)
{
    memset(f->rbuf, 0, (size_t)f->reglen);
    memcpy(f->rbuf, source, (size_t)(len < f->reglen ? len : f->reglen));
}

/* READ KEY, WRITE KEYFROM, REWRITE KEY, DELETE KEY (op 0 to 3) */
static void keyed_op(struct pfile *f, int op, void *target, const void *source, int len,
                     const char *key, int klen)
{
    int r = kreg(f, key, klen), ref;
    unsigned char *k;
    if (r < 0)
        return;
    k = fit_key(f, key, klen);
    ref = kref(f, r);
    switch (op) {
    case 0:
        if (kerror(f, pl1_kio(f->rarea, 0, k, f->rbuf, &ref)))
            return;
        memcpy(target, f->rbuf, (size_t)(len < f->reglen ? len : f->reglen));
        if (f->reglen > len)
            pl1i_filecond(C_RECORD, 21, f->desc);
        return;
    case 1:
        if (f->rkind == R_KCREATE) {
            if (r < f->rnext || (r == f->rnext && !(f->flags & FL_REG3))) {
                key_cond(f, 53);
                return;
            }
            f->rnext = r;
            kfill(f, source, len);
            kerror(f, pl1_kio(f->rarea, f->flags & FL_REG3 ? 2 : 3, k, f->rbuf, &ref));
            return;
        }
        kfill(f, source, len);
        kerror(f, pl1_kio(f->rarea, 2, k, f->rbuf, &ref));
        return;
    case 2:
        kfill(f, source, len);
        kerror(f, pl1_kio(f->rarea, 1, k, f->rbuf, &ref));
        return;
    default:
        if (kerror(f, pl1_kio(f->rarea, 0, k, f->rbuf, &ref)))
            return;
        /* a dummy record as BSAM creates it: X'FF' and zeros in the key,
         * the record's number on its track in the first byte of the data
         * -- WRITE DA puts its record where that byte says */
        memset(f->rbuf, 0, (size_t)f->reglen);
        f->rbuf[0] = (unsigned char)(f->flags & FL_REG3 ? ref & 0xFF : ref % f->rpt + 1);
        memset(k, 0, (size_t)f->keylen);
        k[0] = 0xFF;
        kerror(f, pl1_kio(f->rarea, 3, k, f->rbuf, &ref));
        return;
    }
}

static int is_kfile(struct pfile *f)
{
    return f->rkind == R_KCREATE || f->rkind == R_KDIRECT;
}

/* ---- INDEXED ------------------------------------------------------------ */

/* As PL/I (F) (internals/research/pli/tier32-measurement.md): QISAM to
 * create (keys ascending, checked here: KEY 52 and 53) and for
 * SEQUENTIAL access, deleted records left out by OPTCD=L; BISAM for
 * DIRECT access.  A source key is cut or padded with blanks to KEYLEN;
 * KEYTO gets the recorded key as a string is assigned. */

static void indexed_open(struct pfile *f, int output)
{
    char dd[8];
    int mode, v[4];
    dd8_of(f, dd);
    if (!f->rarea)
        f->rarea = calloc(1, 512);
    if (!f->rarea)
        regional_fail(f);
    if (f->flags & FL_DIRECT) {
        if (output) {
            fprintf(stderr, "PL1C/370: INDEXED file %s: DIRECT OUTPUT is not supported\n",
                    f->dd);
            exit(16);
        }
        mode = (f->flags & FL_INPUT) ? 4 : 3;
        f->rkind = R_IDIRECT;
    } else if (output) {
        mode = 0;
        f->rkind = R_ICREATE;
        f->haslast = 0;
        f->flags |= FL_OUTPUT;
    } else {
        mode = (f->flags & FL_UPDATE) ? 2 : 1;
        f->rkind = R_ISEQ;
        f->flags |= FL_INPUT;
    }
    if (pl1_iopen(f->rarea, dd, mode) != 0) {
        f->rkind = R_NONE;
        regional_fail(f);
    }
    pl1_iinfo(f->rarea, v);
    f->keylen = v[0];
    f->ilog = v[1] ? v[1] : v[2];
    f->iblk = v[1] && v[2] > v[1];
    f->irkp = v[3];
    /* blocked with RKP=0: the key is the front of the logical record,
     * the program's record the rest (Q1IXB: IHE112I for a 20-byte
     * variable, LRECL=20, KEYLEN=5) */
    f->idoff = f->iblk && f->irkp == 0 ? f->keylen : 0;
    f->ilrecl = f->ilog - f->idoff;
    f->iread = 0;
    free(f->rbuf);
    free(f->lastkey);
    free(f->kbuf);
    /* the data area: the key in front (creation), room for BISAM's
     * overflow link after it, or a block */
    f->rbuf = calloc(1, (size_t)(f->keylen + (v[2] > f->ilog ? v[2] : f->ilog) + BDATA + 16));
    f->lastkey = calloc(1, (size_t)f->keylen + 1);
    f->kbuf = calloc(1, (size_t)f->keylen + 1);
    if (!f->rbuf || !f->lastkey || !f->kbuf)
        regional_fail(f);
    f->eof = 0;
    f->qrec = NULL;
    f->qmode = 0;
    f->open = 1;
}

static unsigned char *fit_key(struct pfile *f, const char *key, int klen)
{
    int n = klen < f->keylen ? klen : f->keylen;
    memcpy(f->kbuf, key, (size_t)n);
    memset(f->kbuf + n, ' ', (size_t)(f->keylen - n));
    return f->kbuf;
}

/* An embedded key (RKP > 0) that differs from KEYFROM or KEY: KEY 55,
 * and nothing written (Q3IXB, Q5IXB) */
static int embedded_differs(struct pfile *f, const void *source, int len, const unsigned char *k)
{
    if (f->irkp == 0 || len < f->irkp + f->keylen ||
        !memcmp((const unsigned char *)source + f->irkp, k, (size_t)f->keylen))
        return 0;
    key_cond(f, 55);
    return 1;
}

/* BISAM's exception code (DECB+24): X'80' not found, X'20' no space,
 * X'01' a duplicate; X'02' (an overflow record) is no error.  Raises
 * KEY and returns 1 on an error. */
static int isam_error(struct pfile *f, int exc, int notfound)
{
    int b = (exc >> 8) & 0xFD;
    if (!b)
        return 0;
    key_cond(f, b & 0x01 ? 52 : b & 0x20 ? 57 : b & 0x80 ? (notfound ? notfound : 51) : 55);
    return 1;
}

/* DELETE without KEY: the record read last, of a SEQUENTIAL UPDATE file */
void pl1_delete_seq(void *d)
{
    struct pfile *f = of(d);
    if (!f->open || f->rkind != R_ISEQ || !(f->flags & FL_UPDATE) || !f->qrec) {
        fprintf(stderr, "PL1C/370: DELETE of %.8s without a record READ for UPDATE\n", f->dd);
        exit(16);
    }
    if (f->iblk && f->irkp == 0) {
        pl1i_fileerr(1009, 29, "UNSUPPORTED FILE OPERATION", f->desc);
        return;
    }
    f->qrec[0] = 0xFF;
    pl1_iputx(f->rarea);
    f->qrec = NULL;
}


/* The hook of a REGIONAL or INDEXED file's descriptor (pl1iof.h): the
 * parts of open, close, READ and REWRITE that pl1io.c leaves to KEYED
 * files, so that a program without them does not link this. */
int pl1_key_hook(struct pfile *f, int op, int arg, const void *src, int len,
                 const unsigned char **data) asm("PL1KHOK");

int pl1_key_hook(struct pfile *f, int op, int arg, const void *src, int len,
                 const unsigned char **data)
{
    switch (op) {
    case 0:
        regional_open(f, arg);
        return 0;
    case 1:
        regional_close(f);
        return 0;
    case 2: {
        /* QISAM after SETL B: the data in R1, its key in R0 */
        unsigned char *r[2];
        if (f->rkind == R_KSEQ)
            return kseq_read(f, data);
        if (f->eof || pl1_iget(f->rarea, r) != 0) {
            f->eof = 1;
            return -1;
        }
        if (f->iblk) {
            /* blocked: R1 the logical record, its key in it */
            f->irec = r[0] + f->irkp;
            f->qrec = r[0] + f->idoff;
        } else {
            f->irec = r[1];
            f->qrec = r[0];
        }
        f->qlen = f->ilrecl;
        *data = f->qrec;
        return f->qlen;
    }
    case 4:
        /* LOCATE KEYFROM's record: its key was checked at the LOCATE */
        if (embedded_differs(f, src, len, f->lockey))
            return 0;
        if (f->irkp) {
            memcpy(f->rbuf, src, (size_t)(len < f->ilrecl ? len : f->ilrecl));
        } else {
            memcpy(f->rbuf, f->lockey, (size_t)f->keylen);
            memcpy(f->rbuf + f->keylen, src, (size_t)(len < f->ilrecl ? len : f->ilrecl));
        }
        pl1_iput(f->rarea, f->rbuf);
        return 0;
    default:
        if (f->rkind == R_KSEQ) {
            kseq_rewrite(f, src, len);
            return 0;
        }
        memcpy(f->qrec, src, (size_t)(len < f->qlen ? len : f->qlen));
        pl1_iputx(f->rarea);
        f->qrec = NULL;
        return 0;
    }
}


/* ---- ONKEY ---------------------------------------------------------------- */

/* What ONKEY gives in the on-unit of a KEY or RECORD condition (Q1OK):
 * REGIONAL(1) the last eight characters of the source key, blanks after
 * a shorter one; REGIONAL(2), (3) and INDEXED the recorded key, the
 * KEYLEN characters at its left end -- but the whole source key when
 * an INDEXED data set is created. */
static void onkey(void *d, int output, const char *key, int klen)
{
    struct pfile *f = keyed_file(d, output);
    char b[256];
    int n;
    if (klen > (int)sizeof b)
        klen = sizeof b;
    if (f->rkind == R_ICREATE) {
        n = klen;
        memcpy(b, key, (size_t)n);
    } else if (f->flags & (FL_INDEXED | FL_REG2 | FL_REG3)) {
        n = f->keylen;
        memset(b, ' ', (size_t)n);
        memcpy(b, key, (size_t)(klen < n ? klen : n));
    } else {
        n = 8;
        memset(b, ' ', 8);
        if (klen > 8)
            memcpy(b, key + klen - 8, 8);
        else
            memcpy(b, key, (size_t)klen);
    }
    pl1_setkey(b, n);
}

void pl1_read_key(void *d, void *target, int len, const char *key, int klen)
{
    onkey(d, 0, key, klen);
    read_key(d, target, len, key, klen);
    pl1_setkey(NULL, 0);
}

void pl1_write_key(void *d, const void *source, int len, const char *key, int klen)
{
    onkey(d, 1, key, klen);
    write_key(d, source, len, key, klen);
    pl1_setkey(NULL, 0);
}

void pl1_rewrite_key(void *d, const void *source, int len, const char *key, int klen)
{
    onkey(d, 0, key, klen);
    rewrite_key(d, source, len, key, klen);
    pl1_setkey(NULL, 0);
}

void pl1_delete_key(void *d, const char *key, int klen)
{
    onkey(d, 0, key, klen);
    delete_key(d, key, klen);
    pl1_setkey(NULL, 0);
}


/* ---- LOCATE KEYFROM, READ SET KEY ------------------------------------------ */

/* LOCATE KEYFROM on an INDEXED file being created: the key is checked
 * first (KEY 52, 53: no new buffer, the pointer unchanged, and the
 * record located before still open -- what the program then assigns
 * goes into it, Q1SETK), the record written at the next WRITE, LOCATE
 * or CLOSE */
void pl1_locate_key(void *d, int len, int phase, void **p, const char *key, int klen)
{
    struct pfile *f;
    unsigned char *k;
    int c;
    onkey(d, 1, key, klen);
    f = keyed_file(d, 1);
    if (f->rkind != R_ICREATE) {
        fprintf(stderr, "PL1C/370: LOCATE KEYFROM on %s: INDEXED SEQUENTIAL OUTPUT only\n",
                f->dd);
        exit(16);
    }
    k = fit_key(f, key, klen);
    c = f->haslast ? memcmp(k, f->lastkey, (size_t)f->keylen) : 1;
    if (c <= 0) {
        key_cond(f, c == 0 ? 52 : 53);
        pl1_setkey(NULL, 0);
        return;
    }
    pl1x_flushloc(f);                  /* the record located before */
    memcpy(f->lastkey, k, (size_t)f->keylen);
    f->haslast = 1;
    if (!f->lockey)
        f->lockey = calloc(1, (size_t)f->keylen + 1);
    if (!f->lockey)
        regional_fail(f);
    memcpy(f->lockey, k, (size_t)f->keylen);
    pl1_locate(d, len, phase, p);
    f->lockeyed = 1;
    pl1_setkey(NULL, 0);
}

/* READ SET KEY: a SEQUENTIAL INDEXED file is positioned at the key
 * (KEY 51: the pointer unchanged, the file from its start again);
 * DIRECT is PL/I (F)'s IHE029I, ONCODE 1009 (Q1SETK) */
void pl1_read_set_key(void *d, void **p, const char *key, int klen)
{
    struct pfile *f;
    onkey(d, 0, key, klen);
    f = keyed_file(d, 0);
    if (f->rkind != R_ISEQ) {
        pl1i_fileerr(1009, 29, "UNSUPPORTED FILE OPERATION", f->desc);
        pl1_setkey(NULL, 0);
        return;
    }
    f->eof = 0;
    if (pl1_isetk(f->rarea, fit_key(f, key, klen)) != 0)
        key_cond(f, 51);
    else
        pl1_read_set(d, p);
    pl1_setkey(NULL, 0);
}
