/* pl1pic.h -- numeric character data (PICTURE): the editing of rt/pl1pic.c,
 * compiled into the run time and into the compiler (the attributes of a
 * picture).  A picture is passed as its characters and their number,
 * repetition factors expanded, V included. */
#ifndef PL1PIC_H
#define PL1PIC_H

/* val: packed decimal of 15 digits, the value * 10**q */
void pl1_pic_edit(const unsigned char *val, const char *pic, int n, char *dst)
    __asm__("PL1PEDT");
void pl1_pic_value(const char *src, const char *pic, int n, unsigned char *val)
    __asm__("PL1PVAL");
int pl1_pic_length(const char *pic, int n) __asm__("PL1PLEN");
int pl1_pic_scale(const char *pic, int n) __asm__("PL1PSCL");
int pl1_pic_digits(const char *pic, int n) __asm__("PL1PDIG");
int pl1_pic_edit_digits(const char *d, int nd, int q, int neg, const char *pic, int n,
                        char *dst) __asm__("PL1PEDD");

#endif
