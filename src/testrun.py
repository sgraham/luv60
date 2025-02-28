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

    if (
        (sys.platform == "win32" and "win" in cmds["disabled"])
        or (sys.platform == "darwin" and "mac" in cmds["disabled"])
        or (sys.platform == "linux" and "linux" in cmds["disabled"])
    ):
        return 0

    if cmds["out"] or cmds["err"]:
        res = subprocess.run(
            [ccbin] + cmds["run"].split(" "),
            cwd=root,
            capture_output=True,
            universal_newlines=True,
            env=env,
        )
        out = res.stdout
        err = res.stderr
        if out != cmds["out"]:
            print("got stdout:\n")
            print(out)
            print(hexdump(out.encode("utf-8")))
            print("but expected:\n")
            print(cmds["out"])
            print(hexdump(cmds["out"].encode("utf-8")))
            return 1
        if err != cmds["err"]:
            print("got stderr:\n")
            print(err)
            print(hexdump(err.encode("utf-8")))
            print("but expected:\n")
            print(cmds["err"])
            print(hexdump(cmds["err"].encode("utf-8")))
            return 1
    else:
        res = subprocess.run([ccbin] + cmds["run"].split(" "), cwd=root, env=env)

    if cmds["ret"] == "NOCRASH":
        if res.returncode >= 0 and res.returncode <= 255 and res.returncode != 117:
            return 0
        # Something out of range indicates crash, e.g Windows returns -1073741819 on GPF.
        # Linux is always in the range 0..255, so pick 117 arbitrarily for an ASAN signal.
        return 2
    else:
        if res.returncode != cmds["ret"]:
            print("got return code %d, but expected %d" % (res.returncode, cmds["ret"]))
            return 2


if __name__ == "__main__":
    sys.exit(main())
