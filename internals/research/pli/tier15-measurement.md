# Tier 15 rules, measured against PL/I (F) V5.5

Measured 2026-10-12 on MVSTK5-BLD. Probe (JOB00259), which is also the test program `tests/pli/t81ovf`; `t82ovsa`
and `t83ovla` overflow with no on-unit at all, short and long, and end
with step code 2000 as PL/I (F) does (JOB00272). With Tiers 0 to 14: 85
programs identical (JOB00269, JOB00270, JOB00272).

## OVERFLOW in arithmetic

A floating-point result beyond 7.2E75 is an exponent overflow, which the
machine cannot mask; PL/I (F) catches it in its program interruption
exit and raises OVERFLOW, ONCODE 300 (JOB00259):

- without an on-unit: `IHE300I OVERFLOW AT OFFSET ...`, then ERROR;
- after a normal return of the on-unit the program goes on after the
  instruction, with the value the machine left: the characteristic
  wrapped round, `7E75 * 7E75` short is `3.65458E-03`, `7E75 + 7E75`
  `1.04416E-78`, `-C - C` long `-1.044167702368028E-78`; `A * A * 2`
  raises it once, for the product, and goes on with the wrapped value;
- `(NOOVERFLOW):` on the statement: no condition, the same value;
- an overflow inside the on-unit enters the on-unit again, recursively
  (six levels in the probe, until its count reached 13, then six
  returns).

## The exit

`rt/pl1pie.asm`. The main procedure's prologue calls PL1PION, which
issues SPIE for interruptions 12 (exponent overflow) and 15
(floating-point divide). The PICA names no maskable interruption, so the
program mask stays 0 as before (FIXEDOVERFLOW and SIZE by condition code,
an exponent underflow giving 0, Tier 7); the layout is the SPIE macro's
(program-mask byte from codes 8, 10, 13, 14; then the mask halfword, bit 0
the extension, bits 1 to 15 the codes: X'0009').

As PL/I (F)'s IHEERRA does (Library PLM GY28-6801, chapter 6), the exit
only keeps the PIE (the old PSW and registers 14 to 2) and changes the
address in its PSW: the supervisor resumes at PL1PITR, outside the exit,
with every register as it was. PL1PITR saves registers 3 to 13 and the
floating-point registers in a frame from the NAB and calls
pl1_program_check, which raises the condition. If that returns, PL1PITR
restores them and divides by zero on purpose; the exit recognises the
address after that DE and puts the program's PSW and registers 14 to 2
back into the PIE. So the program resumes with all sixteen registers, the
floating-point registers, the condition code and the program mask as they
were, the only way problem state allows. An interruption inside an
on-unit takes the next frame from the NAB.

The PIE's PSW is in the BC form: the interruption code in its bytes 2-3,
the address in bytes 5-7 (the exit's test of that address is what every
normal return goes through, so t81ovf checks it).

NOOVERFLOW is a second bit (2) in the run time's flag byte that
NOCONVERSION already had (gen.c NOCV): the condition is the run time's to
test, at the interruption.

A message's offset is taken from the interruption's address when it lies
in the current block's code, from the block's last call otherwise.

## In the library routines

EXP's deliberate overflow (`LE` of the largest value, `AER 0,0`), patched
into an error call in Tier 13, is PL/I (F)'s instruction again: the exit
raises OVERFLOW from it. A floating-point divide exception, which only a
routine meets (the generated code tests every divisor), is ZERODIVIDE,
ONCODE 320: `IHE330I`, then ERROR (PL/I (F), JOB00253; no test of
PL1C/370 reaches it, see below).

After a normal return of ON OVERFLOW from EXP inside SINH or COSH, the
routine goes on with the result EXP did not store. Both compilers pass a
temporary for it (PL/I (F): `WS1.1` in the DSA, then `MVC B(4),WS1.1`;
listing of t82hyov, JOB00264), and what it holds is undefined: PL/I (F)
then meets ZERODIVIDE in short SINH(200) and a second OVERFLOW in long
COSH(200) (JOB00253), PL1C/370 other interrupts from other contents. Not
a test for that reason; the probe Q1HYOVF (JOB00253) kept the case.

## Not done

- OVERFLOW in a conversion: PL1C/370's conversions are emulated in C
  (`rt/pl1flt.c`, Tier 7) and meet no interruption.
- NOZERODIVIDE for a divide inside a library routine: the run time has
  no flag for it, so the exit raises ZERODIVIDE regardless.
- UNDERFLOW in addition and subtraction (Tier 7), with the program mask
  still 0.

JOB00266 lost its last twelve programs to the oracle: PL/I (F)'s compile
step ended `IEA703I 106-F ... MODULE ACCESSED IEMRA` (no storage to load a
compiler phase); the same programs ran identical in JOB00269/JOB00270.
