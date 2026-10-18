# Tier 39: ONKEY

Measured 2026-10-13 on MVSTK5-BLD: a probe (JOB00632), with a REGIONAL(1), a REGIONAL(2) (KEYLEN=4) and an INDEXED
(KEYLEN=5) data set. The test `t138oky` is the same program, and it is
identical (JOB00634).

## The rules

ONKEY gives the key of the record that raised the condition, in the
on-unit of KEY or RECORD. Anywhere else it gives a null string: before
any condition, after an on-unit has returned, and in the on-units of
`SIGNAL KEY` and of ENDFILE. What it gives depends on the organisation:

| File | Source key | ONKEY |
|---|---|---|
| REGIONAL(1) | `'X7'` (KEY 54) | `'X7      '`: blanks fill it to eight |
| | `'AB00000099999'` (KEY 56) | `'00099999'`: the last eight characters |
| | `12345`, FIXED BINARY(31) | `'   12345'` |
| | `'3'`, into CHAR(5) (RECORD 21) | `'3       '` |
| REGIONAL(2) | `'AAAA00000001'` (KEY 51) | `'AAAA'`: the recorded key, KEYLEN characters |
| | `'AA00000000'` | `'AA00'` |
| | `'AAAAX'` (KEY 54) | `'AAAA'` |
| | `'7'` | `'7   '` |
| | `3`, FIXED BINARY(31) | `'    '` |
| INDEXED, creation | `'K0001'` again (KEY 52) | `'K0001'` |
| | `'K0000XYZ'` (KEY 53) | `'K0000XYZ'`: the whole source key |
| INDEXED, DIRECT | `'ZZ'` (KEY 51) | `'ZZ   '` |
| | `'K0009ABCDEF'` | `'K0009'` |

In PL/I (F), `LENGTH(ONKEY)` is refused: IEM0807I says argument 1 of
LENGTH is an entry name, and the statement is deleted. Assigned to a
VARYING variable first, its length is as shown above. PL1C/370 takes
`LENGTH(ONKEY)` all the same.

## In PL1C/370

`rt/pl1iokey.c` gives ONKEY before each keyed operation (`pl1_setkey`)
and takes it back after it. `rt/pl1cond.c` copies it into the on-unit's
activation when KEY or RECORD is raised; `ONKEY` reads the activation
running.

## Not done

- ONKEY for REGIONAL(3), which is taken to be as REGIONAL(2) (not
  measured).
- ONKEY in an ERROR on-unit that follows a KEY condition.
- ONKEY in an on-unit called from a KEY on-unit.
