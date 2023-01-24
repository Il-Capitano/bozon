import subprocess
import os
import glob

success_test_files = glob.glob("tests/success/**/*.bz", recursive=True)
warning_test_files = glob.glob("tests/warning/**/*.bz", recursive=True)
error_test_files = glob.glob("tests/error/**/*.bz", recursive=True)
bozon = 'bin\\windows-debug\\bozon.exe' if os.name == 'nt' else './bin/linux-debug/bozon'
flags = [ '--stdlib-dir', 'bozon-stdlib', '-Wall', '--emit=null', '-Itests/import' ]

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
    max((len(test_file) for test_file in success_test_files)) if len(success_test_files) != 0 else 0,
    max((len(test_file) for test_file in warning_test_files)) if len(warning_test_files) != 0 else 0,
    max((len(test_file) for test_file in error_test_files)) if len(error_test_files) != 0 else 0,
))

error = False

def run_process(args):
    process = subprocess.run(args, capture_output=True)
    stdout = process.stdout.decode('utf-8')
    stderr = process.stderr.decode('utf-8')
    return stdout, stderr, process.returncode

total_passed = 0

for test_file in success_test_files:
    print(f'    {test_file:.<{file_name_print_length}}', end='', flush=True)
    stdout, stderr, rc = run_process([ bozon, *flags, test_file ])
    if rc == 0 and stdout == '' and stderr == '':
        total_passed += 1
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
        print(f'exit code: {rc}')

for test_file in warning_test_files:
    print(f'    {test_file:.<{file_name_print_length}}', end='', flush=True)
    stdout, stderr, rc = run_process([ bozon, *flags, test_file ])
    if rc == 0 and (stdout != '' or stderr != ''):
        total_passed += 1
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
        print(f'exit code: {rc}')

for test_file in error_test_files:
    print(f'    {test_file:.<{file_name_print_length}}', end='', flush=True)
    stdout, stderr, rc = run_process([ bozon, *flags, test_file ])
    rerun_stdout, rerun_stderr, rerun_rc = run_process([ bozon, *flags, '--return-zero-on-error', test_file ])
    if rc != 0 and rerun_rc == 0:
        total_passed += 1
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
        print(f'exit code: {rc}')

total_test_count = len(success_test_files) + len(warning_test_files) + len(error_test_files)
passed_percentage = 100 * total_passed / total_test_count

if total_passed == total_test_count:
    print(f'{bright_green}{total_passed}/{total_test_count}{clear} ({bright_green}{passed_percentage:.2f}%{clear}) tests passed')
else:
    print(f'{bright_red}{total_passed}/{total_test_count}{clear} ({bright_red}{passed_percentage:.2f}%{clear}) tests passed')

if error:
    exit(1)
