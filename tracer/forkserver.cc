#include "libtracer.h"

__attribute((constructor)) void __tracer_forkserver(int argc, char **argv) {
    __tracer_init_trace_bits();
    __trace_init_class_lookup16();
    __tracer_init_virgin_bits();
    std::string in_dir, out_dir, out_file_path;
    if (in_dir.size() == 0 || out_dir.size() == 0 || out_file_path.size() == 0)
    {
        FATAL("Please set all flags -o (output path), -p (path for file), -i (input directory)");
    }
    __tracer_fuzz();
}
