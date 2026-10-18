* PL1BDK -- BDAM with recorded keys, for REGIONAL(2) and (3) files.
*
* As PL/I (F) does (internals/research/pli/tier38-measurement.md): a
* key search (READ DK, WRITE DK) and an add (WRITE DA, into the first
* dummy record) start at the beginning of the track the region lies
* on and go on over the whole data set (extended search, OPTCD=E,
* LIMCT the size of the data set).  REGIONAL(2) addresses by relative
* block (OPTCD=R), REGIONAL(3) by relative track (TTR).  READ DI and
* WRITE DI take the key with the data.  The data set is formatted by
* rt/pl1bdam.asm (BSAM load mode).  Called from C (rt/pl1iokey.c): R1
* -> the argument values, R15 the result; each routine takes a save
* area from the NAB.
*
*   int  pl1_kopen(void *a, const char *dd8, int mode, int limct)
*        0, or 1: fails.  mode 1 UPDAT by relative block, 2 INPUT by
*        relative block, 3 UPDAT by TTR, 4 INPUT by TTR; limct goes
*        into DCBLIMCT before OPEN
*   int  pl1_kio(void *a, int op, void *key, void *data, int *ref)
*        op 0 READ DKF, 1 WRITE DK, 2 WRITE DA, 3 WRITE DI, 4 READ DI:
*        the exception code of the DECB; *ref the block reference
*        (3 bytes, right-aligned), on return the feedback of READ DKF
*   void pl1_bclose(void *a)  (rt/pl1bdam.asm)
*
* a is an area of 160 bytes, a fullword aligned, below the line, as
* for rt/pl1bdam.asm: the DCB at +0, the DECB at +DECBOFF, the block
* reference after it.
         COPY  PDPTOP
         CSECT
FRAME    EQU   88
DECBOFF  EQU   104                 the DCB is X'68' bytes long
REFOFF   EQU   136
*
         ENTRY PL1KOPN
PL1KOPN  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         LM    2,5,0(1)            the area, the DDNAME, mode, limct
         L     6,76(,13)
         ST    13,4(,6)
         LR    13,6
         LA    6,FRAME(,13)
         ST    6,76(,13)
         MVC   0(KDCBLEN,2),KMODELR
         C     4,=F'3'
         BL    KOPMOD
         MVC   0(KDCBLEN,2),KMODELT
KOPMOD   MVC   40(8,2),0(3)        DCBDDNAM
         STCM  5,7,81(2)           DCBLIMCT
         C     4,=F'2'
         BE    KOPIN
         C     4,=F'4'
         BE    KOPIN
         OPEN  ((2),UPDAT)
         B     KOPTST
KOPIN    OPEN  ((2),INPUT)
KOPTST   SR    15,15
         TM    48(2),X'10'         DCBOFLGS: opened
         BO    KORET
         LA    15,1
KORET    L     13,4(,13)
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
         DROP  12
*
         ENTRY PL1KIO
PL1KIO   DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         LM    2,6,0(1)            area, op, key, data, A(ref)
         L     7,76(,13)
         ST    13,4(,7)
         LR    13,7
         LA    7,FRAME(,13)
         ST    7,76(,13)
         LA    8,DECBOFF(,2)
         SLL   3,5                 32 bytes per model DECB
         LA    7,KDECBS(3)
         MVC   0(32,8),0(7)
         ST    2,8(,8)             the DCB
         ST    5,12(,8)            the data
         ST    4,20(,8)            the key
         LA    7,REFOFF(,2)
         XC    0(8,7),0(7)
         MVC   0(3,7),1(6)         the block reference, 3 bytes
         ST    7,24(,8)            A(the block reference)
         LR    1,8
         L     15,48(,2)           DCBREAD/DCBWRITE
         BALR  14,15
         LR    1,8
         L     14,8(,1)
         L     15,52(,14)          DCBCHECK (SYNAD: KDSYN)
         BALR  14,15
         LA    7,REFOFF(,2)
         XC    0(4,6),0(6)
         MVC   1(3,6),0(7)         the feedback
         SR    15,15
         ICM   15,3,1(8)           the exception code, 0: done
         L     13,4(,13)
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
         DROP  12
* SYNAD: the exception code stays in the DECB; back into CHECK
KDSYN    BR    14
*
         DS    0F
KMODELR  DCB   DSORG=DA,MACRF=(RKIC,WAKIC),OPTCD=REF,DDNAME=PL1BDK,    X
               SYNAD=KDSYN,LIMCT=1
KDCBLEN  EQU   *-KMODELR
         DS    0F
KMODELT  DCB   DSORG=DA,MACRF=(RKIC,WAKIC),OPTCD=EF,DDNAME=PL1BDK,     X
               SYNAD=KDSYN,LIMCT=1
         DS    0F
KDECBS   READ  KRDK,DKF,,,'S',,,MF=L
         ORG   KDECBS+32
         WRITE KWDK,DK,,,'S',,,MF=L
         ORG   KDECBS+64
         WRITE KWDA,DA,,,'S',,,MF=L
         ORG   KDECBS+96
         WRITE KWDI,DI,,,'S',,,MF=L
         ORG   KDECBS+128
         READ  KRDI,DI,,,'S',,,MF=L
         ORG   KDECBS+160
         LTORG
         END
