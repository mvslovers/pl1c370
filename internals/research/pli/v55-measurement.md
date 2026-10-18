# PL/I (F) V5.5 against the Version 5 documentation

Measured 2026-10-12 on MVSTK5-BLD (jobs JOB00048–JOB00053), with a read-only
comparison against MVSTK5-REF, with probe programs submitted by
`tests/submit.py`.

**Question.** Does the installed PL/I (F) "VERSION 5.5" do anything the
Version 5 documentation does not describe? There is no V5.0 artefact to diff
against, so the documentation is the baseline: Language Reference GC28-8201-4
(Dec 1972), Programmer's Guide GC28-6594-7 (Jan 1971), and the Version 5
Planning Guide C33-0002-0 (Mar 1969), whose purpose is to list what V5 added.

**Answer.** No difference found. V5.5 is a maintenance level of the documented
Version 5, and GC28-8201-4 with GC28-6594-7 is the specification PL1C/370
measures against. This closes "Before Tier 0, item 1" of the roadmap.

## Two stands, one compiler

| | MVSTK5-BLD | MVSTK5-REF |
|---|---|---|
| `IEM*` members in `SYS2.LINKLIB` | 256 | 256, same names |
| content | — | **256 of 256 byte-identical** to BLD |

Read as binary over mvsMF; nothing was submitted to REF.

## Static: maintenance dates

90 of the 256 phases carry an IDR record of Assembler (F) (`360SAS037`); the
other 166 carry none. The dates in those 90:

| date | CSECT records |
|---|---|
| 1972-08-19 | 1 |
| 1972-11-08 | 12 |
| 1972-11-10 | 15 |
| 1972-11-14 | 21 |
| 1974-01-24 | 25 |
| 1974-01-25 | 16 |

Nothing earlier than August 1972 and nothing later than January 1974: the
phases IBM rebuilt at all were rebuilt in two maintenance waves.

Two static comparisons were tried and **do not carry**, so they are not used:
the `IEMnnnnI` message ids (the phases do not hold them as text; 28 ids found
against several hundred messages, and the Programmer's Guide OCR yields only 115
ids), and the phase names (the Program Logic Manual GY28-6800-5 OCR yields 43
names against 256 phases).

## Dynamic: probes

| Probe | Feature | Source of expectation | Expected | Observed | Job |
|---|---|---|---|---|---|
| P0 | trivial program, PL1LFCLG | — | runs | compile CC 4 (IEM0227I only), GO prints `HELLO FROM PL/I (F)  3` | JOB00048 |
| P1 | TRANSLATE | C33-0002-0 | accepted | `XYZDEFGHIJ` | JOB00053 |
| P1 | VERIFY | C33-0002-0 | accepted | `4` | JOB00053 |
| P1 | STRING pseudovariable | GC28-8201-4 | accepted | `HEL`, `LO` | JOB00053 |
| P1 | FIXED BIN(15) halfword | C33-0002-0 | halfword storage | `12345`; IEM0764I reports halfword storage | JOB00053 |
| P1 | RETURNS on PROCEDURE | C33-0002-0 (mandatory in V5) | accepted | `8` | JOB00053 |
| P1 | REFER, relaxed restriction | C33-0002-0 | accepted | `3 XYZ` | JOB00053 |
| P1 | REORDER option | C33-0002-0 | accepted | accepted, no message | JOB00053 |
| P2 | SELECT/WHEN/OTHERWISE | Optimizer R3 (GC33-0009-4) | rejected | CC 12; parsed as assignment: IEM0673I, IEM0080I, IEM0720I | JOB00050 |
| P2 | LEAVE | Optimizer R3 | rejected | CC 12; IEM1793I, IEM0080I | JOB00050 |
| P2 | DO UNTIL | Optimizer R3 | rejected | CC 12; IEM0182I "TEXT BEGINNING 'UNTIL(I >=' SKIPPED" | JOB00050 |
| P2 | DEFAULT statement | Optimizer R1 | rejected | CC 12; IEM0673I, IEM0109I | JOB00050 |
| P2 | %PAGE / %SKIP, NOMACRO | Optimizer | rejected | CC 12; IEM0109I on `%` | JOB00050 |
| P2 | %PAGE / %SKIP, MACRO | Optimizer | rejected | CC 16; IEM4136I "STATEMENT TYPE NOT RECOGNIZABLE", IEM4433I terminal | JOB00051 |
| P2 | FETCH / RELEASE | Optimizer R1 | rejected | CC 12; IEM0673I, IEM0080I | JOB00050 |

Every Optimizer statement is rejected: none of the later language leaked into
V5.5.

## Options

The compiler prints its complete option set (P0, JOB00048):

```
EBCDIC CHAR60 NOMACRO SOURCE2 NOMACDCK COMP SOURCE NOATR NOXREF NOEXTREF
NOLIST LOAD NODECK FLAGW NOSTMT SIZE=0792192 LINECNT=050 OPT=01
SORMGIN=(002,072) NOEXTDIC NONEST OPLIST SYNCHKT
```

Every one of these keywords is described in GC28-6594-7 (keyword search over
its text: each occurs between 6 and over 1,000 times). No undocumented option.

## Found on the way: a rule PL1C/370 must reproduce

The first P1 runs (JOB00049, JOB00051, JOB00052) ended in
`IHE320I FIXEDOVERFLOW` from the internal function. That was the probe's fault,
and it exposes PL/I (F) semantics worth recording:

- **The return attributes of an internal function are not taken from its
  PROCEDURE statement at the call site.** Without `DECLARE f RETURNS(...)` in
  the invoking block, the compiler assumes the defaults implied by the first
  letter of the name (I–N: FIXED BINARY(15), otherwise FLOAT DECIMAL), "even if
  the function procedure is contained in the invoking procedure" (GC28-8201-4,
  "The RETURNS Attribute", pp. 161–162). It warns with IEM0567I and performs no
  conversion.
- **Arguments are not converted to the parameter attributes either** unless the
  caller declares `ENTRY(...)` with parameter descriptors: the literal `4`
  (FIXED DECIMAL(1)) arrived unconverted in a FIXED BINARY(31) parameter
  (JOB00052: no IEM0567I any more, still FIXEDOVERFLOW). With
  `DCL IDBL ENTRY(FIXED BIN(31)) RETURNS(FIXED BIN(31))` the program runs
  correctly (JOB00053, CC 0000).

Differential tests must therefore declare entries the way PL/I (F) requires, or
deliberately test this behaviour.

## What follows for PL1C/370

- The specification for the oracle part is GC28-8201-4 plus GC28-6594-7; no
  V5.5-specific behaviour has to be worked out beyond them.
- The oracle accepts none of the final-tier statements, as the roadmap assumes.
- The probe harness (`submit.py`) works end to end: submit, wait, collect every
  spool file by step and ddname. It is the seed of the differential test
  driver.
