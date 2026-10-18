# Tier 88: LANGLVL(SPROG), pointer arithmetic

PL/I for MVS & VM's compile-time option LANGLVL, from the inventory
(`mvsvm-inventory.md`) (#207).

## The rules

- **The option** (SC26-3113-01, "LANGLVL"): `LANGLVL({OS,SPROG|NOSPROG})`,
  IBM's default `LANGLVL(OS,NOSPROG)`. SPROG "specifies that the
  compiler is to allow extended operations on pointers, including
  arithmetic, and the use of the POINTERADD, BINARYVALUE, and
  POINTERVALUE built-in functions". SYSNULL "is valid without the
  LANGLVL(SPROG) option" (SC26-3114-01, SYSNULL).
- **The operations** (SC26-3114-01, chapter 3, "Pointer Operations"):
  - a pointer plus or minus a BIT, REAL FIXED BINARY(p,0) or REAL FIXED
    DECIMAL(p,0) expression, converted to FIXED BINARY(31,0);
  - two pointers subtracted: their "logical difference", FIXED
    BINARY(31,0);
  - pointers compared with the infix operators;
  - `(PTR1 + 16) -> BASED_PTR` "is invalid", `POINTERADD(PTR1,16) ->
    BASED_PTR` is valid;
  - `DCL CVTPTR POINTER BASED(POINTERVALUE(16))` reads the CVT pointer.

## In PL1C/370

- **The option:** `-langlvl=SPROG` (and `-langlvl=NOSPROG`), or LANGLVL
  among the options of a `%PROCESS` or `*PROCESS` statement, which
  parse.c reads as it skips the statement. The default is NOSPROG, as
  IBM's.
- **Without SPROG,** POINTERADD, POINTERVALUE and BINARYVALUE are errors
  that name the option (resolve.c), and so is pointer arithmetic
  (check.c). This changes Tier 86 and 87: `t233mvs` and `t237sin` now
  begin with `%PROCESS LANGLVL(SPROG);`.
- **With SPROG** (check.c, the binary expression):
  - `P + n`, `n + P` and `P - n` become POINTERADD(P, n) or
    POINTERADD(P, -n);
  - `P2 - P1` becomes `BINARYVALUE(P2) - BINARYVALUE(P1)`;
  - `<`, `<=`, `>`, `>=` of two pointers compare their BINARYVALUEs.
    That is the logical order for any two addresses below 16 MB; NULL
    (X'FF000000') comes out below every address, as a negative value.
  - Each is level MVS for `-lang=F`.

## Test

`t239spr`: each operation, a BIT addend, and `BASED(POINTERVALUE(16))`
(its BINARYVALUE is positive). `t239spr`, `t233mvs` and `t237sin`
identical in JOB01448; the full suite 240 of 240 (see ROADMAP).

## Not done

- LANGLVL in a `%PROCESS` applies from there on to the rest of the
  source, not per external procedure.
- NULL in a `<` or `>` comparison: the manual does not say how it
  orders; PL1C/370 orders it below every address.
