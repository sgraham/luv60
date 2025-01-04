import base64
import glob
import json
import os
import sys

ROOT_DIR = os.path.normpath(
    os.path.join(os.path.abspath(os.path.dirname(__file__)), "..")
)

FILELIST = [
    "base_win.c",
    "gen.c",
    "lex.c",
    "luvc_main.c",
    "parse.c",
    "str.c",
]

CLANG_CL = "C:\\Program Files\\LLVM\\bin\\clang-cl.exe"
LLD_LINK = "C:\\Program Files\\LLVM\\bin\\lld-link.exe"

CONFIGS = {
    'w': {
        'd': {
            'COMPILE': CLANG_CL + ' /showIncludes -std:c11 /nologo /TC /FS /Od /Zi /D_DEBUG /DBUILD_DEBUG=1 /D_CRT_SECURE_NO_DEPRECATE /W4 /WX -mavx2 -mpclmul -Wno-unused-parameter /I$src /I. /c $in /Fo:$out /Fd:$out.pdb',
            'LINK': LLD_LINK + ' /nologo /dynamicbase:no /DEBUG $in /out:$out /pdb:$out.pdb',
        },
        'r': {
            'COMPILE': CLANG_CL + ' /showIncludes -std:c11 /nologo -flto /FS /O2 /Zi /DNDEBUG /DBUILD_DEBUG=0 /D_CRT_SECURE_NO_DEPRECATE /W4 /WX -mavx2 -mpclmul -Wno-unused-parameter /I$src /I. /c $in /Fo$out /Fd:$out.pdb',
            'LINK': LLD_LINK + ' /nologo /dynamicbase:no /ltcg /DEBUG /OPT:REF /OPT:ICF $in /out:$out /pdb:$out.pdb',
        },
        '__': {
            'exe_ext': '.exe',
            'obj_ext': '.obj',
        },
    },
}


def get_tests():
    tests = {}
    for test in glob.glob(os.path.join('test', '*.luv')):
        test = test.replace('\\', '/')
        run = '{self}'
        ret = '0'
        txt = ''
        disabled = False
        run_prefix = '# RUN: '
        ret_prefix = '# RET: '
        txt_prefix = '# TXT: '
        disabled_prefix = '# DISABLED'
        with open(test, 'r', encoding='utf-8') as f:
            for l in f.readlines():
                if l.startswith(run_prefix): run = l[len(run_prefix):].rstrip()
                if l.startswith(ret_prefix): ret = l[len(ret_prefix):].rstrip()
                if l.startswith(txt_prefix): txt += l[len(txt_prefix):].rstrip() + '\n'
                if l.startswith(disabled_prefix): disabled = True
            def sub(t):
                return t.replace('{self}', test)
            if not disabled:
                tests[test] = {'run': sub(run), 'ret': int(ret), 'txt': sub(txt)}
    return tests


def generate(platform, config, settings, cmdlines, tests):
    root_dir = os.path.join('out', platform + config)
    if not os.path.isdir(root_dir):
        os.makedirs(root_dir)

    exe_ext = settings['exe_ext']
    obj_ext = settings['obj_ext']

    luvcexe = 'luvc' + exe_ext

    with open(os.path.join(root_dir, 'build.ninja'), 'w', newline='\n') as f:
        f.write('src = ../../src\n')
        f.write('\n')
        f.write('rule cc\n')
        f.write('  command = ' + cmdlines['COMPILE'] + '\n')
        f.write('  description = CC $out\n')
        f.write('  deps = ' + ('msvc' if platform == 'w' else 'gcc') + '\n')
        if platform != 'w':
            f.write('  depfile = $out.d')
        f.write('\n')
        f.write('rule link\n')
        f.write('  command = ' + cmdlines['LINK'] + '\n')
        f.write('  description = LINK $out\n')
        f.write('\n')
        f.write('rule ripsnip\n')
        f.write('  command = %s $src/clang_rip.py\n' % sys.executable)
        f.write('  description = SNIPRIP $out\n')
        f.write('\n')
        f.write('rule re2c\n')
        f.write('  command = ../../third_party/re2c/%s/re2c%s -W -b -g -i --no-generation-date -o $out $in\n' % (platform, exe_ext))
        f.write('  description = RE2C $out\n')
        f.write('\n')
        f.write('rule testrun\n')
        f.write('  command = %s $src/testrun.py $src/.. %s/%s $data\n' % (
            sys.executable, root_dir, luvcexe))
        f.write('  description = TEST $in\n\n')

        objs = []

        # snippets.c is included by gen.c (so don't build separately)
        snippets = 'snippets.c'
        f.write('build %s: ripsnip | $src/clang_rip.py\n' % snippets)

        # categorizer.c is included by lex.c (so don't build separately)
        categorizer = 'categorizer.c'
        f.write('build %s: re2c $src/categorizer.in.c\n' % categorizer)

        for src in FILELIST:
            obj = os.path.splitext(src)[0] + obj_ext
            objs.append(obj)
            extra_deps = ''
            extra_deps = ' | snippets.c' if src == 'gen.c' else extra_deps
            extra_deps = ' | categorizer.c' if src == 'lex.c' else extra_deps
            f.write('build %s: cc $src/%s%s\n' % (obj, src, extra_deps))

        alltests = []
        for testf, cmds in tests.items():
            f.write('build %s: testrun $src/../%s | %s\n' % (
                testf, testf, luvcexe))
            # b64 <- json <- dict to smuggle through to test script w/o dealing
            # with shell quoting garbage.
            cmds_to_pass = base64.b64encode(bytes(json.dumps(cmds), encoding='utf-8'))
            f.write('  data = %s\n' % str(cmds_to_pass, encoding='utf-8'))
            alltests.append(testf)

        f.write('build %s: link %s\n' % (luvcexe, ' '.join(objs)))

        f.write('\nbuild test: phony ' + ' '.join(alltests) + '\n')

        f.write('\ndefault luvc%s\n' % exe_ext)

        f.write('\nrule gen\n')
        f.write('  command = %s $src/configure.py $in\n' % sys.executable)
        f.write('  description = GEN build.ninja\n')
        f.write('  generator = 1\n')
        f.write('build build.ninja: gen | $src/configure.py\n')


def main():
    if len(sys.argv) != 1:
        print('usage: configure.py')
        return 1

    os.chdir(ROOT_DIR)  # Necessary when regenerating manifest from ninja
    tests = get_tests()
    for platform, pdata in CONFIGS.items():
        if (sys.platform == 'win32' and platform == 'w') or \
                (sys.platform == 'linux' and platform == 'l'):
            for config, cmdlines in pdata.items():
                if config == '__':
                    continue
                generate(platform, config, pdata['__'], cmdlines, tests)


if __name__ == '__main__':
    main()
