package main

//#cgo LDFLAGS: -lm -framework Foundation
//#cgo CFLAGS: -Wno-error=implicit-function-declaration -Wno-deprecated-declarations -Wno-format -Wno-int-conversion
//#include <stdio.h>
//#include <stdlib.h>
//#include "shell_memory.h"
import "C"
import "os"
import "os/exec"
import "unsafe"
import "io/ioutil"
import "io"
import "bytes"
import "fmt"
import "syscall"
import "log"
// import "github.com/djhohnstein/macos_shell_memory/stdouterr"


func main() {
    if len(os.Args) < 2 {
        panic("Require at least one argument to execute.")
    }
    argv := os.Args[1:]
    path, err := exec.LookPath(argv[0])
    if err != nil {
        panic(fmt.Sprintf("%s is not installed in $PATH", argv[0]))
    }
    argv[0] = path
    fileBytes, err := ioutil.ReadFile(path)
    if err != nil {
        panic(fmt.Sprintf("could not read file %s. Reason: %s", path, err.Error()))
    }
    // Create our C-esque arguments
    c_argc := C.int(len(argv))
    c_argv := C.allocArgv(c_argc)
    defer C.free(c_argv)

    // Convert each argv to a char*
    for i, arg := range argv {
        tmp := C.CString(arg)
        defer C.free(unsafe.Pointer(tmp))
        C.addArg(c_argv, tmp, C.int(i))
    }
    cBytes := C.CBytes(fileBytes)
    defer C.free(cBytes)
    cLenBytes := C.int(len(fileBytes))


    // Redirect STD handles to pipes...
    fmt.Println("[Go Code] Redirecting STDOUT...");

    // Clone Stdout to origStdout.
    origStdout, err := syscall.Dup(syscall.Stdout)
    if err != nil {
        log.Fatal(err)
    }
    // Clone Stdout to origStdout.
    origStderr, err := syscall.Dup(syscall.Stderr)
    if err != nil {
        log.Fatal(err)
    }

    rStdout, wStdout, err := os.Pipe()
    if err != nil {
        log.Fatal(err)
    }

    rStderr, wStderr, err := os.Pipe()
    if err != nil {
        log.Fatal(err)
    }


    reader := io.MultiReader(rStdout, rStderr)

    // Clone the pipe's writer to the actual Stdout descriptor; from this point
    // on, writes to Stdout will go to w.
    if err = syscall.Dup2(int(wStderr.Fd()), syscall.Stdout); err != nil {
        log.Fatal(err)
    }

    // Clone the pipe's writer to the actual Stderr descriptor; from this point
    // on, writes to Stderr will go to w.
    if err = syscall.Dup2(int(wStderr.Fd()), syscall.Stderr); err != nil {
        log.Fatal(err)
    }

    // Background goroutine that drains the reading end of the pipe.
    out := make(chan []byte)
    go func() {
        var b bytes.Buffer
        io.Copy(&b, reader)
        out <- b.Bytes()
    }()

    // END redirect

    C.execMachO((*C.char)(cBytes), cLenBytes, c_argc, c_argv)

    // BEGIN redirect

    C.fflush(nil)
    wStdout.Close()
    wStderr.Close()
    syscall.Close(syscall.Stdout)
    syscall.Close(syscall.Stderr)
    // Rendezvous with the reading goroutine.
    b := <-out

    // Restore original Stdout and Stderr.
    syscall.Dup2(origStdout, syscall.Stdout)
    syscall.Dup2(origStderr, syscall.Stderr)
    syscall.Close(origStdout)
    syscall.Close(origStderr)
    fmt.Println("[Go Code] Successfully recovered from bin exit(), captured the following output:\n\n", string(b))


    // END redirect
    C._Exit(0)
}
