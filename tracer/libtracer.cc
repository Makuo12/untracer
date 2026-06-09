#include "libtracer.h"
#include <cstring>
#include <sys/shm.h>

using std::string;
static long savedDi;
register long rdi asm("di"); // the warning is fine - we need the warning because of a bug in dyninst



static u8 *trace_bits = NULL;
static u8 *trace_blocks = NULL;

void __tracer_init_trace_bits(void)
{
    const char *shm_env = getenv(SHM_ID_ENV);
    if (shm_env == NULL || strlen(shm_env) == 0)
    {
        FATAL_C("Environment variable SHM_ID_ENV is not set");
    }

    int shm_id = atoi(shm_env);
    trace_bits = (u8 *)shmat(shm_id, 0, 0);
    if (trace_bits == (u8 *)-1)
    {
        trace_bits = NULL;
        FATAL_C("Failed to link trace_bits to shared memory");
    }
}

void __tracer_init_trace_blocks(void)
{
    const char *shm_env = getenv(SHM_ID_ENV_BLOCKS);
    if (shm_env == NULL || strlen(shm_env) == 0)
    {
        FATAL_C("Environment variable SHM_ID_ENV is not set");
    }

    int shm_id = atoi(shm_env);
    trace_blocks = (u8 *)shmat(shm_id, 0, 0);
    if (trace_blocks== (u8 *)-1)
    {
        trace_blocks = NULL;
        FATAL_C("Failed to link trace_blocks to shared memory");
    }
}

void __tracer_block_hit(int curblkId)
{
    if (trace_bits != NULL && trace_bits[curblkId] == 0 && curblkId < MAP_SIZE)
    {
        trace_bits[curblkId]++;
    }
    trace_blocks[curblkId]++;
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


