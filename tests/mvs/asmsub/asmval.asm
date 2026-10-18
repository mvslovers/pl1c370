* ASMVAL -- an assembler routine for the tests of OPTIONS(ASSEMBLER
* BYVALUE) (t241opt): R1 at a list of two values, FIXED BINARY(31),
* the words themselves.  R15 on return is their sum.
ASMVAL   CSECT
         STM   14,12,12(13)
         L     15,0(,1)
         A     15,4(,1)
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
         END   ASMVAL
