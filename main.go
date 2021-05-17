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
import "fmt"

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
    C.execMachO((*C.char)(cBytes), cLenBytes, c_argc, c_argv)
}
