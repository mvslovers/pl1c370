* PL1TIME -- the time of day and the date, for the TIME and DATE
* built-in functions (rt/pl1rt.c).
*
* PL1TOD(int out[2]) issues the TIME macro's SVC 11 with R1 = 2
* (TIME DEC, as the macro sets it; 0 is TIME TU) and stores what it returns: out[0] the time of day,
* packed HHMMSSth (t tenths, h hundredths of a second), out[1]
* the date, packed 0CYYDDDF.
         COPY  PDPTOP
         CSECT
         ENTRY PL1TOD
PL1TOD   DS    0H
         STM   14,12,12(13)
         L     2,0(,1)              A(out)
         LA    1,2                  TIME DEC
         SVC   11
         ST    0,0(,2)
         ST    1,4(,2)
         LM    14,12,12(13)
         SR    15,15
         BR    14
         END
