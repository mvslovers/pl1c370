# Tier 85: abnormal termination by the system

PL/I (F)'s IHESAP issues "a STAE macro instruction to intercept abnormal
termination by the system" (GC28-6594-7, p. 204). PL1C/370 had none: an
abend ended the program without a word, and a full data set did not
even abend.

## Measured on PL/I (F)

Probe: an ON ERROR and an ON FINISH unit,
then PUT FILE(OUT) into a data set of one track without secondary space.

- JOB01328 and JOB01330 (the second with another job name, to tell the
  job from the program): SYSPRINT gets, after an empty line,
  `IHE012I ABNTEST  ABENDED AT OFFSET 00009C FROM ENTRY POINT Q1ABN WITH
  CC D37 (SYSTEM)`. The job name stands first, padded to eight. Neither
  on-unit runs; the abend goes on, and the step ends SD37 (IEC031I
  D37-04).
- GC28-6594-7, App. K: `jjj ABENDED [IN STMT nnnnn] [AT OFFSET xxxxxx]
  [FROM ENTRY POINT ppp] WITH CC xxx {(SYSTEM)|(USER)}`. (USER) was not
  measured: the probe has no way to issue a user abend.
- JOB01343: the CPU time limit (S322, TIME=(,1)) gives nothing at all on
  PL/I (F)'s SYSPRINT, not even the line written before. Its STAE is not
  driven for S322, so a time-out is no test of it.

## In PL1C/370

- **The ESTAE** (rt/pl1cond.c): the main procedure's prologue calls
  PL1STON after PL1PION. It creates an ESTAE through libc370's
  `__estae`; the exit (a C function on a stack of its own, with the
  SDWA) writes IHE012I in PL/I (F)'s form and returns 0, so the abend
  goes on.
  - The offset is taken as for the other system messages: the PSW of
    the last PRB (SDWANXTP) when it lies in the current PL/I block,
    otherwise the return point of the block's last call.
  - SYSPRINT is closed in the exit, not flushed: after fflush the last
    block was not on the data set once the abend went on (JOB01354),
    after fclose it was (JOB01357).
  - At the end of the program (atexit) the ESTAE is deleted.
  - Two guards, from the libc370 session's review: with SDWACLUP set (the
    task is being ended) the exit does no C run-time work; and when
    SYSPRINT itself is the file that filled, it writes no message (PL/I
    (F) writes it on the console then, not done) and closes SYSPRINT
    with libc370's `__fabandon`, which drops the write that failed.
    Neither guard has a test.
- **The SPIE is given back** (rt/pl1pie.asm, PL1PIOF): PL1PION keeps the
  caller's PICA, and the end of the program restores it. Before, a
  program LINKed to (an external REXX function) left its SPIE behind,
  pointing into a module deleted after it returned.
- **A full data set abends** (rt/pl1io.c, `write_check`): libc370 turns
  an x37 into ENOSPC (its x37 exit), and into EIO when the data set has
  its 16 extents already (IFG0554T then drives SYNAD without the exit;
  the libc370 session's reading of the 3.8j source). The run time
  checks every line and record it writes and abends itself with
  SVC 13: D37 for ENOSPC, B37 for EIO, until libc370 tells them apart
  (`__fx37`, planned for libc370 2.12.0). Corrected after the libc370
  session measured MVS itself with IEBDG: no secondary space is D37-04,
  secondary space used up is B37-04 (not E37, as first assumed here);
  a PDS member whose secondary space is used up abends E37-04 in MVS
  directly, before libc370's exit (libc370#543), and the ESTAE takes
  that like any other abend.
- **The runner:** `<name>.abend` gives a test that abends a job of its
  own, and the PL1C/370 step `COND=EVEN`: without it the abend of the
  PL/I (F) step flushes every later step of the job.

## Tests

- `t231abn`: the probe as a test. Identical in JOB01357, and with the
  guards in JOB01375 (compare.py masks IHE012I's six-digit offset as
  well).
- `t226fct`, `t227cut`, `t228pi` (LINKed by XDRIVER) stay identical
  with the SPIE given back (JOB01338); the full suite: see ROADMAP.

## Not done

- (USER) abends: the format is the manual's, not measured.
- Done since (libc370 2.12.0, 2026-10-15): the code comes from
  `__fx37` -- D37-04 or B37-04 -- and a data set at its 16th extent
  gives ENOSPC too (libc370#538). `t238b37` (secondary space used up)
  is identical in JOB01428: SB37 in both runs.
- `IN STMT nnnnn` in IHE012I: not measured when PL/I (F) writes it.
