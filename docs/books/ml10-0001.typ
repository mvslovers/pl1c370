#import "bookmaster/bookmaster.typ": *

#show: book.with(
  title: "PL1C/370 PL/I Compiler for MVS 3.8j",
  subtitle: "Programmer's Guide",
  short-title: "PL1C/370 Programmer's Guide",
  product: "PL1C/370",
  number: "ML10-0001-0",
  date: "October 2026",
  authors: ("Mike Großmann",),
  edition: [
    #text(font: head-font, weight: "bold", size: 11pt)[Draft (October 2026)]

    This edition applies to PL1C/370 0.4.0 and to all subsequent
    releases and modifications until otherwise indicated in new editions.

    *Draft.* Most chapters are outlines; the text follows with the
    releases of the compiler.

    Comments on this book may be addressed to the issue tracker of
    the mvslovers/pl1c370 repository on GitHub.

    © Copyright Mike Großmann 2026. All rights reserved.
  ],
)

#part("index.html", title: [PL1C/370 Programmer's Guide])[
#titlepage()
#contents()
#figures()

#heading(numbering: none)[About This Book] <about>

This book shows how to compile, link and run PL/I programs for MVS 3.8j
with PL1C/370 (pl1c370), a PL/I compiler that runs on a macOS or Linux
workstation: how to install it, how a PL/I source becomes a load module,
how the load module reaches MVS, and how to run it and find out what went
wrong.

The language itself is described in the companion volume, the
_PL1C/370 Language Reference_\; this book uses only what a task needs.

== Who Should Use This Book

This book is for programmers who write PL/I programs for MVS 3.8j. It
assumes that you know PL/I, and that you can submit a job and read its
output on MVS.

== How This Book Is Organized

#deflist(width: 1.35in,
  [Chapter 1], [“Introducing PL1C/370”: the compiler, its run time, and
    the path from a source to a running program.],
  [Chapter 2], [“Installing PL1C/370”.],
  [Chapter 3], [“Compiling and Linking”.],
  [Chapter 4], [“Running Programs on MVS”.],
  [Appendix A], [“Migrating from PL/I (F)”.],
)

== Related Publications

#deflist(width: 1.35in,
  [ML10-0002], [_PL1C/370 Language Reference_],
  [ML01-0001], [_CC/370 User's Guide_],
)

#mainmatter()
]
#set page(numbering: "1")

#part("pg-intro.html", include "guide/pg-intro.typ")
#part("pg-install.html", include "guide/pg-install.typ")
#part("pg-compile.html", include "guide/pg-compile.typ")
#part("pg-run.html", include "guide/pg-run.typ")

#show: appendices
#part("pg-migrate.html", include "guide/pg-migrate.typ")

#part("index-terms.html", title: [Index])[
#heading(numbering: none)[Index]
#make-index()
]
