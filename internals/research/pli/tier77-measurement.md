# Tier 77: GENERIC with WHEN

ANSI X3.53-1976 writes a generic name's members as `name WHEN
(descriptions)` (CM36 to CM41). PL/I (F)'s form is `name ENTRY(...)`,
measured in Tier 67. The selection rules differ too:

- **PL/I (F):** the member whose descriptors match exactly. No member,
  or more than one, is an error (IEM1065I, IEM1066I).
- **ANSI** (4.4.5.11):
  - the members are tried in order, and the first that matches is
    taken;
  - a description matches when the argument has every attribute it
    names (4.4.5.14);
  - a precision may be a range, `FIXED DEC(1:5)` (4.4.5.15);
  - `*` matches any argument;
  - a member without WHEN serves a reference without arguments.

The two forms cannot be confused, because WHEN marks the ANSI one. So
both are kept: PL/I (F)'s form by F's rules, the WHEN form by the
standard's.

Beyond the oracle. The test `t218gen` is a twin pair, identical in
JOB01179. Its twin calls the member ANSI's rules select. `t205gen` and
`t206gen`, PL/I (F)'s form, are unchanged and identical in the same
job.

## What `t218gen` shows

| Argument | Member | Why |
|---|---|---|
| FIXED BIN(15) | SFB | `FIXED BINARY`, after SAR, which needs an array |
| FIXED DEC(5,2) | SFD | `FIXED DEC(1:5)`: precision 5 is in the range |
| FIXED DEC(7) | SFE | out of SFD's range, and FLOAT does not match; `DEC` does |
| FLOAT DEC(6) | SFL | `FLOAT`, the first that matches |
| CHAR(3) | SCH | `CHAR NONVARYING` |
| CHAR(5) VARYING | SVA | not NONVARYING; `VARYING` |
| an array of FIXED BIN(15) | SAR | `(*) FIXED`, the first member |
| POINTER | SAN | `*` |
| no arguments, `CALL SHOW;` | SNO | the member without WHEN |
| `('Q', I)` and `(I, 'Q')` | T2S, T2N | `WHEN(CHAR, *)` first, then `WHEN(*, *)` |

Order matters. Put first, `WHEN(DEC)` takes the FLOAT DEC(6) argument
as well, as the compiler showed when the test was first written.
ANSI's rule means exactly that.

## In PL1C/370

- **parse.c** (`gen_when`, `gen_prec`) reads the descriptions into
  `struct gdesc`: the attributes, the precision ranges and the asterisk
  bounds.
- **check.c** (`gdesc_match`): every attribute named must be one the
  argument has.
  - An array argument without DIMENSION in the description is tested
    by its elements (4.4.5.13, Step 2).
  - `gen_select` takes the first matching member when any member has
    WHEN.

## Not done

- **Descriptions with levels** (STRUCTURE, MEMBER): refused.
- **ENTRY with descriptors, and RETURNS, in a description:** refused.
  ENTRY alone matches any entry value.

## The suite

All 220 programs are identical: JOB01182 to JOB01190, every other job
number a cleanup job of the print data sets (`submit.py`, PR #159).
