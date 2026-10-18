# Tier 1 rules, measured against PL/I (F) V5.5

Measured 2026-10-12 on MVSTK5-BLD. Probes: oracle
output beside them (`*.oracle`, JOB00070 and JOB00071). The test programs
`tests/pli/t7div` to `t11mix` print byte for byte what PL/I (F) prints
(JOB00077, together with all of Tier 0).

## The NOT sign

mvsMF translates the job stream from Latin-1. The NOT sign reaches PL/I (F)
as X'5F' only when it is sent as **Latin-1 X'AC' (`¬`)**: `A ¬= 2` and
`¬(A = 2)` work (JOB00070, step NAC). `^` and `~` arrive as invalid
characters (IEM0097I). Test sources are UTF-8 and write `¬`; `submit.py`
encodes them as Latin-1. PL1C/370 accepts `¬` and `^`.

## Division

| Operands | Result | Example (oracle) |
|---|---|---|
| FIXED DECIMAL / FIXED DECIMAL | (15, 15-((p1-q1)+q2)) | `A/B` (5,2)/(5,2) = `3.3333333333`; `7/2` = `3.50000000000000` |
| FIXED BINARY / FIXED BINARY | (31, 31-((p1-q1)+q2)), a scaled binary | `I/J` (15)/(15) = `3.50000`; `K/L` (31)/(31) = `142` |
| mixed | the decimal operand converted to binary first | `D50/J` = `50.0000` |

The quotient is truncated toward zero, in both bases (`K=-I/J` = `-3`).

## Scaled binary

A FIXED BINARY(p,q) value is converted to FIXED DECIMAL(1+CEIL(p/3.32),
CEIL(q/3.32)) for output, assignment to decimal and conversion to a string,
**with the binary fraction truncated**, not rounded:

- `J/K` = 2/1000 at scale 16 is the binary 131/65536, printed `0.00199`;
- `I/3` = -7/3 at scale 16 is -152917/65536, printed `-2.33332`.

Assigning a scaled binary to an integer truncates toward zero
(`K=-I/J` = `-3`, not -4).

A decimal fraction meeting a binary operand becomes a binary fraction the
same way, truncated: `I*1.1` with I = 3 is `3.18`, because 1.1 becomes
FIXED BINARY(8,4) = 17/16 (t11mix).

## Numbers as strings

A number becomes the PUT LIST field: p+3 characters, right-justified, a
FIXED BINARY counted as its decimal conversion. `'X=' || I` (BIN(15)) is
11 long; `K || '/'` (BIN(31)) is 15; `C = D` (DEC(7,2)) gives `    -12.50`.

## Bit strings

- PUT LIST writes `'1010'B`.
- A comparison is a BIT(1) value: `PUT LIST(I = J)` writes `'0'B`.
- `&`, `|` and NOT work bit by bit; a shorter operand is padded with zeros.
  Assignment truncates or pads with zeros on the right.
- A bit string is true in IF when any bit is 1, so `IF '10'B & '01'B` is
  false and `IF ¬'10'B` is true (t11mix).

## Built-in functions

| Function | Result | Example |
|---|---|---|
| SUBSTR(s,i[,j]) | CHARACTER | `SUBSTR('ABCDEF',2,3)` = `BCD` |
| INDEX(s,t) | FIXED BINARY(15) | `INDEX('ABCDEF','CD')` = 3 |
| MOD(x,y) | precision p2-q2+max(q1,q2); x - y*FLOOR(x/y) | `MOD(-17,5)` = 3 |
| ABS(x) | the type of x | `ABS(-5)` = 5 |
| MAX, MIN | precision max(pi-qi)+max(qi), scale max(qi) | `MAX(D,1.25)` = `1.25` in a DEC(7,2) field |
| LENGTH(s) | FIXED BINARY(15) | `LENGTH('AB' || 'CDE')` = 5 |

## Not measured yet

- MOD with fractions, and with a negative divisor.
- SUBSTR out of range (STRINGRANGE), and as a pseudovariable.
- Bit strings longer than 32 bits; bit to character or arithmetic
  conversion.
- Division by zero (ZERODIVIDE) and FIXEDOVERFLOW.
