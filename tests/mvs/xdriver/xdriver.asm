XDRIVER  CSECT
*---------------------------------------------------------------------
* XDRIVER -- CALL AN EXTERNAL REXX FUNCTION AS TSO/E REXX DOES
*
*   EXEC PGM=XDRIVER,PARM='MODULE,ARG1,ARG2,...'
*
* LINKS TO MODULE WITH R0 = 0 AND R1 = A(EFPL) WITH THE HIGH BIT:
* SIX WORDS, THE FIFTH A(ARGUMENT TABLE), THE SIXTH A(A(EVALBLOCK)).
* THE TABLE HAS ADDRESS AND LENGTH OF EACH ARGUMENT, X'FF' AFTER THE
* LAST; THE EVALUATION BLOCK 16 BYTES OF HEADER AND 1024 OF DATA,
* EVLEN X'80000000'.  WRITES TO SYSPRINT (RECFM=VBA, LRECL=125, AS
* PL/I PRINT FILES ARE), AFTER THE PROGRAM'S OWN LINES: THE PROGRAM'S
* R15 AND EVLEN, THEN EVDATA,
* 100 CHARACTERS A LINE.  ENDS WITH THE PROGRAM'S R15.
*   PARM='2*MODULE,...' LINKS TO IT TWICE, EACH CALL REPORTED (#220).
*---------------------------------------------------------------------
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         ST    13,SAVE+4
         LA    13,SAVE
         L     2,0(,1)             A(PARM): HALFWORD LENGTH, TEXT
         LA    2,0(,2)
         LH    3,0(,2)             ITS LENGTH
         LA    4,2(,2)             ITS FIRST CHARACTER
         MVC   COUNT,=F'1'
         CH    3,=H'2'             '2*': TWO CALLS
         BL    ONE
         CLC   0(2,4),=C'2*'
         BNE   ONE
         MVC   COUNT,=F'2'
         LA    4,2(,4)
         SH    3,=H'2'
ONE      MVI   ARGT,X'FF'          THE TABLE: X'FF' THROUGHOUT
         MVC   ARGT+1(255),ARGT
         MVC   MODNAME,=CL8' '
*        THE MODULE NAME: UP TO THE FIRST COMMA, AT MOST 8
         LA    5,MODNAME
         LA    6,8
NAMEL    LTR   3,3
         BNP   NAMED
         CLI   0(4),C','
         BE    NAMEC
         LTR   6,6
         BNP   NAMES
         MVC   0(1,5),0(4)
         LA    5,1(,5)
         BCTR  6,0
NAMES    LA    4,1(,4)
         BCTR  3,0
         B     NAMEL
NAMEC    LA    4,1(,4)             PAST THE COMMA
         BCTR  3,0
*        THE ARGUMENTS: COMMA SEPARATED, EACH AN ENTRY OF THE TABLE
         LA    7,ARGT
         LA    8,32                AT MOST 32
ARGL     ST    4,0(,7)             ITS ADDRESS
         SR    9,9                 ITS LENGTH
ARGC     LTR   3,3
         BNP   ARGE
         CLI   0(4),C','
         BE    ARGN
         LA    9,1(,9)
         LA    4,1(,4)
         BCTR  3,0
         B     ARGC
ARGN     ST    9,4(,7)
         LA    4,1(,4)
         BCTR  3,0
         LA    7,8(,7)
         BCT   8,ARGL
         B     NAMED
ARGE     ST    9,4(,7)
NAMED    DS    0H
         SR    0,0
         LA    1,EFPL
         O     1,HIGH
         LA    2,MODNAME
         LINK  EPLOC=(2)
         ST    15,RC
*        SYSPRINT AFTER THE CALL, AT ITS END: THE PROGRAM MAY HAVE
*        WRITTEN TO IT AND CLOSED IT
         OPEN  (PRT,EXTEND)
*        RC AND EVLEN
         BAL   14,CLEAR
         MVC   TEXT(3),=C'RC='
         L     4,RC
         CVD   4,DW
         UNPK  TEXT+3(5),DW
         OI    TEXT+7,X'F0'
         MVC   TEXT+10(6),=C'EVLEN='
         L     4,EVB+8
         CVD   4,DW
         UNPK  TEXT+16(5),DW
         OI    TEXT+20,X'F0'
         LA    6,21
         BAL   14,WRITE
*        EVDATA, 100 A LINE
         L     4,EVB+8
         LTR   4,4
         BNP   DONE
         C     4,=F'1024'
         BNH   *+8
         L     4,=F'1024'
         LA    5,EVB+16
DATAL    BAL   14,CLEAR
         LR    6,4
         C     6,=F'100'
         BNH   *+8
         LA    6,100
         BCTR  6,0
         EX    6,MOVE
         LA    6,1(,6)
         BAL   14,WRITE
         LA    5,100(,5)
         S     4,=F'100'
         BP    DATAL
DONE     CLOSE (PRT)
         L     4,COUNT             ANOTHER CALL?
         BCTR  4,0
         ST    4,COUNT
         LTR   4,4
         BNP   LAST
         MVC   EVB+8(4),=X'80000000'
         B     NAMED
LAST     L     15,RC
         L     13,SAVE+4
         L     14,12(13)
         LM    0,12,20(13)
         BR    14
*        A BLANK RECORD
CLEAR    MVI   TEXT,C' '
         MVC   TEXT+1(119),TEXT
         BR    14
*        WRITE TEXT, R6 CHARACTERS: RDW, ASA ' ', THE TEXT
WRITE    ST    14,WRITER
         LA    7,5(,6)
         STH   7,REC
         XC    REC+2(2),REC+2
         MVI   ASA,C' '
         PUT   PRT,REC
         L     14,WRITER
         BR    14
MOVE     MVC   TEXT(0),0(5)
SAVE     DS    18F
WRITER   DS    F
RC       DS    F
COUNT    DS    F
DW       DS    D
HIGH     DC    X'80000000'
MODNAME  DS    CL8
REC      DS    H,H
ASA      DS    C
TEXT     DS    CL120
PRT      DCB   DDNAME=SYSPRINT,DSORG=PS,MACRF=PM,RECFM=VBA,LRECL=125,  X
               BLKSIZE=129
EFPL     DC    4F'0',A(ARGT),A(EVP)
EVP      DC    A(EVB)
         DS    0F
ARGT     DS    33XL8
EVB      DC    F'0',F'130',X'80000000',F'0'
         DC    4CL256' '
         END   XDRIVER
