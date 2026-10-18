# Tier 9 rules, measured against PL/I (F) V5.5

Measured 2026-10-12 on MVSTK5-BLD. Probes (JOB00182 to
JOB00184), and what the test programs showed against PL/I (F) (JOB00186).
The test programs `tests/pli/t53str` to `t56ari` print what PL/I (F)
prints (JOB00194, together with Tiers 0 to 8: 58 programs).

## Strings

- VERIFY(s,c): the position of the first character of s not in c, 0 if
  none; `VERIFY('', 'A')` is 0. FIXED BINARY(15).
- TRANSLATE(s,r,p): a character found in p becomes the one at the same
  position of r (blank beyond r); a duplicate in p is warned about
  (IEM1695I) and the first one counts. Without p, the whole collating set
  X'00' to X'FF'.
- REPEAT(s,n) is s n+1 times: `REPEAT('AB',2)` is `ABABAB`,
  `REPEAT('X',0)` is `X`.
- HIGH(n), LOW(n): n characters X'FF' or X'00'. UNSPEC: the bits of the
  storage, left to right (`'11001000'B` for `H`).

## Arithmetic

- SIGN is FIXED BINARY(15).
- FLOOR, CEIL, TRUNC of FIXED(p,q) give FIXED(min(N, max(p-q+1,1)), 0) of
  the same base; of FLOAT the same FLOAT. A negative zero stays zero, and PUT LIST prints
  it without a sign (JOB00192).
- ROUND(x,n) of FIXED DECIMAL(p,q) is (p+1, q), rounded half away from
  zero at n places: -12.75 to 1 place is `-12.80`, -0.005 to 2 is
  `-0.010`; n may be negative (3.45 to -1 places is 0).
- FIXED(x[,p[,q]]): FIXED of x's base; without p FIXED DECIMAL(5,0) or
  FIXED BINARY(15,0), not x's precision; with p alone, q is 0. (Found by
  the differential run, JOB00186, not by a probe.)
- FLOAT(x[,p]): without p FLOAT DECIMAL(6) or FLOAT BINARY(21).
- BINARY and DECIMAL without p take x's precision converted (ceil(p*3.32)
  or ceil(p/3.32)); with p and a FIXED result they need q, as PRECISION,
  ADD, MULTIPLY and DIVIDE do: PL/I (F) refuses (p) alone (IEM1061I,
  JOB00183). A FLOAT result takes p alone.
- ADD, MULTIPLY, DIVIDE with (p,q) truncate: DIVIDE(-12.75, 3.45, 10, 4) is
  `-3.6956`.

## Exponentiation

- `**` binds tighter than prefix minus and from the right: `-2**2` is -4,
  `2**3**2` is 512.
- FIXED x and an unsigned integer constant n give FIXED((p+1)*n-1, q*n)
  while that fits in 15 decimal or 31 binary digits; otherwise the result
  is FLOAT of x's base and precision: `I ** 2` with I FIXED BIN(31) is
  FLOAT BIN(31), `2 ** 10` is `1E+03` (FLOAT DEC(1)), `1.5 ** 2` with
  DEC(5,1) is `2.25`, `10 ** 3` is 1000.
- A FLOAT x, or an integer exponent that is not such a constant, goes to
  the library (IHEXIS short, IHEXIL long, GY28-6801-6). Its results fit
  squaring and multiplying from the second highest bit of |n| down, with
  truncating multiplication, and 1/x**|n| for a negative n; x**0 is 1.
  PL1C/370 does that in `rt/pl1flt.c`; `1.1**10` is
  `2.593742460099993` long and `2.59372` short, as measured.

## Not done

`**` with a non-integer exponent (IHEXXS/IHEXXL, the general case) and the
mathematical functions; UNSPEC beyond 32 bits and as a pseudovariable;
ROUND of binary or FLOAT; 0**n with n <= 0 (PL1C/370 raises ERROR, not
measured). A procedure's code was limited to 4096 bytes then (one base
register); `t54ari` and `t56ari` are one probe split in two for that.
Tier 10 lifted the limit to 12 KB.
