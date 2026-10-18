# Tier 74: ANSI's declarative attributes

ANSI X3.53-1976 has attributes that spell out what PL/I (F) implies
(CM23, CM24, `ansi-comparison.md`):
- CONSTANT, VARIABLE;
- PARAMETER (PARM);
- LOCAL;
- STRUCTURE, MEMBER;
- NONVARYING (NONVAR);
- DIMENSION (DIM);
- CONDITION (COND), which declares a condition name.

PL1C/370 refused them, and so does PL/I (F). A program that writes them
is beyond the oracle. The test `t214att` is a twin pair, identical in
JOB01141. Its twin is the same program with the attributes left out,
which is what PL/I (F) compiles.

## The rules taken

- **CONSTANT** goes on a file or an entry declaration. Either is a
  constant without it.
- **VARIABLE** goes on data and labels, which are variables without
  it. An ENTRY or FILE variable is refused as not supported yet.
- **PARAMETER** on a name that is not a parameter of its procedure is
  an error. The name gets the attribute from the parameter list
  anyway (4.3.2).
- **STRUCTURE and MEMBER** must agree with the levels (4.3.3.1, Step
  3):
  - STRUCTURE needs members;
  - MEMBER needs a structure above.
- **NONVARYING** goes on CHARACTER or BIT, and not together with
  VARYING.
- **DIMENSION(bounds)** is the bound list after the name; a declaration
  may not have both.
- **LOCAL** goes on a label variable (6.3.7, Step 2.2.1). PL1C/370 does
  not check the GO TO it restricts.
- **CONDITION** declares the name a condition name. It takes no other
  attributes, and a reference to it as a variable is an error.
- **In a parameter descriptor,** PARAMETER and NONVARYING are accepted
  too.

## Checked on the host

Each of these is refused, with a message:
- `DCL X FIXED PARAMETER;` outside a parameter list;
- `DCL X FIXED STRUCTURE;` and `DCL X FIXED MEMBER;`;
- `DCL C CONDITION; C = 1;`;
- `CHAR(3) VARYING NONVAR`;
- `DCL E ENTRY VARIABLE;`;
- `DCL X FIXED CONSTANT;` and `DCL X FIXED LOCAL;`;
- `DCL A(2) DIM(3) FIXED;`.

## Not done

- **ENTRY and FILE variables.**
- **The scope of a declared condition name:** all names are one, as
  for an undeclared CONDITION(name). See `docs/language-levels.md`.

## The suite

All 216 programs are identical: JOB01143 to JOB01147.
