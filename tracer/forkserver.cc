#include "libtracer.h"


// extern int __real_main(int argc, char **argv);

// int __wrap_main(int argc, char **argv) {
//     __tracer_init_trace_bits();
//     __trace_init_class_lookup16();
//     __tracer_init_virgin_bits();
//     std::string in_dir, out_dir, out_file_path;
//     if (in_dir.size() == 0 || out_dir.size() == 0 || out_file_path.size() == 0)
//     {
//         FATAL("Please set all flags -o (output path), -p (path for file), -i (input directory)");
//     }
//     __tracer_fuzz(argc, argv);
//     return 0;
// }

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