* PL1ATTN -- the attention exit of PL1C/370 (#226).
*
* With the INTERRUPT option an attention at the TSO terminal raises
* the ATTENTION condition (SC33-0006-7, "INTERRUPT Option").  The
* run time establishes this exit by STAX (pl1_attn_on, rt/pl1cond.c)
* when the program runs under a terminal.  The exit only sets the
* flag PL1ATTN; the program polls it at stream I/O and, compiled with
* -interrupt, at the end of each pass of a loop and at a GO TO, and
* raises the condition there, outside the exit.
*
*   void pl1_stax_on(void)    STAX with the exit
*   void pl1_stax_off(void)   STAX without one: the exit cancelled
         COPY  PDPTOP
         CSECT
*
         ENTRY PL1STXN
PL1STXN  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         STAX  PL1ATEX,MF=(E,STAXL)
         LM    14,12,12(13)
         BR    14
         DROP  12
*
         ENTRY PL1STXF
PL1STXF  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         STAX
         LM    14,12,12(13)
         BR    14
         DROP  12
*
* The exit: entered with R14 the return and R15 its address, as an
* asynchronous exit of the task.  It sets the flag and returns.
PL1ATEX  DS    0H
         USING PL1ATEX,15
         L     1,=V(PL1ATTN)
         MVI   0(1),1
         BR    14
         LTORG
         DROP  15
*
STAXL    STAX  PL1ATEX,MF=L
         END
