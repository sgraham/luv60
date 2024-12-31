#!/bin/bash

# build.sh copied from raddbg, ported to old bash (no -v)

set -eu
cd "$(dirname "$0")"

# --- Unpack Arguments --------------------------------------------------------
for arg in "$@"; do declare $arg='1'; done
if [ ! -z ${gcc+x} ];     then clang=1; fi
if [ ! -z ${release+x} ]; then debug=1; fi
if [ -z ${debug+x} ];     then echo "[debug mode]"; fi
if [ -z ${release+x} ];   then echo "[release mode]"; fi
if [ -z ${clang+x} ];     then compiler="${CC:-clang}"; echo "[clang compile]"; fi
if [ -z ${gcc+x} ];       then compiler="${CC:-gcc}"; echo "[gcc compile]"; fi

# --- Unpack Command Line Build Arguments -------------------------------------
auto_compile_flags=''

# --- Compile/Link Line Definitions -------------------------------------------
clang_common="-I../src/ -g -Wall"
clang_debug="$compiler -g -O0 -DBUILD_DEBUG=1 ${clang_common} ${auto_compile_flags}"
clang_release="$compiler -g -O2 -DBUILD_DEBUG=0 ${clang_common} ${auto_compile_flags}"
clang_link="-lpthread -lm -lrt -ldl"
clang_out="-o"
gcc_common="-I../src/ -Wall"
gcc_debug="$compiler -g -O0 -DBUILD_DEBUG=1 ${gcc_common} ${auto_compile_flags}"
gcc_release="$compiler -g -O2 -DBUILD_DEBUG=0 ${gcc_common} ${auto_compile_flags}"
gcc_link="-lpthread -lm -lrt -ldl"
gcc_out="-o"

# --- Choose Compile/Link Lines -----------------------------------------------
if [ -z ${gcc+x} ];     then compile_debug="$gcc_debug"; fi
if [ -z ${gcc+x} ];     then compile_release="$gcc_release"; fi
if [ -z ${gcc+x} ];     then compile_link="$gcc_link"; fi
if [ -z ${gcc+x} ];     then out="$gcc_out"; fi
if [ -z ${clang+x} ];   then compile_debug="$clang_debug"; fi
if [ -z ${clang+x} ];   then compile_release="$clang_release"; fi
if [ -z ${clang+x} ];   then compile_link="$clang_link"; fi
if [ -z ${clang+x} ];   then out="$clang_out"; fi
if [ -z ${debug+x} ];   then compile="$compile_debug"; fi
if [ -z ${release+x} ]; then compile="$compile_release"; fi

# --- Prep Directories --------------------------------------------------------
mkdir -p out

# --- Build & Run Metaprogram -------------------------------------------------
pushd out
../third_party/re2c/mac/re2c -W -b -g -i --no-generation-date -o categorizer.c ../src/categorizer.in.c
popd

# --- Build Everything (@build_targets) ---------------------------------------
pushd out
if [ -z ${luvc+x} ];                  then didbuild=1 && $compile ../src/luvc_main.c               $compile_link $out luvc; fi
popd

# --- Warn On No Builds -------------------------------------------------------
if [ ! -z ${didbuild+x} ]
then
  echo "[WARNING] no valid build target specified; must use build target names as arguments to this script, like \`./m luvc\`."
  exit 1
fi
