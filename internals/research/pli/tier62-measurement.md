# Tier 62: the NAME condition and DATAFIELD

GET DATA meeting a name that is not in its list raises NAME(file).
PL1C/370 had only the system action (IHE100I, from Tier 11). It had no
ON NAME, no SIGNAL NAME and no DATAFIELD. Tier 59's comparison listed
DATAFIELD.

Measured 2026-10-14 on MVSTK5-BLD with a probe
and its input (JOB00990). The tests are identical:

| Test | Covers | Job |
|---|---|---|
| `t195nam` | the probe | JOB00994 |
| `t196nms` | SIGNAL NAME and an unknown name without an on-unit, then with one | JOB00992 |

## The rules

- **NAME** is raised once for each unknown name, after its field is read.
  After a normal return of the on-unit the GET goes on with the next
  name. ONCODE is 10, and ONFILE is the file.
- **DATAFIELD** is the whole data field, the name with its subscripts,
  `=`, the value and the delimiter after it:

  | Input | DATAFIELD | Length |
  |---|---|---|
  | `A=1, X=5, B=2;` | `X=5,` | 4 |
  | `Y(2)=7, S='P,Q'` | `Y(2)=7,` | 7 |
  | `ZZZ='AB;C', A=4;` | `ZZZ='AB;C',` | 11 |
  | `QQ=1.5E3 A=6;` | `QQ=1.5E3 ` (the blank) | 9 |
  | `C=1, Z=-2;` | `Z=-2;` | 5 |

- **Out of an on-unit** DATAFIELD is a null string.
- **After SIGNAL NAME(SYSIN)** the on-unit's DATAFIELD is the field the
  last GET DATA read, `A=6;`.
- **Without an on-unit:** the message `IHE100I FILE SYSIN - UNRECOGNISABLE
  DATA NAME`, after a blank line, and the GET goes on. SIGNAL NAME
  without an on-unit gives the same message (`t196nms`).
- `LENGTH(DATAFIELD)` is refused, as `LENGTH(DATE)` is (Tier 60):
  IEM0807I, JOB00989.

## In PL1C/370

- **NAME** is a file condition (`C_NAME`); its SIGNAL code is 10.
- **GET DATA** (`rt/pl1ioget.c`) keeps the start of each field. After
  the value it passes the field and its delimiter to `pl1i_datafield`,
  then raises NAME for an unknown name.
- **The system action** writes IHE100I and returns.
- **An on-unit that writes** (PUT) ended the data mode of the GET. The
  GET sets it again after the condition; found by the first run of
  `t195nam` (JOB00992).

## The suite

All 198 programs are identical: JOB00996 to JOB00999. (Tier 61's ROADMAP
entry said 195; there were 196, corrected here.)

## Not done

- **GET DATA without a data list:** PL1C/370 refuses it, so NAME for a
  name unknown in the block cannot arise.
- **A data field across two records:** only its part in the last record
  is kept (not measured).
- **DATAFIELD in an ERROR or FINISH on-unit** entered by NAME's standard
  system action: NAME's system action never raises ERROR, so this does
  not arise.
