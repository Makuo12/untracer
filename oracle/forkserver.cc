#include <getopt.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <vector>
#include <cstring>

#include "data.h"
#include "logger.h"
#include "types.h"
#include "config.h"

using std::string;
using std::vector;

void __oracle_fuzz(vector<Entry> &entries, string &out_file_path);

int total_execs = 0;
int total_paths_found = 0;
int total_coverage_found = 0;


void apply(u8 * mem, int position) {
    mem[position >> 3] ^= (128 >> (position & 7));
}

void __oracle_fuzz(vector<Entry> &entries, string &input_file) {
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
            apply(mem, i);
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
            pid_t child = fork();
            if (child == 0) {
                // We are in child
                munmap(mem, entry->size);
                return;
            } else {
                // We are in parent
                int status;
                waitpid(child, &status, 0);
                total_execs += 1;
                entry->single_pass += 1;
                if (WIFSIGNALED(status)) {
                    int sig = WTERMSIG(status);
                    // With sig we check if it was a trap
                    if (sig == SIGTRAP) {
                        // We call tracer
                        total_paths_found += 1;
                        entry->path_found += 1;
                        int can_trace_fd = open(CAN_TRACE_PIPE, O_WRONLY);
                        if (can_trace_fd < 0) {
                            FATAL({"Could not pipe data to trace", CAN_TRACE_PIPE});
                        }
                        ssize_t size = write(can_trace_fd, SUCCESS, strlen(SUCCESS) + 1);
                        if (size < 1) {
                            FATAL({"write could not pipe data to trace", CAN_TRACE_PIPE});
                        }
                        close(can_trace_fd);
                        int done_trace_fd = open(DONE_TRACE_PIPE, O_RDONLY);
                        if (done_trace_fd < 0) {
                            FATAL({"Done trace pipe not working", DONE_TRACE_PIPE});
                        }
                        char buf[1024];
                        read(done_trace_fd, buf, sizeof(buf));
                        close(done_trace_fd);
                        if (strstr(buf, SUCCESS)) {
                            // Done to next
                            total_coverage_found += 1;
                            entry->trace_count += 1;
                        }
                    }
                }
            }
            
            apply(mem, i);
        }
        entry->full_pass += 1;
        SAY({"full pass", std::to_string(total_execs)});
        munmap(mem, entry->size);
        if (global_count == entries.size()) {
            global_count = 0;
        }
    }
}


__attribute__((constructor)) void __oracle_forkserver(int argc, char **argv) {
    std::cout << "Running oracle" << std::endl;
    string in_dir(getenv(IN_DIR) ? getenv(IN_DIR) : "");
    string out_dir(getenv(OUT_DIR) ? getenv(OUT_DIR) : "");
    string input_file(argv[optind]);
    if (in_dir.size() == 0 || out_dir.size() == 0 || input_file.size() == 0) {
        FATAL("Please set all flags -o (output path), -p (path for file), -i (input directory)");
    }
    vector<Entry> entries;
    dirent **items;
    int count = scandir(in_dir.data(), &items, NULL, alphasort);
    if (count < 0) {
        FATAL("Failed to scan input directory");
    }
    if (count == 0) {
        FATAL("No input files found to fuzz");
    }
    struct stat st;
    for (int i = 0; i < count; ++i) {
        if (items[i]->d_type != DT_REG) continue;
        string file_path = in_dir + "/" + items[i]->d_name;
        if (stat(file_path.c_str(), &st) == 0) {
            Entry entry{items[i]->d_name, st.st_size, file_path};
            entries.emplace_back(entry);
        }
        free(items[i]);
    }
    free(items);
    if (entries.empty()) {
        FATAL("No valid input files found to fuzz");
    }
    __oracle_fuzz(entries, input_file);
}
