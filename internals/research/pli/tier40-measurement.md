# Tier 40: INDEXED -- blocked records, embedded keys, READ KEY on SEQUENTIAL

Measured 2026-10-13 on MVSTK5-BLD with the probes in each with its `.dd` file. All data sets are DSORG=IS, LRECL=20, KEYLEN=5,
OPTCD=LY:

- B: RECFM=FB, BLKSIZE=100, RKP=0
- E: RECFM=F, unblocked, RKP=4
- C: RECFM=FB, RKP=2

The runs:

- Q1IXB: JOB00642 and JOB00643, IHE112I; JOB00644
- Q2IXB: JOB00645
- Q3IXB: JOB00646
- Q4IXB: JOB00647, which ended S03B: it opened E for DIRECT UPDATE
  before anything had created E
- Q5IXB: JOB00648. Its ERROR on-unit went back to a label that raised
  KEY 55 again and looped until SB37. The lines before the loop are
  used here.

The tests are identical:

- `t139ixb`: JOB00650
- `t140ixe`: JOB00654

## The program's record

- **Blocked, RKP=0.** The key is the front of the logical record. The
  program's record is the rest: LRECL minus KEYLEN, 15 bytes here. A
  20-byte variable is IHE112I, "RECORD VARIABLE LARGER THAN RECORD
  SIZE". WRITE puts KEYFROM in front of it; READ and KEYTO separate the
  two again.
- **RKP > 0, blocked or not.** The key is part of the record, at RKP.
  The program's record is all LRECL bytes. A WRITE whose embedded key
  differs from KEYFROM is KEY 55 (ONKEY: KEYFROM), and nothing is
  written. This holds when the data set is created and for DIRECT
  WRITE.
  - **REWRITE KEY on unblocked records** checks the embedded key the same
    way (Q5IXB: E0001 stays as it was).
  - **REWRITE on blocked records** does not check it. Q4IXB's
    `YYC0009 REWR` rewritten under C0005 reads back with the key C0009.
- **The system action for KEY 55** is `IHE135I FILE C - KEY
  SPECIFICATION ERROR`, then ERROR (Q5IXB).

## Updating blocked records

- **REWRITE KEY on a DIRECT file** must come right after a READ of the
  same key. Otherwise PL/I (F) writes `IHE030I FILE B - REWRITE/DELETE
  NOT IMMEDIATELY PRECEDED BY READ` and raises ERROR with ONCODE 1007
  (Q2IXB, Q3IXB, Q5IXB). These cases are refused:
  - no READ at all;
  - a READ of another key;
  - a second REWRITE after one READ.
- **DELETE with RKP=0** has no byte to mark the record with, since the
  first byte is the key's. DIRECT and SEQUENTIAL DELETE both write
  `IHE029I FILE B - UNSUPPORTED FILE OPERATION` and raise ERROR with
  ONCODE 1009, with or without a READ before it.
- **DELETE with RKP=2** works, without a READ before it as well.

## READ KEY on a SEQUENTIAL file

READ KEY positions the file at the record with that key and reads it. The
READs after it go on from there (`K0012`, then `K0014`). The key need
not lie ahead: `K0004` after `K0014` works.

A key that is not there is KEY 51. The variable is not changed, and the
next READ gives the first record of the data set (Q3IXB: `K0002` after
`K0011`). The same holds for SEQUENTIAL UPDATE, where REWRITE replaces
the record READ KEY read.

## In PL1C/370

`rt/pl1isam.asm`:

- `PL1ISTK` does ESETL, then SETL K. A key not found (DCBEXCD1 X'80',
  which the new SYNAD returns from) gives ESETL and SETL B again.
- `PL1IBIS` returns the address of the logical record (DECB+16).
- Op 4 is a WRITE K in the DECB the READ KU left behind. In a new DECB,
  BISAM looks the record up again by its embedded key, and a rewritten
  key is not found.

`rt/pl1iokey.c` handles the data differently by case:

- **Where the data starts.** It keeps the program's data at
  `idoff = KEYLEN` (blocked with RKP=0) or 0.
- **What PUT and WRITE KN get:**
  - with RKP > 0, the record alone;
  - unblocked with RKP=0, the key and then the data;
  - blocked with RKP=0, the key in front of the data as the logical
    record.
- **READ on DIRECT UPDATE.** A blocked file reads with READ KU and
  remembers the key for the REWRITE.

`pl1i_fileerr` writes the IHE029I and IHE030I messages and raises ERROR.

## Not done

- V records and more than one DD statement.
- GENKEY.
- KEY 57 (no space).
- Positioning with READ KEY on REGIONAL files.
