import os

ROOT_DIR = os.path.normpath(
    os.path.join(os.path.abspath(os.path.dirname(__file__)), "..")
)

import shutil
import subprocess
import sys
import tempfile


FILES = [
    "LICENSE",
    "dynasm/dasm_arm.h",
    "dynasm/dasm_arm.lua",
    "dynasm/dasm_arm64.h",
    "dynasm/dasm_arm64.lua",
    "dynasm/dasm_mips.h",
    "dynasm/dasm_mips.lua",
    "dynasm/dasm_mips64.lua",
    "dynasm/dasm_ppc.h",
    "dynasm/dasm_ppc.lua",
    "dynasm/dasm_proto.h",
    "dynasm/dasm_x64.lua",
    "dynasm/dasm_x86.h",
    "dynasm/dasm_x86.lua",
    "dynasm/dynasm.lua",
    "dynasm/minilua.c",
    "gen_ir_fold_hash.c",
    "ir.c",
    "ir.h",
    "ir_aarch64.dasc",
    "ir_aarch64.h",
    "ir_builder.h",
    "ir_cfg.c",
    "ir_check.c",
    "ir_disasm.c",
    "ir_dump.c",
    "ir_elf.h",
    "ir_emit.c",
    "ir_fold.h",
    "ir_gcm.c",
    "ir_gdb.c",
    "ir_mem2ssa.c",
    "ir_patch.c",
    "ir_perf.c",
    "ir_private.h",
    "ir_ra.c",
    "ir_save.c",
    "ir_sccp.c",
    "ir_strtab.c",
    "ir_x86.dasc",
    "ir_x86.h",
]


def main():
    proc = subprocess.run(
        ["git", "status", "--porcelain", "--untracked-files=no"],
        capture_output=True,
        check=True,
    )
    if proc.stdout.decode('utf-8').strip() != "":
        print("Run with a clean workdir.\n")
        print("Got: '%s'\n" % proc.stdout.strip())
        sys.exit(1)

    ir_dir = os.path.join(ROOT_DIR, "third_party", "ir")
    shutil.rmtree(ir_dir)

    with tempfile.TemporaryDirectory() as tmpdir:
        os.chdir(tmpdir)
        subprocess.run(["git", "clone", "git@github.com:sgraham/ir.git"], check=True)
        os.chdir(os.path.join(tmpdir, "ir"))
        os.makedirs(ir_dir)
        os.makedirs(os.path.join(ir_dir, 'dynasm'))
        for f in FILES:
            shutil.copyfile(f, os.path.join(ir_dir, f))
        # This is https://github.com/dstogov/ir at d6d7fc489137aab218b04b59d770b497c5ae3832.
        with open(os.path.join(ir_dir, "README"), 'w') as f:
            f.write(
                "This is https://github.com/dstogov/ir forked to https://github.com/sgraham/ir.\n"
            )
            f.write('Minor portability patches carried in the sgraham tree. Last pulled at:\n')
            rev_proc = subprocess.run(['git', 'rev-parse', 'HEAD'], capture_output=True)
            f.write(rev_proc.stdout.decode('utf-8') + '\n')

    os.chdir(ROOT_DIR)
    subprocess.run(['git', 'add', '-u'])
    subprocess.run(['git', 'add', ir_dir])
    subprocess.run(['git', 'status'])


if __name__ == "__main__":
    main()
