# Releasing PL1C/370

A tag `v<version>` runs `.github/workflows/release.yml`: the tag must
match `version` in `mbt.toml`; the release gets the host compiler, the
run time archive, the manuals and their SHA256SUMS.

## Steps

1. `version` in `mbt.toml` set to the release, `TODO.md` and
   `internals/ROADMAP.md` current, the full suite run (`tests/run_mvs.py`).
2. **Books** (`docs/books`): the edition on both title pages
   (`ml10-0001.typ`, `ml10-0002.typ`: `number`, the "This edition applies
   to PL1C/370 x.y.z" line, the date) and the `BOOKS` names in
   `docs/books/Makefile` and `docs/books/web/build.sh` say the same
   edition; `make -C docs/books` builds without errors. The release
   attaches `ml10-0001-<edition>.pdf` and `ml10-0002-<edition>.pdf`
   (artifact `pl1c370-books`) with their SHA256SUMS lines.
3. Tag, push the tag, then rework the release page (root `CLAUDE.md`,
   "Git and release texts").
