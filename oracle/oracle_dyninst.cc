using std::cerr;
using std::cout;
using std::endl;
using std::cin;
using std::vector;
using std::string;
using std::set;
using std::ifstream;
using std::ofstream;

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

// Global blocklists
std::set<std::string> skipLibraries;

const std::set<std::string> skipFunctions = {
    // CRT
    "_start", "__libc_start_main", "_init", "_fini",
    "__libc_csu_init", "__libc_csu_fini", "register_tm_clones",
    "deregister_tm_clones", "__do_global_ctors_aux", "__do_global_dtors_aux",
    "frame_dummy", "__cxa_atexit", "__cxa_finalize",
    "malloc", "calloc", "realloc", "free", "FATAL", "SAY", "__wrap_main",
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
    skipLibraries.insert("liboracle.so"); // Stripped path for easier matching
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

BPatch bpatch;

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <target_executable>\n";
        return -1;
    }

    initSkipLibraries();
    BPatch_process *appProcess = bpatch.processCreate(argv[1], nullptr);
    if (!appProcess)
        return -1;

    BPatch_image *appImage = appProcess->getImage();

    // 1. Get all modules (executable and loaded shared libraries)
    std::vector<BPatch_module *> *modules = appImage->getModules();
    BPatch_breakPointExpr breakPointSnippet;
    size_t breakpointCount = 0;

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
            std::string funcName(funcBuffer);

            // 4. Filter out skipped functions
            if (skipFunctions.find(funcName) != skipFunctions.end())
            {
                std::cout << "[Skip Function] " << funcName << " in " << modName << "\n";
                continue;
            }

            // 5. Safely insert breakpoint snippet at function entry
            std::vector<BPatch_point *> *entries = func->findPoint(BPatch_entry);
            if (entries && !entries->empty())
            {
                appProcess->insertSnippet(breakPointSnippet, *entries);
                breakpointCount++;
            }
        }
    }

    std::cout << "\nSuccessfully inserted " << breakpointCount << " breakpoints.\n";

    // 6. Run the target process loop
    appProcess->continueExecution();
    while (!appProcess->isTerminated())
    {
        bpatch.waitForStatusChange();

        if (appProcess->isStopped())
        {
            // A safe function hit a breakpoint!
            std::cout << "[!] Breakpoint hit inside allowed code base." << std::endl;

            // Handle your tracking/fuzzing state here...

            appProcess->continueExecution();
        }
    }

    return 0;
}