# COMPLEX tier C4: edit-directed output, GET, character to complex

Probes Q10CED, Q10CE2, Q10CE3, Q11CGET and Q12CCNV
(`internals/design/complex.md` section 4, C4), run on PL/I (F) V5.5 in
a throwaway TK5 container of this project's own, JOB00159
(2026-10-19).

## PUT EDIT with the C format item (Q10CED, Q10CE2, Q10CE3)

| Statement | PL/I (F) |
|---|---|
| `PUT EDIT(A)(C(F(8,2)))`, A = 12.5-3.25I | `    12.50   -3.25` |
| `(C(F(8,2),E(12,4)))` | `    12.50 -3.2500E+00` |
| B = -1.5+2.25I, `(C(F(7,2)))` | `   -1.50   2.25` |
| `(C(P'S99V.99'))` | `-01.50+02.25` |
| `(C(P'99V.99',P'S99V.99'))` | `01.50+02.25` |
| `(B, 'I')(C(F(7,2)), A)` | `   -1.50   2.25I` |
| `(A, 7, B)(C(F(8,1)), F(3), C(F(8,1)))` | `     12.5    -3.3  7    -1.5     2.3` |
| a real X = 4.5 with `C(F(6,1))` | `    4.5   0.0` |
| a complex 1.5+2.5I with `F(6,1)` | `   1.5` |
| the same with `F(6,1), F(6,1)` | `   1.5` (the second item unused) |

So the imaginary part's sign appears only when negative (F, E), as for
any F or E item, and a picture shows it as its characters say; a real
value meeting C is edited with the first item and 0 with the second; a
complex value meeting another item has only its real part edited, with
that item, and its imaginary part is not edited at all. The twin
`t294ced` holds every case (identical, JOB00217).

## GET of complex targets (Q11CGE2, JOB00248)

- GET LIST: `1+2I`, `-3.5E1-2I`, `2I`, `5` and `-1.25-0.5I` read as
  1+2I, -35-2I, 0+2I, 5+0I, -1.25-0.5I; a real target given
  `4.5-1I` takes 4.5; `1 +2I` reads `1` as 1+0I and leaves `+2I` for
  the next item.
- GET EDIT `C(F(5),F(5))` of `   12   34` is 12+34I; a complex target
  meeting `F(5)` reads one field as its real part, its imaginary part
  0, and the next item goes to the next target; a real target meeting
  `C(F(4))` reads both fields, the second for nothing.
- GET DATA `A=1+2I, P=-3.5+1I;` assigns both.
- A C field holding an I (`  1.5+2I` under `C(F(5))`) raises
  CONVERSION with ONSOURCE `1.5+2I`; no twin holds it.

The twin `t295cgt` holds the rest (identical, JOB00263). It also found
that PL1C/370 left out SKIP on GET DATA: `GET SKIP DATA` after a GET
EDIT that had read part of a record read on in that record. Fixed in
the same tier.

## Character to complex (Q12CCV2, JOB00248)

| String | COMPLEX FLOAT DEC(6) | CONVERSION, ONCHAR |
|---|---|---|
| `1+2I` | 1+2I | |
| `  -1.5E0+2.5E1I  ` | -1.5+25I | |
| `3I`, `7`, `''` | 0+3I, 7+0I, 0+0I | |
| `12.345-6.789I` | 12.3449-6.78899I; FIXED DEC(7,2) 12.34-6.78I | |
| `4+ZZI` | | the first Z |
| `1 +2I` | | the blank |
| `1+2` | | the central + |
| `+I` | | the + |
| `-2.5E-1-3I`, `5E2I` | -0.25-3I, 0+500I | |

A real target takes the real part (`'1+2I'` gives 1), and PL/I (F)
checks the imaginary part (`'4+ZZI'` raises CONVERSION at the Z). The
twin `t296ccv` holds every case (identical, JOB00263).
