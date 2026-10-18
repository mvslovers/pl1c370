# Tier 64: the CHECK condition

`(CHECK(A, L1, SUB)): P: PROC;` raises CHECK(A) when A is assigned,
CHECK(L1) before the statement labelled L1, and CHECK(SUB) before SUB is
invoked. Tier 59's comparison listed CHECK.

Measured 2026-10-14 on MVSTK5-BLD with a probe
and its input. The output of the first version was not kept; the
version with ON, SIGNAL and REVERT is JOB01016. The test `t199chk` is the probe, with
`ARR` initialised in full, identical in JOB01039.

## The rules

- **The prefix** `CHECK(name, ...)` stands before a PROCEDURE or BEGIN
  statement. It applies to the blocks inside, too: an assignment to A
  in the internal procedure SUB is checked.
- **A variable** raises CHECK after each assignment:
  - `A = A` raises it too: no change of value is needed;
  - GET LIST raises it for each item it assigns;
  - a DO loop raises it for each value of its control variable, the last
    one included (A=1, A=2, A=3 for `DO A = 1 TO 2`);
  - INITIAL does not raise it;
  - an unchecked variable does not (D).
- **The system action for a variable** writes it as PUT DATA does, on a
  line of its own after an empty line, as a message:
  - the whole array after `ARR(2) = 7`;
  - only the member after `S.X = 3` under CHECK(S);
  - `B= 1.50000E+00;`, `C='HELLO';`.
- **The system action for a label or an entry** writes its name, before
  the statement or the call: `L1`, `SUB`.
- **ON CHECK(name)** replaces the system action. ONCODE is 511 for a
  variable and 510 for a label or entry. The on-unit is also entered
  for an assignment in a procedure called from the block (A = 9 in
  SUB).
- **SIGNAL CHECK(name)** acts as the assignment or the label would: the
  on-unit, or the system action with the current value. After REVERT,
  SIGNAL CHECK(A) writes `A= 6;`.
- **A name declared nowhere:** `SIGNAL CHECK(L2)` without a label L2
  makes L2 an implicit variable. ONCODE is then 511, and the system
  action writes `L2= 0;` (scratch tests, not kept: JOB01024, JOB01033).
  PL1C/370 takes such a name as a label (510).

## In PL1C/370

- **parse.c** reads `CHECK(name, ...)` in a prefix and keeps the names
  with the block (`proc->checks`). ON, SIGNAL and REVERT take CHECK(name)
  as they take CONDITION(name).
- **lower.c** inserts a statement S_CHECK:
  - after an assignment, GET or READ INTO with a checked target;
  - at the head of a DO loop's body, and after the loop, for a checked
    control variable;
  - before a statement with a checked label, the labels moved onto it;
  - before the invocation of a checked entry.
- **The S_CHECK of a variable** carries a PUT DATA of it to SYSPRINT.
- **The run time** (`pl1_check`) enters the on-unit, or writes a label's
  name as a message. For a variable it returns 1, and the generated PUT
  runs between `pl1_check_begin` (the empty line) and `pl1_check_end`.

## A fault found on the way

The first full `t199chk` ended with S0C1 in the ON CHECK(A) on-unit
(JOB01020, a dump in JOB01031). The on-unit followed a main procedure of
almost 4 KB:
- Every procedure starts with `USING *,12`, and its body has
  `USING *,12,11,10`.
- The previous procedure's USING of R11 and R10 was still in force.
- The assembler therefore resolved the prologue's branch to `@@FEN1`
  with R11, the main procedure's second base, at the smaller
  displacement. At run time R11 held the caller's value.

Each procedure now ends with `DROP 12,11,10`. The fault is older than
CHECK: any procedure after one that reaches into its second 4 KB could
meet it. No earlier test had that layout.

`submit.py` also takes data and DD statements for a RUN step together
(`NAME:RUN:MODULE::DATA:DDS`), needed for the dump.

## The suite

All 201 programs are identical: JOB01041 to JOB01045.

## Not done

- **NOCHECK** and `CHECK` on a statement other than PROCEDURE or BEGIN.
- **CHECK of an undeclared name** in SIGNAL (above).
- **A function reference** to a checked entry in a WHILE or UNTIL
  condition.
- **The repetitive items** of a GET (`GET LIST((A(I) DO I = 1 TO N))`).
- **Assignments through a pseudo-variable** other than SUBSTR.
- **Variables in BASED, parameter or DEFINED storage:** PL/I (F) does
  not allow them to be checked; PL1C/370 does not refuse them.
