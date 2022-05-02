import subprocess
import os

implemented_opts = [
    'aa', 'aa-eval', 'adce', 'aggressive-instcombine', 'alignment-from-assumptions', 'always-inline', 'annotation2metadata',
    'annotation-remarks', 'argpromotion', 'assumption-cache-tracker', 'barrier', 'basic-aa', 'basiccg', 'bdce', 'block-freq',
    'branch-prob', 'called-value-propagation', 'callsite-splitting', 'cg-profile', 'constmerge', 'correlated-propagation',
    'dce', 'deadargelim', 'demanded-bits', 'div-rem-pairs', 'domtree', 'dse', 'early-cse', 'early-cse-memssa', 'ee-instrument',
    'elim-avail-extern', 'float2int', 'forceattrs', 'function-attrs', 'globaldce', 'globalopt', 'globals-aa', 'gvn', 'indvars',
    'inferattrs', 'inject-tli-mappings', 'inline', 'instcombine', 'instsimplify', 'ipsccp', 'jump-threading', 'lazy-block-freq',
    'lazy-branch-prob', 'lazy-value-info', 'lcssa', 'lcssa-verification', 'libcalls-shrinkwrap', 'licm', 'loop-accesses',
    'loop-deletion', 'loop-distribute', 'loop-idiom', 'loop-load-elim', 'loop-rotate', 'loop-simplify', 'loop-sink', 'loop-unroll',
    'loop-unswitch', 'loop-vectorize', 'loops', 'lower-constant-intrinsics', 'lower-expect', 'mem2reg', 'memcpyopt', 'memdep',
    'memoryssa', 'mldst-motion', 'openmp-opt-cgscc', 'opt-remark-emitter', 'pgo-memop-opt', 'phi-values', 'postdomtree',
    'profile-summary-info', 'prune-eh', 'reassociate', 'rpo-function-attrs', 'scalar-evolution', 'sccp', 'scoped-noalias-aa',
    'simplifycfg', 'slp-vectorizer', 'speculative-execution', 'sroa', 'strip-dead-prototypes', 'tailcallelim', 'targetlibinfo',
    'tbaa', 'transform-warning', 'tti', 'vector-combine', 'verify'
]
max_line_len = 87
result_line_start = ''
opt_bin = 'opt' if os.name == 'nt' else 'opt-14'
args = '-disable-output -debug-pass=Arguments -enable-new-pm=0'
null_input_args = '< NUL 2>&1' if os.name == 'nt' else '< /dev/null 2>&1'

def get_opts(opt_level):
    command = f'{opt_bin} {args} {null_input_args} {opt_level}'
    opt_output = (subprocess.run(command, capture_output=True, shell=True)
        .stdout.decode('utf-8'))
    lines = [opt.strip() for opt in opt_output.strip().replace('\r', '').split('\n')]
    line_beginning = 'Pass Arguments:  '
    size = len(line_beginning)
    for line in lines:
        assert line.startswith(line_beginning)
    lines = [line[size:] for line in lines]
    opts = sum([line.split() for line in lines], [])
    for opt in opts:
        assert opt.startswith('-') and not opt.startswith('--')
    opts = [opt[1:] for opt in opts]

    result_str = result_line_start
    current_len = len(result_str)
    for opt in opts:
        assert opt in implemented_opts, f'{opt} is not implemented!'
        s = f'"{opt}", '
        next_len = current_len + len(s)
        if next_len > max_line_len:
            result_str = result_str[:-1] # remove last space
            result_str += '\n'
            result_str += result_line_start
            current_len = len(result_line_start)
        result_str += s
        current_len += len(s)
    assert result_str[-1] == ' '
    result_str = result_str[:-1]
    result_str += '\n'
    return result_str

def read_file(file_name):
    try:
        with open(file_name, 'r') as file:
            return file.read().replace('\r', '')
    except:
        pass
    return ''

def write_file(file_name, contents):
    with open(file_name, 'w') as file:
        file.write(contents)

for opt_level, file_name in [ ('-O1', 'src/opts_O1.inc'), ('-O2', 'src/opts_O2.inc'), ('-O3', 'src/opts_O3.inc') ]:
    file_contents = read_file(file_name)
    opts = get_opts(opt_level)
    if file_contents != opts:
        write_file(file_name, opts)
