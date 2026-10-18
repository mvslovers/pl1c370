#!/usr/bin/env python3
"""Submit a probe job to MVSTK5-BLD through mvsMF and collect its output.

    submit.py [--print-ds PREFIX] JOBNAME OUTDIR STEP ...

Each STEP is one of

    NAME:PL1LFC:SOURCE[:PARM]          PL/I (F) compile only
    NAME:PL1LFCLG:SOURCE[:PARM[:DATA]] PL/I (F) compile, link and go
    NAME:RUN:MODULE[::DATA[:DDS]]      run MODULE from PL1C370.DEV.LINKLIB

PARM replaces the compiler PARM (PARM.PL1L).  DATA is a file whose lines
become the SYSIN of the program (GO step or RUN step).  Credentials come from
~/repos/mvs/mvs38src/.env (MVSTK5_BLD_CRED); MVSTK5-BLD is the system
~/repos/mvs/mvs38src/tools/systems.json marks as accepting work.  Every spool
file is written to OUTDIR/<jobid>_<n>_<step>_<ddname>.txt.

The spool loses the ASA control byte (mvsMF returns text lines in every
mode), so SKIP(n), PAGE and overprinting are invisible there.  With
--print-ds, the SYSPRINT of every PL1LFCLG GO step and every RUN step goes to
the cataloged data set PREFIX.NAME instead (deleted first, created anew,
RECFM=VBA,LRECL=125,BLKSIZE=129 as PL/I (F) makes it).  A data set read as
text keeps the control byte as the first character of each line; it is
saved to OUTDIR/<jobid>_<NAME>.print.  Each such step also gets a DD
REPORT on the data set PREFIX.NAMER (RECFM=VB,LRECL=125), saved
to OUTDIR/<jobid>_<NAME>.report when the program wrote it.

A step may run without a SYSPRINT DD (the build() field "NOSYSP",
tests/run_mvs.py's <name>.nosysp).

A file SOURCE.dd beside a PL1LFCLG source holds further DD statements
for its GO step (a REGIONAL data set, say); a RUN step takes such a file
as a fifth field.
"""
import base64, http.client, json, os, sys, time, urllib.parse, urllib.request

HOST = "http://mvsdev.lan:8085"
ENV = os.path.expanduser("~/repos/mvs/mvs38src/.env")
DEVLIB = "PL1C370.DEV.LINKLIB"
PRINT_DCB = "DCB=(RECFM=VBA,LRECL=125,BLKSIZE=129)"


def cred():
    for l in open(ENV):
        if l.startswith("MVSTK5_BLD_CRED="):
            return l.strip().split("=", 1)[1]
    sys.exit("no MVSTK5_BLD_CRED in " + ENV)


AUTH = {"Authorization": "Basic " + base64.b64encode(cred().encode()).decode(),
        "X-CSRF-ZOSMF-HEADER": "1"}


def req(method, path, data=None, ctype=None):
    h = dict(AUTH)
    if ctype:
        h["Content-Type"] = ctype
    # mvsMF sometimes drops the connection on the first request after a
    # large deploy (2026-10-13, mvslovers/mvsmf#412): try again, three
    # times at most -- but only GET and DELETE.  A PUT that dropped may
    # already have been taken (a job submitted), and sent again it would
    # run twice.
    tries = 4 if method in ("GET", "DELETE") else 1
    for attempt in range(tries):
        r = urllib.request.Request(HOST + path, data=data, headers=h, method=method)
        try:
            with urllib.request.urlopen(r, timeout=120) as f:
                return f.read()
        except (http.client.RemoteDisconnected, ConnectionResetError):
            if attempt == tries - 1:
                raise
            sys.stderr.write(f"connection dropped ({method} {path[:40]}), again\n")
            time.sleep(30)


def card(line):
    if len(line) > 71:
        sys.exit("card too long: " + line)
    return line


def print_dd(ddname, dsn):
    return [card(f"//{ddname} DD DSN={dsn},DISP=(NEW,CATLG),"),
            "//             UNIT=SYSDA,SPACE=(TRK,(5,5)),",
            card(f"//             {PRINT_DCB}")]


def report_dd(ddname, dsn):
    # PL/I (F) needs a block size for a new STREAM file that is not PRINT
    # (IHE153I otherwise, JOB00101).
    return [card(f"//{ddname} DD DSN={dsn},DISP=(NEW,CATLG),"),
            "//             UNIT=SYSDA,SPACE=(TRK,(5,5)),",
            "//             DCB=(DSORG=PS,RECFM=VB,LRECL=125,BLKSIZE=129)"]


def data_dd(ddname, path):
    lines = [card(f"//{ddname} DD *")]
    for l in open(path):
        l = l.rstrip("\n")
        if len(l) > 80:
            sys.exit("data line over 80 columns: " + l)
        lines.append(l)
    return lines + ["/*"]


def inclib(incdir):
    """IEBUPDTE: the %INCLUDE members of incdir (<member>.pli) into the
    temporary library &&INCLIB, which the compile steps take as
    PL1L.SYSLIB (fixed records of 80, blocked by 5 at most: GC28-6594)."""
    jcl = ["//INC EXEC PGM=IEBUPDTE,PARM=NEW",
           "//SYSPRINT DD SYSOUT=*",
           "//SYSUT2 DD DSN=&&INCLIB,DISP=(NEW,PASS),UNIT=SYSDA,",
           "//             SPACE=(TRK,(5,5,5)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=400)",
           "//SYSIN DD DATA,DLM=@@"]
    for f in sorted(os.listdir(incdir)):
        if not f.endswith(".pli"):
            continue
        jcl.append(f"./ ADD NAME={f[:-4].upper()}")
        for l in open(os.path.join(incdir, f)):
            l = l.rstrip("\n")
            if len(l) > 80:
                sys.exit("include line over 80 columns: " + l)
            jcl.append(l)
    jcl += ["./ ENDUP", "@@"]
    return jcl


def extra_dds(path, step):
    """The DD statements of path (a .dd file beside the source), for the
    GO step of PL1LFCLG (step "GO.") or a RUN step (step "")."""
    lines = []
    if path and os.path.exists(path):
        for l in open(path):
            l = l.rstrip("\n")
            if step and l.startswith("//") and not l.startswith("// "):
                l = "//" + step + l[2:]
            lines.append(card(l))
    return lines


def build(jobname, steps, prefix, incdir=None):
    jcl = [f"//{jobname} JOB (PL1C370),'PL1C370 PROBE',CLASS=A,MSGCLASS=H,",
           "//             MSGLEVEL=(1,1)"]
    includes = incdir and any(s[1] in ("PL1LFCLG", "PL1LFC") and
                              "%INCLUDE" in open(s[2]).read().upper() for s in steps)
    if includes:
        jcl += inclib(incdir)
    printed = [s[0] for s in steps if prefix and s[1] in ("PL1LFCLG", "RUN")]
    if printed:
        jcl.append("//DEL EXEC PGM=IEFBR14")
        for n, name in enumerate(printed):
            for suffix, tag in (("", "D"), ("R", "R")):
                jcl += [card(f"//{tag}{n} DD DSN={prefix}.{name}{suffix},DISP=(MOD,DELETE),"),
                        "//             UNIT=SYSDA,SPACE=(TRK,1)"]
    for name, kind, arg, *rest in steps:
        dsn = f"{prefix}.{name}" if name in printed else None
        parm = rest[0] if rest and rest[0] else None
        data = rest[1] if len(rest) > 1 and rest[1] else None
        dds = rest[2] if len(rest) > 2 and rest[2] else (
            os.path.splitext(arg)[0] + ".dd" if kind == "PL1LFCLG" else None)
        # the program's own PARM (a sixth field): EXEC PARM, PARM.GO
        goparm = rest[3] if len(rest) > 3 and rest[3] else None
        # an external REXX function (a seventh field: XDRIVER's arguments):
        # called by tests/mvs/xdriver, R1 at the EFPL
        xdrv = rest[4] if len(rest) > 4 else None
        # '2*' before the arguments: XDRIVER LINKs to the program twice
        twice = ""
        if xdrv is not None and xdrv.startswith("2*"):
            twice, xdrv = "2*", xdrv[2:]
        if kind == "RUN" and xdrv is not None:
            arg, goparm = "XDRIVER", twice + (f"{arg},{xdrv}" if xdrv else arg)
        # COND=EVEN for a RUN step (an eighth field): it runs after an
        # abend of an earlier step too
        even = len(rest) > 5 and rest[5] == "EVEN"
        # PL/I (F)'s link-edit also autocalls from the development library
        # (a seventh field after the program's arguments: assembler
        # routines a test calls, tests/mvs/asmsub)
        devlib = len(rest) > 6 and rest[6] == "DEVLIB"
        # no SYSPRINT DD at all (an eighth field after them, "NOSYSP"):
        # a PL1LFCLG step's program then runs in a step of its own, NnnN,
        # the procedure's GO step having one (#220)
        nosysp = len(rest) > 7 and rest[7] == "NOSYSP"
        if nosysp:
            dsn = None
        if kind == "RUN":
            jcl += [card(f"//{name} EXEC PGM={arg}" +
                         (f",PARM='{goparm}'" if goparm else "") +
                         (",COND=EVEN" if even else "")),
                    f"//STEPLIB DD DISP=SHR,DSN={DEVLIB}"]
            if not nosysp:
                jcl += print_dd("SYSPRINT", dsn) if dsn else ["//SYSPRINT DD SYSOUT=*"]
            if dsn:
                jcl += report_dd("REPORT", dsn + "R")
            if data:
                jcl += data_dd("SYSIN", data)
            jcl += extra_dds(dds, "")
            continue
        src = open(arg).read()
        uses_inc = includes and "%INCLUDE" in src.upper()
        uses_pp = any(l.lstrip().startswith("%") for l in src.split("\n"))
        if (uses_inc or uses_pp) and not parm:
            parm = "LOAD,NODECK,MACRO"      # the preprocessor: % statements
        skipgo = ",COND.GO=(0,LE)" if (xdrv is not None or nosysp) and kind == "PL1LFCLG" \
            else ""
        if parm:
            jcl.append(card(f"//{name} EXEC {kind},PARM.PL1L='{parm}'" +
                            (f",PARM.GO='{goparm}'" if goparm else "") + skipgo))
        else:
            jcl.append(card(f"//{name} EXEC {kind}" +
                            (f",PARM.GO='{goparm}'" if goparm else "") + skipgo))
        # An object data set of its own: after a failed compile the
        # link-edit is skipped and does not delete &&LOADSET, so the next
        # step's link-edit would take the failed deck too, with its entry
        # point first (JOB00137).
        jcl += [card(f"//PL1L.SYSLIN DD DSN=&&O{name},DISP=(MOD,PASS),"),
                "//             UNIT=SYSDA,SPACE=(80,(250,100))"]
        jcl.append("//PL1L.SYSIN DD *")
        jcl += [card(l.rstrip("\n")) for l in open(arg)]
        jcl.append("/*")
        if uses_inc:
            jcl.append("//PL1L.SYSLIB DD DSN=&&INCLIB,DISP=(OLD,PASS)")
        if kind == "PL1LFCLG":
            # A load library of its own: two PL1LFCLG steps in one job
            # would share &&GOSET, and the second link-edit cannot store
            # GO (IEW0421), so the second GO runs the first program.
            if devlib:
                # before SYSLMOD: overrides go in the procedure's order
                jcl += [card(f"//LKED.SYSLIB DD DISP=SHR,DSN={DEVLIB}"),
                        "//        DD DISP=SHR,DSN=SYS1.PL1LIB"]
            jcl.append(card(f"//LKED.SYSLMOD DD DSN=&&G{name}(GO)"))
            jcl.append(card(f"//LKED.SYSLIN DD DSN=&&O{name},DISP=(OLD,DELETE)"))
            if xdrv is not None:
                # R1 to the main procedure unchanged: normal parameter
                # conventions, IHESAPB, for OPT=1 and 2 IHESAPD (JOB01267);
                # then XDRIVER calls GO in place of the GO step
                opt2 = parm and any(o.strip().upper() in ("OPT=1", "OPT=2")
                                    for o in parm.split(","))
                jcl += ["//        DD *", f" ENTRY {'IHESAPD' if opt2 else 'IHESAPB'}", "/*",
                        card(f"//{name}X EXEC PGM=XDRIVER,PARM='{twice}GO{',' + xdrv if xdrv else ''}',"),
                        card(f"//             COND=((9,LT,{name}.PL1L),(9,LT,{name}.LKED))"),
                        f"//STEPLIB DD DISP=SHR,DSN={DEVLIB}",
                        card(f"//        DD DSN=&&G{name},DISP=(OLD,PASS)")]
                jcl += print_dd("SYSPRINT", dsn) if dsn else ["//SYSPRINT DD SYSOUT=*"]
                if dsn:
                    jcl += report_dd("REPORT", dsn + "R")
                if data:
                    jcl += data_dd("SYSIN", data)
                jcl += extra_dds(dds, "")
                continue
            if nosysp:
                jcl += [card(f"//{name}N EXEC PGM=GO" + (f",PARM='{goparm}'" if goparm else "") +
                             ","),
                        card(f"//             COND=((9,LT,{name}.PL1L),(9,LT,{name}.LKED))"),
                        card(f"//STEPLIB DD DSN=&&G{name},DISP=(OLD,PASS)")]
                if data:
                    jcl += data_dd("SYSIN", data)
                jcl += extra_dds(dds, "")
                continue
            jcl += print_dd("GO.SYSPRINT", dsn) if dsn else ["//GO.SYSPRINT DD SYSOUT=*"]
            if dsn:
                jcl += report_dd("GO.REPORT", dsn + "R")
            if data:
                jcl += data_dd("GO.SYSIN", data)
            jcl += extra_dds(dds, "GO.")
    return "\n".join(jcl) + "\n", printed


def run_job(jobname, jcl, printed, prefix, outdir, reports=None):
    """Submit jcl, wait, save every spool file and print data set; return
    the job id."""
    os.makedirs(outdir, exist_ok=True)
    open(os.path.join(outdir, jobname + ".jcl"), "w").write(jcl)
    d = json.loads(req("PUT", "/zosmf/restjobs/jobs", jcl.encode("latin-1"), "text/plain"))
    jobid = d["jobid"]
    print("submitted", jobname, jobid, flush=True)
    for _ in range(240):
        st = json.loads(req("GET", f"/zosmf/restjobs/jobs/{jobname}/{jobid}"))
        if st.get("status") == "OUTPUT":
            break
        time.sleep(5)
    print("status", st.get("status"), "retcode", st.get("retcode"))
    files = json.loads(req("GET", f"/zosmf/restjobs/jobs/{jobname}/{jobid}/files"))
    for n, f in enumerate(files):
        name = f"{jobid}_{n:02d}_{f.get('stepname') or ''}_{f['ddname']}.txt"
        try:
            txt = req("GET", f"/zosmf/restjobs/jobs/{jobname}/{jobid}/files/{f['id']}/records")
        except urllib.error.HTTPError as e:
            print("  spool file", name, "not readable: HTTP", e.code)
            continue
        open(os.path.join(outdir, name), "wb").write(txt)
    for name in printed:
        for suffix, ext in (("", "print"), ("R", "report")):
            # REPORT of a step whose program never opens it has no end of
            # file: read only those that may have one (reports=None: all)
            if ext == "report" and reports is not None and name not in reports:
                continue
            dsn = f"{prefix}.{name}{suffix}"
            try:
                t = req("GET", "/zosmf/restfiles/ds/" + urllib.parse.quote(dsn))
            except urllib.error.HTTPError as e:
                if ext == "print":
                    print("  no print data set", dsn, e.code)
                continue
            open(os.path.join(outdir, f"{jobid}_{name}.{ext}"), "wb").write(t)
            if ext == "report":
                # the bytes as well: a text read can hide binary differences
                r = urllib.request.Request(HOST + "/zosmf/restfiles/ds/" + urllib.parse.quote(dsn),
                                           headers=dict(AUTH, **{"X-IBM-Data-Type": "binary"}))
                with urllib.request.urlopen(r, timeout=120) as fb:
                    open(os.path.join(outdir, f"{jobid}_{name}.reportbin"), "wb").write(fb.read())
    # off the spool once saved: held output filled MVSTK5-BLD's spool
    # (2026-10-13, $HASP355) and stopped every job on it
    try:
        req("DELETE", f"/zosmf/restjobs/jobs/{jobname}/{jobid}")
    except urllib.error.HTTPError:
        pass
    if printed:
        cleanup(jobname, printed, prefix)
    return jobid


def cleanup(jobname, printed, prefix):
    """Delete the print and REPORT data sets once read: left standing they
    made 898 data sets under HERC01.PL1C, and every catalog listing of
    HERC01 on MVSTK5-BLD took 8 to 16 seconds (2026-10-14)."""
    name = (jobname[:7] + "C")[:8]
    jcl = [f"//{name} JOB (PL1C370),'PL1C370 CLEANUP',CLASS=A,MSGCLASS=H,",
           "//             MSGLEVEL=(1,1)"]
    dds = [f"{prefix}.{n}{suffix}" for n in printed for suffix in ("", "R")]
    for k in range(0, len(dds), 200):          # at most 255 DDs to a step
        jcl.append(f"//DEL{k // 200} EXEC PGM=IEFBR14")
        for n, dsn in enumerate(dds[k:k + 200]):
            jcl += [card(f"//D{n} DD DSN={dsn},DISP=(MOD,DELETE),"),
                    "//             UNIT=SYSDA,SPACE=(TRK,1)"]
    d = json.loads(req("PUT", "/zosmf/restjobs/jobs", ("\n".join(jcl) + "\n").encode("latin-1"),
                       "text/plain"))
    for _ in range(60):
        st = json.loads(req("GET", f"/zosmf/restjobs/jobs/{name}/{d['jobid']}"))
        if st.get("status") == "OUTPUT":
            break
        time.sleep(5)
    if st.get("retcode") != "CC 0000":
        print("cleanup", d["jobid"], st.get("status"), st.get("retcode"), "-- kept on the spool")
        return
    try:
        req("DELETE", f"/zosmf/restjobs/jobs/{name}/{d['jobid']}")
    except urllib.error.HTTPError:
        pass


def main():
    args = sys.argv[1:]
    prefix = None
    if args[:1] == ["--print-ds"]:
        prefix, args = args[1], args[2:]
    jobname, outdir, specs = args[0], args[1], args[2:]
    steps = [s.split(":", 6) for s in specs]
    jcl, printed = build(jobname, steps, prefix)
    # REPORT only of the programs that use it: one never opened has no end
    # of file, and reading such a data set through mvsMF is suspected of
    # damaging the running server (mvslovers/mvsmf#425); a RUN step's
    # source is not at hand, so its REPORT is not read
    reports = {name for name, kind, arg, *rest in steps
               if kind == "PL1LFCLG" and "REPORT" in open(arg).read().upper()}
    run_job(jobname, jcl, printed, prefix, outdir, reports)

if __name__ == "__main__":
    main()
