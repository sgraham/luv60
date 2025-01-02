# See https://scot.tg/2024/12/22/worked-example-of-copy-and-patch-compilation/
# for some explanation.

import subprocess
import sys

# TODO: fixme
CLANG_PATH = "C:\\Program Files\\LLVM\\bin\\clang.exe"
OBJDUMP_PATH = "C:\\Program Files\\LLVM\\bin\\llvm-objdump.exe"
CLANG_FORMAT_PATH = "C:\\Program Files\\LLVM\\bin\\clang-format.exe"


def clang_format_for_patch_header(src):
    src = src.replace("__vectorcall ", "").replace("__vectorcall", "")
    pop = subprocess.Popen(
        [CLANG_FORMAT_PATH], stdin=subprocess.PIPE, stdout=subprocess.PIPE
    )
    return pop.communicate(src.encode("utf-8"))[0].decode("utf-8")


def rip_obj_file(obj_file):
    """
    As a quick first draft, parse the output of llvm-objdump. It wouldn't be very hard
    and probably more reliable to just read the .obj file directly.
    """
    result = subprocess.run(
        [
            OBJDUMP_PATH,
            "--section=.text",
            "--reloc",
            obj_file,
        ],
        stdout=subprocess.PIPE,
    )
    relocs = result.stdout.decode("utf-8").splitlines()
    relocs_no_header = relocs[4:]
    if not (
        relocs_no_header[0].startswith("OFFSET")
        and relocs_no_header[0].endswith("VALUE")
    ):
        print("llvm-objdump reloc parsing failed (no OFFSET header?)")
        sys.exit(1)
    relocs_body = relocs_no_header[1:]
    all_relocs = {}
    for line in relocs_body:
        offset, type, name = line.split()
        offset = int(offset, 16)
        all_relocs[offset] = (type, name)
    # print(all_relocs)

    result = subprocess.run(
        [
            OBJDUMP_PATH,
            "--section=.text",
            "--disassemble",
            "--disassemble-zeroes",
            "--no-addresses",
            "--x86-asm-syntax=intel",
            obj_file,
        ],
        stdout=subprocess.PIPE,
    )
    dis = result.stdout.decode("utf-8").splitlines()
    dis_no_header = dis[5:]
    if not dis_no_header[0].startswith("<") or not dis_no_header[0].endswith(">:"):
        print("llvm-objdump dis parsing failed (no func name?)")
        sys.exit(1)
    dis_body = dis_no_header[1:]
    all_bytes = []
    for line in dis_body:
        bytes, _, instrs = line.partition("\t")
        for b in bytes.split():
            all_bytes.append(int(b, 16))
    # print(all_bytes)

    return (all_bytes, all_relocs)


def process_obj_files_to_patch_func(
    obj_files,
    snip_name,
    snippets_file_handle,
    representative_src,
    consts,
    continuations,
):
    bytes_and_relocs = [rip_obj_file(x) for x in obj_files]

    # With the code bits and relocs in hand, write out a C
    # function to the header that will 'assemble' into our target buffer.

    def could_fallthrough():
        # XXX assuming they're all the same, hopefully ok?
        all_bytes = bytes_and_relocs[0][0]
        all_relocs = bytes_and_relocs[0][1]
        # print(all_bytes)
        # print(all_relocs)
        if len(continuations) != 1:
            return False

        # mcmodel=small
        if all_bytes[-5] == 0xE9:
            at = len(all_bytes) - 4
            r = all_relocs.get(at)
            if not r:
                return False
            if r[0] == "IMAGE_REL_AMD64_REL32" and r[1] == "$CONT0":
                return True

        """ not tested
        # mcmodel=large
        # 000000000000000A: 48 B8 00 00 00 00 00 00 00 00  mov         rax,offset $CONT0
        # 0000000000000014: 48 FF E0           jmp         rax
        print(all_bytes[-13:])
        if all_bytes[-13:] == [0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0x48, 0xFF, 0xE0]:
            at = len(all_bytes) - 11
            r = all_relocs.get(at)
            if not r:
                return False
            if r[0] == "IMAGE_REL_AMD64_ADDR64" and r[1] == "$CONT0":
                return True
        """

        return False

    args = []
    for i, c in enumerate(consts):
        args.append(f"uintptr_t $X{i}")
    args_str_no_continuations = ", ".join(args)
    for i, c in enumerate(continuations):
        args.append(f"void* $CONT{i}")
    args_str = ", ".join(args)

    sf = snippets_file_handle

    def gen_snippet(fallthrough):
        ft = "_fallthrough" if fallthrough else ""
        arg = args_str_no_continuations if fallthrough else args_str
        if arg:
            arg = f", {arg}"
        sf.write(
            f"static inline unsigned char* snip_{snip_name}{ft}(int num_int_regs_in_use, unsigned char* __restrict p{arg}) {{\n"
        )
        sf.write("  switch(num_int_regs_in_use) {\n")
        for ir, br in enumerate(bytes_and_relocs):
            sf.write("    case %d:\n" % ir)
            all_bytes = br[0]
            all_relocs = br[1]
            i = 0
            while i < len(all_bytes):
                b = all_bytes[i]
                r = all_relocs.get(i)
                if r:
                    if r[0] == "IMAGE_REL_AMD64_ADDR64":
                        sf.write(f"      *(uintptr_t*)p = {r[1]};\n")
                        sf.write("      p += 8; /* %s ADDR64 */\n" % r[1])
                        i += 8
                    elif r[0] == "IMAGE_REL_AMD64_REL32":
                        sf.write(
                            f"      *(int32_t*)p = (intptr_t){r[1]} - (intptr_t)p - 4;\n"
                        )
                        sf.write("      p += 4; /* %s REL32 */\n" % r[1])
                        i += 4
                    elif r[0] == "IMAGE_REL_AMD64_ADDR32":
                        sf.write(f"      *(uintptr_t*)p = {r[1]};\n")
                        sf.write("      p += 4; /* %s ADDR32 */\n" % r[1])
                        i += 4
                    else:
                        print("unhandled reloc type %s" % r[0])
                        sys.exit(1)
                else:
                    if fallthrough and i == len(all_bytes) - 5:
                        break
                    sf.write("      *p++ = 0x%02x;\n" % b)
                    i += 1
            sf.write("    break;\n")
        sf.write("    default:\n")
        sf.write('      gen_error("internal error: exceeded maximum int registers.");')
        sf.write("  }\n")
        sf.write("  return p;\n")
        sf.write(f"}}\n\n")

    sf.write("#if 0\n\n")
    sf.write(clang_format_for_patch_header(representative_src))
    sf.write("\n#endif\n\n")
    gen_snippet(False)
    if could_fallthrough():
        gen_snippet(True)


def munge_ll_file(infile, outfile, do_asm_hack, look_for_const, int_reg):
    """
    It doesn't seem to be possible to tell clang to make the calling
    convention of a function the GHC one, so we instead tag both
    definitions and casts before continuation calls as __vectorcall, and
    then manually search and replace vectorcall with ghccc. Additionally,
    change tail calls to musttail to make sure we fail if the continuation
    wouldn't be compiled to a jmp.
    """
    with open(infile, "r") as f:
        contents = f.read()
    contents = contents.replace("tail call x86_vectorcallcc", "musttail call ghccc")
    contents = contents.replace("x86_vectorcallcc", "ghccc")
    if do_asm_hack == "32":
        ghccc_order = [
            "r13d",
            "ebp",
            "r12d",
            "ebx",
            "r14d",
            "esi",
            "edi",
            "r8d",
            "r9d",
            "r15d",
        ]
        repl_reg = ghccc_order[int_reg+1]
        contents = contents.replace(
            f'asm "movl $$${look_for_const}, %eax", "={{ax}},',
            f'asm "movl $$${look_for_const}, %{repl_reg}", "={{{repl_reg}}},')
    elif do_asm_hack == "64":
        ghccc_order = [
            "r13",
            "rbp",
            "r12",
            "rbx",
            "r14",
            "rsi",
            "rdi",
            "r8",
            "r9",
            "r15",
        ]
        repl_reg = ghccc_order[int_reg+1]
        contents = contents.replace(
            f'asm "movq $$${look_for_const}, %rax", "={{ax}},',
            f'asm "movq $$${look_for_const}, %{repl_reg}", "={{{repl_reg}}},')
    with open(outfile, "w", newline="\n") as f:
        f.write(contents)


def toolchain_c_to_objs(
    base_name,
    src_per_int_reg,
    consts,
    continuations,
    snippets_file_handle,
    model,
    do_asm_hack,
):
    obj_files = []
    for i, src in enumerate(src_per_int_reg):
        c_file = base_name + ".%d.c" % i
        first_ll_file = base_name + ".%d.initial.ll" % i
        munged_ll_file = base_name + ".%d.ghc.ll" % i
        obj_file = base_name + ".%d.obj" % i

        with open(c_file, "w", newline="\n") as f:
            f.write(
                """\
#include <stdint.h>
#include <stdlib.h>
extern uintptr_t $X0;
extern uintptr_t $X1;
extern uintptr_t $X2;
#define $X0 ((uintptr_t)&$X0)
#define $X1 ((uintptr_t)&$X1)
#define $X2 ((uintptr_t)&$X2)
extern uintptr_t $CONT0;
extern uintptr_t $CONT1;
extern uintptr_t $CONT2;
#define $CONT0 ((uintptr_t)&$CONT0)
#define $CONT1 ((uintptr_t)&$CONT1)
#define $CONT2 ((uintptr_t)&$CONT2)
"""
            )
            f.write(src)
            f.write("\n")

        subprocess.check_call(
            [
                CLANG_PATH,
                "-emit-llvm",
                c_file,
                "-O3",
                "-S",
                "-o",
                first_ll_file,
            ]
        )
        munge_ll_file(first_ll_file, munged_ll_file, do_asm_hack, '$X0', i)

        # Previously -mcmodel=medium was used here to attempt to encourage
        # longer relocations. In practice, this doesn't work with ASLR, so
        # instead we workaround it for constants with some inline asm hacks.
        # TODO: post with details explained
        subprocess.check_call(
            [
                CLANG_PATH,
                munged_ll_file,
                f"-mcmodel={model}",
                "-O3",
                "-c",
                "-o",
                obj_file,
            ]
        )
        # subprocess.check_call(["dumpbin", "/disasm", obj_file])
        obj_files.append(obj_file)

    assert len(obj_files) == len(src_per_int_reg)
    process_obj_files_to_patch_func(
        obj_files,
        base_name,
        snippets_file_handle,
        src_per_int_reg[0],
        consts,
        continuations,
    )


NUM_INT_VERSIONS = 9


class CToObj:
    def __init__(self, base_name, snippets_file, model="small"):
        self.base_name = base_name
        self.code = [""] * NUM_INT_VERSIONS
        self.consts_used = []
        self.continuations_used = []
        self.snippets_file = snippets_file
        self.model = model
        self.do_asm_hack = None

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        toolchain_c_to_objs(
            self.base_name,
            self.code,
            self.consts_used,
            self.continuations_used,
            self.snippets_file,
            self.model,
            self.do_asm_hack,
        )

    def build_decl(self, args, cc="__vectorcall"):
        for int_regs in range(NUM_INT_VERSIONS):
            result = f"{cc} void {self.base_name}_i{int_regs}(uintptr_t $stack"
            for i in range(int_regs):
                result += f", uintptr_t $r{i}"
            for i in args:
                result += f", {i}"
            result += ")"
            self.emit_specific(int_regs, result)

    def build_const(self, const_index):
        self.emit(f"$X{const_index}")
        self.consts_used.append(const_index)

    def build_continuation(self, cont_index, to_add):
        for int_regs in range(NUM_INT_VERSIONS):
            # 'return' is necessary because it's converted to a [[musttail]],
            # which clang specifies has to be on a return even though it's void.
            num_args = int_regs + len(to_add)
            result = "return ((void (*__vectorcall)(uintptr_t"
            for i in range(int_regs):
                result += ", uintptr_t"
            for i in to_add:  # TODO: assuming all 'int' right now
                result += ", int"
            result += f"))$CONT{cont_index})($stack"
            for i in range(int_regs):
                result += f", $r{i}"
            for i in to_add:
                result += f", {i}"
            result += ");"
            self.emit_specific(int_regs, result)
        self.continuations_used.append(cont_index)

    def asm_hack_32(self):
        self.do_asm_hack = "32"

    def asm_hack_64(self):
        self.do_asm_hack = "64"

    def emit_specific(self, int_regs, text):
        self.code[int_regs] += text

    def emit(self, text):
        for int_regs in range(NUM_INT_VERSIONS):
            self.emit_specific(int_regs, text)


def main():
    with open("..\\out\\snippets.c", "w", newline="\n") as sf:
        sf.write("#include <stdint.h>\n")
        sf.write("#include <stdlib.h>\n\n")

        """
        with CToObj("load_addr", sf) as c:
            c.build_decl([])
            c.emit("{ uintptr_t v = (")
            c.build_const(0)
            c.emit(");")
            c.build_continuation(0, ["v"])
            c.emit("}")

        with CToObj("load", sf) as c:
            c.build_decl([])
            c.emit("{ int v = *(int*)(")
            c.build_const(0)
            c.emit(");")
            c.build_continuation(0, ["v"])
            c.emit("}")
        """

        with CToObj("const_i32", sf) as c:
            c.build_decl([])
            c.emit('{ int v; asm ("movl $')
            c.build_const(0)
            c.emit(', %%eax": "=a"(v));')
            c.build_continuation(0, ["v"])
            c.emit("}")
            c.asm_hack_32()

        """
        with CToObj("add", sf) as c:
            c.build_decl(["int a", "int b"])
            c.emit("{ int v = a + b;")
            c.build_continuation(0, ["v"])
            c.emit("}")

        with CToObj("assign_indirect", sf) as c:
            c.build_decl(["uintptr_t offset", "int v"])
            c.emit("{ *(int*)(offset) = v;")
            c.build_continuation(0, [])
            c.emit("}")

        with CToObj("mul", sf) as c:
            c.build_decl(["int a", "int b"])
            c.emit("{ int v = a * b;")
            c.build_continuation(0, ["v"])
            c.emit("}")

        with CToObj("if_then_else", sf) as c:
            c.build_decl(["bool cond"])
            c.emit("{ if (cond) {")
            c.build_continuation(0, [])
            c.emit("} else {")
            c.build_continuation(1, [])
            c.emit("}")
            c.emit("}")

        with CToObj("sysexit", sf) as c:
            c.build_decl(["int rc"])
            c.emit("{ exit(rc); }");

        with CToObj("return", sf) as c:
            c.build_decl(["int rc"])
            c.emit("{ exit(rc); }");
        """


if __name__ == "__main__":
    main()
