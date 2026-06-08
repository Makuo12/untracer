// forkserver.cc
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include "logger.h"
#include "config.h"
#include "libtracer.h"

static u8 *trace_bits = NULL;

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

void __tracer_block_hit(int curblkId)
{
    if (trace_bits != NULL && trace_bits[curblkId] == 0)
    {
        trace_bits[curblkId]++;
    }
}

int __wrap_main(int argc, char **argv)
{
    __tracer_init_trace_bits();
    return __real_main(argc, argv);
}