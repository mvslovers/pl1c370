#import "../bookmaster/bookmaster.typ": *

= Installing PL1C/370 <pg-install>

#idx("installation")
PL1C/370 needs the CC/370 toolchain and LIBC/370 2.15.0 or later in the
CC/370 system root. Each release on GitHub carries the compiler for
Linux and macOS and the archive #cmd("pl1c370-")_version_#cmd("-lib.tar.gz"),
which holds #cmd("lib/pl1rt.a") and #cmd("include/pl1abi.h").

From a source tree, #cmd("make install") copies the compiler to
#cmd("bin/pl1c370") and the run time to #cmd("cc370/lib/pl1rt.a") and
#cmd("cc370/include/pl1abi.h") below #cmd("PREFIX"), by default
#cmd("~/.local").

// Draft: verifying the installation follows.
