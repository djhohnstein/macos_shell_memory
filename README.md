# Execute Thin Mach-O Binaries in Memory

This is a CGo implementation of the initial technique put forward by [Stephanie Archibald](https://blogs.blackberry.com/en/author/stephanie-archibald) in her blog, [Running Executables on macOS From Memory](https://blogs.blackberry.com/en/2017/02/running-executables-on-macos-from-memory).

## Usage

```
./macos_shell_memory [bin] [args]
```

## Description

Given that `[bin]` is in `$PATH`, `[bin]` is loaded into memory and executed with `[args]` (if provided). Stdout and Stderr will be redirected during binary execution. Normally, when a Mach-O binary finishes execution, the program exits and returns back to the caller (like your terminal); however, this exit call, when called from your current process, will exit your loading process.

To disable this functionality, a new `atexit` routine is registered to rewind stack-state back to before the in-memory Mach-O `main()` function ever executed. Doing so causes instability, and as such, we call `C._Exit` over letting the Go program exit normally.

This weaponization could be modified to point to any thin Mach-O binary, and enforcing the `[bin]` to be in `$PATH` is an arbitrary constraint I've added.

## Important Caveats

This works _only_ for thin Mach-O binaries. This can be seen by issuing the following command:

```
codesign -vvvv -d /path/to/bin
```

For example, `codesign -vvvv -d /bin/ps` returns:
```
Executable=/bin/ps
Identifier=com.apple.ps
Format=Mach-O thin (x86_64)
... snip ...
```

There are certain nuances that I haven't worked out for fat and ARM binaries. Doing so will cause the program to irrecoverably segfault.

## Examples

```
╭─djh@bifrost ~/go/src/github.com/djhohnstein/macos_shell_memory  ‹main*›
╰─$ ./macos_shell_memory ps                                                                      [21/05/20 |12:18PM]
[Go Code] Redirecting STDOUT...
[Go Code] Successfully recovered from bin exit(), captured the following output:

   PID TTY           TIME CMD
72116 ttys000    0:00.00 zsh
47918 ttys003    0:00.00 zsh
78749 ttys003    0:00.01 ./macos_shell_memory ps
  612 ttys004    0:00.00 zsh

╭─djh@bifrost ~/go/src/github.com/djhohnstein/macos_shell_memory  ‹main*›
╰─$ ./macos_shell_memory ls -alht                                                                [21/05/20 |12:18PM]
[Go Code] Redirecting STDOUT...
[Go Code] Successfully recovered from bin exit(), captured the following output:

 total 4752
drwxr-xr-x  13 djh  staff   416B May 20 12:18 .git
-rwxr-xr-x   1 djh  staff   2.3M May 20 12:16 macos_shell_memory
drwxr-xr-x  11 djh  staff   352B May 20 11:50 .
-rw-r--r--   1 djh  staff     0B May 20 11:50 README.md
-rw-r--r--   1 djh  staff   3.1K May 20 11:30 main.go
-rw-r--r--   1 djh  staff   3.9K May 20 11:23 shell_memory.c
drwxr-xr-x   5 djh  staff   160B May 18 16:04 ..
-rw-r--r--   1 djh  staff   883B May 17 16:43 go.sum
-rw-r--r--   1 djh  staff   253B May 17 16:43 go.mod
-rw-r--r--   1 djh  staff    19B May 16 17:15 .gitignore
-rw-r--r--   1 djh  staff   143B May 16 17:09 shell_memory.h

```
