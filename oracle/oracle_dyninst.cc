#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <stdlib.h>
#include <sstream>
#include <climits>
#include <cstring>
#include <fstream>

using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using std::string;
using std::vector;

BPatch bpatch;

void __trace_save_rdi()
{
    savedDi = rdi;
    /*
    asm("pop %rax"); // take care of rip
    asm("push %rdi");
    asm("push %rax");
    */
}

void __trace_restore_rdi()
{
    rdi = savedDi;
    /*
    asm("pop %rax"); // take care of rip
    asm("pop %rdi");
    asm("push %rax");
    */
}

void __oracle_trap_hit() {
    __asm__ volatile("int3");
}


void __oracle_init_dyninst(int argc, char ** argv) {
    BPatch_process *appProc = bpatch.processAttach(argv[0], processId);
    BPatch_addressSpace *app = appProc;
    BPatch_image *appImage = app->getImage();
    vector<BPatch_module *> *modules = appImage->getModules();
    vector<BPatch_module *>::iterator moduleIter;
    vector<BPatch_module *>::iterator moduleIter;
    
    appProc->continueExecution();
    while (!appProc->isTerminated())
    {
        bpatch.waitForStatusChange();
    }
}