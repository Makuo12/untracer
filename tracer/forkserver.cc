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
    if (trace_bits && trace_bits[curblkId] == 0) {
        trace_bits[curblkId]++;
        // SAY({"trace_bits", trace_bits[curblkId]});
    }
}

extern "C" int __wrap_main(int argc, char **argv) {
    __tracer_init_trace_bits();
    return __real_main(argc, argv);
}

// void __tracer_fuzz(int argc, char **argv) {
//     while (true) {
//         int can_trace_fd = open(CAN_TRACE_PIPE, O_RDONLY);
//         if (can_trace_fd < 0) {
//             FATAL({"Tracer:", "Could not pipe data to oracle", CAN_TRACE_PIPE});
//         }
//         char buf[1024];
//         read(can_trace_fd, buf, 1024);
//         close(can_trace_fd);
//         if (std::strstr(buf, SUCCESS)) {
//             // Perform tracing
//             int ret = __real_main(argc, argv);
//             __tracer_classify_counts((u64 *)trace_bits);
//             bool found = __tracer_has_bit();
//             if (found) {
//                 int done_trace_fd = open(DONE_TRACE_PIPE, O_WRONLY);
//                 if (done_trace_fd < 0)
//                 {
//                     FATAL({"Tracer:", "Could not pipe done data to oracle", CAN_TRACE_PIPE});
//                 }
//                 write(done_trace_fd, SUCCESS, std::strlen(SUCCESS) + 1);
//                 close(done_trace_fd);
//             } else {
//                 int done_trace_fd = open(DONE_TRACE_PIPE, O_WRONLY);
//                 if (done_trace_fd < 0) {
//                     FATAL({"Tracer:", "Could not pipe done data to oracle", CAN_TRACE_PIPE});
//                 }
//                 write(done_trace_fd, FAILURE, strlen(FAILURE) + 1);
//                 close(done_trace_fd);
//             }
//         } else {
//             int done_trace_fd = open(DONE_TRACE_PIPE, O_WRONLY);
//             if (done_trace_fd < 0) {
//                 FATAL({"Tracer:", "Could not pipe done data to oracle", CAN_TRACE_PIPE});
//             }
//             write(done_trace_fd, FAILURE, strlen(FAILURE) + 1);
//             close(done_trace_fd);
//         }
//     }
// }