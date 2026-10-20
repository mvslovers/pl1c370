# Builds the PL1C/370 manuals.  Needs typst and the bookmaster submodule
# (git submodule update --init docs/books/bookmaster).

TYPST ?= typst
BM    := bookmaster
# The PDF carries the edition: ml10-0001-0.pdf is the Draft of
# ML10-0001.  Bump the suffix together with the number on the title page.
BOOKS := ml10-0001-0.pdf ml10-0002-0.pdf

.PHONY: all clean

all: $(BOOKS)

ml10-0001-%.pdf: ml10-0001.typ $(wildcard guide/*.typ ex/*/*) $(BM)/bookmaster.typ
	$(TYPST) compile --root . --font-path $(BM)/fonts --ignore-system-fonts $< $@

ml10-0002-%.pdf: ml10-0002.typ $(wildcard ref/*.typ ex/*/*) $(BM)/bookmaster.typ
	$(TYPST) compile --root . --font-path $(BM)/fonts --ignore-system-fonts $< $@

clean:
	rm -f *.pdf
