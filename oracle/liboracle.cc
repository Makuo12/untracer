#include "liboracle.h"
#include <fstream>
#include "logger.h"
#include <cstring>
#include "types.h"

using std::string;
using std::ofstream;

u64 *trap_block_ids;

static long savedDi;
register long rdi asm("di"); // the warning is fine - we need the warning because of a bug in dyninst

void __oracle_init(Entry **entries, int *entry_count, const char *input_file)
{
    // 1. Handle environment variables and default fallback strings
    const char *env_in = getenv(IN_DIR_ENV);
    const char *env_out = getenv(OUT_DIR_ENV);

    const char *in_dir = env_in ? env_in : "test";
    const char *out_dir = env_out ? env_out : "output";

    // 2. Validate string lengths (equivalent to .size() == 0 or empty checking)
    if (strlen(in_dir) == 0 || strlen(out_dir) == 0 || input_file == NULL || strlen(input_file) == 0)
    {
        FATAL_C("Please set all flags -o (output path), -p (path for file), -i (input directory)");
    }

    // 3. Scan directory
    struct dirent **items;
    int count = scandir(in_dir, &items, NULL, alphasort);
    if (count < 0)
    {
        FATAL("Failed to scan input directory");
    }
    if (count == 0)
    {
        free(items); // Don't forget to free the top-level pointer if count is 0
        FATAL("No input files found to fuzz");
    }

    // 4. Set up dynamic array variables to replicate std::vector behavior
    int capacity = 10; // Start with an initial capacity
    *entry_count = 0;
    *entries = (Entry *)malloc(capacity * sizeof(Entry));
    if (*entries == NULL)
    {
        FATAL("Memory allocation failed for entries array");
    }

    struct stat st;
    // 5. Loop through directory items
    for (int i = 0; i < count; ++i)
    {
        if (items[i]->d_type != DT_REG)
        {
            free(items[i]);
            continue;
        }

        char file_path[512];
        snprintf(file_path, sizeof(file_path), "%s/%s", in_dir, items[i]->d_name);

        if (stat(file_path, &st) == 0)
        {
            // Resize dynamic array if capacity is reached (std::vector emulation)
            if (*entry_count >= capacity)
            {
                capacity *= 2;
                Entry *temp = (Entry *)realloc(*entries, capacity * sizeof(Entry));
                if (temp == NULL)
                {
                    FATAL("Memory reallocation failed while scanning entries");
                }
                *entries = temp;
            }
            // Populate the entry data directly into the array block
            Entry *current_entry = &((*entries)[*entry_count]);
            snprintf(current_entry->d_name, sizeof(current_entry->d_name), "%s", items[i]->d_name);
            snprintf(current_entry->file_path, sizeof(current_entry->file_path), "%s", file_path);
            current_entry->st_size = st.st_size;

            (*entry_count)++;
        }
        free(items[i]);
    }
    free(items);

    if (*entry_count == 0)
    {
        free(*entries);
        *entries = NULL;
        FATAL("No valid input files found to fuzz");
    }
}


#ifdef __cplusplus
extern "C"
{
#endif

void __oracle_write_testcase(u8 * mem, Entry * entry, const char *input_file) {
    ofstream file(input_file);
    if (!file.is_open()) {
        FATAL_C("failed to open input file");
    }
    file.write((char *)mem, entry->st_size);
    file.close();
}


void __oracle_init_shm(void) {
    int shm_id = shmget(IPC_PRIVATE, sizeof(*trap_block_ids), IPC_CREAT | IPC_EXCL | 0600);
    if (shm_id < 0) {
        FATAL({"Failed to create shared memory"}); 
    }
    string shm_id_str = std::to_string(shm_id);
    setenv(SHM_TRAP_BLOCK_IDS_ENV, shm_id_str.c_str(), 1);
    trap_block_ids = (u64 *)shmat(shm_id, NULL, 0);
    if (trap_block_ids == (u64 *)-1) {
        shmctl(shm_id, IPC_RMID, NULL);
        FATAL({"Failed to link trap_block_ids to memory"});
    }
}

#ifdef __cplusplus
}
#endif


void __oracle_save_rdi()
{
    savedDi = rdi;
    /*
    asm("pop %rax"); // take care of rip
    asm("push %rdi");
    asm("push %rax");
    */
}

void __oracle_restore_rdi()
{
    rdi = savedDi;
    /*
    asm("pop %rax"); // take care of rip
    asm("pop %rdi");
    asm("push %rax");
    */
}

void __oracle_trap_hit(int blkId) {
    *trap_block_ids = (u64)blkId;
    MEM_BARRIER();
    __asm__ volatile("int3");
}

