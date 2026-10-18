# Tier 71: radix factors B1 to B4

ANSI X3.53-1976 writes bit strings in four radices (CL22, CL23):
- `'101'B` or `'101'B1` in binary;
- `'132'B2` in base 4;
- `'17'B3` in octal;
- `'F0A5'B4` in hexadecimal.

Each digit stands for 1 to 4 bits (4.4.7, Table 4.2). The format items
B1 to B4 write and read bit strings in the same digits (CM145). PL/I (F)
has `'...'B` and the B format only.

Beyond the oracle: the test `t210rdx` is a twin pair, identical in
JOB01111. The twin writes the bits and characters the standard gives.

## The rules taken

- **A constant:** each digit becomes m bits, from 0 to 2^m - 1:
  `'F0A5'B4` is `'1111000010100101'B`, `'17'B3` is `'001111'B`.
- **Output with Bm(w)** (8.7.2.4):
  - zero bits are added at the end to a whole number of digits, and the
    digits are written left-justified in w, blanks after;
  - B3 of the 16 bits above is `741224` (18 bits);
  - more digits than w raise STRINGSIZE, then the digits are cut:
    `B4(2)` writes `F0`;
  - Bm without a width writes all the digits.
- **Input with Bm(w)** (8.7.1.9) takes the digits of the field, blanks
  around them allowed, and m bits for each: `B4(3)` of `7E2` is
  `'011111100010'B`.

## In PL1C/370

- **lex.c** expands the digits of a radix constant to bits, so the rest
  of the compiler sees an ordinary bit constant.
- **parse.c** reads B1 to B4 as the B format, with the radix in the
  format item's d.
- **rt/pl1ioed.c** groups the bits into digits for output. More digits
  than the width raise STRINGSIZE. That holds for B(w) too, where the
  system action changes nothing for a PL/I (F) program.
- **rt/pl1ioget.c** expands each digit read into its bits.

## The suite

All 212 programs are identical: JOB01113 to JOB01117, and JOB01119 for
`t45fge`, whose PL/I (F) compile step ended with condition code 16 in
JOB01116, as before on this stand.
