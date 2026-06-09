#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/mman.h> 
#include "logger.h"
#include "config.h"
#include "data.h"
#include "types.h"
#include "liboracle.h"


void __oracle_fuzz(int argc, char **argv, Entry *entries, int entry_count, const char *input_file)
{
    size_t global_count = 0;

    while (global_count < (size_t)entry_count)
    {
        Entry *entry = &entries[global_count++];

        if (entry->has_issues)
        {
            continue;
        }

        int fd = open(entry->file_path, O_RDONLY);
        if (fd < 0)
        {
            entry->has_issues = 1;
            // Combined messages since C doesn't natively parse {"message", string} initializer lists
            char err_msg[512];
            snprintf(err_msg, sizeof(err_msg), "Failed to open file: %s", entry->d_name);
            continue;
        }

        // // mmap implementation remains identical, just cast to (u8*)
        u8 *mem = (u8 *)mmap(NULL, entry->st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
        close(fd);

        if (mem == MAP_FAILED)
        {
            entry->has_issues = 1;
            char err_msg[512];
            snprintf(err_msg, sizeof(err_msg), "Failed to map file: %s", entry->d_name);
            continue;
        }
        printf("Done with init \n");
        // Fuzz one bit loop
        // int len = entry->st_size << 3;
        for (int i = 0; i < 100; ++i)
        {
            // __oracle_apply(mem, i);

            // Pass the string reference cleanly
            // __oracle_write_testcase(mem, entry, input_file);

            // Execute target main
            __real_main(argc, argv);

            // Re-apply/revert the bit
            // __oracle_apply(mem, i);
            // entry->single_pass += 1;
        }

        // entry->full_pass += 1;
        // munmap(mem, entry->st_size);

        // Loop back around if we hit the end of the entry block
        if (global_count == (size_t)entry_count)
        {
            global_count = 0;
        }
    }
}

int __wrap_main(int argc, char **argv)
{
    Entry *entries = NULL;
    int entry_count = 0;

    // Check bounds before indexing argv
    if (argv[optind] == NULL)
    {
        FATAL_C("Missing input file argument.");
    }
    const char *input_file = argv[optind];
    printf("Calling init\n");
    // Call our initialization handler
    __oracle_init(&entries, &entry_count, input_file);

    // Core Fuzzer orchestration
    __oracle_fuzz(argc, argv, entries, entry_count, input_file);

    // Clean up allocated buffer space
    return 0;
}