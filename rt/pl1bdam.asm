* PL1BDAM -- the access methods of REGIONAL(1) files.
*
* As PL/I (F) does (GY28-6801-6, "REGIONAL Data Sets"): BSAM in load
* mode to create the data set -- WRITE SF for a record, WRITE SD for
* a dummy record, until WRITE returns 8: the extent is full -- and
* BDAM with relative block addressing (OPTCD=R) for DIRECT access.
* SEQUENTIAL input and update go through rt/pl1qsam.asm.  Called
* from C (rt/pl1io.c): R1 -> the argument values, R15 the result;
* each routine takes a save area from the NAB.
*
*   int  pl1_bopen(void *a, const char *dd8, int mode)  0, or 1: fails
*        mode 0: create (BSAM, OUTPUT), 1 BDAM UPDAT, 2 BDAM INPUT
*   int  pl1_bwrite(void *a, void *buf, int dummy)  WRITE's R15
*   int  pl1_bdread(void *a, void *buf, int block)  exception code
*   int  pl1_bdwrite(void *a, void *buf, int block) exception code
*   void pl1_bclose(void *a)
*
* a is an area of 144 bytes, a fullword aligned, below the line:
* the DCB at +0, the DECB at +DECBOFF, the block reference after it.
         COPY  PDPTOP
         CSECT
FRAME    EQU   88
DECBOFF  EQU   104                 the BDAM DCB is X'68' bytes long
REFOFF   EQU   136
*
         ENTRY PL1BOPN
PL1BOPN  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         LM    2,4,0(1)            the area, the DDNAME, the mode
         L     5,76(,13)
         ST    13,4(,5)
         LR    13,5
         LA    5,FRAME(,13)
         ST    5,76(,13)
         LTR   4,4
         BNZ   BOPDA
         MVC   0(SDCBLEN,2),SMODEL
         MVC   40(8,2),0(3)        DCBDDNAM
         OPEN  ((2),OUTPUT)
         B     BOPTST
BOPDA    MVC   0(DDCBLEN,2),DMODEL
         MVC   40(8,2),0(3)
         C     4,=F'1'
         BNE   BOPIN
         OPEN  ((2),UPDAT)
         B     BOPTST
BOPIN    OPEN  ((2),INPUT)
BOPTST   SR    15,15
         TM    48(2),X'10'         DCBOFLGS: opened
         BO    BORET
         LA    15,1
BORET    L     13,4(,13)
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
         DROP  12
*
         ENTRY PL1BWRT
PL1BWRT  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         LM    2,4,0(1)            the area, the buffer, dummy or not
         L     5,76(,13)
         ST    13,4(,5)
         LR    13,5
         LA    5,FRAME(,13)
         ST    5,76(,13)
         LA    6,DECBOFF(,2)
         MVC   0(32,6),WSF
         LTR   4,4
         BZ    BWRSF
         MVC   0(32,6),WSD
BWRSF    ST    2,8(,6)             the DCB
         ST    3,12(,6)            the buffer
         LR    1,6
         L     15,48(,2)           DCBWRITE
         BALR  14,15
         LR    7,15
         C     7,=F'8'             8: no space, nothing written
         BE    BWRRET
         LR    1,6
         L     14,8(,1)
         L     15,52(,14)          DCBCHECK
         BALR  14,15
BWRRET   LR    15,7
         L     13,4(,13)
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
         DROP  12
*
         ENTRY PL1BDRD
PL1BDRD  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         LA    8,RDI
         B     BDIO
         DROP  12
         ENTRY PL1BDWR
PL1BDWR  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         LA    8,WDI
         B     BDIO
         DROP  12
* READ or WRITE by relative block, R8 -> the DECB's model
BDIO     DS    0H
         BALR  12,0
         USING *,12
         LM    2,4,0(1)            the area, the buffer, the block
         L     5,76(,13)
         ST    13,4(,5)
         LR    13,5
         LA    5,FRAME(,13)
         ST    5,76(,13)
         LA    6,DECBOFF(,2)
         MVC   0(32,6),0(8)
         ST    2,8(,6)             the DCB
         ST    3,12(,6)            the buffer
         LA    5,REFOFF(,2)
         STCM  4,7,0(5)            the relative block, 3 bytes
         ST    5,24(,6)            A(the block reference)
         LR    1,6
         L     15,48(,2)           DCBREAD/DCBWRITE
         BALR  14,15
         LR    1,6
         L     14,8(,1)
         L     15,52(,14)          DCBCHECK (SYNAD: BDSYN)
         BALR  14,15
         SR    15,15
         ICM   15,3,1(6)           the exception code, 0: done
         L     13,4(,13)
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
         DROP  12
* SYNAD: the exception code stays in the DECB; back into CHECK
BDSYN    BR    14
*
         ENTRY PL1BCLS
PL1BCLS  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         L     2,0(,1)
         L     4,76(,13)
         ST    13,4(,4)
         LR    13,4
         LA    4,FRAME(,13)
         ST    4,76(,13)
         CLOSE ((2))
         L     13,4(,13)
         LM    14,12,12(13)
         BR    14
         DROP  12
*
         DS    0F
SMODEL   DCB   DSORG=PS,MACRF=WL,RECFM=F,DDNAME=PL1BDAM
SDCBLEN  EQU   *-SMODEL
         DS    0F
DMODEL   DCB   DSORG=DA,MACRF=(RIC,WIC),OPTCD=R,DDNAME=PL1BDAM,        X
               SYNAD=BDSYN
DDCBLEN  EQU   *-DMODEL
         DS    0F
WSF      WRITE WSFD,SF,,,'S',MF=L
         DS    0F
         ORG   WSF+32
WSD      WRITE WSDD,SD,,,'S',MF=L
         ORG   WSD+32
RDI      READ  RDID,DI,,,'S',,,MF=L
         ORG   RDI+32
WDI      WRITE WDID,DI,,,'S',,,MF=L
         ORG   WDI+32
         LTORG
         END
