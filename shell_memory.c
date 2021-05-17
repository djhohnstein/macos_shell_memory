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

#define EXECUTABLE_BASE_ADDR 0x100000000
#define DYLD_BASE 0x00007fff5fc00000

int IS_SIERRA = -1;


void* allocArgv(int argc) {
    char** argv = malloc(sizeof(char *) * argc + 1);
    argv[argc] = NULL;
    return (void*)argv;
}

void addArg(void* argv, char* arg, int i) {
    ((char**)argv)[i] = arg;
}


int is_sierra(void) {
	// returns 1 if running on Sierra, 0 otherwise
	// this works because /bin/rcp was removed in Sierra
	if(IS_SIERRA == -1) {
		struct stat statbuf;
		IS_SIERRA = (stat("/bin/rcp", &statbuf) != 0);
	}
	return IS_SIERRA;
}

int find_epc(unsigned long base, struct entry_point_command **entry) {
	// find the entry point command by searching through base's load commands

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

int load_from_disk(char *filename, char **buf, unsigned int *size) {
	/*
	 What, you say?  this isn't running from memory!  You're loading from disk!!
	 Put down the pitchforks, please.  Yes, this reads a binary from disk...into
	 memory.  The code is then executed from memory.  This here is a POC; in
	 real life you would probably want to read into buf from a socket.
	 */
	int fd;
	struct stat s;

	if((fd = open(filename, O_RDONLY)) == -1) return 1;
	if(fstat(fd, &s)) return 1;

	*size = s.st_size;

	if((*buf = mmap(NULL, (*size) * sizeof(char), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED | MAP_ANON, -1, 0)) == MAP_FAILED) return 1;
	if(read(fd, *buf, *size * sizeof(char)) != *size) {
		free(*buf);
		*buf = NULL;
		return 1;
	}

	close(fd);

	return 0;
}

int execMachO(char* fileBytes, int szFile, int argc, void* argv) {
    NSObjectFileImage fileImage = NULL;
	NSModule module = NULL;
	NSSymbol symbol = NULL;
    void* pSymbolAddress = NULL;

	//struct stat stat_buf;
	int(*main)(int, char**, char**, char**);
	//printf("setting memory value\n");
    int type = ((int *)fileBytes)[3];
    // printf("type: %08d\n", type);
	if(type != 0x8) ((int *)fileBytes)[3] = 0x8; //change to mh_bundle type
    // printf("type after setting: %08d\n", type);
    // *((uint8_t*)file_bytes + 12) =  0x08;
    //printf("set value\n");
	NSCreateObjectFileImageFromMemory(fileBytes, szFile, &fileImage);
	//printf("created file image\n");
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
        // int(*main)(int, char**, char**, char**) = (int(*)(int, char**, char**, char**))(execute_base + epc->entryoff);
        main = (int(*)(int, char**, char**, char**)) (tmp);
    	//printf("got main\n");
    	if(main == NULL){
    		printf("Failed to find address of main\n");
    	}
        char *env[] = {NULL};

		// char *apple[] = {GLOBAL_ARGV[0], NULL};
        main(argc, (char**)argv, NULL, NULL);
		// main(argc, c_argv, env, apple);
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
