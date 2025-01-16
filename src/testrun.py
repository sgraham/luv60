import base64
import json
import os
import subprocess
import sys

# https://gist.github.com/NeatMonster/c06c61ba4114a2b31418a364341c26c0
class hexdump:
    def __init__(self, buf, off=0):
        self.buf = buf
        self.off = off

    def __iter__(self):
        last_bs, last_line = None, None
        for i in range(0, len(self.buf), 16):
            bs = bytearray(self.buf[i : i + 16])
            line = "{:08x}  {:23}  {:23}  |{:16}|".format(
                self.off + i,
                " ".join(("{:02x}".format(x) for x in bs[:8])),
                " ".join(("{:02x}".format(x) for x in bs[8:])),
                "".join((chr(x) if 32 <= x < 127 else "." for x in bs)),
            )
            if bs == last_bs:
                line = "*"
            if bs != last_bs or line != last_line:
                yield line
            last_bs, last_line = bs, line
        yield "{:08x}".format(self.off + len(self.buf))

    def __str__(self):
        return "\n".join(self)

    def __repr__(self):
        return "\n".join(self)


def main():
    out_dir = os.getcwd()

    root = os.path.abspath(sys.argv[1])
    os.chdir(root)

    ccbin = os.path.abspath(sys.argv[2].replace("/", os.path.sep))
    js = base64.b64decode(bytes(sys.argv[3], encoding="utf-8"))
    cmds = json.loads(js)

    if cmds.get("direct"):
        directres = subprocess.run(cmds["run"].split(" "))
        if directres.returncode != cmds["ret"]:
            return 1
        return 0

    # The default is 1, but we would prefer something a little more distinct. Windows will
    # return the exception code (big number) so out of 0..255, but Linux is always in that
    # range, so select something arbitrary as an ASAN signal that's more notable than the
    # default of 1.
    env = os.environ.copy()
    env["ASAN_OPTIONS"] = "exitcode=117"

    input_name = cmds["run"].split(" ")
    assert len(input_name) == 1  # TODO: ssa hacks
    s_name = os.path.join(out_dir, input_name[0] + ".s")
    luvc_cmd = [ccbin] + [input_name[0]] + [s_name]
    luvcres = subprocess.run(
        luvc_cmd, cwd=root, universal_newlines=True, env=env, capture_output=True
    )
    if cmds["crc"] == 1:
        out = luvcres.stderr
        if out != cmds["txt"]:
            print("got output:\n")
            print(out)
            print("but expected:\n")
            print(cmds["txt"])
            return 1
        return 0
    elif luvcres.returncode != 0:
        print(
            "--- STDOUT:\n%s\n\n--- STDERR:\n%s\n\n" % (luvcres.stdout, luvcres.stderr)
        )
        return 1

    com_dbg_name = os.path.join(out_dir, input_name[0] + ".com.dbg")
    gccres = subprocess.run(
        [
            "cross9\\bin\\x86_64-pc-linux-gnu-gcc.exe",
            "-g",
            "-O",
            "-o",
            com_dbg_name,
            s_name,
            "-static",
            "-fno-pie",
            "-no-pie",
            "-mno-red-zone",
            "-fno-omit-frame-pointer",
            "-nostdlib",
            "-nostdinc",
            "-Wl,--gc-sections",
            "-Wl,-z,max-page-size=0x1000",
            "-fuse-ld=bfd",
            "-Wl,-T,ape.lds",
            "-include",
            "cosmopolitan.h",
            "crt.o",
            "ape.o",
            "cosmopolitan.a",
        ],
        shell=True,
        cwd=os.path.join(root, "scratch"),
    )
    if gccres.returncode != 0:
        return 1

    com_name = os.path.join(out_dir, input_name[0] + ".com")
    objcopyres = subprocess.run(
        [
            "cross9\\bin\\x86_64-pc-linux-gnu-objcopy.exe",
            "-S",
            "-O",
            "binary",
            com_dbg_name,
            com_name,
        ],
        shell=True,
        cwd=os.path.join(root, "scratch"),
    )
    if objcopyres.returncode != 0:
        return 1

    if cmds["txt"]:
        res = subprocess.run(
            [com_name], cwd=root, universal_newlines=True, capture_output=True, env=env
        )
        out = res.stdout
        if out != cmds["txt"]:
            print("got output:\n")
            print(out)
            print(hexdump(bytes(out, encoding='utf-8')))
            print("\nbut expected:\n")
            print(cmds["txt"])
            print(hexdump(bytes(cmds["txt"], encoding='utf-8')))
            return 1
    else:
        res = subprocess.run(com_name, cwd=root, env=env)

    if cmds["ret"] == "NOCRASH":
        if res.returncode >= 0 and res.returncode <= 255 and res.returncode != 117:
            return 0
        # Something out of range indicates crash, e.g Windows returns -1073741819 on GPF.
        # Linux is always in the range 0..255, so pick 117 arbitrarily for an ASAN signal.
        return 2
    else:
        # Not sure what, but something in the unholy qbe/cosmo/ape mess is <<8
        # the number we put in eax from main.
        if res.returncode >> 8 != cmds["ret"]:
            print("got return code %d, but expected %d" % (res.returncode, cmds["ret"]))
            return 2


if __name__ == "__main__":
    sys.exit(main())
