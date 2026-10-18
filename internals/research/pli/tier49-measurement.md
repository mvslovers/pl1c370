# Tier 49: SNAP, SIGNAL of disabled conditions, contextual external entries

Measured 2026-10-13 on MVSTK5-BLD with the probes:

| Probe | Job |
|---|---|
| Q1SNAP | JOB00798 |
| Q2SNAP | JOB00799 |
| Q3SNAP | JOB00800 |
| Q4SNAP | JOB00801 |
| Q5SIG | JOB00804 |

The tests are the probes:

- `t157snp` to `t160snp` are Q1SNAP to Q4SNAP; `t158snp` leaves out
  NAME, which PL1C/370 does not have. They are identical: t157snp,
  t159snp and t160snp in JOB00803, t158snp in JOB00806.
- `t161sig` is Q5SIG, identical in JOB00806.
- `t162ctx`, compiled apart, is identical in JOB00808.

## SNAP

`ON condition SNAP on-unit` writes the place first, as a message:

    CONDITION ZDIV OCCURRED AT OFFSET +000BC FROM ENTRY POINT Q1SNAP

Then, for each procedure that called the one running, up to and including
the main procedure, it writes this line, each as a message of its own
(a blank line before it):

         CALLED FROM PROCEDURE WITH ENTRY POINT INNER

BEGIN blocks and on-units are left out of that list:

- DEEPER, called from INNER, called from Q1SNAP, gives INNER and
  Q1SNAP.
- Q, called from an on-unit in P, which was called from a BEGIN block in
  Q2SNAP, gives P and Q2SNAP.

Only after that does the on-unit run.

**The four-character names:**

| Condition | Name | Condition | Name |
|---|---|---|---|
| OVERFLOW | OFLO | UNDERFLOW | UFLO |
| FIXEDOVERFLOW | FOFL | ZERODIVIDE | ZDIV |
| SIZE | SIZE | SUBSCRIPTRANGE | SUBR |
| STRINGRANGE | STRG | CONVERSION | CONV |
| ENDFILE | ENDF | ENDPAGE | ENDP |
| KEY | KEY | RECORD | RECD |
| UNDEFINEDFILE | UNDF | AREA | AREA |
| CONDITION | COND | NAME | NAME |
| ERROR | ERR | FINISH | FIN |

A shorter name is padded to four characters: `CONDITION ERR  OCCURRED`.

**With SYSTEM** (`ON ZERODIVIDE SNAP SYSTEM;`) the system action's
message takes the place of the CONDITION line. The calling procedures
follow it (Q4SNAP: `IHE330I ZERODIVIDE AT ... P`, then `CALLED FROM ...
Q4SNAP`). The ERROR this raises is snapped again by `ON ERROR SNAP`, at
the same offset in P.

**In PL1C/370:**

- The establishment record has a SNAP flag; EST_SIZE is 28 now.
- The calling procedures are found along the frames' saved PL1CURF, a
  chain of PL/I blocks only, whatever C frames of the run time lie
  between.
- The length byte of a procedure's descriptor marks BEGIN blocks and
  on-units (X'80') and the main procedure (X'40').

## SIGNAL of a disabled condition

The following are null statements; their on-units do not run (Q5SIG):

- `SIGNAL SIZE` and `SIGNAL SUBSCRIPTRANGE` without a prefix enabling
  them;
- `(NOZERODIVIDE): SIGNAL ZERODIVIDE`.

PL1C/370 generates nothing for them.

## Contextual external entries

A name declared nowhere and used in a CALL statement is an external
entry by its context (PL1SAMP's `CALL INGSP(IGSP, NULL)`). The arguments
are passed as they are, through `=V(name)`. With this and the empty
descriptors of Tier 48, IBM's SYS1.SAMPLIB(PL1SAMP) compiles. It calls
the 2250 graphics subroutines, so it cannot run here.

## Not done

- SNAP for a condition whose system action writes no message (ENDPAGE,
  FINISH SYSTEM): nothing is written.
- The NAME condition.
- A function reference to an undeclared name with arguments: it remains
  an error (what PL/I (F) does with it is not measured).
