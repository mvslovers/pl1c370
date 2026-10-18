# Tier 57: E and F of character strings, B of FIXED values

Tier 56 measured these with Q1FMT (JOB00901); PL1C/370's run time
stopped with a message there. Measured further on 2026-10-13 on
MVSTK5-BLD. The tests are identical:

| Test | Job |
|---|---|
| `t183fcv`: E and F of strings, B of numbers, B without a width | JOB00918 |
| `t184bfm`: B(w) of FIXED BINARY and FIXED DECIMAL values | JOB00918 |

## E and F of a character string

The string is converted to a number first, then edited:

| Item | Format | Output |
|---|---|---|
| `'1.5E2'` | E(12,3) | `1.500E+02` |
| `' -42.5'` | E(12,3) | `-4.250E+01` |
| `'1.5E2'` | F(8,1) | `150.0` |
| `' -42.5'` | F(8,2) | `-42.50` |
| `'  0.0625 '` (VARYING) | F(10,4) | `0.0625` |
| `'  0.0625 '` (VARYING) | E(14,5) | `6.25000E-02` |
| `'7'` | F(4) | `7` |
| `'-3E-1'` | F(8,2) | `-0.30` |

**PL1C/370** reads the string as blanks, a sign, digits with a point, an
exponent and blanks (`str_number` in `rt/pl1ioed.c`), and edits the
digits as those of a number. A string that is no number still stops the
program with a message. PL/I (F) raises CONVERSION there (not
measured).

## B(w) of a FIXED value

The value is converted to a bit string first: the integer part of its
absolute value, in as many bits as the precision gives. B(w) then
places that bit string as it places any, left-justified in w, cut on
the right.

| Value | Bits | B(w) | Output |
|---|---|---|---|
| 5, FIXED DEC(1) | 4 | B(8) | `0101` |
| 10, FIXED DEC(2) | 7 | B(6) | `000101` |
| -3 | 4 | B(4) | `0011` |
| 2.75, FIXED DEC(3,2) | 4 | B(8) | `0010` |
| 13, FIXED BIN(15) | 15 | B(20) | `000000000001101` |
| 1000, FIXED BIN(31) | 31 | B(40) | 31 bits |
| -19.99, FIXED DEC(5,2) | 10 | B(8) | `00000100` |
| 255, FIXED DEC(7) | 24 | B(30) | `000000000000000011111111` |
| 123456789, FIXED DEC(15) | 31, not 50 | B(60) | 31 bits |

- **The number of bits:**
  - FIXED BINARY(p,q) gives p - q bits.
  - FIXED DECIMAL(p,q) gives ceil((p - q) * 3.32) bits, at most 31.
- **B without a width and a number** is an error: `IHE703I B FORMAT W
  UNSPECIFIED AND LIST ITEM NOT TYPE STRING`, then ERROR (step code
  2000).

**PL1C/370:** the PUT routines of FIXED BINARY and FIXED DECIMAL pass
that number of bits to the edit (`pl1x_bits`). The edit divides the
integer digits by 2, again and again, for the bits.

## Not done

- **B of a FLOAT value, of a numeric picture, or of a scaled FIXED
  BINARY value:** scaled FIXED BINARY variables are not supported at
  all yet. For the others the run time still stops with a message.
- **A string that is no number with E or F:** CONVERSION.
