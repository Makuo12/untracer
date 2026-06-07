#include "liboracle.h"

using std::string;
using std::vector;



static long savedDi;
register long rdi asm("di"); // the warning is fine - we need the warning because of a bug in dyninst


void __oracle_apply(u8 * mem, int position) {
    mem[position >> 3] ^= (128 >> (position & 7));
}

u64 *trap_block_ids;

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


void __oracle_init(vector<Entry> &entries, string &input_file) {
    string in_dir(getenv(IN_DIR) ? getenv(IN_DIR) : "test");
    string out_dir(getenv(OUT_DIR) ? getenv(OUT_DIR) : "output");
    if (in_dir.size() == 0 || out_dir.size() == 0 || input_file.size() == 0) {
        FATAL("Please set all flags -o (output path), -p (path for file), -i (input directory)");
    }
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
}