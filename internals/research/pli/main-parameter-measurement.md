# What a main procedure's parameter receives

The question comes from `TODO.md` item 1: BREXX/370 calls an external
function written in PL/I (F) (`~/repos/mvs/foo`, RXCOMM), whose main
procedure takes a POINTER parameter, `PROCEDURE(EFPL_PTR)
OPTIONS(MAIN)`.

Measured 2026-10-14 on MVSTK5-BLD:
- `RXDIAG.pli`, RXCOMM's declarations, prints the parameter and the
  two words under it;
- `DRIVER.asm` LINKs it twice, with R0 = 0 and the parameter as
  BREXX/370 passed it at two times;
- `run.py` builds the job: PL/I (F), IEWL, IFOX00, IEWL, GO.

## The rule

PL/I (F) hands the main procedure the PARM as a string. IHESAPA
(GY28-6801-6, "Entry point IHESAPA: to pass a PARM parameter") takes
the first word of the list R1 points to as the address of the PARM
area, a halfword length and then the characters. It builds a string
dope vector:
- the address of the characters, that is the area's address + 2;
- the maximum length 100 and the current length.

A POINTER parameter lies over that SDV, so it receives the area's
address **+ 2**.

| Call | R1 | `EFPL_PTR` | Result | Job |
|---|---|---|---|---|
| list -> A(EFPL) | standard OS form | A(EFPL) + 2 = 679086 | EFPL read 2 bytes off: IHE803I PROTECTION VIOLATION | JOB01232 |
| list -> A(EFPL - 2) | BREXX/370, 2020-10-30 (5a19f34: `tmp = tmp - 2`) to 2022-11-29; tags V2R3M0 to V2R5M1 | A(EFPL) = 679084 | ARGNUM 2, ARG(1) HELLO, EVLEN 8, EVDATA `OK-HELLO` | JOB01233 |
| R1 = A(EFPL), the EFPL itself | BREXX/370 since d0093c6 (2022-11-29) | 2: EFPLCOM 0, + 2 | no EFPL | JOB01232, JOB01233 |

The second word under the parameter is X'0064' and the halfword before
the area: the SDV's maximum length 100 and the current length.

## Consequences

- **For PL1C/370:** a main procedure's parameter gets IHESAPA's SDV.
  - A CHAR(100) VARYING parameter is the PARM.
  - A POINTER parameter gets the PARM area's address + 2.
  - PL1C/370 has to do the same (TODO.md, the tier for items 1 and 2).
- **For BREXX/370:** RXPI and RXCUT work only with the form of
  2020-2022 (tags V2R3M0 to V2R5M1): an OS list whose first word is
  A(EFPL) - 2. Since d0093c6 (V2R5M2 on),
  R1 points at the EFPL itself, as TSO/E REXX passes it. A PL/I (F)
  program then gets EFPL_PTR = 2.
  - Whether BREXX/370 goes back to that form, or RXCOMM takes the EFPL
    another way, is brexx370's decision.
  - The measurement went to the brexx370 session.

## Normal parameter conventions: IHESAPB, IHESAPD (JOB01267)

GC28-6594-7 (chapter 15, pages 205-207) names further entry points:
- the usual one is IHESAPA, and for OPT=1 and 2 IHESAPC; both treat R1
  as the PARM;
- IHESAPB (OPT=0) and IHESAPD (OPT=1 and 2) follow "normal parameter
  conventions": R1 "is not examined ... but simply passed on to the main
  procedure".

The idea comes from `~/repos/mvs/foo/README.md`. Measured with
which builds four modules and calls them
from one driver, `DRIVER4.asm`:

| Module | OPT | Link | Called with | Result |
|---|---|---|---|---|
| RXDA | 0 | IHENTRY (IHESAPA) | list -> A(EFPL) - 2 | ARGNUM 2, `OK-HELLO`, RC 0 |
| RXDB | 0 | `ENTRY IHESAPB` | R1 -> EFPL, as TSO/E REXX and BREXX/370 today | ARGNUM 2, `OK-HELLO`, RC 0 |
| RXDC | 2 | IHENTRY (IHESAPC) | list -> A(EFPL) - 2 | ARGNUM 2, `OK-HELLO`, RC 0 |
| RXDD | 2 | `ENTRY IHESAPD` | R1 -> EFPL | ARGNUM 2, `OK-HELLO`, RC 0 |

The driver ended with 15, one bit for each call that returned EVLEN 8,
the data `OK-HELLO` and R15 0.

**With IHESAPB or IHESAPD the EFPL is an ordinary parameter list**
(`RXDIAGB.pli`):
- the main procedure takes six POINTER parameters, P1 to P6;
- `ADDR(P5)` is the argument table (EFPLARG's value is the address of
  the fifth parameter);
- P6 is the evaluation block's address (EFPLEVAL points at a word that
  holds it);
- P1 to P4 must not be touched.

So a PL/I (F) external function works with today's BREXX/370 and with
TSO/E REXX, without a change to the interpreter. It is linked with ENTRY
IHESAPB, or IHESAPD for OPT=1 and 2, and uses an RXCOMM written for that
list.
