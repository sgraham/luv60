:: build.bat copied from raddbg

@echo off
setlocal enabledelayedexpansion
cd /D "%~dp0"

:: --- Unpack Arguments -------------------------------------------------------
for %%a in (%*) do set "%%a=1"
if not "%clang%"=="1" if not "%msvc%"=="1" set clang=1
if not "%release%"=="1" set debug=1
if "%debug%"=="1"   set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]
if "%msvc%"=="1"    set clang=0 && echo [msvc compile]
if "%clang%"=="1"   set msvc=0 && echo [clang compile]
if "%~1"==""                     echo [default mode, assuming `luvc` build] && set luvc=1
if "%~1"=="release" if "%~2"=="" echo [default mode, assuming `luvc` build] && set luvc=1

:: --- Unpack Command Line Build Arguments ------------------------------------
set auto_compile_flags=
if "%asan%"=="1"      set auto_compile_flags=%auto_compile_flags% -fsanitize=address && echo [asan enabled]

:: --- Compile/Link Line Definitions ------------------------------------------
set cl_common=     /I..\src /I. /nologo /Zi /W4 /WX /arch:AVX2 /D_CRT_SECURE_NO_WARNINGS
set clang_common=  -I..\src -I. -fdiagnostics-absolute-paths -Wall -Werror -mavx2 -mpclmul -D_CRT_SECURE_NO_WARNINGS
set cl_debug=      call cl /Od /Ob1 /DBUILD_DEBUG=1 %cl_common% %auto_compile_flags%
set cl_release=    call cl /O2 /DBUILD_DEBUG=0 %cl_common% %auto_compile_flags%
set clang_debug=   call "C:\Program Files\LLVM\bin\clang.exe" -g -O0 -DBUILD_DEBUG=1 %clang_common% %auto_compile_flags%
set clang_release= call "C:\Program Files\LLVM\bin\clang.exe" -g -O3 -DBUILD_DEBUG=0 %clang_common% %auto_compile_flags%
set cl_link=       /link /INCREMENTAL:NO /noexp /link /dynamicbase:no
set clang_link=    -fuse-ld=lld -Wl,/dynamicbase:no
set cl_out=        /out:
set clang_out=     -o

:: --- Choose Compile/Link Lines ----------------------------------------------
if "%msvc%"=="1"      set compile_debug=%cl_debug%
if "%msvc%"=="1"      set compile_release=%cl_release%
if "%msvc%"=="1"      set compile_link=%cl_link%
if "%msvc%"=="1"      set out=%cl_out%
if "%clang%"=="1"     set compile_debug=%clang_debug%
if "%clang%"=="1"     set compile_release=%clang_release%
if "%clang%"=="1"     set compile_link=%clang_link%
if "%clang%"=="1"     set out=%clang_out%
if "%debug%"=="1"     set compile=%compile_debug%
if "%release%"=="1"   set compile=%compile_release%

:: --- Prep Directories -------------------------------------------------------
if not exist out mkdir out

if "%no_re2c%"=="1" echo [skipping re2c]
if not "%no_re2c%"=="1" (
  pushd out
  ..\third_party\re2c\win\re2c.exe -W -b -g -i --no-generation-date -o categorizer.c ..\src\categorizer.in.c
  popd
)

if "%no_snip%"=="1" echo [skipping snippets]
if not "%no_snip%"=="1" (
  pushd snip
  python clang_rip.py
  popd
)

pushd out
if "%luvc%"=="1"                     set didbuild=1 && %compile% ..\src\luvc_main.c                %compile_link% %out%luvc.exe || exit /b 1

popd out

:: --- Warn On No Builds ------------------------------------------------------
if "%didbuild%"=="" (
  echo [WARNING] no valid build target specified; must use build target names as arguments to this script, like `m luvc`.
  exit /b 1
)
