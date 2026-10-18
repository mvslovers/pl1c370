# Tier 69: bit strings in ANSI's string functions

ANSI X3.53-1976 (9.4.4) defines AFTER, BEFORE, DECAT, REVERSE and COPY
on the derived common string type of their arguments:
- a bit string when every string argument is one;
- a character string otherwise, a bit string becoming the characters
  '0' and '1'.

Tier 65 took character strings only. The functions are beyond the
oracle. The test `t208anb` is a twin pair, as in Tier 65: PL1C/370
compiles `t208anb.opt.pli`, PL/I (F) runs `t208anb.pli`. The twin uses
INDEX and SUBSTR of bit strings and the values the standard gives.
They are identical in JOB01092.

## What the test shows

For B = '10110110'B:

| Reference | Value |
|---|---|
| `AFTER(B, '11'B)` | `'0110'B`, length 4 |
| `BEFORE(B, '11'B)` | `'10'B` |
| `AFTER(B, '000'B)` | `''B` |
| `BEFORE(B, '000'B)` | B |
| `DECAT(B, '11'B, '101'B)` | `'100110'B` |
| `DECAT(B, '11'B, '010'B)` | `'11'B` |
| `REVERSE(B)` | `'01101101'B` |
| `COPY('10'B, 3)` | `'101010'B` |
| `COPY('1'B, N)` with N = 3 | `'111'B` |
| `COPY('01'B, 0)` | `''B` |
| `AFTER('XY0110Z', '11'B)` | `'0Z'`: CHARACTER, the bit string taken as '11' |

## In PL1C/370

- **check.c** decides the common type. With a character argument, a bit
  argument is converted by CHAR first.
- **rt/pl1ansi.c** spreads the bits to one character each, runs the
  character function and packs the result again. At most 4096 bits, the
  compiler's longest bit string.

## Also measured: locator qualifiers

`t69/Q1PQ` (JOB01090) shows that PL/I (F) takes only a non-based
pointer variable as a locator qualifier:
- `GETP->X` and `ADDR(A)->X` are refused (IEM0646I, IEM0030I);
- `X BASED` without a pointer is refused too (IEM0154I).

ANSI allows these. They are recorded in `ansi-comparison.md`; PL1C/370
refuses them as PL/I (F) does.

## The suite

All 210 programs are identical: JOB01094 to JOB01098.
