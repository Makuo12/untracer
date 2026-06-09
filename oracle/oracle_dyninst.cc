
#include <iostream>
#include <vector>
#include <set>
#include <string>
#include "BPatch.h"
#include "BPatch_process.h"
#include "BPatch_image.h"
#include "BPatch_module.h"
#include "BPatch_function.h"
#include "BPatch_point.h"
#include "BPatch_snippet.h"

using std::cerr;
using std::cout;
using std::endl;
using std::cin;
using std::vector;
using std::string;
using std::set;
using std::ifstream;
using std::ofstream;

BPatch_function * trace_block_hit;
BPatch_function * restore_rdi;
BPatch_function * save_rdi;

// Global blocklists
std::set<std::string> skipLibraries;

const std::set<std::string> skipFunctions = {
    // CRT
    "_start", "__libc_start_main", "_init", "_fini",
    "__libc_csu_init", "__libc_csu_fini", "register_tm_clones",
    "deregister_tm_clones", "__do_global_ctors_aux", "__do_global_dtors_aux",
    "frame_dummy", "__cxa_atexit", "__cxa_finalize",
    "malloc", "calloc", "realloc", "free", "FATAL", "SAY", "__wrap_main", "main"
    // Oracle functions
    "__oracle_init", "__oracle_init_shm", "__oracle_init@plt",
    "__oracle_trap_hit", "__oracle_trap_hit@plt", "__oracle_save_rdi",
    "__oracle_save_rdi@plt", "__oracle_restore_rdi", "__oracle_fuzz",
    "__oracle_apply", "__oracle_write_testcase", "__oracle_restore_rdi@plt",
    // Oracle tracer internals
    "__tracer_init_virgin_bits", "__tracer_cleanup_trace_bits",
    "__tracer_init_trace_bits", "__tracer_init_count_class16",
    "__tracer_init_class_lookup16", "__tracer_classify_counts", "__tracer_has_bit"};


void initSkipLibraries()
{
    /* List of shared libraries to skip instrumenting. */
    skipLibraries.insert("./libs/libtracer.so");
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
        cerr << "Failed to find entry at 0x" << std::hex << curBlkAddr << std::endl;
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
        cerr << "Failed to insert trace callback at 0x" << std::hex << curBlkAddr << std::endl;
    }

    /* Print some useful info, if requested. */
    if (verbose)
        cout << "Inserted trace callback at 0x" << std::hex << curBlkAddr << " of " << curFuncName << " of size " << std::dec << curBlkSize << std::endl;

    return 0;
}

void iterate_blocks(BPatch_binaryEdit *appBin, vector<BPatch_function *>::iterator funcIter, int *blkIndex)
{

    /* Extract the function's name, and its pointer from the parent function vector. */
    BPatch_function *curFunc = *funcIter;
    char curFuncName[1024];
    curFunc->getName(curFuncName, 1024);
    /* Extract the function's CFG. */
    BPatch_flowGraph *curFuncCFG = curFunc->getCFG();
    if (!curFuncCFG)
    {
        cerr << "Failed to find CFG for function " << curFuncName << endl;
        return;
    }
    /* Extract the CFG's basic blocks and verify the number of blocks isn't 0. */
    BPatch_Set<BPatch_basicBlock *> curFuncBlks;
    if (!curFuncCFG->getAllBasicBlocks(curFuncBlks))
    {
        cerr << "Failed to find basic blocks for function " << curFuncName << endl;
        return;
    }
    if (curFuncBlks.size() == 0)
    {
        cerr << "No basic blocks for function " << curFuncName << endl;
        return;
    }
    /* Set up this function's basic block iterator and start iterating. */
    BPatch_Set<BPatch_basicBlock *>::iterator blksIter;
    for (blksIter = curFuncBlks.begin(); blksIter != curFuncBlks.end(); blksIter++)
    {
        /* Get the current basic block, and its size and address. */
        BPatch_point *curBlk = (*blksIter)->findEntryPoint();
        unsigned int curBlkSize = (*blksIter)->size();
        /* Compute the basic block's adjusted address.	*/
        unsigned long curBlkAddr = (*blksIter)->getStartAddress();
        /* Non-PIE binary address correction. */
        curBlkAddr = curBlkAddr - (long)0x400000;
        unsigned int curBlkID = *blkIndex;
        /* Other basic blocks to ignore. */
        string functionName(curFuncName);
            if (string(functionName) == string("init") ||
                string(functionName) == string("_init") ||
                string(functionName) == string("fini") ||
                string(functionName) == string("_fini") ||
                string(functionName) == string("register_tm_clones") ||
                string(functionName) == string("deregister_tm_clones") ||
                string(functionName) == string("frame_dummy") ||
                string(functionName) == string("__do_global_ctors_aux") ||
                string(functionName) == string("__do_global_dtors_aux") ||
                string(functionName) == string("__libc_csu_init") ||
                string(functionName) == string("__libc_csu_fini") ||
                string(functionName) == string("start") ||
                string(functionName) == string("_start") ||
                string(functionName) == string("__libc_start_main") ||
                string(functionName) == string("__gmon_start__") ||
                string(functionName) == string("__cxa_atexit") ||
                string(functionName) == string("__cxa_finalize") ||
                string(functionName) == string("__assert_fail") ||
                string(functionName) == string("free") ||
                string(functionName) == string("fnmatch") ||
                string(functionName) == string("readlinkat") ||
                string(functionName) == string("malloc") ||
                string(functionName) == string("calloc") ||
                string(functionName) == string("realloc") ||
                string(functionName) == string("argp_failure") ||
                string(functionName) == string("argp_help") ||
                string(functionName) == string("argp_state_help") ||
                string(functionName) == string("argp_error") ||
                string(functionName) == string("argp_parse") ||
                string(functionName) == string("__afl_maybe_log") ||
                string(functionName) == string("__fsrvonly_store") ||
                string(functionName) == string("__fsrvonly_return") ||
                string(functionName) == string("__fsrvonly_setup") ||
                string(functionName) == string("__fsrvonly_setup_first") ||
                string(functionName) == string("__fsrvonly_forkserver") ||
                string(functionName) == string("__fsrvonly_fork_wait_loop") ||
                string(functionName) == string("__fsrvonly_fork_resume") ||
                string(functionName) == string("__fsrvonly_die") ||
                string(functionName) == string("__fsrvonly_setup_abort") ||
                string(functionName) == string(".AFL_SHM_ENV") ||
                (string(functionName).substr(0, 4) == string("targ") && isdigit(string(curFuncName)[5])))
            {
                continue;
            }
        if (skipFunctions.count(functionName))
            continue;
        if ((strstr(functionName.data(), "__tracer")) != NULL)
            continue;
        insert_trace(appBin, curFuncName, curBlk, curBlkAddr, curBlkSize, curBlkID);
        (*blkIndex)++;
        continue;
    }
    return;
}

int main(int argc, char **argv)
{
    initSkipLibraries();
    bpatch.setDelayedParsing(true);
    bpatch.setLivenessAnalysis(false);
    bpatch.setMergeTramp(false);
    const char *targetArgv[] = {
        "/home/makuo12/Documents/forte-research/untracer/target.elf",
        "/home/makuo12/Documents/forte-research/untracer/read.txt",
        nullptr
    };
    BPatch_process *appProcess = bpatch.processCreate(targetArgv[0], targetArgv);
    if (!appProcess)
        return -1;

    BPatch_image *appImage = appProcess->getImage();
    if (app == NULL)
    {
        cerr << "Failed to open binary" << endl;
        return EXIT_FAILURE;
    }
    BPatch_image *appImage = app->getImage();
    if (!app->loadLibrary(tracer_library))
    {
        cerr << "Failed to load binary" << endl;
        return EXIT_FAILURE;
    }
    save_rdi = findFuncByName(appImage, (char *)"__trace_save_rdi");
    restore_rdi = findFuncByName(appImage, (char *)"__trace_restore_rdi");
    trace_block_hit = findFuncByName(appImage, (char *)"__tracer_block_hit");
    if (save_rdi == NULL || restore_rdi == NULL || trace_block_hit == NULL)
    {
        cerr << "Failed to find important functions" << endl;
        return EXIT_FAILURE;
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
            iterate_blocks(app, funcIter, &blkIndex);
        }
    }
    appProcess->continueExecution();

    while (!appProcess->isTerminated())
    {
            std::cerr << "[!] Process received signal, stopping.\n";
        bpatch.waitForStatusChange();
            std::cerr << "[!] Process received signal, stopping.\n";
        if (appProcess->isTerminated())
            break;

        if (appProcess->isStopped())
        {
            // Get the stop reason
            std::cerr << "[!] Process received signal, stopping.\n";

            // Breakpoint hit - just continue silently
            appProcess->continueExecution();
        }
    }

    // Check exit code
    if (appProcess->isTerminated())
    {
        if (appProcess->terminationStatus() == ExitedNormally)
        {
            std::cout << "[+] Process exited normally, code: "
                      << appProcess->getExitCode() << "\n";
        }
        else
        {
            std::cout << "[!] Process terminated abnormally.\n";
            std::cout << "[!] Termination signal: "
                      << appProcess->getExitSignal() << "\n";
        }
    }
}