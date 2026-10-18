# Tier 92: the small gaps of PL/I (F)

Issue #216, block A. Four things PL/I (F) has that PL1C/370 refused or
let through.

## The items

- **INDEX of bit strings** (GC28-8201-4, INDEX): the position of a bit
  string in a bit string. One argument a bit string and the other a
  character string: the bit string is taken as CHARACTER, as PL1C/370
  does for AFTER and BEFORE (Tier 69). PL/I (F) computed INDEX(B,
  '11'B) in t208anb's twin already (JOB01092).
- **UNSPEC of an expression**, `UNSPEC(ADDR(P))`, `UNSPEC(I + 1)`: the
  internal form of the value, as for a variable.
- **REDUCIBLE and IRREDUCIBLE** (abbreviations RED, IRRED), on an entry
  declaration: hints for the optimizer. They are accepted and have no
  effect. On a PROCEDURE statement they are the Optimizing Compiler's
  (SC33-0009-2, PROCEDURE statement); GC28-8201-4 lists RECURSIVE and
  RETURNS there only, and PL/I (F) refuses them (below).
- **`GETP -> X`**, a function reference without arguments as a locator
  qualifier: PL/I (F) refuses it (IEM0646I, JOB01090, Tier 76). With
  `-lang=F`, PL1C/370 accepted it until now.

## In PL1C/370

- **INDEX** (check.c, gen.c, rt/pl1ansi.c): two bit strings call
  PL1INDB. It spreads the bits to one character each, as AFTER and
  BEFORE of bit strings do, and returns the character INDEX of those.
- **UNSPEC** (check.c, gen.c `unspec_tmp`): an argument that is
  neither a variable nor a string is assigned to a temporary of its
  own type, and UNSPEC takes the bytes of that. A bit expression of up
  to 32 bits stays in a register. The length follows the type of the
  expression: `UNSPEC(H + H)` of a FIXED BIN(15) H is FIXED BIN(16),
  so 32 bits. An AREA expression is still refused.
- **REDUCIBLE, IRREDUCIBLE** (parse.c): skipped; on a PROCEDURE
  statement level OPT, so `-lang=F` refuses them there.
- **The qualifier** (resolve.c): once a bare name has turned into a
  function reference, `-lang=F` refuses it as it already refused
  `ADDR(A) -> X` and `F(I) -> X`.

## Test

- **JOB01832:** `t244gap` with REDUCIBLE and IRREDUCIBLE on the
  PROCEDURE statements of R1 and R2. PL/I (F) ends RC 12: IEM0182I
  "TEXT BEGINNING 'REDUCIBLE' SKIPPED" and IEM0181I on both statements.
  The same attributes in the entry declarations pass.
- **JOB01835:** `t244gap` without them on the PROCEDURE statements,
  and `t245red` (`.opt.pli`, its twin without them): both identical.

On the host, the 244 earlier programs compile to the same assembler as
on main, and `-lang=F` refuses `GETP -> X`.

The values worked out from the manual, which PL/I (F) printed in
JOB01835:

| Reference | Value |
|---|---|
| `INDEX(B, '1101'B)`, B = `'0011010011110000'B` | 3 |
| `INDEX(B, '1111'B)`, `INDEX(B, '111'B)` | 9, 9 |
| `INDEX(B, '0000000'B)`, of a null string, in a null string | 0, 0, 0 |
| `INDEX(SUBSTR(B, 5), '1'B)` | 2 |
| `INDEX(B \|\| '1111'B, '01111'B)` | 8 |
| `INDEX('XX1101YY', '1101'B)`, `INDEX('1101'B, '01')` | 3, 3 |
| `UNSPEC(ADDR(I)) = UNSPEC(P)` | `'1'B` |

PL/I (F) takes UNSPEC of each expression kind in the test, with the
same lengths: `UNSPEC(H + H)` 32 bits, `UNSPEC(D + 1)` 32 bits
(`'00000000000000000001001001001100'B`, 124 packed in four bytes).
