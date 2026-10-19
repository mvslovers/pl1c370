# COMPLEX tier C2: what PL/I (F) does with `*`, `/`, ABS and `**`

Probes Q5CMUL, Q5CMU2, Q6CFIX, Q7CABS, Q8CPOW, Q8CPE1 to Q8CPE3
(`internals/design/complex.md` section 4, C2), run on PL/I (F) V5.5 in
a throwaway TK5 container of this project's own (2026-10-19), jobs
JOB00075, JOB00077 and JOB00079 there. The container runs the same
TK5 system as MVSTK5-BLD; the whole suite gives the same results on
both (PR #17).

## 1. FLOAT multiplication and division: the bits

The method is the one C28-6590-0 gives ("Complex Operations"):
`a*c - b*d`, `b*c + a*d` for the product; Smith's method for the
quotient (q = d/c, then (a+b*q)/(c+d*q) and (b-a*q)/(c+d*q) when
ABS(c) >= ABS(d), otherwise the operands taken as (b-aI)/(d-cI)). The
question was the arithmetic it is done in. An exact model of
System/370 short and long arithmetic (products of short operands long,
additions with one guard digit, truncation) reproduces every probe
value with **plain short or long arithmetic in that order**: the short
products truncated to short by the subtraction or addition, which is
what LE, ME, SER gives. Computing the quotient as a product with the
reciprocal of c+d*q misses by one or two units in at least one part of
each of the four quotients; SQRT(a*a+b*b) for ABS misses by eight units
(40B504F2 for 40B504EA).

| Value | PL/I (F) (UNSPEC) |
|---|---|
| (1.5+2.25I)*(3-0.5I) | 415A0000 41600000 |
| (1.5+2.25I)/(3-0.5I) | 405D67C9 40CF9148 |
| (1.5+2.25I)/(0.5-3I), the second branch | C0A60DD7 409BACF4 |
| (0.1+0.7I)*(0.3+0.9I) | C0999998 404CCCCA |
| (0.1+0.7I)/(0.3+0.9I) | 40BBBBBC 40222221 |
| (0.3+0.9I)/(0.1+0.7I) | 41151EB8 C03D70A5 |
| (0.1+0.7I)*2.5 | 403FFFFE 411BFFFF |
| 2.5/(0.1+0.7I) | 407FFFFC C1380000 |
| (0.1+0.7I)/2.5 | 3FA3D706 4047AE14 |
| the same product, long | C099999999999999 404CCCCCCCCCCCCA |
| the same quotient, long | 40BBBBBBBBBBBBBC 4022222222222221 |
| (0.3+0.9I)/(0.1+0.7I), long | 41151EB851EB851E C03D70A3D70A3D72 |
| (0.1+0.7I)*CONJG(0.3-0.9I)/(1.3-0.9I) | C06B8522 BFF5C2C6 |

A long operand meeting a short one makes the operation long
(`D*A`, 16 digits printed). Smith's method avoids the overflow of
c*c+d*d: (1E40+1E40I)/(1E40+1E40I) is 1.

## 2. Conditions

- A product whose four partial products overflow raises OVERFLOW
  **four times**; four underflowing ones raise UNDERFLOW four times, and
  the value is 0. Without an on-unit each underflow prints
  `IHE340I UNDERFLOW` (four messages for `A**3` of a tiny A).
- (1+1I)/(0+0I) raises ZERODIVIDE **three times**, short and long: q,
  and the two numerators divided by c+d*q.
- A complex value divided by a real 0 raises ZERODIVIDE **twice**: each
  part is divided by the real value, not through the complex routine.
- A complex FIXED DECIMAL division by 0+0I raises ZERODIVIDE twice; after
  a normal return of the on-unit the program ends with `IHE806I DATA
  INTERRUPT` (JOB00077). A test leaves that on-unit by GO TO.

## 3. ABS

`max * SQRT(1 + (min/max)**2)` (C28-6590-0) gives every FLOAT value to
the bit: ABS(3+4I) 41500000, ABS(1+1I) 4116A09E, ABS(0.1+0.7I) 40B504EA.
ABS(7E75+7E75I) raises OVERFLOW once.

ABS of COMPLEX FIXED is **not** the truncated square root in every case
(precision (MIN(N,p+1),q), as GC28-8201-4 states):

| Value | exact | PL/I (F) |
|---|---|---|
| FIXED DEC(5,2) 1+1I | 1.4142 | 1.41 |
| FIXED DEC(5,2) -999.99-999.99I | 1414.1994 | 1414.20 |
| FIXED DEC(15,3) 2+3I | 3.60555 | 3.606 |
| FIXED BIN(15) 32767+32767I | 46339.42 | 46340 |
| FIXED BIN(31,8) 2.5+3.25I | 4.10030 (4.09765 truncated in 8 bits) | 4.1015625 |

So the library's fixed square root is sometimes one unit of the last
place high ("small truncation errors inevitably occur", C28-6590-0).

## 4. Integer powers

- `z**n`, FLOAT: z**0 is 1; otherwise square-and-multiply from the
  leading bit of ABS(n), through the multiplication; for n < 0 the
  result is then divided into 1+0I by the complex division. Every bit
  agrees: (0.1+0.7I)**2 C07AE147 4023D708, **5 401D7B1C 402255AF,
  **-3 C112B020 4129374D, **13 3F2B9177 BEC3D44A, **-7 C197A976
  4162D3B0, long **5 401D7B2031CEAF22 402255B035BD512C, long **-3
  C112B020C49BA5E3 4129374BC6A7EF9F. The reciprocal taken as
  (c-dI)/(c*c+d*d) misses.
- A FIXED base keeps FIXED for a positive integer constant n when
  (p+1)*n-1 fits (FIXED DEC(2) 1+1I: **2 `0+2I`, **3 `-2+2I`, **4
  `-4+0I`; FIXED DEC(3,1) 1.5-0.5I: **2 `2.00-1.50I`, **3
  `2.250-3.250I`; FIXED BIN(15) 2+1I **2 `3+4I`); otherwise it is FLOAT
  of the base's base and precision: P**7 `8.0E+00-8.0E+00I` (FLOAT
  DEC(2)), U**3 `2.0000E+00+1.1000E+01I` (FLOAT BIN(15)), and with a
  variable exponent P**N and U**N; P**0 and P**-1 go to the short FLOAT
  routine too.
- 0**n for n <= 0, 0**0 included, is an error: message `IHE209I IHEXIW
  X=0, Y LE 0 IN X**Y`, ONCODE 1554 (short), `IHEXIZ`, 1555 (long);
  FIXED DEC(3) P**0 and P**-1 give 1554.
- (1E20+1E20I)**4 raises OVERFLOW once (only b*d of the second squaring
  overflows).

## 5. FIXED multiplication and division (for C2b)

- Precisions are the real rules on the parts (SC26-3114-01 Table 16):
  FIXED DEC(5,2) * (5,2) prints as (11,4) `5.6250+6.0000I`, `/` as
  (15,10) `0.3648648648+0.8108108108I`; FIXED BIN(15) `/` is (31,16).
- Division truncates each part toward zero:
  (2+3I)/(7-5I) `-0.0135135135+0.4189189189I`.
- FIXED DEC(8) (99999999+1I)*(99999999-1I): FIXEDOVERFLOW once (the real
  part has 16 digits), the value its low 15 digits `999999800000002+0I`.
- DIVIDE(P,Q,9,4) and DIVIDE(P,Q,5,1) and decimal MULTIPLY are the
  quotient or product truncated to the precision given.
- **MULTIPLY of COMPLEX FIXED BINARY is wrong in PL/I (F):**
  MULTIPLY(U,V,20,0) and MULTIPLY(U,V,31,0) with U = 3+4I and V = 1-2I
  give `2-14I`, MULTIPLY(U,V,20,3) `2.0-14.0I`, where `U*V` gives
  `11-2I` (JOB00077, JOB00079). PL1C/370 computes 11-2I; no twin
  compares it.
