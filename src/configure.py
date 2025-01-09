import base64
import glob
import json
import os
import sys

ROOT_DIR = os.path.normpath(
    os.path.join(os.path.abspath(os.path.dirname(__file__)), "..")
)

COMMON_FILELIST = [
    "base_win.c",
    # "gen.c",
    "gen_ssa.c",
    "lex.c",
    "parse.c",
    "str.c",
    "type.c",
]

LUVC_FILELIST = [
    "luvc_main.c",
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

CLANG_CL = "C:\\Program Files\\LLVM\\bin\\clang-cl.exe"
LLD_LINK = "C:\\Program Files\\LLVM\\bin\\lld-link.exe"

CONFIGS = {
    "w": {
        "d": {
            "COMPILE": CLANG_CL
            + " /showIncludes -std:c11 /nologo /TC /FS /Od /Zi /D_DEBUG /DBUILD_DEBUG=1 /D_CRT_SECURE_NO_DEPRECATE /W4 /WX $extra -mavx2 -mpclmul -Wno-unused-parameter /I$src /I. /c $in /Fo:$out /Fd:$out.pdb",
            "LINK": LLD_LINK
            + " /nologo /dynamicbase:no /DEBUG $in /out:$out /pdb:$out.pdb",
        },
        "r": {
            "COMPILE": CLANG_CL
            + " /showIncludes -std:c11 /nologo -flto -fuse-ld=lld /FS /O2 /Zi /DNDEBUG /DBUILD_DEBUG=0 /D_CRT_SECURE_NO_DEPRECATE /W4 /WX $extra -mavx2 -mpclmul -Wno-unused-parameter /I$src /I. /c $in /Fo$out /Fd:$out.pdb",
            "LINK": LLD_LINK
            + " /nologo /dynamicbase:no /ltcg /DEBUG /OPT:REF /OPT:ICF $in /out:$out /pdb:$out.pdb",
        },
        "p": {
            "COMPILE": CLANG_CL
            + " /showIncludes /nologo -flto -fuse-ld=lld /FS /O2 /Zi /DTRACY_ENABLE=1 /DNDEBUG /DBUILD_DEBUG=0 /D_CRT_SECURE_NO_DEPRECATE /I$src/../third_party/tracy/public/tracy /W4 /WX $extra -mavx2 -mpclmul -Wno-unused-parameter /I$src /I. /c $in /Fo$out /Fd:$out.pdb",
            "LINK": LLD_LINK
            + " /nologo /dynamicbase:no /ltcg /DEBUG /OPT:REF /OPT:ICF $in /out:$out /pdb:$out.pdb",
        },
        "__": {
            "exe_ext": ".exe",
            "obj_ext": ".obj",
        },
    },
}


def get_tests():
    tests = {}
    for test in glob.glob(os.path.join("test", "**.luv")):
        test = test.replace("\\", "/")
        run = "{self}"
        ret = "0"
        crc = "0"
        txt = ""
        disabled = False
        run_prefix = "# RUN: "
        ret_prefix = "# RET: "
        crc_prefix = "# CRC: "
        txt_prefix = "# TXT: "
        disabled_prefix = "# DISABLED"
        with open(test, "r", encoding="utf-8") as f:
            for l in f.readlines():
                if l.startswith(run_prefix):
                    run = l[len(run_prefix) :].rstrip()
                if l.startswith(ret_prefix):
                    ret = l[len(ret_prefix) :].rstrip()
                if l.startswith(crc_prefix):
                    crc = l[len(crc_prefix) :].rstrip()
                if l.startswith(txt_prefix):
                    txt += l[len(txt_prefix) :].rstrip() + "\n"
                if l.startswith(disabled_prefix):
                    disabled = True

            def sub(t):
                t = t.replace("{self}", test)
                spaces = len(test) * " "
                return t.replace("{ssss}", spaces)

            if not disabled:
                tests[test] = {
                    "run": sub(run),
                    "ret": int(ret),
                    "crc": int(crc),
                    "txt": sub(txt),
                }
    return tests


def generate(platform, config, settings, cmdlines, tests):
    root_dir = os.path.join("out", platform + config)
    if not os.path.isdir(root_dir):
        os.makedirs(root_dir)

    exe_ext = settings["exe_ext"]
    obj_ext = settings["obj_ext"]

    luvcexe = "luvc" + exe_ext

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
        f.write("rule ripsnip\n")
        f.write("  command = %s $src/clang_rip.py\n" % sys.executable)
        f.write("  description = SNIPRIP $out\n")
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
            return os.path.splitext(src)[0] + obj_ext

        common_objs = []
        for src in COMMON_FILELIST:
            obj = getobj(src)
            common_objs.append(obj)
            extra_deps = ""
            extra_deps = " | snippets.c" if src == "gen.c" else extra_deps
            extra_deps = " | categorizer.c" if src == "lex.c" else extra_deps
            f.write("build %s: cc $src/%s%s\n" % (obj, src, extra_deps))

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

        unittest_objs = []
        for src in UNITTEST_FILELIST:
            obj = getobj(src)
            unittest_objs.append(obj)
            f.write("build %s: cc $src/%s\n" % (obj, src))

        lexbench_objs = []
        for src in LEXBENCH_FILELIST:
            obj = getobj(src)
            lexbench_objs.append(obj)
            f.write("build %s: cc $src/%s\n" % (obj, src))
            f.write(
                '  extra=-DFILENAME="""%s"""\n'
                % os.path.join(root_dir, "dumbbench.luv").replace("\\", "/")
            )

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
            % ("unittests" + exe_ext, " ".join(common_objs + unittest_objs))
        )

        f.write("build run_unittests: testrun unittests.exe\n")
        cmds = {
            "run": os.path.join(root_dir, "unittests" + exe_ext) + ' -q',
            "ret": 0,
            "direct": True,
        }
        cmds_to_pass = base64.b64encode(bytes(json.dumps(cmds), encoding="utf-8"))
        f.write("  data = %s\n" % str(cmds_to_pass, encoding="utf-8"))
        alltests.append("run_unittests")

        f.write(
            "build %s: link %s | dumbbench.luv\n"
            % ("lexbench" + exe_ext, " ".join(common_objs + lexbench_objs))
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
        if (sys.platform == "win32" and platform == "w") or (
            sys.platform == "linux" and platform == "l"
        ):
            for config, cmdlines in pdata.items():
                if config == "__":
                    continue
                generate(platform, config, pdata["__"], cmdlines, tests)


if __name__ == "__main__":
    main()
