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
    // https://stackoverflow.com/questions/37657326/go-passing-argv-to-c-function
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
    c_argc := C.int(len(argv))
    c_argv := C.allocArgv(c_argc)
    defer C.free(c_argv)

    for i, arg := range argv {
        tmp := C.CString(arg)
        defer C.free(unsafe.Pointer(tmp))
        C.addArg(c_argv, tmp, C.int(i))
    }
    cBytes := C.CBytes(fileBytes)
    defer C.free(cBytes)
    cLenBytes := C.int(len(fileBytes))


    // BEGIN redirect
    fmt.Println("[Go Code] Redirecting STDOUT...");
    // Clone Stdout to origStdout.
    origStdout, err := syscall.Dup(syscall.Stdout)
    if err != nil {
        log.Fatal(err)
    }

    r, w, err := os.Pipe()
    if err != nil {
        log.Fatal(err)
    }

    // Clone the pipe's writer to the actual Stdout descriptor; from this point
    // on, writes to Stdout will go to w.
    if err = syscall.Dup2(int(w.Fd()), syscall.Stdout); err != nil {
        log.Fatal(err)
    }

    // Background goroutine that drains the reading end of the pipe.
    out := make(chan []byte)
    go func() {
        var b bytes.Buffer
        io.Copy(&b, r)
        out <- b.Bytes()
    }()

    // END redirect

    C.execMachO((*C.char)(cBytes), cLenBytes, c_argc, c_argv)

    // BEGIN redirect

    C.fflush(nil)
    w.Close()
    syscall.Close(syscall.Stdout)

    // Rendezvous with the reading goroutine.
    b := <-out

    // Restore original Stdout.
    syscall.Dup2(origStdout, syscall.Stdout)
    syscall.Close(origStdout)
    fmt.Println("[Go Code] Successfully recovered from bin exit(), captured the following output:\n\n", string(b))


    // END redirect
    C._Exit(0)
}
