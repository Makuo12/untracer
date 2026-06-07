// forkserver.cc
#include "libtracer.h"
#include <sys/shm.h>


using std::string;
u8 * trace_bits;

extern "C" int __real_main(int argc, char **argv);

void __tracer_init_trace_bits(void) {
    string shm_str(getenv(SHM_ID_ENV) ? getenv(SHM_ID_ENV) : "");
    if (shm_str.size() == 0) {
        FATAL({"Environment variable", SHM_ID_ENV, "is not set"});
    }
    int shm_id = std::stoi(shm_str);
    trace_bits = (u8 *)shmat(shm_id, 0, 0);
    if (trace_bits == (u8 *)-1) {
        FATAL({"failed to link trace_bits to memory", shm_str});
    }
}

void __tracer_block_hit(int curblkId) {
    if (trace_bits && trace_bits[curblkId] < 255) {
        trace_bits[curblkId]++;
        // SAY({"trace_bits", trace_bits[curblkId]});
    }
}

extern "C" int __wrap_main(int argc, char **argv) {
    __tracer_init_trace_bits();
    return __real_main(argc, argv);
}
