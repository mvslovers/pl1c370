# Tier 102: INITIAL CALL

Issue #222, block C. GC33-0009-4 and SC26-3114-01 (INITIAL attribute):
`INITIAL CALL entry-reference [(argument, ...)]` calls the procedure
when the variable is allocated; "INITIAL CALL cannot be used to
initialize static data". Not in PL/I (F): OPT.

## In PL1C/370

- **parse.c:** INITIAL CALL keeps a CALL statement with the variable
  (`initcall`), resolved in its block (resolve.c).
- **lower.c:** an AUTOMATIC variable's CALL among the block's
  initializations, in the order of the declarations; a CONTROLLED
  one's after each ALLOCATE, with its INITIAL values. STATIC, BASED and
  factored names with INITIAL CALL are refused.

## Test

`t259icl` (`.opt.pli`; its PL/I (F) twin writes the calls out at the
block's entry and after each ALLOCATE): an AUTOMATIC array, a
CONTROLLED array allocated twice, an array in an internal procedure,
the called procedures counting their calls in a STATIC variable. With
an AUTOMATIC counter declared after the variable, PL1C/370 initialized
the counter after the call (JOB02222), where the twin, with the call
written out after the declarations, counts it; the manuals do not say
which comes first, so the test does not depend on it. Identical in
JOB02226; the full suite 260 of 260 in JOB02229 to JOB02247.
