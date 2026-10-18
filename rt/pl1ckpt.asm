* PL1CKPT -- PLICKPT, PLIREST, PLICANC: checkpoint and restart (#233).
*
* The interface of PL/I (F)'s IHECKPS, IHECKPT, IHEREST and IHERESN,
* as measured on PL/I (F) V5.5 (Tier 108,
* internals/design/sort-checkpoint.md):
*
*   PL1CKPT  CALL PLICKPT [(ddname [, check-id [, org [, code]]])]:
*            a DCB DSORG=PS,MACRF=W,DDNAME=SYSCHK, the ddname over it
*            (8 characters at most), the check-id (16 at most; 8 for a
*            PO data set), CHKPT (SVC 63; it opens and closes the DCB
*            itself), its return code unchanged into code.  An org of
*            other than PS or PO gives 8 and no checkpoint.  Without a
*            check-id the system makes one (the length X'FF').
*            R1 -> A(a block): A(ddname), its length, A(check-id), its
*            length, A(org), its length, A(code) or 0; a length of 0
*            or less is an argument omitted (gen.c).  In assembler:
*            after a restart the program resumes after the SVC 63 with
*            R15 4, its registers and storage as they were.
*   PL1REST  CALL PLIREST: ABEND with the user code 4092, an automatic
*            restart if RD and the system allow it.
*   PL1CANC  CALL PLICANC: CHKPT CANCEL.
         COPY  PDPTOP
         CSECT
*
         ENTRY PL1CKPT
PL1CKPT  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         L     9,0(,1)             the argument block
         MVC   WDCB(MODELEND-MODEL),MODEL
         MVC   WID,=CL16' '
         LA    6,255               no check-id: the system's
         SR    7,7                 the code
         L     3,4(,9)             the ddname
         LTR   3,3
         BNP   CKID
         CH    3,=H'8'
         BNH   CKDD1
         LA    3,8
CKDD1    MVC   WDCB+40(8),=CL8' '  DCBDDNAM
         BCTR  3,0
         L     2,0(,9)
         EX    3,MVCDD
CKID     L     3,12(,9)            the check-id
         LTR   3,3
         BNP   CKORG
         CH    3,=H'16'
         BNH   CKID1
         LA    3,16
CKID1    LR    6,3
         BCTR  3,0
         L     2,8(,9)
         EX    3,MVCID
CKORG    L     3,20(,9)            the organization
         LTR   3,3
         BNP   CKTAKE
         LA    7,8                 neither PS nor PO: 8, no checkpoint
         CH    3,=H'2'
         BNE   CKCODE
         L     2,16(,9)
         CLC   0(2,2),=C'PS'
         BE    CKPS
         CLC   0(2,2),=C'PO'
         BNE   CKCODE
         MVC   WDCB+26(2),=X'0200' DCBDSORG PO
         CH    6,=H'255'
         BE    CKPS
         LA    6,8                 a member: 8 characters
CKPS     SR    7,7
CKTAKE   LA    1,WDCB
         ST    1,CKLIST
         LA    1,WID
         ST    1,CKLIST+4
         STC   6,CKLIST+4          AL1(the length),AL3(the check-id)
         LA    1,CKLIST
         SVC   63                  CHKPT
         LR    7,15
CKCODE   L     2,24(,9)            the code, when given
         LTR   2,2
         BZ    CKRET
         ST    7,0(,2)
CKRET    LM    14,12,12(13)
         BR    14
MVCDD    MVC   WDCB+40(0),0(2)
MVCID    MVC   WID(0),0(2)
         DROP  12
*
         ENTRY PL1REST
PL1REST  DS    0H
         STM   14,12,12(13)
         LA    1,4092              ABEND 4092, user, no dump
         SVC   13
*
         ENTRY PL1CANC
PL1CANC  DS    0H
         STM   14,12,12(13)
         SR    1,1                 CHKPT CANCEL
         SVC   63
         LM    14,12,12(13)
         BR    14
*
         LTORG
MODEL    DCB   DSORG=PS,MACRF=W,DDNAME=SYSCHK
MODELEND EQU   *
WDCB     DS    0D
         DS    XL(MODELEND-MODEL)
CKLIST   DC    2A(0)
WID      DS    CL16
         END
