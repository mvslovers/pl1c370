# Tier 34: the DEFAULT statement -- beyond the oracle

PL/I (F) has no DEFAULT statement. The specification is GC33-0009-4
(October 1976): chapter 7 ("Recognition of Names", the standard
defaults and "DEFAULT Statement") and section J (figure J.2 and the
general rules of DEFAULT). Tests are twins, as in Tier 33:
`<name>.opt.pli` uses DEFAULT, and `<name>.pli` declares explicitly
what the DEFAULT statements give. `t127dflt` and `t128dft2` are
identical (JOB00548).

## The rules implemented

- **Form.** `DEFAULT spec [, spec]...;`, abbreviated DFT. A spec is
  `RANGE(r [, r]...) attributes` or a factored `(spec, spec)
  attributes`, whose attributes go to each spec inside. A range is an
  identifier (`RANGE(NUM)`: the names that begin with NUM), a letter
  range (`A:C`: the names whose first letter lies in it), or `*` (all
  names).
- **Attributes:** FIXED, FLOAT, BINARY, DECIMAL, CHARACTER, BIT,
  VARYING, POINTER and PICTURE. REAL, ALIGNED, UNALIGNED and AUTOMATIC
  are accepted and change nothing. `VALUE(...)` gives the precision of
  a base and scale pair (`FIXED DECIMAL(9,3)`, `FLOAT BINARY(21)`; both
  keywords are required) and the length of CHARACTER or BIT.
- **What is taken.** Only the classes of attributes a declaration
  lacks are taken from the spec:
  - A name with no data attributes takes the spec's string, locator or
    picture attribute.
  - A string takes VARYING, and its length from VALUE if none was
    given.
  - An arithmetic name takes base and scale where they are missing,
    then its precision from the VALUE entry of the base and scale it
    now has.
  - What is still missing comes from the standard rules: a missing
    scale is FLOAT and a missing base DECIMAL. With neither, the name
    goes by its first letter: I to N FIXED BINARY(15), all others
    FLOAT DECIMAL(6).
  - Examples (`t127dflt`): `RANGE(A:C) FIXED BINARY` makes `B2 FIXED`
    FIXED BINARY(15) and `C3 DECIMAL` FIXED DECIMAL(5).
    `RANGE(S) VARYING VALUE(CHARACTER(10))` makes `S1 CHAR`
    CHARACTER(10) VARYING and `S3 BIT` BIT(1) VARYING.
- **Scope.** A DEFAULT statement applies to its whole block wherever it
  stands, and to the blocks inside it. A block's own DEFAULT for a
  range takes the place of an outer one for the names it covers: under
  `RANGE(A) FLOAT` in a contained block, A9 is FLOAT DECIMAL and does
  not inherit the outer FIXED BINARY. `DEFAULT RANGE(*);` with no
  attributes restores the standard rules in its block (`t128dft2`,
  block C).
- **Implicit declarations** (names never declared, and undeclared
  parameters) take only the main procedure's DEFAULT statements,
  because GC33-0009-4 treats them as declared in the external
  procedure.
- **Overlapping ranges in one block:** the first specification written
  that covers the name applies. GC33-0009-4 gives no order, so the
  tests do not depend on it.

## Not done

DESCRIPTORS, VALUE(AREA(n)), dimensions, INITIAL, storage-class
attributes and the RETURNS default of function names. Also missing,
from the declarations themselves: factored declarations with attributes
of their own inside the parentheses (`DCL (A FIXED, B FLOAT)`).
