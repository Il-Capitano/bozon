import subprocess
import os

config = ''

if os.name == 'nt':
    llc_process = subprocess.Popen(
        [ 'llvm-config', '--host-target' ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding='utf-8'
    )
    stdout, stderr = llc_process.communicate()
    default_target = stdout.strip()
else:
    llc_process = subprocess.Popen(
        [ 'llvm-config-16', '--host-target' ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding='utf-8'
    )
    stdout, stderr = llc_process.communicate()
    default_target = stdout.strip()

config += f'#define BOZON_CONFIG_NATIVE_TARGET "{default_target}"\n'

config = f'#ifndef CONFIG_H\n#define CONFIG_H\n\n{config}\n#endif // CONFIG_H\n'

try:
    with open('src/config.h', 'r') as config_file:
        config_file_contents = config_file.read()
except:
    config_file_contents = ''

if config_file_contents != config:
    with open('src/config.h', 'w') as config_file:
        config_file.write(config)
