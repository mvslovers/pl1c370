* PL1PIE -- the program interruption exit of PL1C/370.
*
* PL1PION, called from the main procedure's prologue, issues a
* SPIE for interruptions 12 (exponent overflow) and 15
* (floating-point divide).  No maskable interruption is named,
* so the program mask stays 0, as it is without the exit.
*
* The exit itself only saves the old PSW and registers 14 to
* 2 from the PIE and changes the address in the PIE's PSW:
* the supervisor then resumes at PL1PITR, outside the exit,
* with every register as it was at the interruption.
* PL1PITR saves registers 3 to 13 and the
* floating-point registers in a frame from the NAB and calls
* pl1_program_check (C), which raises the condition.  If that
* returns (a normal return of the on-unit, or the condition
* disabled), PL1PITR restores what it saved and divides by
* zero on purpose at PL1PIRT: the exit, entered for that, puts
* the saved PSW and registers 14 to 2 back into the PIE, and
* the supervisor resumes the program after the interrupted
* instruction with all sixteen registers, the condition code
* and the program mask as they were.  An interruption inside
* an on-unit goes the same way, one frame further on.
*
* The frame of PL1PITR (FRAME bytes from the NAB):
*   0   save area, 4 back chain, 76 NAB
*   88  copy of the PIE: old PSW (8), registers 14 to 2 (20)
*   116 registers 3 to 13
*   160 floating-point registers 0, 2, 4, 6
*   192 argument list: interruption code, PSW address
         COPY  PDPTOP
         CSECT
FRAME    EQU   200
*
* PL1PION: establish the exit
         ENTRY PL1PION
PL1PION  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         LA    1,PL1PICA
         SVC   14                  SPIE
         ST    1,OLDPICA           the caller's, back at the end
         LM    14,12,12(13)
         BR    14
         DROP  12
*
* PL1PIOF: the caller's SPIE again (none: R1 0 cancels ours), so that
* a program LINKed to -- an external REXX function -- leaves no exit
* behind in a module that is deleted after it returns
         ENTRY PL1PIOF
PL1PIOF  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         L     1,OLDPICA
         SVC   14                  SPIE
         LM    14,12,12(13)
         BR    14
         DROP  12
*
* The exit: R1 -> the PIE, R14 the return, R15 its address.
* Registers 3 to 13 are the program's and are left alone.
PL1PIEX  DS    0H
         USING PL1PIEX,15
         CLC   9(3,1),=AL3(PL1PIRT)
         BE    PIEXRES
* an interruption of the program: keep the PIE, resume at PL1PITR
         MVC   PIESAVE(28),4(1)
         LA    0,PL1PITR
         STCM  0,7,9(1)
         BR    14
* the trap of PL1PITR: the program's PSW and registers 14 to 2 back
PIEXRES  L     2,PIERES
         MVC   4(28,1),88(2)
         BR    14
         DROP  15
*
* Entered from the exit, with the program's registers.  Registers
* 14 to 2 are in PIESAVE, so they are free here.
PL1PITR  DS    0H
         L     15,76(,13)          the frame, from the NAB
         STM   3,13,116(15)
         LR    2,15
         BALR  12,0
         USING *,12
         MVC   88(28,2),PIESAVE
         STD   0,160(,2)
         STD   2,168(,2)
         STD   4,176(,2)
         STD   6,184(,2)
         ST    13,4(,2)
         LA    0,FRAME(,2)
         ST    0,76(,2)
         LR    13,2
         SR    0,0
         ICM   0,3,90(13)          the interruption code
         ST    0,192(,13)
         L     0,92(,13)
         LA    0,0(,0)             the address after the instruction
         ST    0,196(,13)
         LA    1,192(,13)
         L     15,=V(PL1PICK)
         BALR  14,15
* back to the program, by the exit
         LR    2,13
         LD    0,160(,2)
         LD    2,168(,2)
         LD    4,176(,2)
         LD    6,184(,2)
         XC    192(4,2),192(2)
         ST    2,PIERES
         LM    3,13,116(2)
         DE    0,192(,2)           divide by zero: the exit resumes
PL1PIRT  DS    0H                  the program (the PSW points here)
         DROP  12
         LTORG
*
         DS    0F
PL1PICA  DC    X'00',AL3(PL1PIEX)  no program mask bits
         DC    X'0009'             interruptions 12 and 15
PIESAVE  DS    XL28                the PIE of the last interruption
PIERES   DS    A                   the frame PL1PITR resumes from
OLDPICA  DC    A(0)                the caller's PICA (PL1PIOF)
         END
