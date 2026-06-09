#ifndef LIBTRACER_H_
#define LIBTRACER_H_
#include "data.h"
#include "logger.h"
#include "types.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
    int __real_main(int, char **);
    void __tracer_init_trace_bits(void);
    void __tracer_init_trace_blocks(void);
}

#endif




#endif