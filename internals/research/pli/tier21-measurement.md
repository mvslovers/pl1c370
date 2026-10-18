# Tier 21 rules, measured against PL/I (F) V5.5

Measured 2026-10-13 on MVSTK5-BLD. Probe (JOB00336), which is also the test `tests/pli/t98bits` (JOB00338). With
`t99bits2` and Tiers 0 to 20: 101 programs identical (JOB00342 to
JOB00344).

## Bit strings

- BIT(n) beyond 32 bits and BIT(n) VARYING. Assignment pads a fixed
  target with zeros on the right or truncates it; a VARYING target takes
  the source's length up to its own (`V = '101'B`: length 3; a BIT(40)
  into BIT(8) VARYING: the first 8 bits).
- `&` and `|` pad the shorter operand with zeros; `¬` keeps the length;
  `||` joins (`V || L`: 3 + 40 = 43 bits).
- SUBSTR(x, i [, j]) of a bit string is a bit string (SUBSTR(L, 30) of a
  BIT(40) is 11 bits).
- Comparison pads the shorter with zeros (`'1'B = '1000'B` is true).
- A bit string as a condition is true when a bit is 1; the null string
  `''B` is false.
- Assigned to CHARACTER, the bits become `'1'` and `'0'` characters;
  assigned to FIXED BINARY, the bits' value (`SUBSTR(L,1,31)` of
  `1010...` is 1431655765).
- PUT LIST and PUT DATA quote them (`'...'B`), the A format does not.

PL1C/370 keeps a bit string of at most 32 bits whose operands are short
too in a register, as before; any other bit expression is computed in
storage, address and length in bits, by `rt/pl1rt.c` (`pl1_bit_*`). A
VARYING bit string is a halfword of its length in bits and the bytes.
The longest bit string is 4096 bits (frame temporaries).

## Not done

GET into a bit string of more than 32 bits, `||` of a bit string and a
character string, UNSPEC of more than 32 bits, a function returning a
long bit string, the B format item.
