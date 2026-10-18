#!/usr/bin/env python3
"""Differential test of PL1C/370 against PL/I (F) on MVSTK5-BLD.

    tests/run_mvs.py [name ...]

For every tests/pli/<name>.pli (or the names given):
  1. compile it with build/host/pl1c370 into build/gen/<name>.asm,
  2. keep the module list between the TESTS markers of mbt.toml current,
  3. mbt build --all; delete PL1C370.DEV.LINKLIB and mbt deploy it anew
     (target m38b, or the one MBT_TARGET names: env for a throwaway
     container, with mbt 3's MBT_TARGET_* variables),
  4. one job: per program a PL/I (F) compile-link-go step and a step that
     runs the PL1C/370 module, both printing to data sets,
  5. compare the two print files (ASA byte included, trailing blanks not),
     the REPORT files when the program wrote one, and the condition codes
     of the two steps (PL/I (F) ends a program after ERROR with 2000).
A file tests/pli/<name>.in becomes the SYSIN of both programs, a file
tests/pli/<name>.dd adds its DD statements to both steps, a file
tests/pli/<name>.parm holds PL/I (F)'s compiler PARM (CHAR48 in it gives
PL1C/370 -char48), a file tests/pli/<name>.goparm the program's own PARM
for both runs, a file tests/pli/<name>.xdrv makes the program an external
REXX function: PL/I (F) links it with ENTRY IHESAPB (IHESAPD for OPT=1,
2), PL1C/370 compiles it with -plist, and XDRIVER (tests/mvs/xdriver)
calls both with the arguments the file holds.  A file tests/pli/<name>.abend
says the program ends in an abend: it gets a job of its own, and the
PL1C/370 step runs with COND=EVEN after the PL/I (F) one.  A file
tests/pli/<name>.asmsub names assembler routines (tests/mvs/asmsub) the
program calls: linked into the PL1C/370 module, found by PL/I (F)'s
link-edit in the development library.  A file
tests/pli/<name>.reply holds the operator's replies to the program's
DISPLAY ... REPLY, one a line: the program gets a job of its own, and
the replies are given at the console of m38b in order, to the PL/I (F)
run and then to the PL1C/370 run -- only to WTORs whose text begins
with the test's name (T248DSP), never to another job's.  For a program
with a DISPLAY statement the console messages of both steps (from the
job log) are compared as well.  A file tests/pli/<name>.nosysp runs
both without a SYSPRINT DD: the console messages (code offsets masked)
and the condition codes are compared, and neither may have a print file.
A file
tests/pli/<name>.cc holds the condition code PL1C/370 must end with
where PL/I (F) cannot give it (PLIRETC); the twin's own is then not
compared.  A file
tests/pli/<name>.opt.pli is what PL1C/370 compiles in place of <name>.pli:
a program in the Optimizing Compiler's language beyond PL/I (F), whose
<name>.pli twin spells out the expansion GC33-0009-4 gives for it.
Every other program is compiled with -lang=F, and -lang=F must refuse
each <name>.opt.pli -- unless a file tests/pli/<name>.sem says that the
source is PL/I (F)'s and only its meaning differs in the default
language (the PARM split at '/', for one).
Exit 0 only when every pair is identical.  Results: build/mvs/<jobid>/.
"""
import glob, json, os, re, subprocess, sys, threading, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
sys.path.insert(0, os.path.join(ROOT, "tests"))
import submit  # noqa: E402

BEGIN, END = "# BEGIN TESTS (tests/run_mvs.py)", "# END TESTS"
PER_JOB = 50
MAX_JCL = 150000


def module(name):
    return ("T" + re.sub(r"[^A-Z0-9]", "", name.upper()))[:8]


def run(cmd, env=None):
    r = subprocess.run(cmd, env=env, capture_output=True, text=True)
    if r.returncode != 0:
        sys.stdout.write(r.stdout + r.stderr)
        sys.exit(f"FAILED: {' '.join(cmd)}")
    return r.stdout


def uses_report(n):
    """a FILE(REPORT), DCL REPORT or TITLE('REPORT'), not the word in a
    comment (t152rl)"""
    return re.search(r"FILE\s*\(\s*REPORT\s*\)|DCL\s+REPORT\b|DECLARE\s+REPORT\b|"
                     r"TITLE\s*\(\s*'REPORT'\s*\)",
                     open(f"tests/pli/{n}.pli").read().upper()) is not None


def units(n):
    """<name>.sep: the source's external procedures compiled apart, each
    from the text between its *PROCESS cards, and linked together (the
    main procedure's first: PL/I (F)'s link order).  Else one assembly."""
    if not os.path.exists(f"tests/pli/{n}.sep"):
        return [f"build/gen/{n}.asm"] + asmsubs(n)
    return [f"build/gen/{n}_{k}.asm" for k in range(len(split_units(n)))] + asmsubs(n)


def asmsubs(n):
    """<name>.asmsub: assembler routines the program calls, one source of
    tests/mvs/asmsub a line: linked into the PL1C/370 module, and found by
    PL/I (F)'s link-edit in the development library (their own modules,
    mbt.toml)"""
    p = f"tests/pli/{n}.asmsub"
    return [l.strip() for l in open(p) if l.strip()] if os.path.exists(p) else []


def split_units(n):
    parts, cur = [], []
    for line in open(f"tests/pli/{n}.pli", encoding="utf-8"):
        if line.upper().startswith("*PROCESS"):
            parts.append(cur)
            cur = []
        else:
            cur.append(line)
    parts.append(cur)
    return parts


def update_toml(names, tso=()):
    text = open("mbt.toml").read()
    block = [BEGIN]
    for n in sorted(list(names) + list(tso)):
        srcs = ", ".join(f'"{u}"' for u in units(n)) if n not in tso else \
            f'"build/gen/{n}.asm"'
        block += ["", f"[module.{module(n)}]", f"sources = [{srcs}]",
                  "rent    = false", "reus    = false"]
    block += ["", END]
    if BEGIN in text:
        text = re.sub(re.escape(BEGIN) + r".*?" + re.escape(END), "\n".join(block), text, flags=re.S)
    else:
        text = text.rstrip("\n") + "\n\n" + "\n".join(block) + "\n"
    open("mbt.toml", "w").write(text)


def step_codes(outdir, jobid):
    """The condition code of each program's step: the GO step of a
    PL1LFCLG step (Onn), or its XDRIVER step (OnnX) for an external
    function, the RUN step (Unn).  An abend shows as its code."""
    codes = {}
    for path in glob.glob(os.path.join(outdir, f"{jobid}_*_JESYSMSG.txt")):
        for line in open(path, encoding="latin-1"):
            m = re.search(r"IEF142I \S+ (?:(\S+) )?(\S+) - STEP WAS EXECUTED - COND CODE (\d+)",
                          line)
            if m and re.fullmatch(r"O\d+[XN]", m.group(2)):
                codes[m.group(2)[:-1]] = m.group(3)    # XDRIVER or no SYSPRINT
            elif m and (m.group(2).startswith("U") or m.group(1) == "GO"):
                codes[m.group(2)] = m.group(3)
            m = re.search(r"IEF472I \S+ (?:(\S+) )?(\S+) - COMPLETION CODE (\S+)", line)
            if m and re.fullmatch(r"O\d+[XN]", m.group(2)):
                codes[m.group(2)[:-1]] = "ABEND " + m.group(3)
            elif m and (m.group(2).startswith("U") or m.group(1) == "GO"):
                codes[m.group(2)] = "ABEND " + m.group(3)
    return codes


def parm(n):
    """<name>.parm: PL/I (F)'s compiler PARM for the test, '' for the
    procedure's own"""
    p = f"tests/pli/{n}.parm"
    return open(p).read().strip() if os.path.exists(p) else ""


def xdrv(n):
    """<name>.xdrv: an external REXX function, called by XDRIVER with
    these arguments (tests/mvs/xdriver); None for an ordinary program"""
    p = f"tests/pli/{n}.xdrv"
    return open(p).read().strip() if os.path.exists(p) else None


def replies(n):
    """<name>.reply: the operator's replies to the program's WTORs, in
    order (DISPLAY ... REPLY); [] for none"""
    p = f"tests/pli/{n}.reply"
    return [l.rstrip("\n") for l in open(p) if l.strip()] if os.path.exists(p) else []


def console(cmd):
    r = subprocess.run(["mbt", "target", "console", submit.TARGET, "--", cmd],
                       env=dict(os.environ, MBT_TARGET=submit.TARGET), capture_output=True,
                       text=True, timeout=120)
    return r.stdout + r.stderr


def wtors():
    """the WTORs outstanding on m38b: (id, text) from D R,L (IEE110I,
    then a line 'nn,text' each)"""
    out = console("D R,L")
    return re.findall(r"^\s*(\d\d),(.*?)\s*$", out, re.M)


class Operator(threading.Thread):
    """Gives the replies, in order, to the program's WTORs: those whose
    text begins with the test's name, so that no other job's is ever
    answered."""

    def __init__(self, name, answers):
        super().__init__(daemon=True)
        self.name = name.upper()
        self.answers = list(answers)
        self.done = threading.Event()
        self.log = []

    def run(self):
        while self.answers and not self.done.is_set():
            try:
                mine = [i for i, t in wtors() if t.startswith(self.name)]
                for i in mine[:1]:
                    out = console(f"R {i},{self.answers[0]}")
                    self.log.append(out.strip())
                    # the echo can be lost (JOB01915): taken if the id
                    # is no longer outstanding
                    if f"REPLY TO {i} IS" in out or i not in [w for w, _ in wtors()]:
                        self.answers.pop(0)
            except Exception as e:      # a console timeout must not end the
                self.log.append(repr(e))  # thread: the job would wait (JOB02107)
                mine = []
            if not mine:
                time.sleep(3)


def wto_lines(outdir, jobid):
    """The console messages of each program's step from the job log:
    '+text' a WTO, '@text' a WTOR (its reply id dropped), each given to
    the step whose IEFACTRT line follows it -- the GO step of a PL1LFCLG
    step (Onn), the RUN step (Unn)."""
    steps, pend = {}, []
    for path in glob.glob(os.path.join(outdir, f"{jobid}_*_JESMSGLG.txt")):
        for line in open(path, encoding="latin-1"):
            m = re.match(r"\s*\d+\.\d\d\.\d\d JOB\s*\d+\s(?: (\+)|@\d\d )(.*)$", line)
            if m:
                pend.append(("+" if m.group(1) else "@") + m.group(2).rstrip())
                continue
            m = re.search(r"PLITEST\s+([OU]\d+)(N?)\s+(\S+)\s+.*(RC=|AB )", line)
            if m:
                if m.group(1).startswith("U") or m.group(3) == "GO" or m.group(2):
                    steps[m.group(1)] = pend
                pend = []
    return steps


def abend(n):
    """<name>.abend: the program ends in an abend (in both runs)"""
    return os.path.exists(f"tests/pli/{n}.abend")


def nosysp(n):
    """<name>.nosysp: both programs run without a SYSPRINT DD; their
    console messages and condition codes are compared (#220)"""
    return os.path.exists(f"tests/pli/{n}.nosysp")


def displays(n):
    """the program has a DISPLAY statement"""
    return re.search(r"\bDISPLAY\s*\(", open(f"tests/pli/{n}.pli").read().upper()) is not None


def alone(n):
    """a program that needs a job of its own: an abend, or replies"""
    return abend(n) or bool(replies(n))


def char48(n):
    """PL1C/370's option for CHAR48 in the test's PARM"""
    return ["-char48"] if "CHAR48" in parm(n).upper().split(",") else []


def main():
    all_names = sorted(os.path.basename(p)[:-4] for p in glob.glob("tests/pli/*.pli")
                       if not p.endswith(".opt.pli"))
    names = sys.argv[1:] or all_names
    run(["make", "-s"])
    os.makedirs("build/gen", exist_ok=True)
    for n in all_names:
        # <name>.opt.pli: the same program in the Optimizing Compiler's
        # language, compiled by PL1C/370; PL/I (F) runs <name>.pli, the
        # expansion GC33-0009-4 gives for it
        src = f"tests/pli/{n}.opt.pli" if os.path.exists(f"tests/pli/{n}.opt.pli") else \
            f"tests/pli/{n}.pli"
        # a program PL/I (F) compiles as it stands: in PL/I (F)'s language,
        # so -lang=F, which refuses anything beyond it
        lang = [] if src.endswith(".opt.pli") else ["-lang=F"]
        if os.path.exists(f"tests/pli/{n}.sep"):
            for k, part in enumerate(split_units(n)):
                p = f"build/gen/{n}_{k}.pli"
                open(p, "w", encoding="utf-8").write("".join(part))
                run(["build/host/pl1c370", "-I", "tests/pli/inc"] + lang + [p, "-o",
                     f"build/gen/{n}_{k}.asm"])
            continue
        run(["build/host/pl1c370", "-I", "tests/pli/inc"] + lang + char48(n) +
            (["-plist"] if xdrv(n) is not None else []) + [src, "-o",
             f"build/gen/{n}.asm"])
        if src.endswith(".opt.pli") and not os.path.exists(f"tests/pli/{n}.sem"):
            # and the other way round: -lang=F must refuse it (unless
            # <name>.sem says only its meaning differs in the default)
            r = subprocess.run(["build/host/pl1c370", "-lang=F", "-I", "tests/pli/inc"] +
                               char48(n) + (["-plist"] if xdrv(n) is not None else []) +
                               [src, "-o", os.devnull], capture_output=True, text=True)
            if r.returncode == 0:
                sys.exit(f"{src}: -lang=F accepts it, though it is beyond PL/I (F)")
    # test/tso/<name>.pli: programs for the interactive tests of mbt test
    # --tso (test/tso/*.lua), deployed with the rest as T<NAME>
    tso = sorted(os.path.basename(p)[:-4] for p in glob.glob("test/tso/*.pli"))
    for n in tso:
        run(["build/host/pl1c370", f"test/tso/{n}.pli", "-o", f"build/gen/{n}.asm"])
    update_toml(all_names, tso)
    run(["mbt", "build", "--all"])
    env = dict(os.environ, MBT_TARGET=submit.TARGET)
    # A fresh library every run: replaced members leave dead space, and
    # nothing holds this development library.  Not --reallocate: it grows
    # the request with every run (128 tracks, then 32768, IEF257I).
    try:
        submit.req("DELETE", f"/zosmf/restfiles/ds/{submit.DEVLIB}")
    except Exception:
        pass                      # not there yet
    # Allocated here: mbt 3 allocates a missing library with 20 directory
    # blocks, too few for 140 modules (IEB148I, 2026-10-13).
    submit.req("POST", f"/zosmf/restfiles/ds/{submit.DEVLIB}", json.dumps(
        {"unit": "SYSDA", "dsorg": "PO", "alcunit": "TRK", "primary": 1500,
         "secondary": 500, "dirblk": 100, "recfm": "U", "blksize": 15040,
         "lrecl": 0}).encode(), "application/json")
    out = run(["mbt", "deploy", "--ignore-space"], env)
    if "Deploy complete" not in out:
        sys.exit("deploy did not complete:\n" + out)
    members = submit.req("GET", f"/zosmf/restfiles/ds/{submit.DEVLIB}/member").decode()
    missing = [module(n) for n in names if f'"{module(n)}"' not in members]
    if missing:
        sys.exit(f"not in {submit.DEVLIB}: {missing}")
    # A job has at most 255 steps (IEF602I): a PL1LFCLG step is three, the
    # run one, so a job takes at most PER_JOB programs.  mvsMF before 1.2.0
    # dropped the connection on jobs from about 70 KB on (mvsmf#412); 1.2.0
    # took 150 KB (JOB00694), so a job stays below MAX_JCL bytes.
    outdir = "build/mvs"
    results = []                          # (index, name, jobid)
    jobs = []

    def program_steps(i):
        n = names[i]
        data = f"tests/pli/{n}.in" if os.path.exists(f"tests/pli/{n}.in") else ""
        dd = f"tests/pli/{n}.dd" if os.path.exists(f"tests/pli/{n}.dd") else ""
        gp = f"tests/pli/{n}.goparm"
        goparm = open(gp).read().strip() if os.path.exists(gp) else ""
        x = xdrv(n)
        ns = "NOSYSP" if nosysp(n) else None
        return [[f"O{i:02d}", "PL1LFCLG", f"tests/pli/{n}.pli", parm(n), data, dd, goparm, x,
                 None, "DEVLIB" if asmsubs(n) else None, ns],
                [f"U{i:02d}", "RUN", module(n), "", data, dd, goparm, x,
                 "EVEN" if abend(n) else None, None, ns]]

    start = 0
    while start < len(names):
        end = start + 1
        # a program that ends in an abend gets a job of its own: the abend
        # of the PL/I (F) step flushes every later step without COND=EVEN
        while end < len(names) and end - start < PER_JOB and not alone(names[start]) \
                and not alone(names[end]):
            steps = [st for i in range(start, end + 1) for st in program_steps(i)]
            if len(submit.build("PLITEST", steps, submit.PREFIX, "tests/pli/inc")[0]) > MAX_JCL:
                break
            end += 1
        steps = [st for i in range(start, end) for st in program_steps(i)]
        jcl, printed = submit.build("PLITEST", steps, submit.PREFIX, "tests/pli/inc")
        reports = {f"{p}{i:02d}" for i in range(start, end) if uses_report(names[i])
                   for p in "OU"}
        op = None
        if replies(names[start]):
            # a job of its own: the replies, for the PL/I (F) run and then
            # for the PL1C/370 run
            op = Operator(names[start], replies(names[start]) * 2)
            op.start()
        jobid = submit.run_job("PLITEST", jcl, printed, submit.PREFIX, outdir, reports)
        if op:
            op.done.set()
            if op.answers:
                print(f"{names[start]}: {len(op.answers)} replies not given", *op.log, sep="\n")
        jobs.append(jobid)
        results += [(i, names[i], jobid) for i in range(start, end)]
        start = end
    codes, wtos = {}, {}
    for jobid in jobs:
        codes[jobid] = step_codes(outdir, jobid)
        wtos[jobid] = wto_lines(outdir, jobid)
    failed = 0
    print(f"{'program':10} {'PL/I (F)':10} {'PL1C/370':10} result   ({', '.join(jobs)})")
    for i, n, jobid in results:
        a = os.path.join(outdir, f"{jobid}_O{i:02d}.print")
        b = os.path.join(outdir, f"{jobid}_U{i:02d}.print")
        if nosysp(n):
            # no print file on either side: the console and the codes
            r = subprocess.CompletedProcess([], 0 if not os.path.exists(a) and
                                            not os.path.exists(b) else 1,
                                            "a print file where none should be\n", "")
        else:
            r = subprocess.run([sys.executable, "tests/compare.py"] +
                               (["--math"] if os.path.exists(f"tests/pli/{n}.math") else []) +
                               [a, b], capture_output=True, text=True)
        ok = r.returncode == 0
        ra = os.path.join(outdir, f"{jobid}_O{i:02d}.report")
        rb = os.path.join(outdir, f"{jobid}_U{i:02d}.report")
        # only a program that writes REPORT has one to compare: a data set
        # no program opened has no end of file, and reads as whatever an
        # earlier one left on its tracks (JOB00307)
        has_report = uses_report(n)
        if has_report and (os.path.exists(ra) or os.path.exists(rb)):
            # a second output file, REPORT: compared as plain lines
            rr = subprocess.run([sys.executable, "tests/compare.py", "--plain", ra, rb],
                                capture_output=True, text=True)
            if rr.returncode != 0:
                ok = False
                r = rr
            ba, bb = ra + "bin", rb + "bin"
            if ok and os.path.exists(ba) and os.path.exists(bb) and \
                    open(ba, "rb").read() != open(bb, "rb").read():
                ok = False
                r = subprocess.CompletedProcess([], 1, "REPORT differs in its bytes\n", "")
        wa, wb = wtos[jobid].get(f"O{i:02d}", []), wtos[jobid].get(f"U{i:02d}", [])
        # only for a program that DISPLAYs: the console is no output of
        # the others, and LIBC/370 writes its own messages there (an
        # exhausted heap, t209ans)
        if nosysp(n):
            mask = re.compile(r"( AT OFFSET \+?)[0-9A-F]{5,6}")
            wa = [mask.sub(r"\1*****", l) for l in wa]
            wb = [mask.sub(r"\1*****", l) for l in wb]
        if ok and (displays(n) or nosysp(n)) and wa != wb:
            ok = False
            r = subprocess.CompletedProcess([], 1, "console messages differ:\n" +
                                            "".join(f"- {l}\n" for l in wa) +
                                            "".join(f"+ {l}\n" for l in wb), "")
        ca, cb = codes[jobid].get(f"O{i:02d}"), codes[jobid].get(f"U{i:02d}")
        cc = f"tests/pli/{n}.cc"
        if ok and os.path.exists(cc) and ca is not None:
            # a code PL/I (F) cannot give (PLIRETC): PL1C/370's must be the
            # one the file names
            ca = open(cc).read().strip()
        if ok and (ca is None or ca != cb):
            ok = False
            r = subprocess.CompletedProcess([], 1, f"condition codes differ: {ca} {cb}\n", "")
        failed += not ok
        print(f"{n:10} {'ok' if os.path.exists(a) else 'missing':10} "
              f"{'ok' if os.path.exists(b) else 'missing':10} "
              f"{('CLOSE' if r.stdout.startswith('CLOSE') else 'IDENTICAL') if ok else 'DIFFERS'}"
              f"  {ca}")
        if not ok:
            print(r.stdout + r.stderr)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
