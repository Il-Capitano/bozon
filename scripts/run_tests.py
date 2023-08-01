import subprocess
import os
import glob
import re

def remove_ansi_colors(s):
    ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
    return ansi_escape.sub('', s)

class ProcessPool:
    def __init__(self, commands):
        self.commands = commands
        self.processes = []
        self.current_index = 0

    def start_next_process(self):
        assert self.current_index != len(self.commands)
        self.processes.append(subprocess.Popen(
            self.commands[self.current_index],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            encoding='utf-8'
        ))
        self.current_index += 1

    def start(self, process_count=None):
        if process_count is None:
            process_count = os.cpu_count()
        process_count = min(process_count, len(self.commands));
        for _ in range(process_count):
            self.start_next_process()

    def get_next_result(self):
        if len(self.processes) == 0:
            return None

        stdout, stderr = self.processes[0].communicate()
        returncode = self.processes[0].returncode
        command = self.processes[0].args
        self.processes.pop(0)
        if self.current_index != len(self.commands):
            self.start_next_process()

        return command, remove_ansi_colors(stdout), remove_ansi_colors(stderr), returncode

success_test_files = glob.glob("tests/success/**/*.bz", recursive=True)
warning_test_files = glob.glob("tests/warning/**/*.bz", recursive=True)
error_test_files = glob.glob("tests/error/**/*.bz", recursive=True)
bozon = 'bin\\windows-debug\\bozon.exe' if os.name == 'nt' else './bin/linux-debug/bozon'
flags = [ '--stdlib-dir', 'bozon-stdlib', '-Wall', '--debug-no-emit-file', '-Itests/import' ]

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

def print_test_fail_info(command, stdout, stderr, rc, wanted_messages=None):
    print(' '.join(command))
    if stdout != '':
        print('stdout:')
        print(stdout)
    if stderr != '':
        print('stderr:')
        print(stderr)
    print(f'exit code: {rc}')
    if wanted_messages is not None:
        print('wanted error messages:')
        for m in wanted_messages:
            print(m)

total_passed = 0
failed_tests_info = []

success_commands = [[ bozon, *flags, test_file ] for test_file in success_test_files]
success_pool = ProcessPool(success_commands)
success_pool.start()
for test_file in success_test_files:
    print(f'    {test_file:.<{file_name_print_length}}', end='', flush=True)
    command, stdout, stderr, rc = success_pool.get_next_result()
    if rc == 0 and stdout == '' and stderr == '':
        total_passed += 1
        print(f'{bright_green}OK{clear}')
    else:
        print(f'{bright_red}FAIL{clear}')
        failed_tests_info.append((test_file, command, stdout, stderr, rc))
        print_test_fail_info(*failed_tests_info[-1][1:])

warning_commands = [[ bozon, *flags, test_file ] for test_file in warning_test_files]
warning_pool = ProcessPool(warning_commands)
warning_pool.start()
for test_file in warning_test_files:
    print(f'    {test_file:.<{file_name_print_length}}', end='', flush=True)
    command, stdout, stderr, rc = warning_pool.get_next_result()
    if rc == 0 and (stdout != '' or stderr != ''):
        total_passed += 1
        print(f'{bright_green}OK{clear}')
    else:
        print(f'{bright_red}FAIL{clear}')
        failed_tests_info.append((test_file, command, stdout, stderr, rc))
        print_test_fail_info(*failed_tests_info[-1][1:])

def get_error_messages(test_file):
    result = []
    with open(test_file, 'r') as f:
        for line in f:
            if not line.startswith('// error:') and not line.startswith('// note:') and not line.startswith('// warning:') and not line.startswith('// suggestion:'):
                break
            result.append(line[3:-1])
    return result

def get_error_messages_from_compiler_output(output):
    result = []
    while True:
        indices = [output.find(m) for m in ('error:', 'note:', 'warning:', 'suggestion:') if output.find(m) != -1]
        i = min(indices) if len(indices) != 0 else -1
        if i == -1:
            break
        line_end = output.find('\n', i);
        result.append(output[i:line_end])
        output = output[line_end:]
    return result

error_commands = [
    [ bozon, *flags, test_file ] if p == 0 else [ bozon, *flags, '--return-zero-on-error', test_file ]
    for test_file in error_test_files
    for p in (0, 1)
]
error_pool = ProcessPool(error_commands)
error_pool.start()
for test_file in error_test_files:
    print(f'    {test_file:.<{file_name_print_length}}', end='', flush=True)
    command, stdout, stderr, rc = error_pool.get_next_result()
    rerun_command, rerun_stdout, rerun_stderr, rerun_rc = error_pool.get_next_result()
    wanted_messages = get_error_messages(test_file)
    compiler_messages = get_error_messages_from_compiler_output(stderr)

    wanted_error_fail = len(wanted_messages) == 0 or wanted_messages != compiler_messages

    if not wanted_error_fail and rc != 0 and rerun_rc == 0:
        total_passed += 1
        print(f'{bright_green}OK{clear}')
    else:
        print(f'{bright_red}FAIL{clear}')
        failed_tests_info.append((test_file, command, stdout, stderr, rc, wanted_messages))
        print_test_fail_info(*failed_tests_info[-1][1:])

total_test_count = len(success_test_files) + len(warning_test_files) + len(error_test_files)
passed_percentage = 100 * total_passed / total_test_count

if total_passed == total_test_count:
    print(f'{bright_green}{total_passed}/{total_test_count}{clear} ({bright_green}{passed_percentage:.2f}%{clear}) tests passed')
else:
    print(f'{bright_red}{total_passed}/{total_test_count}{clear} ({bright_red}{passed_percentage:.2f}%{clear}) tests passed')

for test_file, *info in failed_tests_info:
    print('')
    print(f'{bright_red}FAILED:{clear} {test_file}:')
    print_test_fail_info(*info);

if len(failed_tests_info) != 0:
    exit(1)
