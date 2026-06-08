// forkserver.cc
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include "logger.h"
#include "config.h"
#include "libtracer.h"


int __wrap_main(int argc, char **argv)
{
    __tracer_init_trace_bits();
    return __real_main(argc, argv);
}