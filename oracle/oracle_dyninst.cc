
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
    skipLibraries.insert("./libs/liboracle.so"); // Stripped path for easier matching
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
}

// Helper to check if a module path contains any blacklisted library names
bool shouldSkipModule(const std::string &modName)
{
    for (const auto &lib : skipLibraries)
    {
        if (modName.find(lib) != std::string::npos)
        {
            return true;
        }
    }
    return false;
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


int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <target_executable>\n";
        return -1;
    }
    const char *targetArgv[] = {
        "/home/makuo12/Documents/forte-research/untracer/target.elf",
        "/home/makuo12/Documents/forte-research/untracer/read.txt",
        nullptr
    };
    initSkipLibraries();
    BPatch bpatch;
    bpatch.setTypeChecking(true);
    bpatch.setDelayedParsing(true);
    bpatch.setLivenessAnalysis(false);
    bpatch.setMergeTramp(false);
    BPatch_process *appProcess = bpatch.processCreate(targetArgv[0], targetArgv);
    if (!appProcess)
        return -1;

    BPatch_image *appImage = appProcess->getImage();

    // 1. Get all modules (executable and loaded shared libraries)
    std::vector<BPatch_module *> *modules = appImage->getModules();
    BPatch_nullExpr n;
    size_t breakpointCount = 0;
    trace_block_hit = findFuncByName(appImage, (char *)"test_hit");
    for (BPatch_module *mod : *modules)
    {
        char modBuffer[512];
        mod->getName(modBuffer, sizeof(modBuffer));
        std::string modName(modBuffer);

        // 2. Filter out skipped libraries
        if (shouldSkipModule(modName))
        {
            std::cout << "[Skip Library] " << modName << "\n";
            continue;
        }

        // 3. Get all functions within the allowed module
        std::vector<BPatch_function *> *functions = mod->getProcedures();
        for (BPatch_function *func : *functions)
        {
            char funcBuffer[512];
            func->getName(funcBuffer, sizeof(funcBuffer));
            std::string curFuncName(funcBuffer);

            // 4. Filter out skipped functions
            if (skipFunctions.find(curFuncName) != skipFunctions.end())
            {
                // std::cout << "[Skip Function] " << funcName << " in " << modName << "\n";
                continue;
            }
            if (string(curFuncName) == string("init") ||
                string(curFuncName) == string("_init") ||
                string(curFuncName) == string("fini") ||
                string(curFuncName) == string("_fini") ||
                string(curFuncName) == string("register_tm_clones") ||
                string(curFuncName) == string("deregister_tm_clones") ||
                string(curFuncName) == string("frame_dummy") ||
                string(curFuncName) == string("__do_global_ctors_aux") ||
                string(curFuncName) == string("__do_global_dtors_aux") ||
                string(curFuncName) == string("__libc_csu_init") ||
                string(curFuncName) == string("__libc_csu_fini") ||
                string(curFuncName) == string("start") ||
                string(curFuncName) == string("_start") ||
                string(curFuncName) == string("__libc_start_main") ||
                string(curFuncName) == string("__gmon_start__") ||
                string(curFuncName) == string("__cxa_atexit") ||
                string(curFuncName) == string("__cxa_finalize") ||
                string(curFuncName) == string("__assert_fail") ||
                string(curFuncName) == string("free") ||
                string(curFuncName) == string("fnmatch") ||
                string(curFuncName) == string("readlinkat") ||
                string(curFuncName) == string("malloc") ||
                string(curFuncName) == string("calloc") ||
                string(curFuncName) == string("realloc") ||
                string(curFuncName) == string("argp_failure") ||
                string(curFuncName) == string("argp_help") ||
                string(curFuncName) == string("argp_state_help") ||
                string(curFuncName) == string("argp_error") ||
                string(curFuncName) == string("argp_parse") ||
                string(curFuncName) == string("__afl_maybe_log") ||
                string(curFuncName) == string("__fsrvonly_store") ||
                string(curFuncName) == string("__fsrvonly_return") ||
                string(curFuncName) == string("__fsrvonly_setup") ||
                string(curFuncName) == string("__fsrvonly_setup_first") ||
                string(curFuncName) == string("__fsrvonly_forkserver") ||
                string(curFuncName) == string("__fsrvonly_fork_wait_loop") ||
                string(curFuncName) == string("__fsrvonly_fork_resume") ||
                string(curFuncName) == string("__fsrvonly_die") ||
                string(curFuncName) == string("__fsrvonly_setup_abort") ||
                string(curFuncName) == string(".AFL_SHM_ENV") ||
                (string(curFuncName).substr(0, 4) == string("targ") && isdigit(string(curFuncName)[5])))
            {
                continue;
            }
            // 5. Safely insert breakpoint snippet at function entry
            std::vector<BPatch_point *> *entries = func->findPoint(BPatch_entry);
            if (entries && !entries->empty())
            {
                BPatch_Vector<BPatch_snippet *> instArgsDynfix;
                BPatch_Vector<BPatch_snippet *> instArgs;

                BPatch_constExpr argCurBlkID();
                instArgs.push_back(&argCurBlkID);

                BPatch_funcCallExpr instExprSaveRdi(*save_rdi, instArgsDynfix);
                BPatch_funcCallExpr instExprRestRdi(*restore_rdi, instArgsDynfix);
                BPatch_funcCallExpr instExprTrace(*trace_block_hit, instArgs);
                BPatchSnippetHandle *handle;

                /* RDI fix handling. */
                if (true)
                    handle = appProcess->insertSnippet(instExprSaveRdi, *curBlk, BPatch_callBefore, BPatch_lastSnippet);

                /* Instruments the basic block. */
                handle = appBin->insertSnippet(instExprTrace, *curBlk, BPatch_callBefore, BPatch_lastSnippet);

                /* Wrap up RDI fix handling. */
                if (true)
                    handle = appBin->insertSnippet(instExprRestRdi, *curBlk, BPatch_callBefore, BPatch_lastSnippet);
                /* Verify instrumenting worked. If all good, advance blkIndex and return. */
                if (!handle)
                {
                    cerr << "Failed to insert trace callback at 0x" << std::hex << curBlkAddr << std::endl;
                }
                breakpointCount++;
            }
        }
    }

    std::cout << "\nSuccessfully inserted " << breakpointCount << " breakpoints.\n";

    appProcess->continueExecution();

    while (!appProcess->isTerminated())
    {
        bpatch.waitForStatusChange();

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

    return 0;
}