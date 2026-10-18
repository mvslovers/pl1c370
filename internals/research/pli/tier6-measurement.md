# Tier 6 rules, measured against PL/I (F) V5.5

Measured 2026-10-12 on MVSTK5-BLD. Probes (JOB00137 to
JOB00141, JOB00149). The test programs `tests/pli/t27zdiv` to `t40inpl`
print what PL/I (F) prints and end with the same condition code (JOB00155,
together with Tiers 0 to 5: 42 programs).

**JOB00137 holds one result that is not what it seems.** C3CONV and C4CONV
failed to compile there (character constants, see below), their link-edit
was skipped and left their object decks in `&&LOADSET`, and C5FOFL's GO
step ran C3CONV's code. `submit.py` gives every step an object data set of
its own since; a GO step after a failed compile in the same job of an older
probe is suspect.

## On-units

- An ON statement establishes the on-unit for the block activation that
  executes it. A called procedure inherits it; what the procedure
  establishes itself ends when it returns; REVERT ends it earlier and the
  caller's on-unit applies again (C7SIG, CEGOTO).
- ON ... SYSTEM establishes the system action.
- GO TO out of an on-unit to a label of the establishing block works also
  when the condition was raised in a called procedure (CEGOTO).
- ONCODE is 0 outside an on-unit, also after a GO TO out of one.

## ONCODE

| Condition | ONCODE |
|---|---|
| ZERODIVIDE (binary, decimal, MOD, SIGNAL) | 320 |
| FIXEDOVERFLOW | 310 |
| CONVERSION in an assignment or expression | 612 |
| CONVERSION in GET LIST | 613 |
| SIGNAL CONVERSION | 600 |
| ENDFILE | 70 |
| CONDITION(name) | 500 |
| SIGNAL ERROR | 9 |
| FINISH (SIGNAL, and at the normal end) | 4 |

ERROR and FINISH raised by a system action keep the code of the
condition that caused them (ERROR unit and FINISH unit both see 320 after
ZERODIVIDE, 612 after CONVERSION, 70 after ENDFILE).

## The system action

| Condition | Message | Then |
|---|---|---|
| ZERODIVIDE | `IHE330I ZERODIVIDE` | ERROR |
| FIXEDOVERFLOW | `IHE320I FIXEDOVERFLOW` | ERROR |
| CONVERSION | `IHE604I ERROR IN CONVERSION FROM CHARACTER STRING TO ARITHMETIC` | ERROR |
| ENDFILE | `IHE140I FILE f - END OF FILE ENCOUNTERED` | ERROR |
| CONDITION(name) | `IHE501I CONDITION name` | continues |
| ERROR | none | FINISH, end |
| FINISH | none | continues |

Each message goes on with ` AT OFFSET +nnnnn FROM ENTRY POINT p`, p being
the procedure in which the condition was raised. The offset belongs to
each compiler's code and can never agree: `compare.py` masks it in both
files and compares the rest of the line.

IHE501I names the condition only up to seven characters (the compiler also
warns that it truncates such an external name to seven, IEM2867I in the
listing of JOB00174; whether that is why the message leaves it out is not
known): `CONDITION AB`,
`CONDITION ABCDEFG`, but `CONDITION AT OFFSET ...` for ABCDEFGH and
longer (JOB00140, JOB00141). The probe of JOB00141 looped after that part (its on-unit
went back to a label that raised the condition again, SB37) and is not kept;
`tests/pli/t37name` has its names.

**The end after ERROR**: FINISH is raised, then the step ends with
condition code **2000**. A normal return from an ERROR on-unit ends the
program the same way, and so does SIGNAL ERROR, which writes no message.
At the normal end of the main procedure FINISH is raised too, with
ONCODE 4, and the step ends with 0. A normal return from a FINISH on-unit
after SIGNAL FINISH continues after the SIGNAL.

**Where the message goes** (CDLAY, C6ERR, C7SIG): on SYSPRINT, as
follows -- the line begun is ended, the (now) empty line is written as a
record, the message is a record of its own, and a new empty line is
begun. So a message has a blank line before it, unless it is the first
line of a page (then the `1` record is that line), and a PUT LIST without
SKIP after it starts on a line of its own. The message record is not cut
at the line width (IHE799I fills 120 characters). Whether it counts towards the 60 lines of a
page is not measured.

## CONVERSION

- ONCHAR and ONSOURCE are the character and the field that failed;
  assigning to them changes **the source field itself** (a variable
  `S = '12A4'` reads `1234` after `ONCHAR = '3'`), and the conversion is
  done again from it.
- A normal return without changing the field: `IHE799I RETURN ATTEMPTED
  FROM CONVERSION ON-UNIT BUT SOURCE FIELD NOT MODIFIED`, then ERROR.
- A character constant that is no number is converted when PL/I (F)
  compiles, and refused: IEM1804I, a severe error. PL1C/370 refuses it.
- `'1B'` converts to 1: PL/I (F) reads it as a binary constant. PL1C/370
  does not accept that form (tests avoid it).

## FIXEDOVERFLOW

- Binary: a result of +, -, * beyond 31 bits. A FIXED BINARY(15)
  variable at 32767 plus 1 becomes -32768 and raises nothing.
- Decimal: a result of more than 15 digits (999999999999999 + 1; a
  product of 16 digits keeps its 15 rightmost ones).
- Assignment truncates and raises nothing (SIZE, disabled): DEC(5) from
  123456 is 23456, DEC(3,1) from 100.9 is 0.9.
- **Except an addition PL/I (F) makes in the target's field**: `X = X + Y`,
  `X = Y + X` and `X = X - Y`, X FIXED DECIMAL and Y a decimal constant or
  scalar of the same scale. A sum beyond the bytes of X raises
  FIXEDOVERFLOW and keeps the rightmost digits (99999 + 1 in DEC(5) is 0,
  99999 - (-99999) is 99998). `X = Z + 1`, `F = X + 1` and `R = R + 1` with
  a scale to align raise nothing (CJINPL, JOB00149).
- An even precision has room for one more digit, and PL/I (F) keeps and
  prints it: DEC(4) holding 9999 + 1 prints `10000`, in the field width of
  DEC(4).

## Not measured, or not done

- CONVERSION in GET EDIT, and its ONCODE; the message of CONVERSION in GET
  LIST without an on-unit.
- The entry point name a message gives for a condition raised inside an
  on-unit (PL1C/370 names the procedure the on-unit is written in).
- FIXED DECIMAL to FIXED BINARY(31) beyond 2**31: PL/I (F) keeps the low
  32 bits (9999999999 becomes 1410065407) and raises nothing; PL1C/370's
  CVB ends with a fixed-point divide exception (0C9). To do with SIZE.
- A GO TO out of a FINISH on-unit after ERROR, not measured: PL1C/370
  then raises FINISH no second time, neither at a later ERROR nor at the
  normal end.
- Condition prefixes, SIZE, SNAP, STOP and EXIT.
