* PL1FETCH -- FETCH and RELEASE of an OPTIONS(ASSEMBLER) entry (gen.c,
* Tier 91): the module is LOADed at its FETCH, or at its first CALL
* when no FETCH came before, and DELETEd at its RELEASE.
*
* PL1FTCH(name) returns the entry point of the module name (CL8),
* LOADing it (SVC 8, EPLOC) when it is not in the table yet; a module
* that cannot be found ends in LOAD's S806, as with PL/I (F).
* PL1RLSE(name) DELETEs it (SVC 9) when it is in the table, and frees
* its slot; a RELEASE of what is not loaded does nothing.
*
* The table: 16 slots of the name (CL8) and the entry point (A), the
* name X'00' when the slot is free.
         COPY  PDPTOP
         CSECT
*
         ENTRY PL1FTCH
PL1FTCH  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         L     2,0(,1)             A(the name)
         LA    3,FTAB
         LA    4,16
FSRCH    CLC   0(8,3),0(2)
         BE    FFOUND
         LA    3,12(,3)
         BCT   4,FSRCH
         LA    3,FTAB              not loaded: a free slot
         LA    4,16
FFREE    CLI   0(3),0
         BE    FLOAD
         LA    3,12(,3)
         BCT   4,FFREE
         LA    1,4091              more than 16 modules fetched:
         SVC   13                  ABEND U4091
FLOAD    LR    0,2                 LOAD EPLOC=(name)
         SR    1,1
         SVC   8
         MVC   0(8,3),0(2)
         ST    0,8(,3)
FFOUND   L     15,8(,3)
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
         DROP  12
*
         ENTRY PL1RLSE
PL1RLSE  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         L     2,0(,1)             A(the name)
         LA    3,FTAB
         LA    4,16
RSRCH    CLC   0(8,3),0(2)
         BE    RFOUND
         LA    3,12(,3)
         BCT   4,RSRCH
         B     RDONE               not loaded: nothing to do
RFOUND   LR    0,2                 DELETE EPLOC=(name)
         SVC   9
         XC    0(12,3),0(3)        the slot free again
RDONE    LM    14,12,12(13)
         SR    15,15
         BR    14
         DROP  12
*
         LTORG
FTAB     DC    16XL12'00'
         END
