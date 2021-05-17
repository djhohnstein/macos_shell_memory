#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <assert.h>

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/dyld.h>

#include "shell_memory.h"


// Allocate a new char** pointer to hold new arguments
void* allocArgv(int argc) {
    char** argv = malloc(sizeof(char *) * argc + 1);
    argv[argc] = NULL;
    return (void*)argv;
}

// Stuff arguments into the char** pointer as doing this
// strictly in Go sucks.
void addArg(void* argv, char* arg, int i) {
    ((char**)argv)[i] = arg;
}

// Find the entry point command by searching through base's load commands.
// This will give us the offset required to execute the MachO
int find_epc(unsigned long base, struct entry_point_command **entry) {
	struct mach_header_64 *mh;
	struct load_command *lc;

	unsigned long text = 0;

	*entry = NULL;

	mh = (struct mach_header_64 *)base;
	lc = (struct load_command *)(base + sizeof(struct mach_header_64));
	for(int i=0; i<mh->ncmds; i++) {
		if(lc->cmd == LC_MAIN) {	//0x80000028
			*entry = (struct entry_point_command *)lc;
			return 0;
		}

		lc = (struct load_command *)((unsigned long)lc + lc->cmdsize);
	}

	return 1;
}

// Executes a MachO (given by fileBytes) with requisite arguments.
int execMachO(char* fileBytes, int szFile, int argc, void* argv) {
    NSObjectFileImage fileImage = NULL;
	NSModule module = NULL;
	NSSymbol symbol = NULL;
    void* pSymbolAddress = NULL;

	int(*main)(int, char**, char**, char**);
    int type = ((int *)fileBytes)[3];
	if(type != 0x8) ((int *)fileBytes)[3] = 0x8; //change to mh_bundle type
	NSCreateObjectFileImageFromMemory(fileBytes, szFile, &fileImage);

	if(fileImage == NULL){
		return -1;
	}
	module = NSLinkModule(fileImage, "module", NSLINKMODULE_OPTION_PRIVATE |
						                NSLINKMODULE_OPTION_BINDNOW);


    symbol = NSLookupSymbolInModule(module, "__mh_execute_header");

    if(type == 0x2) { //mh_execute
		struct entry_point_command *epc;
        pSymbolAddress = NSAddressOfSymbol(symbol);
		if(find_epc(pSymbolAddress, &epc)) {
			fprintf(stderr, "Could not find ec.\n");
			goto err;
		}

        unsigned long tmp = pSymbolAddress + epc->entryoff;
        printf("Invoking addr: 0x%llX (symbol addr: 0x%llX, epc->entroff is 0x%llX)\n", tmp, pSymbolAddress, epc->entryoff);
        main = (int(*)(int, char**, char**, char**)) (tmp);

        if(main == NULL){
    		printf("Failed to find address of main\n");
    	}

		main(argc, (char**)argv, NULL, NULL);

        NSUnLinkModule(module, NSLINKMODULE_OPTION_PRIVATE | NSLINKMODULE_OPTION_BINDNOW);
    	NSDestroyObjectFileImage(fileImage);
        return 0;
	}
err:
    if (module != NULL) {
        NSUnLinkModule(module, NSLINKMODULE_OPTION_PRIVATE | NSLINKMODULE_OPTION_BINDNOW);
    }
    if (fileImage != NULL) {
        NSDestroyObjectFileImage(fileImage);
    }
    return -1;
}
