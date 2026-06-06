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
using std::cout;
using std::endl;
using std::cin;
using std::vector;
using std::string;


vector<string> skipLibraries;

BPatch_function * trace_block_hit;
BPatch_function * restore_rdi;
BPatch_function * save_rdi;

int dynfix = 0;

using namespace Dyninst;
BPatch bpatch;


void initSkipLibraries()
{
    /* List of shared libraries to skip instrumenting. */
    skipLibraries.push_back("libUnTracerDyninst.so");
    skipLibraries.push_back("libc.so.6");
    skipLibraries.push_back("libc.so.7");
    skipLibraries.push_back("ld-2.5.so");
    skipLibraries.push_back("ld-linux.so.2");
    skipLibraries.push_back("ld-lsb.so.3");
    skipLibraries.push_back("ld-linux-x86-64.so.2");
    skipLibraries.push_back("ld-lsb-x86-64.so");
    skipLibraries.push_back("ld-elf.so.1");
    skipLibraries.push_back("ld-elf32.so.1");
    skipLibraries.push_back("libstdc++.so.6");
    return;
}

BPatch_function *findFuncByName(BPatch_image *appImage, char *curFuncName)
{
    BPatch_Vector<BPatch_function *> funcs;
    if (NULL == appImage->findFunction(curFuncName, funcs) || !funcs.size() || NULL == funcs[0])
    {
        cerr << "Failed to find " << curFuncName << " function." << endl;
        return NULL;
    }

    return funcs[0];
}

int insert_trace(BPatch_binaryEdit *appBin, char *curFuncName, BPatch_point *curBlk, unsigned long curBlkAddr, unsigned int curBlkSize, unsigned int curBlkID)
{
    /* Verify curBlk is instrumentable. */
    if (curBlk == NULL)
    {
        cerr << "Failed to find entry at 0x" << hex << curBlkAddr << endl;
        return EXIT_FAILURE;
    }
    BPatch_Vector<BPatch_snippet *> instArgsDynfix;
    BPatch_Vector<BPatch_snippet *> instArgs;

    BPatch_constExpr argCurBlkID(curBlkID);
    instArgs.push_back(&argCurBlkID);

    BPatch_funcCallExpr instExprSaveRdi(*save_rdi, instArgsDynfix);
    BPatch_funcCallExpr instExprRestRdi(*restore_rdi, instArgsDynfix);
    BPatch_funcCallExpr instExprTrace(*trace_block_hit, instArgs);
    BPatchSnippetHandle *handle;

    /* RDI fix handling. */
    if (dynfix)
        handle = appBin->insertSnippet(instExprSaveRdi, *curBlk, BPatch_callBefore, BPatch_lastSnippet);

    /* Instruments the basic block. */
    handle = appBin->insertSnippet(instExprTrace, *curBlk, BPatch_callBefore, BPatch_lastSnippet);

    /* Wrap up RDI fix handling. */
    if (dynfix)
        handle = appBin->insertSnippet(instExprRestRdi, *curBlk, BPatch_callBefore, BPatch_lastSnippet);
    /* Verify instrumenting worked. If all good, advance blkIndex and return. */
    if (!handle)
    {
        cerr << "Failed to insert trace callback at 0x" << hex << curBlkAddr << endl;
    }

    /* Print some useful info, if requested. */
    if (verbose)
        cout << "Inserted trace callback at 0x" << hex << curBlkAddr << " of " << curFuncName << " of size " << dec << curBlkSize << endl;

    return 0;
}

void iterate_blocks(BPatch_binaryEdit *appBin, vector < BPatch_function * >::iterator funcIter, int *blkIndex) {

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
        auto tracer_mark = strstr(functionName.data(), "__tracer");
		if (tracer_mark != NULL|| 
            functionName == string("init") ||
			functionName == string("free") ||
			functionName == string("malloc") ||
			functionName == string("calloc") ||
			functionName == string("realloc")
            )
			continue;
        insert_trace(appBin, curFuncName, curBlk, curBlkAddr, curBlkSize, curBlkID);
		(*blkIndex)++;
		continue;
	}
	return;
}

int main(int argc, char **argv) {
    initSkipLibraries();
    // initSkipAddresses();
    // bpatch.setDelayedParsing(true);
    // bpatch.setLivenessAnalysis(false);
    // bpatch.setMergeTramp(false);
    BPatch_binaryEdit *app = bpatch.openBinary("tracer.elf");
    if (appBin == NULL)
    {
        cerr << "Failed to open binary" << endl;
        return EXIT_FAILURE;
    }
    BPatch_image *appImage = app->getImage();
    save_rdi = findFuncByName(appImage, "__trace_save_rdi");
    restore_rdi = findFuncByName(appImage, "__trace_restore_rdi");
    trace_block_hit = findFuncByName(appImage, "__tracer_block_hit");
    vector<BPatch_module *> *modules = appImage->getModules();
    vector<BPatch_module *>::iterator moduleIter;
    for (moduleIter = modules->begin(); moduleIter != modules->end(); ++moduleIter)
    {
        /* Extract module name and verify whether it should be skipped based. */
        char curModuleName[1024];
        (*moduleIter)->getName(curModuleName, 1024);
        if ((*moduleIter)->isSharedLib())
        {
            if (!includeSharedLib || skipLibraries.find(curModuleName) != skipLibraries.end())
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
            iterateBlocks(appBin, funcIter, &blkIndex);
        }
    }
    /* If specified, save the instrumented binary and verify success. */
    if (outputBinary)
    {
        if (verbose)
            cout << "Saving the instrumented binary to " << outputBinary << " ..." << endl;
        if (!appBin->writeFile(outputBinary))
        {
            cerr << "Failed to write output file: " << outputBinary << endl;
            return EXIT_FAILURE;
        }
    }
    if (verbose)
        cout << "All done!" << endl;
}