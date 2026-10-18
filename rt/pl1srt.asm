* PL1SRT -- the LINK to the sort program for PLISRTA to PLISRTD
* (rt/pl1sort.c, #233).
*
*   int pl1_srtlink(unsigned *r1)
*
* LINK EP=SORT with R1 at the word r1 points to (X'80' and the address
* of the parameter list), as PL/I (F) does; the sort's return
* code is returned.  The sort gets a save area of its own, taken from
* the next available byte of the caller's frame (+76), so that it does
* not overwrite the registers saved here; its address is kept for the
* exits.
*
* PL1SE15, PL1SE35: the exits whose addresses go into the list.  The
* sort enters them with R13 at its save area, R1 at its list for the
* exit.  They save the sort's registers there, take a save area above
* the one PL1SRTL gave the sort -- the program's stack goes on from
* there -- and call pl1_srtexit (rt/pl1sort.c), which calls the PL/I
* procedure; they return its code in R15, and for E15 the record's
* address in R1.  One sort at a time: the state is static.
*
* PL1SABN: abandon the sort (rt/pl1sort.c sort_leave).  The caller's
* registers are kept, the program's SPIE is cancelled, and the exit
* returns to the sort as though pl1_srtexit had returned: E15 8, E35 8
* when the sort's list has no record, else 4.  The exits answer so from
* then on without a call, and after the LINK PL1SRTL resumes the caller
* of PL1SABN -- with the SPIE again, and PL1SRTHK cleared.
*
*   void pl1_callproc(void *entry, void *alist)
*
* calls a PL/I procedure as the generated code does: R1 at its argument
* list, R0 0 (its first entry point), a save area of its own from the
* caller's next available byte.
         COPY  PDPTOP
         CSECT
*
         ENTRY PL1SRTL
PL1SRTL  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         L     2,0(,1)             A(the word for R1)
         L     3,76(,13)           the save area for the sort
         ST    3,SRTSA
         ST    13,4(,3)            chained to this one
         ST    3,8(,13)
         LA    4,80(,3)
         ST    4,76(,3)
         LR    13,3
         LR    1,2
         LA    15,SRTEP
         SVC   6                   LINK EP=SORT
         L     13,4(,13)
         CLI   ABANDON,0
         BNE   SRTRES
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
SRTRES   MVI   ABANDON,0           the sort was abandoned: resume
         L     1,=V(PL1SRTHK)
         XC    0(4,1),0(1)
         L     15,=V(PL1PION)
         BALR  14,15
         L     13,CONT13
         LM    14,12,12(13)
         SR    15,15
         BR    14
         DROP  12
*
SRTEP    DC    A(SRTNAME),A(0)
SRTNAME  DC    CL8'SORT'
*
         ENTRY PL1SE15
PL1SE15  DS    0H
         USING PL1SE15,15
         STM   14,12,12(13)        the sort's registers
         LA    2,0                 E15
         B     SEXCOM
         DROP  15
*
         ENTRY PL1SE35
PL1SE35  DS    0H
         USING PL1SE35,15
         STM   14,12,12(13)
         LA    2,1                 E35
         B     SEXCOM
         DROP  15
*
SEXCOM   BALR  12,0
         USING *,12
         CLI   ABANDON,0
         BNE   SEXABN
         ST    13,SORTR13
         L     3,SRTSA             the save area the sort was given
         LA    4,80(,3)            one above it for the dispatcher
         ST    3,4(,4)
         LA    5,88(,4)
         ST    5,76(,4)
         LR    13,4
         ST    2,SXARGS            pl1_srtexit(e35, list, &r1)
         ST    1,SXARGS+4
         LA    5,SXR1
         ST    5,SXARGS+8
         LA    1,SXARGS
         L     15,=V(PL1SRTX)
         BALR  14,15
         L     1,SXR1              E15: the record
         L     13,SORTR13
         L     14,12(,13)
         LM    2,12,28(13)
         BR    14
* abandoned: no call; E15 8, E35 8 without a record, else 4
SEXABN   LA    15,8
         LTR   2,2
         BZ    SEXABR
         L     3,0(,1)
         LTR   3,3
         BZ    SEXABR
         LA    15,4
SEXABR   L     14,12(,13)
         LM    2,12,28(13)
         BR    14
         DROP  12
*
         ENTRY PL1SABN
PL1SABN  DS    0H
         STM   14,12,12(13)        the caller's, for the resumption
         BALR  12,0
         USING *,12
         ST    13,CONT13
         LA    13,ABNSA
         L     15,=V(PL1PIOF)      the program's SPIE off
         BALR  14,15
         MVI   ABANDON,1
         LA    15,8
         ICM   2,15,SXARGS         E15: 8
         BZ    SABN1
         L     2,SXARGS+4          the sort's list
         L     3,0(,2)
         LTR   3,3
         BZ    SABN1
         LA    15,4
SABN1    L     13,SRTSA            back into the exit at its call of
         LA    13,80(,13)          pl1_srtexit, with the code
         ST    15,16(,13)
         LM    14,12,12(13)
         BR    14
         DROP  12
*
         ENTRY PL1CALP
PL1CALP  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         L     2,0(,1)             the entry point
         L     3,4(,1)             the argument list
         L     4,76(,13)           a save area at the NAB
         ST    13,4(,4)
         ST    4,8(,13)
         LA    5,80(,4)
         ST    5,76(,4)
         LR    13,4
         LR    15,2
         LR    1,3
         SR    0,0                 the first entry point
         BALR  14,15
         L     13,4(,13)
         LM    14,12,12(13)
         SR    15,15
         BR    14
         DROP  12
*
         LTORG
         ENTRY PL1SRSA
PL1SRSA  DS    0F
SRTSA    DC    A(0)
CONT13   DC    A(0)
ABNSA    DC    18F'0'
ABANDON  DC    X'00'
SORTR13  DC    A(0)
SXARGS   DC    3A(0)
SXR1     DC    A(0)
         END
