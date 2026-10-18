# Tier 38: REGIONAL(2) and REGIONAL(3)

Measured 2026-10-13 on MVSTK5-BLD. The probes are in each with a `.dd` file: DSORG=DA, RECFM=F, BLKSIZE=20, KEYLEN=4, on 3390
volumes, which hold 57 such records per track.

- Q1R2 (JOB00609)
- Q2R2 (JOB00612; JOB00610 failed on `LENGTH(ONKEY)`, IEM0807I, and
  JOB00611 built its keys wrongly)
- Q3R3 (JOB00610)
- Q4R2 and Q5R3 (JOB00613)

The tests `t136rg2` and `t137rg3` are identical (JOB00624).

## Keys

- **Data set and dummy records.** A REGIONAL(2) or (3) data set holds F
  records with recorded keys (KEYLEN). A dummy record has X'FF' in the
  first byte of its key. BSAM writes it with zeros in the rest of the key
  and the record's number R on its track in the first data byte.
- **The region** is the last eight characters of the source key, or all
  of a shorter key. Digits may have blanks in front of them. A letter,
  or blanks only, is KEY 54:
  - `KEY('AAAA')` and `KEY('AAAAX0000000')` are KEY 54.
  - `KEY('    ')` is KEY 54.
  - A region beyond the data set is KEY 56.
- **The recorded key** is the KEYLEN characters at the left end of the
  source key, padded with blanks if the key is shorter:
  - `'AA00000000'` searches for `AA00` in region 0.
  - `'AAAAAA00000000'` searches for `AAAA` in region 0.
  - `KEYFROM('7')` writes `'7   '` into region 7.
  - `KEYFROM(I)` with I FIXED BINARY(31) 9 has a blank recorded key: the
    key converts to a string with blanks in front of the 9.
- **KEYTO** gets the recorded key, as a string is assigned: `CHAR(12)`
  gets `'AAAA        '`, `CHAR(2)` gets `'AA'`.
- **The REGIONAL(2) region is a record number; the REGIONAL(3) region
  is a track.**

## Searching

- **A READ, REWRITE or DELETE** looks for the recorded key from the
  beginning of the track the region lies on, over the whole data set,
  wrapping around at its end. The first record found with that key is
  taken; duplicates are allowed.
  - Q4R2 has DUPK in regions 5 and 58. `DUPK00000010` finds region 5
    and `DUPK00000100` finds region 58. `DUPK00000170` wraps around to
    region 5.
  - Q3R3: `CCCC00000000` finds CCCC on track 1, and `AAAA00000001` finds
    AAAA on track 0.
  - None found is KEY 51.
- **A DIRECT WRITE** puts its record into the first dummy record from
  the beginning of the region's track on, again over the whole data set
  (Q4R2):
  - X10 lands in region 0, Z60 in region 57 and Y170 in region 114, the
    first record of each track.
  - Sixty records written to region 100 fill track 1, then go on in
    track 2.
  - A full data set is KEY 57 (Q3R3, Q5R3: 169 records fill the 171
    places, since two hold the records written before).
  - Duplicate keys are not refused.
- **DELETE** makes the record a dummy record again. A later WRITE takes
  that place (Q1R2: HHHH into BBBB's region 2).

## Sequential access

- **SEQUENTIAL OUTPUT** writes in ascending regions:
  - REGIONAL(2) puts each record into its own region. The same region a
    second time, or a lower one, is KEY 53, and the record is not
    written (Q1R2: `CCCC00000002` after `BBBB00000002`).
  - REGIONAL(3) takes several records per track. Only a lower track is
    KEY 53.
- **SEQUENTIAL INPUT and UPDATE** read the records in their order on
  the data set and leave the dummy records out. Q1R2's data set of 57
  regions reads as 4 records. REWRITE replaces the record just read.
- **DIRECT OUTPUT** creates the data set with dummy records only.

## In PL1C/370

`rt/pl1bdk.asm` does BDAM with keys, with extended search (OPTCD=E):

- READ DKF, WRITE DK, WRITE DA, WRITE DI and READ DI.
- REGIONAL(2) uses relative blocks (OPTCD=R), REGIONAL(3) TTR.
- LIMCT is the size of the data set (DCBREL). `rt/pl1iokey.c` opens the
  data set once by TTR and once by relative block: the two DCBREL values
  give the records per track.

The operations work as follows:

- **OUTPUT opens** first write dummy records over the whole extent with
  BSAM (`rt/pl1bdam.asm`). SEQUENTIAL OUTPUT then uses WRITE DI to the
  region (REGIONAL(2)) or WRITE DA to the track (REGIONAL(3)).
- **DELETE** finds the record (READ DKF, whose feedback gives its
  address), then writes a dummy record as BSAM makes one, R included.
  WRITE DA puts a record where that byte says. With a 0 there, a
  measured run put the next record over R0 and lost it, and the track
  took no more records.
- **SEQUENTIAL access** reads by relative block, READ DI with the key.

`rt/pl1bdam.asm` had its DECB at +96 in a BDAM DCB of X'68' bytes; it is
at +104 now. The check meant to catch that, `DS 0XL(DECBOFF+1-len)`,
assembles without an error in as370 when the length is negative.

## Not done

- ONKEY (PL/I (F) has it: in Q1R2 it gives the recorded-key part `CCCC`
  of `CCCC00000002`).
- V and U records in REGIONAL(3).
- KEY 52 (no case was found).
- LIMCT from the DD statement.
- A REGIONAL(3) track that overflows during SEQUENTIAL OUTPUT.
