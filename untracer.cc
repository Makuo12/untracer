// untracer.cc
#include <string>
#include <sys/user.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstring>
#include <map>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/mman.h>

#include "types.h"
#include "config.h"
#include "logger.h"
// #include "libtracer.h"

using std::string;
using std::vector;
using std::ifstream;
using std::ofstream;
using std::fstream;
using std::map;
using std::cout;
using std::cerr;
using std::endl;

int total_paths_found = 0;
int total_coverage_found = 0;

u8 * trace_bits;
static int __trace_shm_id = -1;

u8 __trace_virgin_bits[MAP_SIZE];
u16 __trace_class_lookup16[MAP_SIZE];

void __tracer_init_virgin_bits(void) {
    memset(__trace_virgin_bits, 255, MAP_SIZE);
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

void __tracer_init_count_class16(void)
{

    u32 b1, b2;
    for (b1 = 0; b1 < 256; b1++)
        for (b2 = 0; b2 < 256; b2++)
            __trace_class_lookup16[(b1 << 8) + b2] =
                (__trace_count_class_lookup8[b1] << 8) |
                __trace_count_class_lookup8[b2];
}


void __tracer_init_class_lookup16() {
    __trace_count_class_lookup8[0] = 0;
    __trace_count_class_lookup8[1] = 1;
    __trace_count_class_lookup8[2] = 2;
    __trace_count_class_lookup8[3] = 3;
    for (int i = 4;   i <= 7;   i++) __trace_count_class_lookup8[i] = 8;
    for (int i = 8;   i <= 15;  i++) __trace_count_class_lookup8[i] = 16;
    for (int i = 16;  i <= 31;  i++) __trace_count_class_lookup8[i] = 32;
    for (int i = 32;  i <= 127; i++) __trace_count_class_lookup8[i] = 64;
    for (int i = 128; i <= 255; i++) __trace_count_class_lookup8[i] = 128;
    __tracer_init_count_class16();
}


void __tracer_classify_counts(u64 *mem) {
    u32 i = MAP_SIZE >> 3;
    while (i--) {
        if (unlikely(*mem)) {
            u16 *mem16 = (u16 *)mem;
            mem16[0] = __trace_class_lookup16[mem16[0]];
            mem16[1] = __trace_class_lookup16[mem16[1]];
            mem16[2] = __trace_class_lookup16[mem16[2]];
            mem16[3] = __trace_class_lookup16[mem16[3]];
        }
        mem++;
    }
}

bool __tracer_has_bit(void) {
    int i = MAP_SIZE >> 3;
    u64 * trace = (u64 *)trace_bits;
    u64 * virgin = (u64 *)__trace_virgin_bits;
    bool found = false;
    while(i--) {
        u8 * tr = (u8 *)trace;
        u8 * vr = (u8 *)virgin;
        if ((tr[0] && vr[0] == 0xff) || (tr[1] && vr[1] == 0xff) ||
            (tr[2] && vr[2] == 0xff) || (tr[3] && vr[3] == 0xff) ||
            (tr[4] && vr[4] == 0xff) || (tr[5] && vr[5] == 0xff) ||
            (tr[6] && vr[6] == 0xff) || (tr[7] && vr[7] == 0xff))
            found = true;
        *virgin &= ~*trace;
        ++trace;
        ++virgin;
    }
    return found;
}

void modify_oracle(string &path_to_oracle, vector<u64> &list, map<u64, char> &breakpoint)
{
    u64 addr;
    char flag[1] = {(char)0xCC};
    int offset = 0;
    fstream oracle_file(path_to_oracle, std::ios::in | std::ios::out | std::ios::binary); // add binary mode
    for (int i = 0; i < MAP_SIZE && i < (int)list.size(); i++)
    {
        addr = list[i] + offset;
        if (addr != 0)
        {
            char original;
            oracle_file.seekg(addr, std::ios::beg);
            oracle_file.read(&original, 1);
            oracle_file.seekp(addr, std::ios::beg); // seekp for writing
            breakpoint[addr] = original;
            oracle_file.write(flag, 1);
        }
    }
    oracle_file.close();
}
void setup_bblist(vector<u64> &list, const string &path_to_bblock)
{
    ifstream file(path_to_bblock); // was hardcoded "./output/.bblist"
    string buf;
    while ((getline(file, buf)))
    {
        char *c = std::strtok((char *)buf.c_str(), ",");
        long long addr = std::strtoll(c, nullptr, 16);
        list.emplace_back(addr);
    }
    file.close();
}

std::string generateRandomFilename(const std::string& extension = ".txt") {
    static std::mt19937 rng(std::random_device{}());
    static const std::string chars = "abcdefghijklmnopqrstuvwxyz0123456789";

    std::uniform_int_distribution<size_t> charDist(0, chars.size() - 1);
    std::uniform_int_distribution<int> lengthDist(8, 16); // filename length between 8 and 16

    int length = lengthDist(rng);
    std::string filename;
    filename.reserve(length);

    for (int i = 0; i < length; i++) {
        filename += chars[charDist(rng)];
    }

    return filename + extension;
}

void trace(const string &path_to_oracle, const string &path_to_trace, const string &path_to_input, string &in_dir, string &out_dir)
{
    memset(trace_bits, 0, MAP_SIZE);
    pid_t pid = fork();
    if (pid == 0)
    {
        // Child — execute the oracle binary with input file
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
        // Parent — wait for child to finish
        int status;
        waitpid(pid, &status, 0);

        // Copy input file to in_dir
        auto saveInputFile = [&]()
        {
            struct stat st;
            if (stat(path_to_input.c_str(), &st) == 0)
            {
                ifstream input_file(path_to_input, std::ios::binary);
                string filename = generateRandomFilename();
                ofstream file(in_dir + "/" + filename, std::ios::binary);
                if (input_file.is_open() && file.is_open())
                {
                    file << input_file.rdbuf();
                }
                input_file.close();
                file.close();
            }
        };

        if (WIFEXITED(status))
        {
            __tracer_classify_counts((u64 *)trace_bits);
            bool found = __tracer_has_bit();
            if (found)
            {
                saveInputFile();
            }
        }
        else if (WIFSIGNALED(status))
        {
            __tracer_classify_counts((u64 *)trace_bits);
            bool found = __tracer_has_bit();
            if (found)
            {
                saveInputFile();
            }
        }
    }
    else
    {
        perror("fork failed");
        exit(1);
    }
}

int main(int argc, char **argv)
{
    vector<u64> bblist;
    map<u64, char> breakpoint;
    bool first_stop = true;
    string path_to_oracle;
    string path_to_trace;
    string path_to_bblock;
    int opt;
    string path_to_input(getenv(INPUT_FILE_ENV) ? getenv(INPUT_FILE_ENV) : "");
    string in_dir(getenv(IN_DIR_ENV) ? getenv(IN_DIR_ENV) : "input");
    string out_dir(getenv(OUT_DIR_ENV) ? getenv(OUT_DIR_ENV) : "output");
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
    __tracer_init_class_lookup16();
    __tracer_init_trace_bits();
    __tracer_init_virgin_bits();
    setup_bblist(bblist, path_to_bblock);
    modify_oracle(path_to_oracle, bblist, breakpoint);

    // Remaining args after options (e.g. target binary args)
    char *args[] = {(char *)path_to_oracle.c_str(), (char *)path_to_input.c_str(), NULL};

    pid_t pid = fork();
    if (pid == 0)
    {
        // Child
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execvp(args[0], args);
        exit(1);
    }
    int status;
    while (true)
    {
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
        {
            cout << "child exited" << endl;
            exit(1);
        }
        if (WIFSTOPPED(status))
        {
            int sig = WSTOPSIG(status);
            if (sig == SIGTRAP) {
                if (first_stop) {
                    first_stop = false;
                    ptrace(PTRACE_CONT, pid, NULL, 0);
                    continue;
                }
                trace(path_to_oracle, path_to_trace, path_to_input, in_dir, out_dir);
                struct user_regs_struct regs;
                ptrace(PTRACE_GETREGS, pid, NULL, &regs);

                // This will print 40147b the FIRST time
                // And it will print 401171 (your breakpoint) the SECOND time
                u64 vaddr    = regs.rip - 1;
                u64 file_off = vaddr - 0x400000;
                printf("Child stopped at RIP: %lld\n", file_off);
                auto found = breakpoint.find(file_off);

                if (found != breakpoint.end())
                {
                    printf("[BP RESTORE] vaddr=0x%lx | original_byte=0x%02x\n",
                        (unsigned long)vaddr, (u8)found->second);

                    u64 data = ptrace(PTRACE_PEEKTEXT, pid, vaddr, NULL);
                    printf("[BP RESTORE] word_before=0x%016lx | low_byte=0x%02x\n",
                        (unsigned long)data, (u8)data);

                    u64 restored = (data & ~0xFFULL) | (u8)found->second;
                    printf("[BP RESTORE] word_after =0x%016lx | low_byte=0x%02x\n",
                        (unsigned long)restored, (u8)restored);

                    errno = 0;
                    if (ptrace(PTRACE_POKETEXT, pid, vaddr, restored) == -1)
                    {
                        perror("[BP RESTORE] PTRACE_POKETEXT failed");
                        exit(1);
                    }

                    u64 verify = ptrace(PTRACE_PEEKTEXT, pid, vaddr, NULL);
                    printf("[BP RESTORE] word_verify=0x%016lx | low_byte=0x%02x | %s\n",
                        (unsigned long)verify, (u8)verify,
                        ((u8)verify == (u8)found->second) ? "OK" : "MISMATCH!");

                    regs.rip = vaddr;
                    ptrace(PTRACE_SETREGS, pid, NULL, &regs);
                    breakpoint.erase(found);
                } else {
                    printf("Address not found at RIP: %lld\n", file_off);
                }

                // Tell the SAME child to keep running
            }
            ptrace(PTRACE_CONT, pid, NULL, sig == SIGTRAP ? 0 : sig);
        }
    }

    return 0;
}