# Tier 43: area arguments of another size, UNSPEC beyond 32 bits

Measured 2026-10-13 on MVSTK5-BLD. The probe
ran as JOB00710; JOB00709 was a first form that declared an OFFSET
relative to a non-BASED area (IEM0606I, condition code 12). The test
`t144adm` is that probe, and it is identical (JOB00713). `t145uns`, the
UNSPEC test, is identical too (JOB00715).

## Area arguments

GC28-8201-4 calls for a dummy when an area argument has another size
than an AREA(n) parameter. PL/I (F) V5.5 makes none (Q1ADUM):

- **AREA(100) passed to an AREA(200) parameter.** In the procedure the
  area's control word reads size 100 and extent 24. An ALLOCATE there
  lands in the caller's A: back in the caller its extent is 32, and the
  caller's X still holds 11.
- **AREA(300) passed to the same parameter.** The procedure sees size
  300.
- **AREA(100) passed to an AREA(50) parameter.** An allocation of 80
  bytes succeeds, since A has the room. The parameter's declared size
  limits nothing.

So an area is always passed by reference and keeps its own size, which
is what PL1C/370 did for AREA(*) and for equal sizes. It now takes any
size: the refusal is gone.

## UNSPEC

UNSPEC of a variable of more than 32 bits is its bytes as a bit string.
`t145uns` covers:

- CHAR(6)
- a long FLOAT
- a FIXED DECIMAL(9,2)
- a BIT(40)
- an AREA, its control information included
- assignment to BIT(64)
- SUBSTR
- LENGTH, which is 48 for CHAR(6)
- comparison with `UNSPEC('ABC123')`

A string expression also works. The longest is MAXBITS (4096); a
structure or an expression of another type is refused. In gen.c it is
`gen_bits`: the variable's address and 8 times its size.
