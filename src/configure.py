import base64
import glob
import json
import os
import sys

ROOT_DIR = os.path.normpath(
    os.path.join(os.path.abspath(os.path.dirname(__file__)), "..")
)

COMMON_FILELIST = [
    "../third_party/ir/ir.c",
    "../third_party/ir/ir_cfg.c",
    "../third_party/ir/ir_check.c",
    "../third_party/ir/ir_dump.c",
    "../third_party/ir/ir_emit.c",
    "../third_party/ir/ir_gcm.c",
    "../third_party/ir/ir_mem2ssa.c",
    "../third_party/ir/ir_patch.c",
    "../third_party/ir/ir_ra.c",
    "../third_party/ir/ir_save.c",
    "../third_party/ir/ir_sccp.c",
    "../third_party/ir/ir_strtab.c",
    "arena.c",
    "base_mac.c",
    "base_win.c",
    "lex.c",
    "parse_code_gen.c",
    "parse_syntax_check.c",
    "str.c",
    "token.c",
    "type.c",
]

LUVC_FILELIST = [
    "luvc_main.c",
]

GEN_IR_FOLD_HASH_FILELIST = [
    "../third_party/ir/gen_ir_fold_hash.c",
]

UNITTEST_FILELIST = [
    "test_main.c",
    "lex_test.c",
    "str_test.c",
    "type_test.c",
]

LEXBENCH_FILELIST = [
    "lex_bench.c",
]

CLANG_CL_WIN = "C:\\Program Files\\LLVM\\bin\\clang-cl.exe"
LLD_LINK_WIN = "C:\\Program Files\\LLVM\\bin\\lld-link.exe"
CLANG = "clang"

DEBUG_DEFINES = "-DIR_DEBUG -D_DEBUG -DBUILD_DEBUG=1 -D_CRT_SECURE_NO_DEPRECATE"
RELEASE_DEFINES = "-DNDEBUG -DBUILD_DEBUG=0 -D_CRT_SECURE_NO_DEPRECATE"

WIN_COMMON_CC_FLAGS = "/showIncludes /nologo /FS /Zi /W4 /WX $extra -mavx2 -mpclmul -Wno-unused-parameter /I$src /I. /c $in /Fo$out /Fd:$out.pdb"
WIN_COMMON_LD_FLAGS = "/nologo /DEBUG $in /out:$out /pdb:$out.pdb"

CONFIGS = {
    "w": {
        "d": {
            "COMPILE": f"{CLANG_CL_WIN} /Od {DEBUG_DEFINES} {WIN_COMMON_CC_FLAGS}",
            "LINK": f"{LLD_LINK_WIN} {WIN_COMMON_LD_FLAGS}",
            "ML": f"{CLANG_CL_WIN} /nologo /D_CRT_SECURE_NO_WARNINGS /wd4132 /wd4324 $in /link /out:$out",
        },
        "r": {
            "COMPILE": f"{CLANG_CL_WIN} -flto -fuse-ld=lld /O2 {RELEASE_DEFINES} {WIN_COMMON_CC_FLAGS}",
            "LINK": f"{LLD_LINK_WIN} /ltcg /OPT:REF /OPT:ICF {WIN_COMMON_LD_FLAGS}",
            "ML": CLANG_CL_WIN
            + " /nologo /D_CRT_SECURE_NO_WARNINGS /wd4132 /wd4324 $in /link /out:$out",
        },
        "p": {
            "COMPILE": f"{CLANG_CL_WIN} -flto -fuse-ld=lld /O2 /DTRACY_ENABLE=1 {RELEASE_DEFINES} /I$src/../third_party/tracy/public/tracy {WIN_COMMON_CC_FLAGS}",
            "LINK": f"{LLD_LINK_WIN} /ltcg /OPT:REF /OPT:ICF {WIN_COMMON_LD_FLAGS}",
            "ML": CLANG_CL_WIN
            + " /nologo /D_CRT_SECURE_NO_WARNINGS /wd4132 /wd4324 $in /link /out:$out",
        },
        "a": {
            "COMPILE": f"{CLANG_CL_WIN} -fsanitize=address /Od {DEBUG_DEFINES} {WIN_COMMON_CC_FLAGS}",
            "LINK": f"{CLANG_CL_WIN} $in -fsanitize=address /link /out:$out /pdb:$out.pdb",
            "ML": CLANG_CL_WIN
            + " /nologo /D_CRT_SECURE_NO_WARNINGS /wd4132 /wd4324 $in /link /out:$out",
        },
        "__": {
            "exe_ext": ".exe",
            "obj_ext": ".obj",
        },
    },
    "m": {
        # Temporarily using Rosetta until aarch64 aggregate passing can be resolved.
        "d": {
            "COMPILE": f"{CLANG} -MMD -MF $out.d -arch x86_64 -O0 -g {DEBUG_DEFINES} -Wall -Werror $extra -Wno-unused-parameter -I$src -I. -c $in -o $out",
            "LINK": CLANG + " -arch x86_64 -g $in -o $out",
            "ML": CLANG + " -arch x86_64 $in -o $out",
        },
        "r": {
            "COMPILE": f"{CLANG} -MMD -MF $out.d -arch x86_64 -flto -O3 -g {RELEASE_DEFINES} -Wall -Werror $extra -Wno-unused-parameter -I$src -I. -c $in -o $out",
            "LINK": CLANG + " -arch x86_64 -g $in -o $out",
            "ML": CLANG + " -arch x86_64 $in -o $out",
        },
        "__": {
            "exe_ext": "",
            "obj_ext": ".o",
        },
    },
}


def get_tests():
    tests = {}
    files = glob.glob(os.path.join("test", "*.luv")) + glob.glob(
        os.path.join("test", "**", "*.luv")
    )
    for test in files:
        test = test.replace("\\", "/")
        run = "{self}"
        ret = "0"
        out = ""
        err = ""
        disabled = []
        run_prefix = "# RUN: "
        ret_prefix = "# RET: "
        err_prefix = "# ERR: "
        out_prefix = "# OUT: "
        disabled_linux_prefix = "# DISABLED_LINUX"
        disabled_win_prefix = "# DISABLED_WIN"
        disabled_mac_prefix = "# DISABLED_MAC"
        disabled_prefix = "# DISABLED"
        imported_prefix = "# IMPORTED"
        ret_set = False
        with open(test, "r", encoding="utf-8") as f:
            for l in f.readlines():
                if l.startswith(run_prefix):
                    run = l[len(run_prefix) :].rstrip()
                elif l.startswith(ret_prefix):
                    ret = l[len(ret_prefix) :].rstrip()
                    ret_set = True
                elif l.startswith(err_prefix):
                    err += l[len(err_prefix) :].rstrip() + "\n"
                elif l.startswith(out_prefix):
                    out += l[len(out_prefix) :].rstrip() + "\n"
                elif l.startswith(disabled_linux_prefix):
                    disabled.append('linux')
                elif l.startswith(disabled_win_prefix):
                    disabled.append('win')
                elif l.startswith(disabled_mac_prefix):
                    disabled.append('mac')
                elif l.startswith(disabled_prefix) or l.startswith(imported_prefix):
                    disabled.extend(['linux', 'win', 'mac'])

            def sub(t):
                t = t.replace("{self}", test)
                spaces = len(test) * " "
                return t.replace("{ssss}", spaces)

            if not disabled and not ret_set and not out and not err:
                print("Nothing being tested in %s?" % test)
                sys.exit(1)
            tests[test] = {
                "run": sub(run),
                "ret": int(ret),
                "out": sub(out),
                "err": sub(err),
                "disabled": disabled
            }

    return tests


def generate(platform, config, settings, cmdlines, tests):
    root_dir = os.path.join("out", platform + config)
    if not os.path.isdir(root_dir):
        os.makedirs(root_dir)

    exe_ext = settings["exe_ext"]
    obj_ext = settings["obj_ext"]

    luvcexe = "luvc" + exe_ext
    miniluaexe = "minilua" + exe_ext
    gen_ir_fold_hash_exe = "gen_ir_fold_hash" + exe_ext

    with open(os.path.join(root_dir, "build.ninja"), "w", newline="\n") as f:
        f.write("src = ../../src\n")
        f.write("\n")
        f.write("rule cc\n")
        f.write("  command = " + cmdlines["COMPILE"] + "\n")
        f.write("  description = CC $out\n")
        f.write("  deps = " + ("msvc" if platform == "w" else "gcc") + "\n")
        if platform != "w":
            f.write("  depfile = $out.d")
        f.write("\n")
        f.write("rule link\n")
        f.write("  command = " + cmdlines["LINK"] + "\n")
        f.write("  description = LINK $out\n")
        f.write("\n")
        f.write("rule mlbuild\n")
        f.write("  command = " + cmdlines["ML"] + "\n")
        f.write("  description = CC $out\n")
        f.write("\n")
        f.write("rule dynasm_w\n")
        f.write(
            "  command = ./minilua"
            + exe_ext
            + " $src/../third_party/ir/dynasm/dynasm.lua -D WIN=1 -D X64=1 -D X64WIN=1 -o $out $in\n"
        )
        f.write("  description = DYNASM $out\n")
        f.write("\n")
        f.write("rule dynasm\n")
        f.write(
            "  command = ./minilua $src/../third_party/ir/dynasm/dynasm.lua -o $out $in\n"
        )
        f.write("  description = DYNASM $out\n")
        f.write("\n")
        f.write("rule ripsnip\n")
        f.write("  command = %s $src/clang_rip.py\n" % sys.executable)
        f.write("  description = SNIPRIP $out\n")
        f.write("\n")
        f.write("rule gen_ir_fold_hash\n")
        if platform == "w":
            f.write(
                "  command = cmd /c %s < $src/../third_party/ir/ir_fold.h > ir_fold_hash.h\n"
                % gen_ir_fold_hash_exe
            )
        else:
            f.write(
                "  command = ./%s < $src/../third_party/ir/ir_fold.h > ir_fold_hash.h\n"
                % gen_ir_fold_hash_exe
            )
        f.write("  description = GEN_IR_FOLD_HASH\n")
        f.write("\n")
        f.write("rule gendumbbench\n")
        f.write("  command = %s $src/gen_dumbbench.py\n" % sys.executable)
        f.write("  description = GEN_DUMBBENCH\n")
        f.write("\n")
        f.write("rule re2c\n")
        f.write(
            "  command = ../../third_party/re2c/%s/re2c%s -W -b -i --no-generation-date -o $out $in\n"
            % (platform, exe_ext)
        )
        f.write("  description = RE2C $out\n")
        f.write("\n")
        f.write("rule testrun\n")
        f.write(
            "  command = %s $src/testrun.py $src/.. %s/%s $data\n"
            % (sys.executable, root_dir, luvcexe)
        )
        f.write("  description = TEST $in\n\n")

        # snippets.c is included by gen.c (so don't build separately)
        f.write("build snippets.c: ripsnip | $src/clang_rip.py\n")

        # categorizer.c is included by lex.c (so don't build separately)
        f.write("build categorizer.c: re2c $src/categorizer.in.c\n")

        f.write(
            "build dumbbench.c dumbbench.lua dumbbench.py dumbbench.luv: gendumbbench | $src/gen_dumbbench.py\n"
        )

        def getobj(src):
            return os.path.splitext(src)[0].replace("..", "__") + obj_ext

        def get_extra(src):
            if platform == "w":
                return " /FI $src/force_include.h"
            else:
                return " -include $src/force_include.h"

        common_objs = []
        for src in COMMON_FILELIST:
            if sys.platform == "darwin" and "_win." in src:
                continue
            elif sys.platform == "win32" and "_mac." in src:
                continue
            obj = getobj(src)
            common_objs.append(obj)
            extra_deps = ""
            extra_deps = " | snippets.c" if src == "gen.c" else extra_deps
            extra_deps = " | categorizer.c" if src == "token.c" else extra_deps
            extra_deps = (
                " | ir_emit_x86.h ir_emit_aarch64.h"
                if src == "../third_party/ir/ir_emit.c"
                else extra_deps
            )
            extra_deps = (
                " | ir_fold_hash.h" if src == "../third_party/ir/ir.c" else extra_deps
            )
            f.write("build %s: cc $src/%s%s\n" % (obj, src, extra_deps))
            f.write("  extra=%s\n" % get_extra(src))

        if config == "p":
            tracy_cpp = "../third_party/tracy/public/TracyClient.cpp"
            obj = getobj(tracy_cpp)
            common_objs.append(obj)
            f.write("build %s: cc $src/%s\n" % (obj, tracy_cpp))
            f.write(
                "  extra=-Wno-missing-field-initializers -Wno-unused-variable -Wno-cast-function-type-mismatch -Wno-microsoft-cast -Wno-unused-function -Wno-unused-but-set-variable\n"
            )

        luvc_objs = []
        for src in LUVC_FILELIST:
            obj = getobj(src)
            luvc_objs.append(obj)
            f.write("build %s: cc $src/%s\n" % (obj, src))
            f.write("  extra=%s\n" % get_extra(src))

        unittest_objs = []
        for src in UNITTEST_FILELIST:
            obj = getobj(src)
            unittest_objs.append(obj)
            f.write("build %s: cc $src/%s\n" % (obj, src))
            f.write("  extra=%s\n" % get_extra(src))

        gen_ir_fold_hash_objs = []
        for src in GEN_IR_FOLD_HASH_FILELIST:
            obj = getobj(src)
            gen_ir_fold_hash_objs.append(obj)
            # No force_include.h here.
            f.write("build %s: cc $src/%s\n" % (obj, src))

        lexbench_objs = []
        for src in LEXBENCH_FILELIST:
            obj = getobj(src)
            lexbench_objs.append(obj)
            f.write("build %s: cc $src/%s\n" % (obj, src))
            fn = os.path.join(root_dir, "dumbbench.luv").replace("\\", "/")
            if sys.platform == "win32":
                f.write('  extra=-DFILENAME="""%s""" %s\n' % (fn, get_extra(src)))
            else:
                f.write('  extra=-DFILENAME=\\"%s\\" %s\n' % (fn, get_extra(src)))

        alltests = []
        for testf, cmds in tests.items():
            f.write("build %s: testrun $src/../%s | %s\n" % (testf, testf, luvcexe))
            # b64 <- json <- dict to smuggle through to test script w/o dealing
            # with shell quoting garbage.
            cmds_to_pass = base64.b64encode(bytes(json.dumps(cmds), encoding="utf-8"))
            f.write("  data = %s\n" % str(cmds_to_pass, encoding="utf-8"))
            alltests.append(testf)

        f.write("build %s: link %s\n" % (luvcexe, " ".join(common_objs + luvc_objs)))
        f.write(
            "build %s: link %s\n"
            % (gen_ir_fold_hash_exe, " ".join(gen_ir_fold_hash_objs))
        )
        f.write(
            "build %s: mlbuild $src/../third_party/ir/dynasm/minilua.c\n" % miniluaexe
        )

        f.write(
            "build ir_emit_x86.h: dynasm_w $src/../third_party/ir/ir_x86.dasc | %s\n"
            % miniluaexe
        )
        f.write(
            "build ir_emit_aarch64.h: dynasm $src/../third_party/ir/ir_aarch64.dasc | %s\n"
            % miniluaexe
        )

        f.write("build ir_fold_hash.h: gen_ir_fold_hash | %s\n" % gen_ir_fold_hash_exe)

        f.write(
            "build %s: link %s\n"
            % ("unittests" + exe_ext, " ".join(common_objs + unittest_objs))
        )

        f.write("build run_unittests: testrun unittests%s\n" % exe_ext)
        cmds = {
            "run": os.path.join(root_dir, "unittests" + exe_ext) + " -q",
            "ret": 0,
            "direct": True,
        }
        cmds_to_pass = base64.b64encode(bytes(json.dumps(cmds), encoding="utf-8"))
        f.write("  data = %s\n" % str(cmds_to_pass, encoding="utf-8"))
        alltests.append("run_unittests")

        common_without_higher_level = [
            x for x in common_objs if "parse_" not in x and "type." not in x
        ]
        f.write(
            "build %s: link %s | dumbbench.luv\n"
            % (
                "lexbench" + exe_ext,
                " ".join(common_without_higher_level + lexbench_objs),
            )
        )

        f.write("\nbuild test: phony " + " ".join(alltests) + "\n")

        f.write(
            "\ndefault luvc%s unittests%s lexbench%s\n" % (exe_ext, exe_ext, exe_ext)
        )

        f.write("\nrule gen\n")
        f.write("  command = %s $src/configure.py $in\n" % sys.executable)
        f.write("  description = GEN build.ninja\n")
        f.write("  generator = 1\n")
        f.write("build build.ninja: gen | $src/configure.py\n")


def main():
    if len(sys.argv) != 1:
        print("usage: configure.py")
        return 1

    os.chdir(ROOT_DIR)  # Necessary when regenerating manifest from ninja
    tests = get_tests()
    for platform, pdata in CONFIGS.items():
        if (
            (sys.platform == "win32" and platform == "w")
            or (sys.platform == "linux" and platform == "l")
            or (sys.platform == "darwin" and platform == "m")
        ):
            for config, cmdlines in pdata.items():
                if config == "__":
                    continue
                generate(platform, config, pdata["__"], cmdlines, tests)


if __name__ == "__main__":
    main()
