// untracer.cc
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/user.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ptrace.h>
#include <sys/types.h>

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstring>
#include <map>
#include <unordered_set>
#include <iterator>
#include <random>
#include <sstream>
#include <iomanip>

#include "types.h"
#include "config.h"
#include "logger.h"
#include "libtracer.h"
#include "liboracle.h"

using std::string;
using std::vector;
using std::fstream;
using std::iterator;
using std::ifstream;
using std::ofstream;
using std::unordered_set;
using std::map;
using std::cout;
using std::cerr;
using std::unordered_set;
using std::endl;

int total_paths_found = 0;
int total_coverage_found = 0;

u8 * trace_bits;
static int __trace_shm_id = -1;

u8 * trace_blocks;
static int __trace_block_shm_id = -1;


// ADD THESE:
u8 __trace_virgin_blocks[BLOCK_SIZE];


void __tracer_init_virgin_blocks(void)
{
    memset(__trace_virgin_blocks, 0xFF, BLOCK_SIZE); // all unseen
}

void __tracer_cleanup_trace_bits(void) {
    if (trace_bits) {
        shmdt(trace_bits);
        trace_bits = NULL;
    }
    if (__trace_shm_id >= 0) {
        shmctl(__trace_shm_id, IPC_RMID, NULL);
        __trace_shm_id = -1;
    }
    if (__trace_block_shm_id >= 0) {
        shmctl(__trace_block_shm_id, IPC_RMID, NULL);
        __trace_block_shm_id = -1;
    }
}
void __tracer_init_trace_blocks(void) {
    int shm_id = shmget(IPC_PRIVATE, BLOCK_SIZE, IPC_CREAT | IPC_EXCL | 0666);
    if (shm_id < 0) {
        FATAL({"key failed to get shm_id", std::to_string(0)});
    }
    __trace_block_shm_id = shm_id;
    if (atexit(__tracer_cleanup_trace_bits) != 0) {
        shmctl(shm_id, IPC_RMID, NULL);
        FATAL("Failed to register shared memory cleanup");
    }
    string shm_str = std::to_string(shm_id);
    setenv(SHM_ID_ENV_BLOCKS, shm_str.c_str(), 1);
    trace_blocks = (u8 *)shmat(shm_id, 0, 0);
    if (trace_blocks == (u8 *)-1) {
        shmctl(shm_id, IPC_RMID, NULL);
        FATAL({"failed to link trace_blocks to memory", std::to_string(shm_id)});
    }
}

void __tracer_init_trace_bits(void) {
    // string key_name(getenv(SHM_KEY_FILE_NAME) ? getenv(SHM_KEY_FILE_NAME) : "");
    // if (key_name.size() == 0) {
    //     FATAL({"Environment variable", SHM_KEY_FILE_NAME, "is not set"});
    // }
    // key_t key = ftok(key_name.c_str(), PROJECT_NAME);
    // if (key == -1) {
    //     FATAL({"ftok failed to generate key", key_name});
    // }
    int shm_id = shmget(IPC_PRIVATE, MAP_SIZE, IPC_CREAT | IPC_EXCL | 0666);
    if (shm_id < 0) {
        FATAL({"key failed to get shm_id", std::to_string(0)});
    }
    __trace_shm_id = shm_id;
    if (atexit(__tracer_cleanup_trace_bits) != 0) {
        shmctl(shm_id, IPC_RMID, NULL);
        FATAL("Failed to register shared memory cleanup");
    }
    string shm_str = std::to_string(shm_id);
    setenv(SHM_ID_ENV, shm_str.c_str(), 1);
    trace_bits = (u8 *)shmat(shm_id, 0, 0);
    if (trace_bits == (u8 *)-1) {
        shmctl(shm_id, IPC_RMID, NULL);
        FATAL({"failed to link trace_bits to memory", std::to_string(shm_id)});
    }
}

static u8 __trace_count_class_lookup8[256];

void copy_binary(char *src_path, char *dst_path)
{
    struct stat st = {0};
    if (stat(src_path, &st) < 0)
    {
        perror(src_path);
        exit(1);
    }

    char *data = (char *)malloc(st.st_size);
    if (data == NULL)
    {
        perror("malloc");
        exit(1);
    }

    FILE *src_file = fopen(src_path, "rb");
    if (src_file == NULL)
    {
        perror(src_path);
        free(data);
        exit(1);
    }

    FILE *dst_file = fopen(dst_path, "wb");
    if (dst_file == NULL)
    {
        perror(dst_path);
        fclose(src_file);
        free(data);
        exit(1);
    }

    if (fread(data, 1, st.st_size, src_file) != (size_t)st.st_size)
    {
        perror(src_path);
        fclose(src_file);
        fclose(dst_file);
        free(data);
        exit(1);
    }

    if (fwrite(data, 1, st.st_size, dst_file) != (size_t)st.st_size)
    {
        perror(dst_path);
        fclose(src_file);
        fclose(dst_file);
        free(data);
        exit(1);
    }

    fclose(src_file);
    fclose(dst_file);
    free(data);

    if (chmod(dst_path, 0777) < 0)
    {
        perror(dst_path);
        exit(1);
    }
}

void copy_binary(u8 *src, size_t src_size, char *dst_path)
{
    FILE *dst_file = fopen(dst_path, "wb");
    if (dst_file == NULL)
    {
        perror(dst_path);
        exit(1);
    }
    if (fwrite(src, 1, src_size, dst_file) != src_size)
    {
        perror(dst_path);
        fclose(dst_file);
        exit(1);
    }
    fclose(dst_file);
    if (chmod(dst_path, 0777) < 0)
    {
        perror(dst_path);
        exit(1);
    }
}
void modify_oracle(string &path_to_oracle, vector<u64> &list, map<u64, u8> &breakpoint, unordered_set<int> &hits)
{
    u64 addr;
    char flag[1] = {(char)0xCC};
    int offset = 0;
    fstream oracle_file(path_to_oracle, std::ios::in | std::ios::out | std::ios::binary);
    for (decltype(list.size()) i = 0; i < list.size(); i++)
    {
        // Skip indexes that were already hit
        if (hits.count(i) > 0) {
            cout << "skipping " << list[i] << " block: " << i << endl;
            continue;
        }

        addr = list[i] + offset;
        if (addr != 0)
        {
            char original;
            oracle_file.seekg(addr, std::ios::beg);
            oracle_file.read(&original, 1);
            oracle_file.seekp(addr, std::ios::beg);
            breakpoint[addr] = original;
            oracle_file.write(flag, 1);
        }
    }
    oracle_file.close();
}

void setup_bblist(vector<u64> &list, const string &path_to_bblock)
{
    ifstream file(path_to_bblock); 
    string buf;
    while ((getline(file, buf)))
    {
        long long addr = std::strtoll(buf.c_str(), nullptr, 16);
        list.emplace_back(addr);
    }
    file.close();
}

std::string generateRandomFilename(const std::string& extension = ".txt") {
    static std::mt19937 rng(std::random_device{}());
    static const std::string chars = "abcdefghijklmnopqrstuvwxyz0123456789";

    std::uniform_int_distribution<size_t> charDist(0, chars.size() - 1);
    std::uniform_int_distribution<int> lengthDist(8, 16); 

    int length = lengthDist(rng);
    std::string filename;
    filename.reserve(length);

    for (int i = 0; i < length; i++) {
        filename += chars[charDist(rng)];
    }
    filename += extension;
    return filename;
}

bool getHitBlocks(unordered_set<int> &hits)
{
    bool new_found = false;
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        if (trace_blocks[i] != 0)
        {
            hits.insert(i);
            if (__trace_virgin_blocks[i] == 0xFF) // first time seeing this block
            {
                __trace_virgin_blocks[i] = 0; // mark as seen
                new_found = true;
            }
        }
    }
    if (hits.size() > 0)
    {
        cout << "hit blocks: " << hits.size() << endl;
    }
    return new_found;
}

void trace(const string &path_to_oracle, const string &path_to_trace, const string &path_to_input,
           const string &in_dir, const string &out_dir, unordered_set<int> &index_found,
          vector<u64> &bblist, map<u64, u8> &breakpoint, string &new_path_to_oracle)
{
    cout << "on trace" << endl;
    memset(trace_bits, 0, MAP_SIZE);
    memset(trace_blocks, 0, BLOCK_SIZE); // clear block hits before each run
    pid_t pid = fork();
    if (pid == 0)
    {
        char *args[] = {
            (char *)path_to_trace.c_str(),
            (char *)path_to_input.c_str(),
            NULL};
        execvp(args[0], args);
        perror("execvp failed");
        exit(1);
    }
    else if (pid > 0)
    {
        int status;
        waitpid(pid, &status, 0);
        MEM_BARRIER();

        bool new_blocks = getHitBlocks(index_found); // check virgin map inside

        if (new_blocks)
        {
            cout << "new blocks found, rebuilding oracle" << endl;
            copy_binary((char *)path_to_oracle.data(), (char *)new_path_to_oracle.data());
            modify_oracle(new_path_to_oracle, bblist, breakpoint, index_found);
        }
        else
        {
            cout << "no new blocks, skipping oracle rebuild" << endl;
        }
    }
    else
    {
        perror("fork failed");
        exit(1);
    }
}

void create_copy_oracle(string &path_to_oracle) {
    string out_file_name("./output/oracle.elf");
    ifstream in_file(path_to_oracle, std::ios::binary);
    ofstream out_file(out_file_name, std::ios::binary);
    if (!out_file.is_open() || !in_file.is_open()) {
        FATAL({"failed to open files", path_to_oracle, out_file_name});
    }
    out_file << in_file.rdbuf();
    out_file.close();
    in_file.close();
    path_to_oracle.clear();
    path_to_oracle += out_file_name;
}

void fork_child(char **args,
                const string &path_to_oracle,
                const string &path_to_trace,
                const string &path_to_input,
                const string &in_dir,
                const string &out_dir,
                unordered_set<int> &indexes_found,
                vector<u64> &bblist, map<u64, u8> &breakpoint, string &new_path_to_oracle
)
{
    pid_t pid = fork();

    if (pid == 0)
    {
        // Child
        // ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execvp(args[0], args);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFSIGNALED(status))
    {
        int term_sig = WTERMSIG(status);
        if (term_sig == SIGTRAP)
        {
            trace(path_to_oracle, path_to_trace, path_to_input, in_dir, out_dir, indexes_found, bblist, breakpoint, new_path_to_oracle);
        }
        // already dead if WIFSIGNALED, just reap any remaining state
        // waitpid(pid, NULL, WNOHANG);
    }
}

int main(int argc, char **argv)
{
    cout << "starting untracer" << endl;
    vector<u64> bblist;
    string path_to_oracle;
    string path_to_trace;
    string path_to_bblock;
    unordered_set<int> indexes_found;
    string path_to_input(getenv(INPUT_FILE_ENV) ? getenv(INPUT_FILE_ENV) : "");
    string in_dir(getenv(IN_DIR_ENV) ? getenv(IN_DIR_ENV) : "pdf_test");
    string out_dir(getenv(OUT_DIR_ENV) ? getenv(OUT_DIR_ENV) : "output");
    string new_path_to_oracle("./output/oracle.elf");
    int opt;
    while ((opt = getopt(argc, argv, "o:t:b:")) != -1)
    {
        switch (opt)
        {
        case 'o':
            path_to_oracle = optarg;
            break;
        case 't':
            path_to_trace = optarg;
            break;
        case 'b':
            path_to_bblock = optarg;
            break;
        default:
            cerr << "Usage: " << argv[0] << " -o <oracle> -t <trace> -b <bblock> -i <input>" << endl;
            return EXIT_FAILURE;
        }
    }
    // Validate
    if (path_to_oracle.empty() || path_to_trace.empty() || path_to_bblock.empty() || path_to_input.empty())
    {
        cerr << "Usage: " << argv[0] << " -o <oracle> -t <trace> -b <bblock> -i <input>" << endl;
        return EXIT_FAILURE;
    }
    __tracer_init_trace_bits();
    __tracer_init_trace_blocks();
    __tracer_init_virgin_blocks();
    setup_bblist(bblist, path_to_bblock);
    Entry *entries = NULL;
    size_t entry_count = 0;
    __oracle_init(&entries, &entry_count, path_to_input.c_str());
    copy_binary((char *)path_to_oracle.data(), (char *)new_path_to_oracle.data());
    modify_oracle(new_path_to_oracle, bblist, breakpoint, indexes_found);
    char *args[] = {(char *)new_path_to_oracle.c_str(), (char *)"./input/cur_input", NULL};
    size_t global_count = 0;
    while (true)
    {
        if (global_count >= entry_count) {
            global_count = 0;
        }
        Entry *entry = &entries[global_count++];
        if (strstr(entry->file_path, ".pdf") == NULL) {
            continue;
        }
        int fd = open(entry->file_path, O_RDONLY);
        if (fd < 0)
        {
            entry->has_issues = 1;
            char err_msg[512];
            snprintf(err_msg, sizeof(err_msg), "Failed to open file: %s", entry->d_name);
            continue;
        }
        u8 *mem = (u8 *)mmap(NULL, entry->st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
        close(fd);

        if (mem == MAP_FAILED)
        {
            entry->has_issues = 1;
            char err_msg[512];
            snprintf(err_msg, sizeof(err_msg), "Failed to map file: %s", entry->d_name);
            continue;
        }
        for (int i = 0; i < 100; ++i)
        {
            __oracle_apply(mem, i);
            copy_binary(mem, entry->st_size, (char *)"./input/cur_input");
            cout << entry->file_path << endl;
            fork_child(args,
                    path_to_oracle,
                    path_to_trace,
                    path_to_input,
                    in_dir,
                    out_dir,
                    indexes_found, bblist, breakpoint, new_path_to_oracle
            );
            // Re-apply/revert the bit
            __oracle_apply(mem, i);
            entry->single_pass += 1;
        }
    }

    return 0;
}
