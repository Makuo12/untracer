#include "liboracle.h"

using std::vector;
using std::string;

int total_execs = 0;
int total_paths_found = 0;
int total_coverage_found = 0;

extern int __real_main(int argc, char **argv);

int __wrap_main(int argc, char **argv) {
    std::cout << "Running oracle" << std::endl;
    vector<Entry> entries;
    string input_file(argv[optind]);
    __oracle_init(entries, input_file);
    __oracle_fuzz(argc, argv, entries, input_file);
    return 0;
}

void __oracle_fuzz(int argc, char **argv, vector<Entry> &entries, string &input_file) {
    decltype(entries.size()) global_count = 0;
    while (global_count < entries.size()) {
        auto entry = &entries[global_count++];
        if (entry->has_issues) {
            continue;
        }
        int fd = open(entry->file_path.data(), O_RDONLY);
        if (fd < 0) {
            entry->has_issues = true;
            FATAL({"Failed to open file", entry->filename}, false);
            continue;
        }
        u8 *mem = (u8 *)mmap(NULL, entry->size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
        close(fd);
        if (mem == MAP_FAILED) {
            entry->has_issues = true;
            FATAL({"Failed to map file", entry->filename}, false);
            continue;
        }
        // fuzz one bit
        int len = entry->size << 3;
        for (int i = 0; i < len; ++i) {
            __oracle_apply(mem, i);
            // we want to write to the output file
            int out_fd = open(input_file.data(), O_RDWR);
            if (out_fd < 0) {
                FATAL({"input_file does not want to open", input_file});
            }
            ftruncate(out_fd, entry->size);
            u8 *mem_out_file = (u8 *)mmap(NULL, entry->size, PROT_READ | PROT_WRITE, MAP_SHARED, out_fd, 0);
            close(out_fd);
            if (mem_out_file == MAP_FAILED) {
                FATAL({"input_file does not want to map", input_file});
            }
            std::memcpy(mem_out_file, mem, entry->size);
            munmap(mem, entry->size);
            __real_main(argc, argv);
            entry->single_pass += 1;
        }
        entry->full_pass += 1;
        SAY({"full pass", std::to_string(total_execs)});
        munmap(mem, entry->size);
        if (global_count == entries.size()) {
            global_count = 0;
        }
    }
}

