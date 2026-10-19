# Design note: COMPLEX (#2)

Status: tier C1 built (Tier 109). Of the questions in section 7, C1
took Q-1 (new kinds), Q-2 (the parts are split in check.c, not in a
pass of its own: a value is split before it is typed, which needs
check's typing), Q-5 (folding: none needed; `1+2I` is an addition of
constants whose precision the real rules give) and settled Q-7 by
measurement. Q-3, Q-4, Q-6 and Q-8 remain for C2 to C5. The test
numbers in section 4 were taken by the time C1 was built: C1's twins
are t273cdc to t277cad.

Status: draft for the maintainer's decision. Nothing here is built, and
no probe has been run. "To be measured" marks every point where the
manuals leave the answer open or where the OCR text cannot be trusted.

## 0. Conventions

- **GC28-8201-4** (PL/I (F) Language Reference, Dec 1972) is cited by
  its *printed* page number and section, not by the PDF page. The two
  drift apart: PDF 108 is printed p. 104, PDF 280 is p. 274, PDF 297 is
  p. 289, PDF 336 is p. 328.
- **GC33-0009-4** (Optimizer Language Reference) is cited by its
  printed page as well (PDF 380 is p. 367).
- **GC28-6594-7** (F Programmer's Guide), **GY28-6801-6** (F Subroutine
  Library PLM) and **C28-6590-0** (PL/I Subroutine Library,
  Computational Subroutines, 1966) print the same number as the PDF
  page in the ranges cited here.
- Code references are `file:line` in the tree as of 2026-10-17 (main,
  Tier 105 level; `build/host/pl1c370` built 09:41).

## 1. What the language requires

### 1.1 Data and constants

- **Mode is a third arithmetic axis**, beside base and scale. A complex
  item is a pair, real part first. "Each of the base, scale, and
  precision attributes applies to both fields" (GC28-8201-4 p. 30,
  "Complex Arithmetic Data"). Default is REAL (p. 328, "COMPLEX and
  REAL").
- **There are no complex constants.** An imaginary constant is a real
  constant of any type (not sterling) followed by `I`: `27I`,
  `3.968E10I`, `11011.01BI` (p. 30). `38+27I` is an expression.
- **The precision of such an expression follows the expression rules**:
  `1 + 1I` is (2,0) (p. 228, "Avoiding Common Errors", item 10). So
  even a program that only assigns `1+2I` to a FLOAT COMPLEX variable
  evaluates a FIXED DECIMAL complex addition first, unless the compiler
  folds it. Whether PL/I (F) folds is to be measured (probe Q2CLIST).
- **Storage** (GC28-6594-7 p. 202, Figure 15-1): COMPLEX FLOAT short is
  8 bytes, fullword aligned; long is 16 bytes, doubleword aligned. The
  real part comes first: on transmission "the real part is transmitted
  before the imaginary part" (GC28-8201-4 p. 103). UNSPEC of a complex
  value is twice the length of the real one (p. 287, item 7). The
  layout of COMPLEX FIXED (two halfwords, two fullwords, two packed
  fields) is inferred from these, not stated; to be measured (Q1CDCL).
- **Defaults**: ALIGNED for COMPLEX like all arithmetic data (p. 226).
- **Numeric pictures**: `PICTURE '...' COMPLEX`, one picture for both
  parts (p. 31; p. 348, rule 4c). COMPLEX is the only other attribute
  allowed with PICTURE (p. 328).
- **INITIAL**: STATIC CHARACTER or BIT data cannot be initialized with
  complex values in F (p. 343, rule 13; GC28-6594-7 p. 303).

### 1.2 Expressions

- **Mode conversion**: "If the modes of the two operands differ, the
  real operand is converted to complex mode (by acquiring an imaginary
  part of zero with the same base, scale, and precision as the real
  part)" (p. 46, "MODE"). Target mode is "COMPLEX unless both operands
  are REAL" (p. 58).
- **Exponentiation exception**: if the exponent is a REAL fixed-point
  integer constant or a variable of precision (p,0), it keeps REAL mode
  (p. 46; p. 58-59, with the example `G ** B`).
- **Complex ** special cases**: `x**y` with x = 0: result 0 if REAL(y)
  > 0 and IMAG(y) = 0; ERROR if x = 0 and REAL(y) <= 0 or IMAG(y) ¬= 0
  (p. 48, rule 2, OCR partly garbled; C28-6590-0 p. 28 states the same
  test for IHEXXW/Z).
- **Precision**: the fixed-point formulas of p. 47-48 apply to complex
  results too; p. 228 item 10 confirms it for addition. For
  multiplication and division PL/I (F) computes in the library at its
  own internal precision (section 1.4) and assigns to the result
  precision.
- **Comparison**: only `=` and `¬=` for complex operands (p. 50). PL/I
  (F) replaces `<`, `<=`, `>=`, `>` by `=` and issues IEM1608I,
  severity E (GC28-6594-7 p. 397).
- **Complex to real**: the real part (p. 45; p. 270, "Mode
  Conversion"). **Real to complex**: imaginary part zero.
- **Character to arithmetic**: the string may hold
  `[+|-]real-constant{+|-}imaginary-constant`, no blank before the
  central sign (p. 271-272). A null string gives zero.
- **Complex to character** (p. 274, "Complex to Character-string"): the
  length is 1 + twice the real length. Real part as for a real source,
  imaginary part always signed, `I` appended, one character longer
  than the real part, no blanks between. The two examples on p. 274
  are OCR-damaged (`bl~7300E+Ol+l.5000E+00I`, `bbbO.133+0.007I`); the
  exact fields are to be measured (Q2CLIST). Figure 4-3 note (p. 60)
  gives the same length rule; complex numeric character fields
  concatenate their two parts with nothing between (p. 272).
- **Arithmetic to bit string** takes "the absolute value" (p. 45,
  p. 274). What PL/I (F) does with a complex source is not stated; to
  be measured (Q3CMODE).
- GC33-0009-4 p. 331 adds: a character string holding a complex value,
  assigned to a real target, has its imaginary part not checked. PL/I
  (F) is to be measured (Q12CCNV).

### 1.3 Built-in functions and pseudo-variables

From GC28-8201-4 Section G:

| Function | Rule | Page |
|---|---|---|
| `COMPLEX(x,y)`, `CPLX` | x, y real; result x + yI. The precision is not stated in F; GC33-0009-4 p. 367 gives MAX(p1,p2) for FLOAT arguments, and a FIXED formula that is only partly legible in the OCR ("... MAX(q1,q2)) where (p1,q1) and (p2,q2) are the precisions ...") | 289; abbreviation p. 245 |
| `REAL(x)` | x "must be a complex expression"; base, scale, precision unchanged | 292 |
| `IMAG(x)` | imaginary part, mode real | 290 |
| `CONJG(x)` | x must be complex; attributes unchanged | 289 |
| `ABS(x)` | complex: SQRT(re**2+im**2), mode real; complex FIXED (p,q) gives (MIN(N,p+1),q) | 288 |
| `ADD`, `MULTIPLY`, `DIVIDE` | complex arguments exist in the library (IHEADV, IHEMPU/V, IHEDVU/V); the language text was not read for this note | GY28-6801-6 p. 84 |
| `BINARY` | "The scale and mode of this value are the same as those of x" | 289 |
| `DECIMAL`, `FIXED`, `FLOAT`, `PRECISION` | mode kept: assumed from BINARY, not read; to be checked | 289-290 |
| `ROUND` | "base, scale, and mode ... are those of the value of expression" | 292 |
| `CEIL`, `FLOOR`, `TRUNC`, `SIGN`, `MAX`, `MIN`, `MOD` | argument must not be complex | 289-293 |
| `UNSPEC` | complex: twice the real length | 287 |
| Mathematical | real or complex unless stated; result has the argument's mode, base, precision | 293 |
| complex allowed | `ATAN(x)` (x ¬= ±1I), `ATANH` (x ¬= ±1), `COS`, `COSH`, `EXP`, `LOG` (x ¬= 0), `SIN`, `SINH`, `SQRT`, `TAN`, `TANH` | Figure G-1, 298-299 |
| real only | `ATAN(x,y)`, `ATAND`, `COSD`, `SIND`, `TAND`, `ERF`, `ERFC`, `LOG2`, `LOG10` | 294-297 |

Pseudo-variables (p. 305-306): `COMPLEX(a,b)` takes only complex values
(a and b both arrays of the same bounds if one is); `REAL(c)` and
`IMAG(c)` take real or complex values, c an element or array variable.
PL/I (F) warns IEM1625I (W) when the REAL pseudo-variable's argument is
not complex and treats it as having a zero imaginary part
(GC28-6594-7 p. 400); IEM0873I and IEM0874I refuse a malformed COMPLEX
pseudo-variable (p. 378).

GC33-0009-4 differs: REAL, IMAG and CONJG convert a real argument to
complex (p. 367, 370, 376). That is a deviation of the default level
from `-lang=F`.

### 1.4 How PL/I (F) computes (the library)

From GY28-6801-6 p. 83-84 (Figures 43-45) and C28-6590-0:

- Add, subtract, negate, compare: compiled code. This is an inference:
  the library supports the operations "for which the F level compiler
  neither generates in-line code nor ... uses the library conversion
  package" (GY28-6801-6 p. 83), and Figure 45 (p. 84) lists no module
  for them.
- Float multiply: IHEMZW/IHEMZZ, `a*c - b*d`, `b*c + a*d`, 64 bytes each
  (C28-6590-0 p. 25-26). Linkage RA = A(z1), RB = A(z2), RC = A(target)
  in R1-R3, the PL/I standard (GY28-6801-6 p. 11, p. 123).
- Float divide: IHEDZW/IHEDZZ, Smith's method: if |c| >= |d|, q = d/c,
  re = (a + b*q)/(c + d*q), im = (b - a*q)/(c + d*q); otherwise the
  operands are swapped as (b - aI)/(d - cI). The comparison is done in
  short precision in both modules (C28-6590-0 p. 26). 104 bytes each.
- Float ABS: IHEABW/IHEABZ, `max * SQRT(1 + (min/max)**2)`, 0 for 0
  (C28-6590-0 p. 35), calls IHESQS/IHESQL (GY28-6801-6 p. 88).
- Fixed multiply and divide: IHEMZU (binary), IHEMZV (decimal), exact
  products, `(a*c + b*d)/(c*c + d*d)` for divide, FIXEDOVERFLOW and
  ZERODIVIDE (C28-6590-0 p. 24-25). Fixed ABS: IHEABU, IHEABV with a
  Newton-Raphson square root; "small truncation errors inevitably
  occur" in complex fixed division and ABS (p. 19, p. 34-35).
- Integer power: IHEXIU/V/W/Z by repeated squaring through the
  multiply routine, reciprocal for a negative exponent (p. 26-27).
  General power: IHEXXW/Z as EXP(z2*LOG(z1)) (p. 28).
- Complex math: IHESQW/Z, IHEEXW/Z, IHELNW/Z, IHESNW/Z (SIN, COS, SINH,
  COSH), IHETNW/Z (TAN, TANH), IHEATW/Z (ATAN, ATANH)
  (GY28-6801-6 p. 83, Figure 44). They call the real routines.
- Complex I/O and conversion: IHEUPA, IHEUPB (address of the imaginary
  part), IHEVCS (character to complex) (GY28-6801-6 p. 147-148).
- Sizes of all of them: GC28-6594-7 p. 270-278 (section 6).

### 1.5 Stream I/O

- **LIST and DATA**: in the stream a complex item is
  `[+|-]real-constant{+|-}imaginary-constant`; "A blank must not
  precede the central + or -" (p. 104). Output uses the complex to
  character conversion (p. 104, p. 272).
- **DATA output of a complex numeric character variable** is not valid
  input again (p. 107, rule 7b).
- **C format** (p. 264, "The C Format Item"): `C(real-item[,real-item])`,
  each F, E or P. The second defaults to the first. On input, two
  adjacent fields; the letter I raises CONVERSION. On output, I is
  never appended; with F or E the sign of the imaginary part is printed
  only when negative; with P only if the picture has S, + or -. An `I`
  wanted in the output is a separate data item with an A or P item.
  IEM0198I refuses a malformed C item (GC28-6594-7 p. 331).
- What PL/I (F) does with a complex item meeting an F item, or a real
  item meeting a C item, is not stated; to be measured (Q10CED).

### 1.6 Conditions

- OVERFLOW, UNDERFLOW, ZERODIVIDE from the float routines; FIXEDOVERFLOW
  and SIZE from the fixed ones (C28-6590-0 p. 19 categories, p. 24-28).
- ERROR with ONCODE (GC28-8201-4 p. 310): 1554/1555 invalid exponent in
  complex short/long integer exponentiation; 1556/1557 in complex
  general exponentiation; 1558/1559 invalid argument of complex ATAN or
  ATANH. Messages IHE210I "X=0, Y NOT POSITIVE REAL IN X**Y" and IHE211I
  "Z=+I OR -I IN ATAN(Z) OR Z=+1 OR -1 IN ATANH(Z)" (GC28-6594-7 p. 486,
  OCR; exact text and which ONCODE prints which message: to be
  measured, Q8CPOW, Q9CMATH).
- CONVERSION for bad complex input (p. 264, p. 271).

## 2. What PL1C/370 has today

COMPLEX does not appear anywhere in `src/` or `rt/`
(`LC_ALL=C grep -i complex`: one hit, a "too complex" message,
gen.c:163). Measured with `build/host/pl1c370`:

| Source | Result | Where refused |
|---|---|---|
| `DCL Z COMPLEX FLOAT DEC(6);` | "attribute COMPLEX is not supported yet" | parse.c:1596 |
| `DCL X REAL FLOAT DEC(6);` | "attribute REAL is not supported yet" | parse.c:1596 (REAL is accepted only in generic descriptions, parse.c:792, and DEFAULT, parse.c:1894) |
| `X = 2I;` | "';' expected": `I` lexed as a separate identifier | lex.c:403-436 |
| `IMAG(X)`, `REAL(X)`, `CPLX(1,2)`, `COMPLEX(1,2)`, `CONJG(X)` | "neither an array nor a known function" | no entry in the table resolve.c:200-235 |
| `PUT EDIT(X)(C(F(5)));` | "format item C is not supported yet" | parse.c:2723 |
| parameter descriptor / RETURNS with COMPLEX | "attribute ... is not supported in a descriptor yet" | parse.c:888-889 (read, not run) |

Binary constants are refused altogether ("only decimal constants are
supported", lex.c:421-424), so `11011.01BI` waits for them.

The parts a design touches:

- **Types**: `enum tkind` (pl1c.h:97-98) has TY_FIXBIN, TY_FIXDEC,
  TY_FLTDEC, TY_FLTBIN; `struct type` (pl1c.h:117-123) carries k, p, q.
  No mode. `struct rawattr` (pl1c.h:135-139), for DEFAULT, has no mode
  either. There is no extended float (FLOAT DEC <= 16, BIN <= 53,
  parse.c:688-692), so complex extended (an Optimizer feature) does not
  arise.
- **Predicates**: `arith()` and `is_string()` (check.c:26-27),
  `is_float()`/`flt_long()` (pl1c.h:524-525). The compiler tests types
  mostly through these (104 source lines in check.c, gen.c, lower.c
  call `arith(` or `is_float(`) and through the kind names directly
  (172 uses of TY_FIXBIN, TY_FIXDEC, TY_FLTDEC, TY_FLTBIN in
  `src/*.c`, comparisons and assignments together).
- **Type rules**: `float_common` (check.c:55) and `common` (check.c:103)
  settle base and scale; the result type of infix operators is
  computed in `type_of` (check.c:1729-1957); `type_power`
  (check.c:1686) expands a FIXED `x**n` into multiplications;
  `type_eq` (check.c:166) compares k, p, q; `string_len` (check.c:91)
  gives the PUT LIST length; ABS is typed at check.c:1605.
- **Layout**: `elem_size`/`elem_align` (lower.c:28-62), `type_size`
  (gen.c:3261).
- **Code**: FLOAT values live in 8-byte frame temporaries; each
  operation loads FPR 0 (and 2) and stores the result, nothing stays in
  a floating-point register across a call (gen.c:1480-1484, `gen_flt`
  gen.c:1612-1809). FIXED DECIMAL lives in 8-byte packed temporaries
  (`gen_dec` gen.c:1251). FIXED BINARY lives in a general register
  (`gen_bin`/`gen_bin_at` gen.c:804-812); expressions use R2-R9
  (CLAUDE.md), and "out of registers" is fatal (gen.c:163). Stores and
  conversions on assignment: `gen_store` (gen.c:2799), `gen_assign`
  (gen.c:3143) with `is_in_place` (gen.c:3131). STATIC INITIAL:
  `dc_value` (gen.c:5753).
- **Mathematical functions** call the run time's own routines in
  `rt/pl1m*.c` (C, a member per family, `math_entry` in gen.c). Their errors
  go through `pl1_math_error` and the table `math_errors`
  (rt/pl1cond.c), which ends at ONCODE 1553.
- **PUT LIST**: the compiler emits one call per item by type
  (`gen_put_item` gen.c:4028-4142: PL1LFLT, PL1LFXS, PL1LFXD, ...); the
  run time places the item at the next tab (`place`, rt/pl1io.c:710).
- **PUT EDIT**: a table of (kind, w, d) shorts per format item, built by
  `format_list` (parse.c:2593) and `format_begin` (gen.c:4361); the
  run time walks it in `next_data` (rt/pl1ioed.c:65). The kinds are two
  parallel enums, `enum fkind` (pl1c.h:358-361) and the `K_` enum
  (rt/pl1iof.h:90-92), not in `rt/pl1abi.h`.
- **GET**: one call per target by type (`gen_get_item` gen.c:4410-4492);
  tokens by `list_item`/`edit_field` (rt/pl1ioget.c:77, 142), numbers by
  `parse_number` (rt/pl1ioget.c:278). GET DATA: `gen_get_data`
  (gen.c:4315), `pl1_get_data` (rt/pl1ioget.c:488).
- **Pseudo-variables**: handled one by one in `check_stmts`, S_ASSIGN
  (check.c:2249-2315) and `gen_assign` (gen.c:3147).

## 3. Proposed design

### 3.1 Representation: two options, the maintainer chooses

**A. Four new kinds** TY_CFIXBIN, TY_CFIXDEC, TY_CFLTBIN, TY_CFLTDEC,
with `part_type(t)` giving the real kind and `cplx_of(t)` the reverse.

- Every existing `t.k == TY_FLTDEC`, `arith()`, `is_float()` is false for
  a complex value, so code that knows nothing of complex refuses it
  ("arithmetic needs arithmetic operands") or reaches its `fatal`
  default. That is the failure this project wants: loud, at compile
  time.
- Cost: every place that accepts complex must say so; helpers such as
  `bin_prec`, `dec_scale`, `flt_long` are called on `part_type(t)`.

**B. A mode flag** `int cplx` in `struct type`.

- Smaller diff; base/scale/precision code works on the parts unchanged.
- Risk: the predicates and the direct kind tests among the 172 uses
  of the kind names treat a complex value as real, silently, unless
  audited. `arith()` and
  `is_float()` could be made false for complex to recover most of A's
  safety, but direct `t.k ==` tests would still need the audit.

**Recommendation: A**, for the safe failure. Storage either way: the
real part first, then the imaginary part, each laid out as the real
type; size twice, alignment the part's (lower.c:28-62, gen.c:3261).
COMPLEX PICTURE is deferred (tier C5); it would need a TY_CPIC or a
flag on TY_PIC.

`struct rawattr` and `struct dflt` get a mode bit so that DEFAULT
(GC33-0009-4, already OPT) can supply COMPLEX/REAL; `type_eq`
(check.c:166) compares mode so that arguments and dummies see it.

### 3.2 Front end

- **lex.c:403-436**: a number followed by `I` (not followed by an
  identifier character) becomes an imaginary constant: a T_NUM with an
  imaginary mark. `2I` keeps the attributes of `2`, FIXED DECIMAL(1,0)
  (GC28-8201-4 p. 30).
- **struct expr** (pl1c.h:294): E_NUM gets an `imag` flag; its type is
  the complex kind of the constant's real type.
- **parse.c**: COMPLEX, CPLX and REAL in DECLARE (the loop parse.c:1215
  to 1597), in descriptors and RETURNS (`attrs`, parse.c:819-895), in
  DEFAULT (parse.c:1883-1954), in generic descriptions (parse.c:792).
  COMPLEX with PICTURE refused until C5.
- **resolve.c:200-235**: REAL, IMAG, CONJG, COMPLEX, CPLX at level L_F.
- **Constant folding**: propose to fold `real-constant ± imaginary-
  constant` into one complex constant *only if* probe Q2CLIST shows
  PL/I (F) gives the folded precision; otherwise compile the FIXED
  DECIMAL complex addition as written (p. 228 says the precision is
  that of the addition, so folding must not change the attributes).

### 3.3 Type rules (check.c)

- Mode in `type_of` (check.c:1729): result complex unless both operands
  are real; the real operand gets a zero imaginary part of its own
  base, scale and precision (p. 46). Base and scale as today, on the
  part types (`float_common`, `common`).
- Fixed precision per operator from p. 47-48 on the parts.
- `type_power` (check.c:1686): the exponent exception of p. 58-59;
  complex `z**n` with an integer constant n stays a power (no expansion
  into multiplications, since PL/I (F) uses IHEXIU/V and its precision
  limit n*(p+1)-1 <= 31 or 15, C28-6590-0 p. 26-27).
- Comparison of complex operands: `=` and `¬=` only; others are an
  error (PL/I (F)'s IEM1608I is severity E, so the compile fails there
  too, with condition code 8).
- Built-ins as in section 1.3; ABS of complex returns the real kind,
  fixed precision (MIN(N,p+1),q). REAL/IMAG/CONJG of a real argument:
  refused under `-lang=F` if probe Q4CBIF confirms PL/I (F) refuses it,
  accepted by default as GC33-0009-4 allows: `beyond_f(line, L_OPT,
  ...)` where check.c sees the real argument, an OPT mark in
  `docs/language-levels.md`, and one `.opt.pli` twin that `-lang=F`
  must refuse (CLAUDE.md, "Language levels").
- Pseudo-variables REAL, IMAG, COMPLEX in `check_stmts` S_ASSIGN
  (check.c:2249) beside SUBSTR, STRING, UNSPEC.
- `string_len` (check.c:91): 2*len+1 for complex.

### 3.4 Code: a split pass, run-time calls for the rest

Proposal: a new pass `src/cplx.c` between check (main.c:197) and gen
(main.c:203),
when types are known. It rewrites complex expressions into real ones
over the parts:

- A **part selector** on E_VAR (0 whole, 1 real, 2 imaginary): the type
  is the part type, `gen_place` (gen.c:286) adds the offset of the
  imaginary part. Constants split into their two real constants.
- **Inline** (what PL/I (F) compiles inline, GY28-6801-6 p. 84): `+`,
  `-`, prefix `-`, CONJG, REAL, IMAG, COMPLEX, assignment with mode and
  base/scale conversion, `=` and `¬=`. `Z = A + B` becomes
  `Zr = Ar + Br; Zi = Ai + Bi` through the existing `gen_flt`/`gen_dec`/
  `gen_bin` paths, so float truncation, SIZE and FIXEDOVERFLOW come out
  as they do for real values. `Z = W` compares as `Zr = Wr & Zi = Wi`.
- **Operands evaluated once**: a complex operand that is not a plain
  reference or constant (a function call, a library result) is first
  stored into a complex temporary (16 bytes at most, in the frame like
  today's temporaries, gen.c:141).
- **Overlap**: if the target occurs in the source (`Z = CONJG(Z)` is
  safe part by part, `Z = COMPLEX(IMAG(Z), REAL(Z))` is not), the value
  goes into a temporary first, as `is_in_place` (gen.c:3131) decides
  today for strings.
- **Out of line**: `*`, `/`, ABS, `**`, the mathematical functions, and
  ADD/MULTIPLY/DIVIDE of complex FIXED: one run-time call with the
  addresses of the operands (materialized as above) and of a result
  temporary. This matches PL/I (F) and keeps the register use of
  `gen_flt` unchanged; an inline four-product multiply would cost code
  under the 12 KB per procedure limit (CLAUDE.md) for no gain in
  agreement.

The alternative is native support in gen.c: a `gen_cplx` returning a
16-byte temporary, parallel to `gen_flt` and `gen_dec`. It avoids the
new pass but duplicates the base and scale cases inside gen.c (6446
lines already). Open question Q-2.

### 3.5 Arithmetic in the run time

- **Float multiply, divide, ABS and integer power** (done, Tier 114):
  C in the run time after the methods C28-6590-0 describes, in plain
  short or long arithmetic in the order it gives, *not* in extended
  precision: that is what reproduces PL/I (F)'s bits and how often it
  raises OVERFLOW, UNDERFLOW and ZERODIVIDE
  (`research/pli/c2-measurement.md`; `rt/pl1cx.h`).
- **General power and the eleven mathematical functions** (C3): C in
  the run time, computed in extended precision and rounded once, as
  `rt/pl1m*.c` does for the real functions (maintainer, 2026-10-18:
  no IBM code in the run time). The results are at least as accurate
  as PL/I (F)'s; the last digit can differ from it, and a twin that
  prints it records the difference.
- **Fixed complex** multiply, divide, ABS, power, ADD, MULTIPLY and
  DIVIDE (done, Tier 114): C as well, exact in a wide integer. Multiplication is exact; division and ABS
  truncate as C28-6590-0 p. 19 describes, and the twins show where the
  last digit differs. (Q-3, settled: C.)
- New error rows in `math_errors` (rt/pl1cond.c:644-661) for 1554-1559.

### 3.6 Conversions

- Complex to real, real to complex: in the split pass (part selection
  and a zero imaginary part).
- Complex to character (p. 274): a run-time routine building
  `real||signed-imag||'I'` from the two real conversions that exist
  today (`pl1i_format_number`, rt/pl1rt.c:19; `pl1i_flt_field`,
  rt/pl1rtfl.c:17). The imaginary part's sign always present; its
  field one longer than the real one. Exact padding to be measured.
- Character to complex (p. 271-272): a parser for the two-constant
  form, raising CONVERSION as `parse_number` does; used by assignment,
  GET LIST and GET DATA.
- Complex to bit: to be measured, then implemented or refused.
- Complex numeric character: C5.

### 3.7 Stream I/O

- **PUT LIST, PUT DATA**: one new entry, e.g. `PL1LCPX(addr, kind, p,
  q)`, that builds the complex character string and calls `place`
  (rt/pl1io.c:710) once: the pair is one item for tabbing. PUT DATA
  goes through the same `place` with the name prefix.
- **PUT EDIT with C**: parse `C(item[,item])` in `format_list`
  (parse.c:2593) into a K_C marker followed by two real items (the
  second a copy of the first if omitted). A complex data item takes the
  K_C and edits its parts with the next two items through the existing
  F/E/P editing (rt/pl1ioed.c: `f_field` 134, `e_field` 411,
  `edit_value` 278), with the C sign rule (sign only if negative). The
  K_ table is shared by compiler and run time, so `PL1_ABI_STAMP`
  (rt/pl1abi.h:14) changes, and the kinds should move into
  `rt/pl1abi.h` while at it.
- **GET LIST, GET EDIT, GET DATA**: a complex target reads one list
  token in the two-constant form, or two adjacent fields under C. GET
  DATA needs a kind code for complex in its table (`pl1_get_data`,
  rt/pl1ioget.c:488).

### 3.8 What the run time gets, and where

The memory rule decides the layout: autocall links whole members
(CLAUDE.md, `rt/`; measured in
`internals/research/pli/startup-measurement.md`: pl1io.o is linked
whole, 20,772 bytes of text, into every program). So:

- `rt/pl1cxm*.c`: the float routines, in **several members**, at
  least operators (multiply, divide, ABS) apart from powers and
  functions, so that `Z = A * B` does not pull in complex LOG.
- `rt/pl1cx.c`: complex to and from character, PUT LIST/DATA and GET
  entries for complex; fixed complex arithmetic if written in C.
- Complex editing for PUT EDIT and the C item in `rt/pl1ioed.c` and GET
  in `rt/pl1ioget.c` are inside members every I/O program links. Keep
  only the dispatch there (a few dozen bytes: recognising K_C, calling
  through a V-con into `pl1cx.c` that the compiler references only for
  complex items). If that indirection is refused, the complex editing
  and parsing code (estimate 1-2 KB) is carried by every program that
  does stream I/O. State this in the PR that adds it.

A program without COMPLEX then carries nothing new.

## 4. Tiers

Tier numbers are given when scheduled; here C1 to C5. Each ends with
twin tests that print. Every twin but one is a `.pli` (COMPLEX is PL/I
(F)), so `-lang=F` must accept them; the exception is the `.opt.pli`
of C1 for REAL/IMAG/CONJG of a real argument (if Q-7 is accepted). Test numbers continue after t263;
probe programs follow main
procedure names (and so test names) of at most seven characters, `¬` for NOT, RETURNS
declared in the caller, on-units left after the first entry
(CLAUDE.md).

### C1: data, constants, inline arithmetic, PUT LIST

Scope: the attribute (DECLARE, descriptors, RETURNS, DEFAULT), storage,
imaginary constants, assignment with mode conversion, `+`, `-`, prefix
`-`, `=`, `¬=`, REAL/IMAG/CONJG/COMPLEX/CPLX as functions and as
pseudo-variables, PUT LIST of complex values (needed for any twin:
the runner compares SYSPRINT), complex to character, STATIC and
AUTOMATIC INITIAL with complex constants, arrays and structure members
of complex type through lower.c, UNSPEC.

Probes first:
- **Q1CDCL** layout: UNSPEC of each complex kind (FLOAT DEC(6), (16),
  BIN(21), (53), FIXED BIN(15), (31,5), FIXED DEC(5,2), (6,2)) with known
  values; a complex member in a structure next to a CHAR(1), for
  alignment.
- **Q2CLIST** PUT LIST fields: the p. 274 values (`17.3+1.5I` in COMPLEX
  DEC FLOAT(5), `0.133+0.007I` in COMPLEX DEC FIXED(4,3)), negative real
  and imaginary parts, zero imaginary part, FIXED BIN complex, FLOAT
  BIN complex; constants written directly: `PUT LIST(2I)`,
  `PUT LIST(1+2I)`, `PUT LIST(0.133+0.007I)`, `PUT LIST(-1-1I)` (shows
  whether PL/I (F) folds and with which precision); an item that does
  not fit at the end of a PRINT line; PUT DATA of a complex variable.
- **Q3CMODE** conversions: real = complex, complex = real, mixed
  operands of all base/scale pairs, complex to CHAR(30) by assignment,
  complex to BIT(8), `=` and `¬=` results.
- **Q4CBIF** COMPLEX/CPLX precision of the result (FIXED (3,1) with
  (5,0); FLOAT(6) with FLOAT(16)), REAL/IMAG/CONJG/pseudo-variables
  including arrays; and, in a compile-only job, REAL(x) and CONJG(x)
  with a real x and the REAL pseudo-variable with a real target
  (IEM1625I expected); its COMPLEX/CPLX precision results are
  compared with GC33-0009-4 p. 367 (MAX(p1,p2) for FLOAT).
- **Q16CERR** compile-only refusals: `<` on complex (IEM1608I), the
  COMPLEX pseudo-variable with one argument (IEM0873I), STATIC CHAR
  INITIAL(3+4I) (p. 343 rule 13), CEIL/MAX/MOD of complex.

Twins: `t264cpx` (declarations, layout through UNSPEC, assignment,
INITIAL), `t265cad` (+ - prefix - CONJG, mixed modes, FIXED and FLOAT,
SIZE/FIXEDOVERFLOW on a part), `t266cbf` (REAL, IMAG, COMPLEX, CPLX,
pseudo-variables, arrays, = and ¬=), `t267cls` (PUT LIST/PUT DATA of
complex values, complex to character), and if Q-7 is accepted
`t268rea.opt.pli` (REAL/IMAG/CONJG of real arguments, the default
level only); the C2 twins then move up by one.

### C2: multiply, divide, ABS, integer power, fixed precision

*Done (Tier 114, twins `t285cml` to `t290cfa`).* The split pass did
not become a pass of its own: check.c's `cx_part` makes one B_CXOP
run-time call per operator, whose value goes into a complex
temporary; the part asked for first carries the call, the other is
read from the temporary (`cx_call`). A real operand that can be taken
twice multiplies or divides each part in line, as PL/I (F) does (two
ZERODIVIDEs for a real 0 divisor). FIXED is exact in the run time
(`rt/pl1cxf.c`); a FIXED power that stays FIXED is a chain of
multiplications.

Scope: `*`, `/`, ABS for FLOAT and FIXED (Q-4), `z**n`, ADD, MULTIPLY,
DIVIDE with complex arguments, ZERODIVIDE, OVERFLOW, UNDERFLOW,
FIXEDOVERFLOW, ONCODE 1554/1555.

Probes first:
- **Q5CMUL** float multiply and divide, short and long, with |c| >= |d|
  and |c| < |d|, operands near the exponent limits, `(1+1I)/(0+0I)`
  under an on-unit for ZERODIVIDE (entered once).
- **Q6CFIX** fixed complex multiply and divide, binary and decimal,
  precisions near 15 and 31 digits, FIXEDOVERFLOW and SIZE, ADD/
  MULTIPLY/DIVIDE with explicit precision.
- **Q7CABS** ABS of float and fixed complex values, including equal
  parts, a zero part, and the (MIN(N,p+1),q) precision shown by PUT
  LIST field width.
- **Q8CPOW** `z**0`, `z**1`, `z**2`, `z**-2`, `z**15` for FLOAT and FIXED,
  `0**-1` complex (ONCODE 1554/1555 and the message text).

Twins: `t268cml` (float * / with both division branches),
`t269cfx` (fixed * / ADD MULTIPLY DIVIDE), `t270abs` (ABS),
`t271pow` (integer powers, ERROR cases).

### C3: mathematical functions and general power

Scope: SQRT, EXP, LOG, SIN, COS, SINH, COSH, TAN, TANH, ATAN(z), ATANH
for complex, `z1**z2`, ONCODE
1556-1559, the refusals of section 1.3 (ATAND, COSD, ERF, ... of
complex).

Probes first:
- **Q9CMATH** each function, short and long, at a handful of points
  covering each branch named in C28-6590-0 p. 58-67; `ATAN(1I)`,
  `ATANH(1)` with a complex argument, `LOG(0+0I)` (ONCODE 1558/1559 and
  messages).
- **Q8CPOW** (second part) `z1**z2` general, `0**(0+1I)`,
  `0**(-1+0I)` (ONCODE 1556/1557).

Twins: `t272mth` (short), `t273mtl` (long), `t274cxx` (general power
and its ERROR cases).

### C4: edit-directed I/O, GET, character to complex

Scope: the C format item for PUT EDIT and GET EDIT; GET LIST and GET
DATA of complex targets; character to complex conversion with
CONVERSION, ONSOURCE, ONCHAR; PUT EDIT of a complex item.

Probes first:
- **Q10CED** PUT EDIT with `C(F(8,2))`, `C(F(8,2),E(12,4))`,
  `C(P'S99V.99')`, negative and positive imaginary parts, the I as a
  separate A item; in separate jobs, because either may not compile: a
  real item meeting C, and a complex item meeting F.
- **Q11CGET** GET LIST of `1+2I`, `-3.5E1-2I`, `2I`, `5`, `1 +2I`
  (blank before the central sign: CONVERSION expected), GET EDIT with
  `C(F(5),F(5))` and an `I` in the field (CONVERSION), GET DATA
  `Z=1+2I;`.
- **Q12CCNV** `Z = '1+2I'`, `Z = '  -1.5E0+2.5E1I  '`, `X = '1+2I'` with a
  real X (does PL/I (F) check the imaginary part? GC33-0009-4 p. 331
  says the Optimizer does not), CONVERSION's ONSOURCE for a bad complex
  string.

Twins: `t275ped` (PUT EDIT C), `t276get` (GET LIST/EDIT/DATA, with a
`.in`), `t277cnv` (character to complex, CONVERSION).

### C5: the rest

Scope, each item only if the maintainer keeps it: PICTURE COMPLEX
(storage, assignment, PUT LIST as the concatenated string, P items
under C), complex binary constants (after binary constants exist at
all, lex.c:424), ROUND/PRECISION/FIXED/FLOAT/BINARY/DECIMAL of complex,
SUM/PROD/POLY of complex arrays through lower.c (PL/I (F) has IHESSX,
IHEPSW, IHEYGW etc., GC28-6594-7 p. 274-278), complex arguments and
parameters with dummy conversion, complex RETURNS.

Probes first: **Q13CPIC** (PICTURE COMPLEX: UNSPEC, PUT LIST, PUT DATA,
PUT EDIT), **Q14CINI** (INITIAL forms, repetition factors with complex
constants), **Q15CARG** (complex parameters, a real argument for a
complex parameter, RETURNS(COMPLEX FLOAT)).

Twins: `t278pic`, `t279agg`, `t280arg`.

## 5. Cost

Line counts are estimates from the size of comparable tiers, not
measurements.

| Tier | Compiler | Run time | Tests |
|---|---|---|---|
| C1 | ~1,100 lines: lex 30, parse 80, pl1c.h 30, check 350, new cplx.c 450, gen 150 (part offset, temporaries, PUT LIST call, STATIC INITIAL), lower 20 | ~200 lines C (complex to character, PUT LIST/DATA entry) | 4 twins, 5 probes |
| C2 | ~250 | ~300 lines C for float, ~400 for fixed | 4 twins, 4 probes |
| C3 | ~80 (complex `math_entry` variants) | ~500 lines C; 6 rows in pl1cond.c | 3 twins, 1-2 probes |
| C4 | ~120 (C item, K_C, GET kinds), ABI stamp | ~450 lines C (editing under C, GET parsing, character to complex) | 3 twins, 3 probes |
| C5 | ~300 if all of it | ~200 | 3 twins, 3 probes |

**Run time on MVS.** For scale, the sizes of PL/I (F)'s modules for
the same work (GC28-6594-7 p. 270-278); C written after the same
methods comes out larger:

| Group | Modules | Bytes |
|---|---|---|
| float * / ABS | MZW, MZZ, DZW, DZZ, ABW, ABZ | 592 |
| float z**n | XIW, XIZ | 512 |
| float functions | SQW 152, SQZ 144, EXW 136, EXZ 136, LNW 268, LNZ 288, SNW 320, SNZ 368, TNW 184, TNZ 184, ATW 304, ATZ 296 | 2,780 |
| float z1**z2 | XXW, XXZ | 560 |
| fixed * / ABS z**n | MZU 240, MZV 672, ABU 184, ABV 544, XIU 120, XIV 192 | 1,952 |
| fixed ADD MULTIPLY DIVIDE | ADV 96, MPU 240, MPV 288, DVU 408, DVV 576 | 1,608 |
| IBM's complex I/O and conversion (for scale; ours is C) | UPA 192, UPB 232, VCS 480 | 904 |

All of it together is about 9 KB of text. A program links only the
members it references, if the files are split as in 3.8. The complex
functions call the real ones (GY28-6801-6 p. 88, p. 107, p. 117), so a
program using one complex function also links the members of the real
ones it calls. For
scale, the object files in `build/` are about twice their
text: `pl1io.o` is 38 KB on disk against 20,772 bytes of text in the
load module (startup-measurement.md).

A typical complex program (FLOAT * and /, ABS, PUT LIST) would add
about 2 KB of text: the operators, the complex PUT LIST and the
character conversion in C (estimate). The frame grows by the
16-byte temporaries a statement uses.

## 6. Risks

- **Representation (A vs B)**: B saves work now and risks silent wrong
  code later; A costs more lines in every complex-aware place. See
  3.1.
- **Byte equality**: what the run time computes out of line can
  differ from PL/I (F) in the last digit, where PL/I (F) is the less
  accurate (as for the real functions since `rt/pl1m*.c`). The twins
  will show it; budget time for it.
- **Evaluation order and overlap** in the split pass: function calls
  with side effects inside complex expressions, targets that occur in
  their own source. Each needs a twin case.
- **PUT LIST field form**: the only manual examples are OCR-damaged;
  C1 cannot be finished before Q2CLIST.
- **ABI**: the C format item changes the format table layout shared
  with the run time (`PL1_ABI_STAMP`); a GET DATA kind code too.
- **Every program's size**: if complex editing or parsing lands inside
  pl1io.o/pl1ioget.o instead of behind a reference only complex code
  makes, every stream-I/O program grows (estimate 1-2 KB).
- **m38b**: results there carry the soak caveat in TODO.md ("Now: m38b
  after the soak"), as for every tier.

## 7. Open questions for the maintainer

- **Q-1** Representation: new kinds (A, recommended) or a mode flag (B)?
- **Q-2** Split pass `src/cplx.c` (recommended) or native complex code
  paths in gen.c?
- **Q-3** Fixed complex multiply, divide, ABS, power, ADD/MULTIPLY/
  DIVIDE: *settled* (maintainer, 2026-10-18): C, no IBM code.
- **Q-4** Is COMPLEX FIXED in scope beyond what C1 needs for
  `1+2I`-style constants? FLOAT complex is what real programs use;
  FIXED complex multiply/divide is the costliest part of C2.
- **Q-5** Constant folding of `real ± imaginary` constants: only if
  PL/I (F) is shown to fold (Q2CLIST)?
- **Q-6** PICTURE COMPLEX and complex binary constants: in C5, or out
  of the plan?
- **Q-7** REAL/IMAG/CONJG of a real argument by default (GC33-0009-4
  allows, F refuses or warns): accept as an OPT deviation, as proposed?
  *Settled by the probe:* PL/I (F) takes them as functions without a
  message (t227 Q5CERR); only the REAL pseudo-variable of a real target
  warns (IEM1625I) and compiles. So they are PL/I (F)'s, no OPT mark,
  and every C1 twin is a `.pli`.
- **Q-8** The C item and K_C: move the format kinds into `rt/pl1abi.h`
  with the ABI stamp change, as proposed?
