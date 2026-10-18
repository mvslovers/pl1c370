* PL1QSAM -- QSAM in update mode, for RECORD UPDATE files.
*
* LIBC/370 opens no record stream for update ('+' is for byte
* streams), so the run time does what PL/I (F) does itself: a DCB
* opened UPDAT, GET in locate mode, PUTX to put the record just got
* back in place (rt/pl1io.c).  Called from C: R1 -> the argument
* values, R15 the result; each routine takes a save area from the
* NAB, since the access method saves its registers in it.
*
*   int  pl1_qopen(void *dcb, const char *dd8)   0, or 1: not open
*   int  pl1_qopeni(void *dcb, const char *dd8)  the same, for INPUT
*   int  pl1_qget(void *dcb, unsigned char **rec) 0, or 1: end of file
*   void pl1_qputx(void *dcb)
*   void pl1_qclose(void *dcb)
*
* dcb is an area of DCBLEN bytes, a fullword aligned, below the line;
* *rec is the record in the buffer (a V record with its RDW).
         COPY  PDPTOP
         CSECT
FRAME    EQU   88
*
         ENTRY PL1QOPN
PL1QOPN  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         L     2,0(,1)             the DCB area
         L     3,4(,1)             the DDNAME, 8 characters
         L     4,76(,13)
         ST    13,4(,4)
         LR    13,4
         LA    4,FRAME(,13)
         ST    4,76(,13)
         MVC   0(DCBLEN,2),MODEL
         MVC   40(8,2),0(3)        DCBDDNAM
         OPEN  ((2),UPDAT)
         SR    15,15
         TM    48(2),X'10'         DCBOFLGS: opened
         BO    QORET
         LA    15,1
QORET    L     13,4(,13)
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
         DROP  12
*
* INPUT only: a REGIONAL(1) data set read SEQUENTIAL INPUT
         ENTRY PL1QOPI
PL1QOPI  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         L     2,0(,1)
         L     3,4(,1)
         L     4,76(,13)
         ST    13,4(,4)
         LR    13,4
         LA    4,FRAME(,13)
         ST    4,76(,13)
         MVC   0(DCBLEN,2),IMODEL
         MVC   40(8,2),0(3)        DCBDDNAM
         OPEN  ((2),INPUT)
         SR    15,15
         TM    48(2),X'10'
         BO    QIRET
         LA    15,1
QIRET    L     13,4(,13)
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
         DROP  12
*
         ENTRY PL1QGET
PL1QGET  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         L     2,0(,1)             the DCB
         L     3,4(,1)             where the record's address goes
         L     4,76(,13)
         ST    13,4(,4)
         LR    13,4
         LA    4,FRAME(,13)
         ST    4,76(,13)
         GET   (2)
         ST    1,0(,3)
         SR    15,15
         B     QGRET
* EODAD: entered with the registers of the GET
QEOD     LA    15,1
QGRET    L     13,4(,13)
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
         DROP  12
*
         ENTRY PL1QPTX
PL1QPTX  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         L     2,0(,1)
         L     4,76(,13)
         ST    13,4(,4)
         LR    13,4
         LA    4,FRAME(,13)
         ST    4,76(,13)
         PUTX  (2)
         L     13,4(,13)
         LM    14,12,12(13)
         BR    14
         DROP  12
*
         ENTRY PL1QCLS
PL1QCLS  DS    0H
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
MODEL    DCB   DSORG=PS,MACRF=(GL,PM),DDNAME=PL1QSAM,EODAD=QEOD
DCBLEN   EQU   *-MODEL
         DS    0F
IMODEL   DCB   DSORG=PS,MACRF=GL,DDNAME=PL1QSAM,EODAD=QEOD
         LTORG
         END
