# Tier 104: the Optimizer's preprocessor

Issue #224, block C. GC33-0009-4 (chapter 16, marked as changed from
PL/I (F)) and SC26-3114-01 ("Preprocessor Built-In Functions"):

- built-in functions beyond SUBSTR: INDEX(x, y), LENGTH(x), COUNTER
  (a 5-character number, 00001 first), COMPILETIME (18 characters,
  `DD MMM YY HH.MM.SS`, a leading zero of the day blank), PARMSET(p)
  ('1' if the procedure's parameter p was given); always active in
  preprocessor statements, in the program's text once %ACTIVATE or
  %DECLARE ... BUILTIN names them;
- an argument may be omitted (PARMSET is then '0'), and in a reference
  from the program's text a null argument counts as omitted;
- `%ACTIVATE name NORESCAN` (or RESCAN, the default): the value is put
  into the text without being scanned again.

## Measured on PL/I (F) V5.5

Probes JOB02273: INDEX and COUNTER are
undeclared identifiers to PL/I (F)'s preprocessor (IEM4130I,
processing terminated), NORESCAN is a syntax error (IEM4265I). All of
it is OPT.

On the way, JOB02280 (Q4FIN): a variable that is not an array with an
INITIAL list of two values, `DCL (B, YY) CHAR(2) INIT('B1', 'Y2')`, is
an error to PL/I (F) (IEM0578I, condition code 8, the INITIAL
ignored). PL1C/370 took the first value; it now refuses it too.

## In PL1C/370

pp.c: the built-in functions (a preprocessor procedure of the same name
wins), omitted arguments and the parameter's `set` flag, NORESCAN on
the variable, the built-in functions in the program's text when
activated or declared BUILTIN. Each is level OPT (`beyond_f`).
check.c: IEM0578I's rule.

## Test

`t261ppb` (`.opt.pli`, its twin the preprocessor's output written
out): INDEX and LENGTH in assignments, NORESCAN against a rescanned
value, COUNTER twice in the text, a procedure with PARMSET called with
two arguments, one, and one omitted, and the length of COMPILETIME's
value. Identical in JOB02278; the full suite 262 of 262 in JOB02282 to
JOB02300.
