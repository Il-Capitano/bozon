# bozon

```
import std::format;

function main()
{
    std::print("Hello {}!\n", "World");
}
```

Bozon is a general-purpose compiled programming language that aims to provide high performance without sacrificing safety or ease-of-use.

## Build

The only tested platforms at the moment are Windows with [MSYS2](https://www.msys2.org/) and Ubuntu 22.04+.

Required libraries:
* LLVM 22.x
* dwarfstack (Windows only)
* [libbacktrace](https://github.com/ianlancetaylor/libbacktrace) (Linux only)

Building is done with [`cppb`](https://github.com/Il-Capitano/cppb), and uses the latest versions of Clang and LLD by default:
```bash
# You may need to do this if you get an error:
$ touch ryu_format.windows.bc
$ touch ryu_format.linux.bc

# Release build:
$ cppb build --build-mode release

# Debug build:
$ cppb build
```
