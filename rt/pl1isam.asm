* PL1ISAM -- the access methods of INDEXED files.
*
* As PL/I (F) does (GY28-6801-6, "INDEXED Data Sets"): QISAM to
* create the data set (load mode, PUT in move mode, the key in front
* of the data) and for SEQUENTIAL access (scan mode: SETL B, GET in
* locate mode -- the key and the data -- and PUTX), BISAM for DIRECT
* access (READ K, READ KU, WRITE K, WRITE KN; MACRF with C, or OPEN
* loads no CHECK module and DCBSETL stays 1).  Called from C
* (rt/pl1io.c): R1 -> the argument values, R15 the result; each
* routine takes a save area from the NAB.
*
*   int  pl1_iopen(void *a, const char *dd8, int mode)  0, or 1: fails
*        mode 0 create, 1 scan INPUT, 2 scan UPDAT, 3 BISAM UPDAT,
*        4 BISAM INPUT
*   void pl1_iinfo(void *a, int *v)    KEYLEN, LRECL, BLKSIZE, RKP
*   int  pl1_iput(void *a, void *rec)  QISAM PUT: key, then the data
*   int  pl1_iget(void *a, char **rec) 0, or 1 at end: rec[0] the
*        data, rec[1] the key
*   void pl1_iputx(void *a)
*   int  pl1_ibisam(void *a, int op, void *area, void *key, char **rec)
*        op 0 READ K, 1 READ KU, 2 WRITE K, 3 WRITE KN, 4 WRITE K in
*        the DECB of the READ KU before it (blocked records): the
*        exception code of the DECB; *rec the logical record (DECB+16)
*   int  pl1_isetk(void *a, void *key)  SETL K: 0, or 1 when there is
*        no record with the key -- then SETL B, from the start again
*   void pl1_iclose(void *a)
*
* a is an area of 512 bytes, a fullword aligned, below the line: the
* DCB at +0, the DECB at +DECBOFF.
         COPY  PDPTOP
         CSECT
FRAME    EQU   88
DECBOFF  EQU   400
*
         ENTRY PL1IOPN
PL1IOPN  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         LM    2,4,0(1)            the area, the DDNAME, the mode
         L     5,76(,13)
         ST    13,4(,5)
         LR    13,5
         LA    5,FRAME(,13)
         ST    5,76(,13)
         LR    11,4                the mode, kept over OPEN
         SLL   4,2
         L     8,MODELS(4)
         LR    6,2
         LA    7,DCBMAX
         LR    9,7
         MVCL  6,8                 the model DCB
         MVC   40(8,2),0(3)        DCBDDNAM
         LR    4,11
         C     4,=F'0'
         BE    IOPOUT
         C     4,=F'1'
         BE    IOPIN
         C     4,=F'4'
         BE    IOPIN
         OPEN  ((2),UPDAT)
         B     IOPTST
IOPOUT   OPEN  ((2),OUTPUT)
         B     IOPTST
IOPIN    OPEN  ((2),INPUT)
IOPTST   SR    15,15
         TM    48(2),X'10'         DCBOFLGS: opened
         BZ    IOPBAD
         C     4,=F'1'
         BE    IOPSETL
         C     4,=F'2'
         BNE   IORET
IOPSETL  SETL  (2),B               the key and the data, from the start
         SR    15,15
         B     IORET
IOPBAD   LA    15,1
IORET    L     13,4(,13)
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
         DROP  12
*
         ENTRY PL1IINF
PL1IINF  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         LM    2,3,0(1)            the area, the result vector
         USING IHADCB,2
         SR    4,4
         IC    4,DCBKEYLE
         ST    4,0(,3)
         LH    4,DCBLRECL
         ST    4,4(,3)
         LH    4,DCBBLKSI
         ST    4,8(,3)
         LH    4,DCBRKP
         ST    4,12(,3)
         DROP  2
         LM    14,12,12(13)
         BR    14
         DROP  12
*
         ENTRY PL1IPUT
PL1IPUT  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         LM    2,3,0(1)            the area, the record (key, data)
         L     4,76(,13)
         ST    13,4(,4)
         LR    13,4
         LA    4,FRAME(,13)
         ST    4,76(,13)
         PUT   (2),(3)
         SR    15,15
         L     13,4(,13)
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
         DROP  12
*
         ENTRY PL1IGET
PL1IGET  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         LM    2,3,0(1)            the area, where the address goes
         L     4,76(,13)
         ST    13,4(,4)
         LR    13,4
         LA    4,FRAME(,13)
         ST    4,76(,13)
         ST    13,IGSAVE           for the EODAD exit
         ST    3,IGSAVE+4
         GET   (2)
         ST    1,0(,3)             the data
         ST    0,4(,3)             the key (unblocked, SETL B)
         SR    15,15
         B     IGRET
* EODAD: its own base, the frame of the GET from IGSAVE
IGEOD    BALR  12,0
         USING *,12
         L     13,IGSAVE
         LA    15,1
         DROP  12
IGRET    L     13,4(,13)
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
*
         ENTRY PL1IPTX
PL1IPTX  DS    0H
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
         ENTRY PL1IBIS
PL1IBIS  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         LM    2,5,0(1)            the area, op, the data area, the key
         L     9,16(,1)            where the record's address goes
         L     6,76(,13)
         ST    13,4(,6)
         LR    13,6
         LA    6,FRAME(,13)
         ST    6,76(,13)
         LA    6,DECBOFF(,2)
         C     3,=F'4'
         BNE   IBNEW
         MVC   4(2,6),BDECBS+64+4  WRITE K, the rest as READ KU left it
         XC    0(4,6),0(6)         the ECB
         B     IBGO
IBNEW    LR    7,3
         SLL   7,5                 32 bytes a model
         LA    7,BDECBS(7)
         MVC   0(32,6),0(7)
         ST    2,8(,6)             the DCB
         ST    4,12(,6)            the data area
         ST    5,20(,6)            the key
IBGO     LR    1,6
         LA    8,88                READ, WRITE K: DCB+88
         C     3,=F'3'
         BNE   IBCALL
         LA    8,92                WRITE KN: DCB+92
IBCALL   L     15,0(8,2)
         BALR  14,15
         CHECK (6),DSORG=IS        (SYNAD: IBSYN)
         MVC   0(4,9),16(6)        the logical record
         SR    15,15
         ICM   15,3,24(6)          the exception code
         L     13,4(,13)
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
         DROP  12
* SYNAD: the exception code stays in the DECB
IBSYN    BR    14
* SYNAD of QISAM: the exception code stays in DCBEXCD1
IQSYN    BR    14
*
         ENTRY PL1ISTK
PL1ISTK  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         LM    2,3,0(1)            the area, the key
         L     4,76(,13)
         ST    13,4(,4)
         LR    13,4
         LA    4,FRAME(,13)
         ST    4,76(,13)
         USING IHADCB,2
         ESETL (2)
         MVI   DCBEXCD1,0
         SETL  (2),K,(3)
         SR    15,15
         TM    DCBEXCD1,X'80'      no record with the key
         BZ    ISTRET
         ESETL (2)
         SETL  (2),B
         LA    15,1
ISTRET   L     13,4(,13)
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
         DROP  2
         DROP  12
*
* The DCB exit of OPEN: R1 -> the DCB.  F records with no LRECL get
* LRECL = BLKSIZE, as PL/I (F)'s open process sets it.
IOPEXIT  DS    0H
         USING IHADCB,1
         LH    0,DCBLRECL
         LTR   0,0
         BNZR  14
         MVC   DCBLRECL,DCBBLKSI
         BR    14
         DROP  1
         DS    0F
IEXLST   DC    X'85',AL3(IOPEXIT)  the last entry: the DCB exit
*
         ENTRY PL1ICLS
PL1ICLS  DS    0H
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
         LTORG
         DS    0F
IGSAVE   DC    2A(0)               the frame of PL1IGET, its target
MODELS   DC    A(LMODEL,IMODEL,SMODEL,BMODEL,BMODEL)
LMODEL   DCB   DSORG=IS,MACRF=PM,DDNAME=PL1ISAM,EXLST=IEXLST
         DS    0F
IMODEL   DCB   DSORG=IS,MACRF=(GL,SK),DDNAME=PL1ISAM,EODAD=IGEOD,      X
               EXLST=IEXLST,SYNAD=IQSYN
         DS    0F
SMODEL   DCB   DSORG=IS,MACRF=(GL,SK,PU),DDNAME=PL1ISAM,EODAD=IGEOD,   X
               EXLST=IEXLST,SYNAD=IQSYN
         DS    0F
BMODEL   DCB   DSORG=IS,MACRF=(RUC,WUAC),DDNAME=PL1ISAM,SYNAD=IBSYN,   X
               EXLST=IEXLST
DCBMAX   EQU   *-BMODEL
         DS    0XL(DECBOFF+1-DCBMAX)    the DCBs end before the DECB
         DS    0F
BDECBS   READ  BRDK,K,,,'S',MF=L
         ORG   BDECBS+32
         READ  BRDKU,KU,,,'S',MF=L
         ORG   BDECBS+64
         WRITE BWRK,K,,,'S',MF=L
         ORG   BDECBS+96
         WRITE BWRKN,KN,,,'S',MF=L
         ORG   BDECBS+128
         DCBD  DSORG=IS
         END
