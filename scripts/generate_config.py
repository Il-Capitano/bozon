import subprocess
import os
import argparse

def get_llvm_info():
    try:
        llvm_config = 'llvm-config' if os.name == 'nt' else 'llvm-config-16'
        llvm_config_process = subprocess.Popen(
            [ llvm_config, '--host-target' ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            encoding='utf-8'
        )
        stdout, stderr = llvm_config_process.communicate()
        return {
            'default_target': stdout.strip()
        }
    except:
        return None

def config_variable_name(s):
    return f'backend_{s.lower()}'
def config_macro_name(s):
    return f'BOZON_CONFIG_BACKEND_{s.upper()}'

native_target_triple_detection = '''#ifndef BOZON_CONFIG_NATIVE_TARGET
#if defined(__x86_64__) || defined(_M_X64)
#define BOZON_CONFIG_NATIVE_TARGET_ARCH "x86_64"
#else
#define BOZON_CONFIG_NATIVE_TARGET_ARCH "unknown"
#endif
#define BOZON_CONFIG_NATIVE_TARGET_VENDOR "unknown"
#if defined(_WIN32)
#define BOZON_CONFIG_NATIVE_TARGET_OS "windows"
#elif defined(__linux__)
#define BOZON_CONFIG_NATIVE_TARGET_OS "linux"
#else
#define BOZON_CONFIG_NATIVE_TARGET_OS "unknown"
#endif
#define BOZON_CONFIG_NATIVE_TARGET_ENV "unknown"
#define BOZON_CONFIG_NATIVE_TARGET BOZON_CONFIG_NATIVE_TARGET_ARCH "-" BOZON_CONFIG_NATIVE_TARGET_VENDOR "-" BOZON_CONFIG_NATIVE_TARGET_OS "-" BOZON_CONFIG_NATIVE_TARGET_ENV
#endif
'''

available_backends = [ 'llvm', 'c' ]

parser = argparse.ArgumentParser()
parser.add_argument('--target', help='Default target triple')
available_backends_help = ' '.join((backend.upper() for backend in available_backends))
parser.add_argument('--backends', help=f'Comma separated list of enabled backends; leave empty to enable all backends (available backends: {available_backends_help})')

args = parser.parse_args()

enabled_backends = list(map(lambda s: s.lower(), args.backends.split(','))) if args.backends is not None else available_backends

config = ''

config += 'namespace config\n{\n\n'

llvm_info = get_llvm_info()

if 'LLVM' in enabled_backends and llvm_info is None:
    print('error: unable to find \'llvm-config\'')
    exit(1)

for backend in available_backends:
    if backend in enabled_backends:
        config += f'inline constexpr bool {config_variable_name(backend)} = true;\n'
        config += f'#define {config_macro_name(backend)}\n'
    else:
        config += f'inline constexpr bool {config_variable_name(backend)} = false;\n'

if args.target is not None:
    config += f'#define BOZON_CONFIG_NATIVE_TARGET "{args.target}"\n'
elif llvm_info is not None:
    target = llvm_info['default_target']
    config += f'#define BOZON_CONFIG_NATIVE_TARGET "{target}"\n'
else:
    config += native_target_triple_detection

config += '\n} // namespace config\n'

# add include guards
config = f'#ifndef CONFIG_H\n#define CONFIG_H\n\n{config}\n#endif // CONFIG_H\n'

try:
    with open('src/config.h', 'r') as config_file:
        config_file_contents = config_file.read()
except:
    config_file_contents = ''

if config_file_contents != config:
    with open('src/config.h', 'w') as config_file:
        config_file.write(config)
