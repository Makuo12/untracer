#include "libtracer.h"

using std::string;
static long savedDi;
register long rdi asm("di"); // the warning is fine - we need the warning because of a bug in dyninst

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

void __trace_save_rdi()
{
    savedDi = rdi;
    /*
    asm("pop %rax"); // take care of rip
    asm("push %rdi");
    asm("push %rax");
    */
}

void __trace_restore_rdi()
{
    rdi = savedDi;
    /*
    asm("pop %rax"); // take care of rip
    asm("pop %rdi");
    asm("push %rax");
    */
}


void __tracer_init_trace_bits(void) {
    key_t key = ftok(SHM_KEY_FILE, PROJECT_NAME);
    if (key == -1) {
        FATAL({"ftok failed to generate key", SHM_KEY_FILE});
    }
    int shm_id = shmget(key, MAP_SIZE, IPC_CREAT | IPC_EXCL | 0666);
    if (shm_id < 0) {
        FATAL({"key failed to get shm_id", std::to_string(key)});
    }
    __trace_shm_id = shm_id;
    if (atexit(__tracer_cleanup_trace_bits) != 0) {
        shmctl(shm_id, IPC_RMID, NULL);
        FATAL("Failed to register shared memory cleanup");
    }
    string shm_str = std::to_string(key);
    setenv(SHM_ID_ENV, shm_str.c_str(), 1);
    trace_bits = (u8 *)shmat(shm_id, 0, 0);
    if (trace_bits == (u8 *)-1) {
        shmctl(shm_id, IPC_RMID, NULL);
        FATAL({"failed to link trace_bits to memory", std::to_string(key)});
    }
}

void __tracer_block_hit(int curblkId) {
    if (trace_bits && trace_bits[curblkId] == 0) {
        trace_bits[curblkId]++;
    }
}

static u8 __trace_count_class_lookup8[256];


void __trace_init_class_lookup16() {
    __trace_count_class_lookup8[0] = 0;
    __trace_count_class_lookup8[1] = 1;
    __trace_count_class_lookup8[2] = 2;
    __trace_count_class_lookup8[3] = 3;
    for (int i = 4;   i <= 7;   i++) __trace_count_class_lookup8[i] = 8;
    for (int i = 8;   i <= 15;  i++) __trace_count_class_lookup8[i] = 16;
    for (int i = 16;  i <= 31;  i++) __trace_count_class_lookup8[i] = 32;
    for (int i = 32;  i <= 127; i++) __trace_count_class_lookup8[i] = 64;
    for (int i = 128; i <= 255; i++) __trace_count_class_lookup8[i] = 128;
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

void __tracer_fuzz() {
    while (true) {
        int can_trace_fd = open(CAN_TRACE_PIPE, O_RDONLY);
        if (can_trace_fd < 0) {
            FATAL({"Tracer:", "Could not pipe data to oracle", CAN_TRACE_PIPE});
        }
        char buf[1024];
        read(can_trace_fd, buf, 1024);
        close(can_trace_fd);
        if (std::strstr(buf, SUCCESS)) {
            // Perform tracing
            pid_t child = fork();
            if (child == 0) {
                // We are in child
                // We want to write to the output file
                return;
            } else {
                // We are in parent
                int status;
                waitpid(child, &status, 0);
                // Here we calculate coverage
                __tracer_classify_counts((u64 *)trace_bits);
                bool found = __tracer_has_bit();
                if (found) {
                    int done_trace_fd = open(DONE_TRACE_PIPE, O_WRONLY);
                    if (done_trace_fd < 0)
                    {
                        FATAL({"Tracer:", "Could not pipe done data to oracle", CAN_TRACE_PIPE});
                    }
                    write(done_trace_fd, SUCCESS, std::strlen(SUCCESS) + 1);
                    close(done_trace_fd);
                } else {
                    int done_trace_fd = open(DONE_TRACE_PIPE, O_WRONLY);
                    if (done_trace_fd < 0) {
                        FATAL({"Tracer:", "Could not pipe done data to oracle", CAN_TRACE_PIPE});
                    }
                    write(done_trace_fd, FAILURE, strlen(FAILURE) + 1);
                    close(done_trace_fd);
                }
            }
        } else {
            int done_trace_fd = open(DONE_TRACE_PIPE, O_WRONLY);
            if (done_trace_fd < 0) {
                FATAL({"Tracer:", "Could not pipe done data to oracle", CAN_TRACE_PIPE});
            }
            write(done_trace_fd, FAILURE, strlen(FAILURE) + 1);
            close(done_trace_fd);
        }
    }
}