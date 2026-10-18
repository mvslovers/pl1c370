* ASMTST -- an assembler routine for the tests of OPTIONS(ASSEMBLER)
* (t240asm): R1 at a list of addresses, the last with its high-order
* bit on.  The first argument is a FIXED BINARY(31) function code:
*   1  A, B, C   C = A + B, all FIXED BINARY(31)
*   2  S         the five bytes at S become 'HELLO'
*   3  ...       nothing more
*   5  ...       R15 is 10 times the number of arguments plus the
*                number of words of zeros (omitted arguments, OPTIONAL)
* R15 on return is the number of arguments, for 5 as above.
* Deployed as a module of its own, for PL/I (F)'s link-edit to find
* by autocall, and linked into the PL1C/370 test module.
ASMTST   CSECT
         STM   14,12,12(13)
         LR    12,15
         USING ASMTST,12
         SR    3,3                 the number of arguments
         SR    9,9                 the words of zeros among them
         LR    2,1
COUNT    LA    3,1(,3)
         L     10,0(,2)
         LA    10,0(,10)           the address, without the high bit
         LTR   10,10
         BNZ   NOTZERO
         LA    9,1(,9)
NOTZERO  TM    0(2),X'80'
         BO    COUNTED
         LA    2,4(,2)
         B     COUNT
COUNTED  L     4,0(,1)             A(the code)
         LA    4,0(,4)
         L     4,0(,4)
         C     4,=F'1'
         BNE   NOT1
         L     5,4(,1)             A(A)
         L     6,8(,1)             A(B)
         L     7,12(,1)            A(C), the last
         LA    7,0(,7)             without the high-order bit
         L     8,0(,5)
         A     8,0(,6)
         ST    8,0(,7)
         B     DONE
NOT1     C     4,=F'5'
         BNE   NOT5
         MH    3,=H'10'
         AR    3,9
         B     DONE
NOT5     C     4,=F'2'
         BNE   DONE
         L     5,4(,1)             A(S)
         LA    5,0(,5)
         MVC   0(5,5),=C'HELLO'
DONE     LR    15,3
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
         LTORG
         END   ASMTST
