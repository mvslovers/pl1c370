# Tier 98: the LINKed case

Issue #220, block B: a program LINKed to more than once in one address
space, R15 as its return code, and a run without a SYSPRINT DD.

## Measured on PL/I (F) V5.5

- **The return code of a program PL/I (F) compiles** comes from
  `CALL IHESARC(n)` (IHETSAC for a task), declared `ENTRY(FIXED
  BINARY(31,0))` (GC28-6594-7, "Return Codes"). PL1C/370 did not know
  the name; it is now PLIRETC's.
- **LINKed twice** (`t250lnk`, XDRIVER with `'2*'`, JOB02080): each call
  is a fresh copy of the module (a STATIC counter is 1 both times), R15
  is IHESARC's value (5), the EVALBLOCK carries the result. The second
  call's PUT opens SYSPRINT for output again, so only it is left in the
  data set, on both compilers. Identical.
- **LINKed twice without a PUT** (`t251lnq`, JOB02083): PL/I (F) never
  opens SYSPRINT, and both of XDRIVER's reports remain; a PL1C/370
  program lost the first, because LIBC/370's start-up opened SYSPRINT
  for output, which empties it. LIBC/370 2.15.0 opens the standard
  streams on first use (asked of the libc370 session): identical in
  JOB02109.
- **No SYSPRINT DD** (probe JOB02085):
  PL/I (F) answers the first PUT with `IEC130I SYSPRINT DD STATEMENT
  MISSING` and IHE150I on the console, as `IHE150I jobname  FILE
  SYSPRINT - CANNOT BE OPENED (INCORRECT OR NO DD CARD) AT OFFSET ...`
  in lines of 72 characters, and ends with 2000. PL1C/370 ended with 0:
  LIBC/370 had allocated a SYSOUT data set for SYSPRINT itself.

## In PL1C/370

- **resolve.c:** `CALL IHESARC(n)` and `CALL IHETSAC(n)` of a declared
  external entry are PLIRETC.
- **pl1io.c:** no SYSPRINT DD, asked of the DD chain (`get_dsab`, not
  of stdout, whose flags would open it on LIBC/370 2.15.0): a PUT raises
  UNDEFINEDFILE (IHE150I, then ERROR), and every message of the run time
  goes to the console instead (rt/pl1cons.c: route code 11, descriptor
  code 7, the job name after the message number, 72 characters a line).
  A message with no PUT before it (ZERODIVIDE's IHE330I) is written
  there without UNDEFINEDFILE, as PL/I (F) does.
- **The test runner:** `<name>.nosysp` runs both programs without a
  SYSPRINT DD (the PL/I (F) program in a step of its own, the
  procedure's GO step having one) and compares their console messages,
  code offsets masked, and condition codes; XDRIVER's PARM `'2*...'`
  (from `<name>.xdrv`) LINKs twice.

## Dummy arguments (#266)

Reported from a learning project: `CALL P((X))` passed X itself, the
parser having dropped the parentheses; SC26-3114-01 ("Dummy
Arguments") makes it a dummy, and so does PL/I (F). And an argument to
an internal procedure whose attributes differ from the parameter's was
refused, with a message suggesting an ENTRY declaration an internal
procedure cannot have. The default language now makes it a dummy of the
parameter's attributes (SC26-3114-01: "for internal entry constants,
the attributes are derived from the parameter declarations"); PL/I (F)
passes it unconverted, and `-lang=F` refuses it with that said.

## Tests

`t250lnk` (LINKed twice, IHESARC), `t251lnq` (LINKed twice, no PUT),
`t252nsp` (a PUT without SYSPRINT), `t253nsz` (ZERODIVIDE, CONVERSION
and ERROR without SYSPRINT), `t254arg` (dummy arguments): identical in
JOB02109 and JOB02111, on LIBC/370 2.15.0; the full suite 255 of 255
in JOB02114 to JOB02132.
