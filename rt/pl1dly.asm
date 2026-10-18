* PL1DLAY -- the DELAY statement (#228): STIMER WAIT for the number of
* milliseconds given, in the hundredths of a second STIMER counts;
* 0 or less does not wait (t228 Q3EVT).
*
*   void pl1_delay(int ms)
         COPY  PDPTOP
         CSECT
*
         ENTRY PL1DLAY
PL1DLAY  DS    0H
         STM   14,12,12(13)
         BALR  12,0
         USING *,12
         L     3,0(,1)             the milliseconds
         LTR   3,3
         BNP   DRET
         SR    2,2
         D     2,=F'10'            hundredths
         LTR   3,3
         BNP   DRET
         ST    3,DINT
         STIMER WAIT,BINTVL=DINT
DRET     LM    14,12,12(13)
         BR    14
         DROP  12
         LTORG
DINT     DC    F'0'
         END
