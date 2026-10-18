# Tier 7 rules, measured against PL/I (F) V5.5

Measured 2026-10-12 on MVSTK5-BLD. Probes with their
oracle output (JOB00156 to JOB00160). The test programs `tests/pli/t41flst`
to `t47fmix` print what PL/I (F) prints and write the same record, byte for
byte (JOB00170, together with Tiers 0 to 6: 49 programs).

## Types

- FLOAT DECIMAL(p) is short (4 bytes) up to 6 digits, long (8 bytes) up to
  16; FLOAT BINARY(p) short up to 21 bits, long up to 53. Bare FLOAT is
  FLOAT DECIMAL(6), and so is a name outside I to N declared implicitly,
  or declared without attributes; DECIMAL or BINARY alone is FLOAT.
- PUT LIST writes p digits of a FLOAT DECIMAL(p), ceil(p/3.32) of a FLOAT
  BINARY(p) (21 bits: 7, 53 bits: 16), as `d.ddddE+xx` right-justified in
  p+6 positions; FLOAT DEC(1) has no point (`1E+00`). The conversion to
  CHARACTER gives the same string, sign position included.
- A constant with an exponent has the precision of its digits: `2.5E10`
  is FLOAT DECIMAL(2), `1E0` FLOAT DECIMAL(1).
- Mixed operands: FIXED becomes FLOAT of its own base and precision; bases
  that differ become binary (a decimal precision p turns into
  ceil(p*3.32)); the result has the larger precision. So FLOAT DEC(6) +
  FIXED BIN(31) is FLOAT BIN(31) (long, printed with 10 digits), FLOAT
  DEC(6) * FIXED DEC(7,2) is FLOAT DEC(7), `1E0 + 1` FLOAT DEC(1).
- A constant is converted to the precision of the operation or target it
  meets: `B = 1E-70` with B long gives the long value, `2.0E0/3` is a
  short division.

## The conversions follow PL/I (F)'s method

Short values looked exact at first; long ones were off in the last digit
(1E-70 nine units low, 9.9999995 printed `...498`). UNSPEC showed which
direction: both. The model below reproduces every value measured.

**Float to decimal.** Take |x| as a long
float. While its characteristic is not 78: e = characteristic - 78,
n = min(78, round(|e| * log10 16)) (computed as (|e| * 19728 + 8192) >>
14), then divide (e > 0) or multiply (e < 0) by 10**(n mod 10) and by
10**(10 * (n div 10)), taking the powers from a table, and
count n into the scale. At characteristic 78 the 56-bit fraction is the
decimal integer (16 or 17 digits). MD and DD truncate. The table holds
10**1 to 10**9 and 10**10, 10**20, ..., 10**70 as long floats.

**Decimal to float.** The decimal integer I (17 digits at most)
with scale q, value I * 10**-q: if I >= 2**56, one digit is dropped
(I / 10, q - 1); I becomes a float exactly; then divide (q > 0) or multiply
by 10**(|q| mod 10) and by 10**10k, k up to 7 at a time, from the same
table. The program mask is set to 0 meanwhile, so an underflow gives 0.

**Constants are converted by the compiler with the same table, but the
digits are first padded to seventeen** (`3E-25` is 30000000000000000 *
10**-41). This fits all thirteen constants measured; the string `'3E-25'`
converted at run time is two units lower than the constant `3E-25`. A
constant of seventeen digits behaved oddly (`12345678901234567E-5` came
out ten times too small); PL1C/370 refuses more than sixteen.

`rt/pl1flt.c` implements both and the table exactly (big integers,
no C double); the compiler links the same file for its constants.
`tests/host/t_float.c` checks it against every value measured.

## Other conversions

- FIXED BINARY to FLOAT, FLOAT to FIXED BINARY: exact, truncated towards
  zero (2147483647 becomes 2147483392 in short form, 2.5 * 3.3 is 8).
- FLOAT to FIXED DECIMAL: through IHEVFA, truncated to the target's scale
  (2.5 / 3 to DEC(7,2) is 0.83).
- Long to short: the first word, truncated; short to long: exact.
- A string to FLOAT: `'2.5'`, `'-1.5E3'`; more than sixteen digits is
  CONVERSION.
- A string met by a FLOAT operand is FLOAT DECIMAL(15), not the FIXED
  DECIMAL(15,0) of Tier 5: 3.25 + '1.5' is `4.75000000000000E+00`, and
  `R.S * '2.5'` keeps its fraction too (JOB00166).

## Arithmetic

The machine does it: AE/AD, SE/SD, ME/MD, DE/DD, all truncating. A short
product is long in the register; PL/I (F) keeps its first word before the
next operation (`A*A*A` equals `T = A*A; T*A`, JOB00158). Division by zero
is ZERODIVIDE (320), checked before DE/DD. With the program mask PL/I (F)
runs under, `A - A` raises nothing; an exponent underflow is UNDERFLOW
(ONCODE 330): `IHE340I UNDERFLOW` and the program goes on with 0
(JOB00156, JOB00162).
PL1C/370 detects it for products and quotients (the machine gives 0 with
the mask clear; operands that are not 0 tell it apart).

## PUT EDIT and GET EDIT

- E(w,d,s): s significant digits (default d+1), **rounded** half up,
  s-d of them before the point, the exponent to match: `E(12,3,5)` of
  -12345.6789 is `-12.346E+03`. F(w,d) rounds too (0.5 to `1`, 0.125 to
  `0.13`). LIST and the conversion to CHARACTER truncate.
- An E item too narrow for its digits, the point and E+xx is deleted
  (IEM1871I); one with no room for a minus sign only warns (IEM1872I).
- GET EDIT: E(w,d) and F(w,d) without a point take d decimal places
  (`12345` in E(10,2) is 123.45). A blank E field raises CONVERSION, ONCODE
  607, ONSOURCE blank; a blank F field is 0. `12345E` is CONVERSION 607.

## Not done

- OVERFLOW: the exponent-overflow interrupt cannot be masked, and
  PL1C/370 has no program-check exit, so a result beyond 7.2E75 abends
  0CC where PL/I (F) raises OVERFLOW (ONCODE 300). ON OVERFLOW is refused.
- UNDERFLOW in addition and subtraction, and in conversions.
- A character constant with an exponent assigned to FIXED (`X = '1E5'`):
  Tier 5 refused it at compile time, PL1C/370 now raises CONVERSION at run
  time; PL/I (F) accepts it. Not measured further.
- `**`, the mathematical built-in functions, MOD of FLOAT values, FLOAT
  subscripts, binary FLOAT constants, the E format for FIXED values.
