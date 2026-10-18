# Tier 63: the REFER option

`DCL 1 S BASED(P), 2 N FIXED BIN(31), 2 A(M REFER(N))`: ALLOCATE sets N
from M, and every reference takes A's upper bound from N, the N of the
same allocation. PL/I (F) allows one REFER in a declaration, for the last
element of a based structure: the upper bound of its leading dimension,
a string length or an area size (GC28-8201-2, "Self-defining Data").
Tier 59's comparison listed REFER.

Measured 2026-10-14 on MVSTK5-BLD with the probes:

| Probe | Job | What it showed |
|---|---|---|
| Q1REF with halfwords, two versions | before JOB01002 (the outputs were not kept) | ended with IHE803I |
| Q2V1 to Q2V3 | JOB01002 | the halfword case, isolated |
| Q1REF with fullwords | JOB01003 | the rules below |

The tests are identical:

| Test | Covers | Job |
|---|---|---|
| `t197ref` | Q1REF with fullwords | JOB01007 |
| `t198rfs` | READ SET of records whose bound differs | JOB01009 |

## The rules

- **ALLOCATE** sets the member from the variable: M = 5 gives N = 5,
  with HBOUND and DIM 5 and LBOUND 1.
- **Every reference takes the member's current value:**
  - after `N = 3`, `PUT LIST(A)` writes three elements, `SUM(A)` is
    their sum (`1.400000000E+01`) and HBOUND is 3;
  - for `CHAR(K REFER(L))`, after `L = 3`, C is `ABC` with LENGTH 3;
  - `C = 'XY'` then assigns three characters, `XY ` with the blank;
  - after `L = 7`, C is `XY DEFG`, the old characters behind them.
- **Each allocation has its own bound:** P->N = 5 and P2->N = 2.
  `P2->A = 7` assigns two elements, and `P->A(5)` keeps its value.
- **The bound in a minor structure:** `F(0:J REFER(D.E), 2)` with J = 2
  sets E to 2. HBOUND(F,1) is 2, and F has six elements.
- **READ SET** takes the bound from the record. Records with N = 2, 4
  and 1 give two, four and one elements (`t198rfs`).
- **HBOUND and DIM** print as FIXED BINARY(15), though N is FIXED
  BINARY(31).

## PL/I (F) with halfword variables

With N, M and the elements FIXED BINARY(15), PL/I (F) ends with
`IHE803I PROTECTION VIOLATION` when it reads the last element:
- `PUT LIST(A)` writes four of five elements;
- a loop over A(I) fails at A(5).

Q2V1 (JOB01002) isolates it: halfword N, M and elements fail. FIXED
BINARY(31) elements (Q2V2) and CHAR(4) elements with fullword N and M
(Q2V3) work. The tests therefore use fullwords. PL1C/370 generates the
halfword case as it does the fullword one (LH and STH); no test runs it.

## In PL1C/370

- **parse.c:** `x REFER(y)` is read in a bound and in a CHAR length.
- **resolve.c** checks PL/I (F)'s rules:
  - x is a FIXED BINARY variable outside the structure;
  - y is a FIXED BINARY member of it;
  - the element is the last of a based structure.
- **The layout** holds one unit: one element of the leading dimension,
  or one character. A string is then typed with the length 256, as a
  CHAR(*) parameter is. ALLOCATE adds (x - base) units to the size and
  stores x into y.
- **lower.c:** the REFER bound is a run-time bound. Loops, SUM and
  HBOUND/DIM take y under the pointer of the reference (P2->A: P2->N).
- **gen.c:** the length of a REFER string is loaded from y, at its
  offset from the string in the same allocation.
- **Pointer-qualified arrays:** `P2->A = 7` and `PUT LIST(P2->A)` were
  refused before this tier ("cannot be assigned as a whole yet"). They
  now go through the loops of Tier 61's cross sections. A qualified
  structure is still refused.

## The suite

All 200 programs are identical: JOB01011 to JOB01014.

## Not done

- **REFER for an AREA size.**
- **ALLOCATE IN an area** of a structure with REFER.
- **Record I/O:** INTO, FROM and LOCATE of a structure with REFER
  (refused). PL/I (F) transmits the length the current value gives.
  The refusal was added after the full run; no test reaches it.
- **SUBSCRIPTRANGE** on a REFER bound: the check compares with the one
  element laid out (not measured).
- **A REFER string** as an argument, a GET target or the STRING option.
