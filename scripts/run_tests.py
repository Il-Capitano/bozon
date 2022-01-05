import subprocess
import os
import glob

test_files = glob.glob("tests/*.bz")
bozon = 'bin\\windows-debug\\bozon.exe' if os.name == 'nt' else 'bin/linux-debug/bozon'

# enable colors for windows
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

for test_file in test_files:
    process = subprocess.run(
        [ bozon, '--stdlib-dir', 'bozon-stdlib', '-Wall', '--emit=null', test_file ],
        capture_output=True, shell=True
    )
    stdout = process.stdout.decode('utf-8')
    stderr = process.stderr.decode('utf-8')
    if process.returncode == 0:
        print(f'    {test_file:.<60}{bright_green}OK{clear}')
    else:
        print(f'    {test_file:.<60}{bright_red}FAIL{clear}')
        if stdout != '':
            print('stdout:')
            print(stdout)
        if stderr != '':
            print('stderr:')
            print(stderr)
