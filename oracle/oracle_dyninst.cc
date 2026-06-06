#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <set>

#include <sstream>
#include <climits>
#include <cstring>
#include <fstream>

#include "dyninst_handle.h"

using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using std::set;
using std::string;
using std::vector;


BPatch bpatch;
BPatch_function * save_rdi;
BPatch_function * restore_rdi;
BPatch_function * oracle_trap_hit;

extern u64 *trap_block_ids;

extern std::map<int, BPatchSnippetHandle *> snippet_handles;

int verbose = 0;
int dynfix = 0;

set<string> skipLibraries;
void __oracle_initSkipLibraries()
{
    /* List of shared libraries to skip instrumenting. */
    skipLibraries.insert("liboracle_dyninst.so");
    skipLibraries.insert("libc.so.6");
    skipLibraries.insert("libc.so.7");
    skipLibraries.insert("ld-2.5.so");
    skipLibraries.insert("ld-linux.so.2");
    skipLibraries.insert("ld-lsb.so.3");
    skipLibraries.insert("ld-linux-x86-64.so.2");
    skipLibraries.insert("ld-lsb-x86-64.so");
    skipLibraries.insert("ld-elf.so.1");
    skipLibraries.insert("ld-elf32.so.1");
    skipLibraries.insert("libstdc++.so.6");
    return;
}

BPatch_function *__oracle_findFuncByName(BPatch_image *appImage, char *curFuncName)
{
    BPatch_Vector<BPatch_function *> funcs;
    if (NULL == appImage->findFunction(curFuncName, funcs) || !funcs.size() || NULL == funcs[0])
    {
        cerr << "Failed to find " << curFuncName << " function." << endl;
        return NULL;
    }

    return funcs[0];
}


int __oracle_insert_trap(BPatch_addressSpace *appBin, char *curFuncName, BPatch_point *curBlk, unsigned long curBlkAddr, unsigned int curBlkSize, unsigned int curBlkID)
{
    /* Verify curBlk is instrumentable. */
    if (curBlk == NULL)
    {
        cerr << "Failed to find entry at 0x" << std::hex << curBlkAddr << std::endl;
        return EXIT_FAILURE;
    }
    BPatch_Vector<BPatch_snippet *> instArgsDynfix;
    BPatch_Vector<BPatch_snippet *> instArgs;

    BPatch_constExpr argCurBlkID(curBlkID);
    instArgs.push_back(&argCurBlkID);

    BPatch_funcCallExpr instExprSaveRdi(*save_rdi, instArgsDynfix);
    BPatch_funcCallExpr instExprRestRdi(*restore_rdi, instArgsDynfix);
    BPatch_funcCallExpr instExproracle(*oracle_trap_hit, instArgs);
    BPatchSnippetHandle *handle;

    /* RDI fix handling. */
    if (dynfix)
        handle = appBin->insertSnippet(instExprSaveRdi, *curBlk, BPatch_callBefore, BPatch_lastSnippet);

    /* Instruments the basic block. */
    handle = appBin->insertSnippet(instExproracle, *curBlk, BPatch_callBefore, BPatch_lastSnippet);
    snippet_handles[curBlkID] = handle;

    /* Wrap up RDI fix handling. */
    if (dynfix)
        handle = appBin->insertSnippet(instExprRestRdi, *curBlk, BPatch_callBefore, BPatch_lastSnippet);
    /* Verify instrumenting worked. If all good, advance blkIndex and return. */
    if (!handle)
    {
        cerr << "Failed to insert oracle callback at 0x" << std::hex << curBlkAddr << std::endl;
    }

    /* Print some useful info, if requested. */
    if (verbose)
        cout << "Inserted oracle callback at 0x" << std::hex << curBlkAddr << " of " << curFuncName << " of size " << std::dec << curBlkSize << std::endl;

    return 0;
}

void __oracle_iterate_blocks(BPatch_addressSpace *appBin, vector < BPatch_function * >::iterator funcIter, int *blkIndex) {

	/* Extract the function's name, and its pointer from the parent function vector. */
	BPatch_function *curFunc = *funcIter;
	char curFuncName[1024];
	curFunc->getName(curFuncName, 1024); 
	/* Extract the function's CFG. */
	BPatch_flowGraph *curFuncCFG = curFunc->getCFG();
	if (!curFuncCFG) {
		cerr << "Failed to find CFG for function " << curFuncName << endl;
		return;
	}
	/* Extract the CFG's basic blocks and verify the number of blocks isn't 0. */
	BPatch_Set < BPatch_basicBlock * >curFuncBlks;
	if (!curFuncCFG->getAllBasicBlocks(curFuncBlks)) {
		cerr << "Failed to find basic blocks for function " << curFuncName << endl;
		return;
	} 
	if (curFuncBlks.size() == 0) {
		cerr << "No basic blocks for function " << curFuncName << endl;
		return;
	}
	/* Set up this function's basic block iterator and start iterating. */
	BPatch_Set < BPatch_basicBlock * >::iterator blksIter;
	for (blksIter = curFuncBlks.begin(); blksIter != curFuncBlks.end(); blksIter++) {
		/* Get the current basic block, and its size and address. */
		BPatch_point *curBlk = (*blksIter)->findEntryPoint();
		unsigned int curBlkSize = (*blksIter)->size();	 
		/* Compute the basic block's adjusted address.	*/
		unsigned long curBlkAddr = (*blksIter)->getStartAddress();
		/* Non-PIE binary address correction. */ 
		curBlkAddr = curBlkAddr - (long) 0x400000;
		unsigned int curBlkID = *blkIndex;
		/* Other basic blocks to ignore. */
        string functionName(curFuncName);
        if (skipFunctions.count(functionName))
            continue;
        if ((strstr(functionName.data(), "__oracle")) != NULL)
            continue;
        __oracle_insert_trap(appBin, curFuncName, curBlk, curBlkAddr, curBlkSize, curBlkID);
		(*blkIndex)++;
		continue;
	}
	return;
}



void __oracle_init_dyninst(int argc, char ** argv) {
    int processId = getpid(); 
    BPatch_process *appProc = bpatch.processAttach(argv[0], processId);
    BPatch_addressSpace *app = appProc;
    BPatch_image *appImage = app->getImage();
    int blkIndex = 0;

    save_rdi = __oracle_findFuncByName(appImage, (char *)"__oracle_save_rdi");
    restore_rdi = __oracle_findFuncByName(appImage, (char *)"__oracle_restore_rdi");
    oracle_trap_hit = __oracle_findFuncByName(appImage, (char *)"__oracle_trap_hit");
    if (save_rdi == NULL || restore_rdi == NULL || oracle_trap_hit == NULL) {
        cerr << "Failed to find important functions" << endl;
        exit(1);
    }
    vector<BPatch_module *> *modules = appImage->getModules();
    vector<BPatch_module *>::iterator moduleIter;
    for (moduleIter = modules->begin(); moduleIter != modules->end(); ++moduleIter)
    {
        /* Extract module name and verify whether it should be skipped based. */
        char curModuleName[1024];
        (*moduleIter)->getName(curModuleName, 1024);
        if ((*moduleIter)->isSharedLib())
        {
            if (skipLibraries.find(curModuleName) != skipLibraries.end())
            {
                if (verbose)
                {
                    cout << "Skipping library: " << curModuleName << endl;
                }
                continue;
            }
        }

        /* Extract the module's functions and iterate through its basic blocks. */
        vector<BPatch_function *> *funcsInModule = (*moduleIter)->getProcedures();
        vector<BPatch_function *>::iterator funcIter;

        for (funcIter = funcsInModule->begin(); funcIter != funcsInModule->end(); ++funcIter)
        {
            /* Go through each function's basic blocks and insert callbacks accordingly. */
            __oracle_iterate_blocks(app, funcIter, &blkIndex);
        }
    }
    appProc->continueExecution();
    // while (!appProc->isTerminated())
    // {
    //     bpatch.waitForStatusChange();
    // }
}