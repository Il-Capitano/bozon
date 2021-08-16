import subprocess

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
    'memoryssa', 'mldst-motion', 'openmpopt', 'opt-remark-emitter', 'pgo-memop-opt', 'phi-values', 'postdomtree',
    'profile-summary-info', 'prune-eh', 'reassociate', 'rpo-function-attrs', 'scalar-evolution', 'sccp', 'scoped-noalias-aa',
    'simplifycfg', 'slp-vectorizer', 'speculative-execution', 'sroa', 'strip-dead-prototypes', 'tailcallelim', 'targetlibinfo',
    'tbaa', 'transform-warning', 'tti', 'vector-combine', 'verify'
]
max_line_len = 87
result_line_start = ''

def get_opts(opt_level):
    opt_output = (subprocess.run(f'opt {opt_level} -disable-output -debug-pass=Arguments < NUL 2>&1', capture_output=True, shell=True)
        .stdout.decode('utf-8'))
    lines = [opt.strip() for opt in opt_output.strip().split('\r\n')]
    line_beginning = 'Pass Arguments:  '
    size = len(line_beginning)
    for line in lines:
        assert line.startswith(line_beginning)
    lines = [line[size:] for line in lines]
    opts = sum([line.split() for line in lines], [])
    for opt in opts:
        assert opt.startswith('-') and not opt.startswith('--')
    opts = [opt[1:] for opt in opts]

    for opt in opts:
        assert opt in implemented_opts, f'{opt} is not implemented!'

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
    return result_str

for opt_level in [ '-O1', '-O2', '-O3' ]:
    print(f'======== {opt_level} ========')
    print(get_opts(opt_level))
