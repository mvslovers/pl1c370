# COMPLEX tier C5: arguments, RETURNS, built-in and array functions

Probes Q15CARG and Q16CBIF (`internals/design/complex.md` section 4,
C5), run on PL/I (F) V5.5 in a throwaway TK5 container of this
project's own, JOB00294 (2026-10-19).

## Arguments, parameters, RETURNS (Q15CARG)

- A complex variable for a parameter of its type is passed by
  reference: `CALL SUB(A, P)` changes A and P.
- A real argument for a complex parameter gets a dummy (IEM0802I, a
  warning): `CALL SUB(X, 2.5+1I)` with X = 2 gives Z = 0+4I in SUB.
- `F(X)` with a real X and `F(2I)` with a FIXED imaginary constant are
  converted to the parameter's COMPLEX FLOAT DEC(6) (dummies).
- A short complex argument for a long parameter: a dummy (`G(A)`).
- RETURNS(COMPLEX FLOAT DEC(6)) from `RETURN(CONJG(Z) + 1)`, and from a
  real value (`RETURN(Y)`, imaginary part 0); `REAL(F(A)) + 1`,
  `F(A) * 2 + F(F(A))`.

The twin `t298car` holds these (identical, JOB00332), H with its
array descriptor `(*)` included (PL1C/370 parsed such descriptors only
in GENERIC before; Tier 117 added them to ENTRY).

## Built-in and array functions (Q16CBIF)

ROUND(P,1), ROUND(P,0), PRECISION(P,5,1), FIXED(A,7,2), FLOAT(P,6),
BINARY(P), DECIMAL(A), FIXED(P) of complex values act on each part
with the real rules (`12.300-6.800I`, `12.000-7.000I`, `12.3-6.7I`,
`12.34-6.78I`, `12.3447-6.7880I`, `12-6I`); SUM and PROD of complex
arrays are complex FLOAT (FIXED DEC(5,1) elements give
`3.5000E+00-2.0000E+00I`), POLY too; array assignments `ZA = ZA * 2`
and `ZA = ZA + PA` work element by element. The twin `t297cbf` holds
them (identical, JOB00332), ROUND of a FLOAT value included (Tier 117
added it: the rightmost bit of the fraction set to 1).
