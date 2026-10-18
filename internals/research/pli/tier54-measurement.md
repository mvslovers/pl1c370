# Tier 54: entries with RETURNS in a procedure without RETURNS

Found in suc.pli from teampli.net's string routines, which PL/I (F)
compiles. Like the other programs from there, it is not in this
repository. SETSUC has no RETURNS. Its secondary entries FAIL and SUC
return BIT(1):

```
SETSUC: PROC(VAL); ... RETURN;
FAIL:   ENTRY RETURNS(BIT(1)); RETURN(¬PVLSEUS);
SUC:    ENTRY RETURNS(BIT(1)); RETURN(PVLSEUS);
```

Measured 2026-10-13 on MVSTK5-BLD with a probe (JOB00873). It adds a third entry, `COUNT: ENTRY RETURNS(FIXED BIN(15))`,
which returns other attributes than FAIL and SUC. The test `t175suc` is
the probe, identical in JOB00875.

## The rules

- **Each entry point has the RETURNS of its own ENTRY statement.** The
  procedure is called as a subroutine through SETSUC (`RETURN;`) and as
  a function through FAIL, SUC and COUNT.
- **The attributes of the entry entered decide:** COUNT returns N as
  FIXED BIN(15), SUC returns PVLSEUS as BIT(1).
- **The output** (columns shortened):

  ```
  '1'B   '0'B   0
  '0'B   '1'B   1
  '1'B   '0'B   2
  SUCCESS
  NOT FAILED
  ```

  This is after no call of SETSUC, then after `CALL SETSUC('0'B)`, then
  after `CALL SETSUC('1'B)`.
- PL/I (F) writes IEM0567I for each function reference in the main
  procedure, a warning (CC 4).

## In PL1C/370

- **A procedure without RETURNS** takes the RETURNS of its entries. A
  RETURN without a value is an error only when every entry point
  returns a value.
- **Entries with other attributes:** a RETURN with a value stores it for
  the entry entered. Its number was saved at the entry, and the RETURN
  compares it with each entry that returns a value of the expression's
  kind (arithmetic, bit or character). For each, it converts the value
  to that entry's attributes. Under another entry the result is left
  unset, since the RETURN is not meant for it: `RETURN(N)` in COUNT is
  never reached through SUC.

## Not done

- **A BIT(*) or CHAR(*) parameter of a procedure compiled apart**, called
  through an ENTRY declaration with a fixed length (`DCL SETSUC
  ENTRY(BIT(1))`): the caller passes the argument's address, while the
  procedure expects a descriptor. Q1SUC compiled apart (a `.sep` test)
  gave '0'B after `CALL SETSUC('1'B)`, JOB00877. Compiled together,
  PL1C/370 sees the parameter and passes the descriptor.
- **A RETURN whose value is of another kind** than an entry's attributes
  (a bit string for an arithmetic entry) is not converted for that
  entry.
