* P0HELLO -- a PUT LIST of a string translated by hand into the code
* PL1C/370 is to generate: CC/370 linkage, a frame from the C stack,
* run-time calls with argument values.  Proves the seam before the
* compiler emits it.
         COPY  PDPTOP
         CSECT
         ENTRY @@MAIN
@@MAIN   DS    0H
         BALR  15,0
         USING *,15
         L     15,=V(@@CRT0)
         BR    15
         DROP  15
         LTORG
* 1  HELLO: PROC OPTIONS(MAIN);
* Frame: 0-87 header, 88-95 argument list, 96-117 S.
MAIN     PDPPRLG CINDEX=0,FRAME=120,BASER=12,ENTRY=YES
         B     @@FEN0
         LTORG
@@FEN0   EQU   *
         DROP  12
         BALR  12,0
         USING *,12
* 2  DCL S CHAR(20) VARYING INIT('ABC');
         MVC   96(2,13),=H'3'
         MVC   98(3,13),=C'ABC'
* 3  PUT SKIP LIST('HELLO FROM PL/I (F)', LENGTH(S));
         MVC   88(4,13),=F'1'
         LA    1,88(,13)
         L     15,=V(PL1SKIP)
         BALR  14,15
         MVC   88(4,13),=A(@@L1)
         MVC   92(4,13),=F'19'
         LA    1,88(,13)
         L     15,=V(PL1LCHR)
         BALR  14,15
         LH    2,96(,13)
         ST    2,88(,13)
         MVC   92(4,13),=F'15'
         LA    1,88(,13)
         L     15,=V(PL1LFXB)
         BALR  14,15
* 4  END HELLO;
         SR    15,15
         PDPEPIL
         LTORG
@@L1     DC    C'HELLO FROM PL/I (F)'
         END   @@MAIN
