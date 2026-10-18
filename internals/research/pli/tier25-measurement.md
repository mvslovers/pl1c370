# Tier 25 rules, measured against PL/I (F) V5.5

Measured 2026-10-13 on MVSTK5-BLD. Probe (JOB00378), which is also the test `tests/pli/t106psv` (JOB00382). With
Tiers 0 to 24: 108 programs identical (JOB00384 to JOB00386).

## SUBSTR as a pseudovariable

- `SUBSTR(s, i [, j]) = x` assigns x to that part of s, padded with
  blanks or cut to its length: `SUBSTR(C, 8) = '1234'` of a CHAR(10)
  takes `123`, `SUBSTR(C, 1, 3) = 'Z'` gives `Z  `.
- Of a VARYING string the part lies in its current value, and the
  length stays: `SUBSTR(V, 2, 3) = '**'` of `ABCDEF` is `A** EF`, 6.
- Of a bit string the bits are padded with zeros.

## CHAR and BIT

- CHAR(x) of an arithmetic value is the PUT LIST field, a string
  (LENGTH(CHAR(X)) is 8 for FIXED DECIMAL(5,2)); CHAR(x, n) its first n
  characters (CHAR(K, 4) of FIXED BINARY(15) 5: four blanks), padded with
  blanks (`CHAR('AB', 5)`).
- BIT(x) of a FIXED value is the binary digits of its magnitude: FIXED
  BINARY(p) gives p bits (BIT(5): `'000000000000101'B`), FIXED
  DECIMAL(p) CEIL(p*3.32) (BIT(-3): `'0011'B`); of a string its `1`
  and `0`; BIT(x, n) cut or padded with zeros (`BIT('1011', 6)` is
  `'101100'B`, `BIT('0101'B, 8)` assigned to FIXED BINARY 80).

## Not done

SUBSTR of a picture as a target, BIT of a FLOAT or scaled value, a
non-constant length in CHAR and BIT, the STRING and UNSPEC
pseudovariables.
