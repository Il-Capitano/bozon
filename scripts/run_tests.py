import subprocess
import os
import glob

success_test_files = glob.glob("tests/success/*.bz")
warning_test_files = glob.glob("tests/warning/*.bz")
error_test_files = glob.glob("tests/error/*.bz")
bozon = 'bin\\windows-debug\\bozon.exe' if os.name == 'nt' else './bin/linux-debug/bozon'
flags = [ '--stdlib-dir', 'bozon-stdlib', '-Wall', '--emit=null', '-Itests/import', '-Ocomptime-opt-level=3' ]

# enable colors for windows
if os.name == 'nt':
    os.system('color')

clear = "\033[0m";

black   = "\033[30m"
red     = "\033[31m"
green   = "\033[32m"
yellow  = "\033[33m"
blue    = "\033[34m"
magenta = "\033[35m"
cyan    = "\033[36m"
white   = "\033[37m"

bright_black   = "\033[90m"
bright_red     = "\033[91m"
bright_green   = "\033[92m"
bright_yellow  = "\033[93m"
bright_blue    = "\033[94m"
bright_magenta = "\033[95m"
bright_cyan    = "\033[96m"
bright_white   = "\033[97m"

file_name_print_length = 3 + max((
    57,
    max((len(test_file) for test_file in success_test_files)),
    max((len(test_file) for test_file in warning_test_files)),
    max((len(test_file) for test_file in error_test_files)),
))

error = False

for test_file in success_test_files:
    print(f'    {test_file:.<{file_name_print_length}}', end='', flush=True)
    process = subprocess.run(
        [ bozon, *flags, test_file ],
        capture_output=True
    )
    stdout = process.stdout.decode('utf-8')
    stderr = process.stderr.decode('utf-8')
    if process.returncode == 0 and stdout == '' and stderr == '':
        print(f'{bright_green}OK{clear}')
    else:
        error = True
        print(f'{bright_red}FAIL{clear}')
        if stdout != '':
            print('stdout:')
            print(stdout)
        if stderr != '':
            print('stderr:')
            print(stderr)
        print(f'exit code: {process.returncode}')

for test_file in warning_test_files:
    print(f'    {test_file:.<{file_name_print_length}}', end='', flush=True)
    process = subprocess.run(
        [ bozon, *flags, test_file ],
        capture_output=True
    )
    stdout = process.stdout.decode('utf-8')
    stderr = process.stderr.decode('utf-8')
    if process.returncode == 0 and (stdout != '' or stderr != ''):
        print(f'{bright_green}OK{clear}')
    else:
        error = True
        print(f'{bright_red}FAIL{clear}')
        if stdout != '':
            print('stdout:')
            print(stdout)
        if stderr != '':
            print('stderr:')
            print(stderr)
        print(f'exit code: {process.returncode}')

for test_file in error_test_files:
    print(f'    {test_file:.<{file_name_print_length}}', end='', flush=True)
    process = subprocess.run(
        [ bozon, *flags, test_file ],
        capture_output=True
    )
    stdout = process.stdout.decode('utf-8')
    stderr = process.stderr.decode('utf-8')
    process_rerun = subprocess.run(
        [ bozon, *flags, '--return-zero-on-error', test_file ],
        capture_output=True
    )
    if process.returncode != 0 and process_rerun.returncode == 0:
        print(f'{bright_green}OK{clear}')
    else:
        error = True
        print(f'{bright_red}FAIL{clear}')
        if stdout != '':
            print('stdout:')
            print(stdout)
        if stderr != '':
            print('stderr:')
            print(stderr)
        print(f'exit code: {process.returncode}')

if error:
    exit(1)
