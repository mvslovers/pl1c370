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

## Not yet built (C4b)

- GET LIST: `1+2I`, `-3.5E1-2I`, `2I`, `5` and `-1.25-0.5I` read as
  1+2I, -35-2I, 0+2I, 5+0I, -1.25-0.5I; `1 +2I` reads `1` as 1+0I
  and leaves `+2I` as the next item.
- GET EDIT `C(F(5))` of a field `+2I`: CONVERSION, ONSOURCE `+2I`,
  ONCHAR `I`.
- Character to complex with constant strings is converted at compile
  time by PL/I (F), and a bad constant is refused there (IEM1800I); a
  probe with variables is still to be run.
