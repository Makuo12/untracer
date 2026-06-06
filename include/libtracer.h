#ifndef LIBTRACER_H_
#define LIBTRACER_H_

#include <getopt.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>

#ifdef __cplusplus
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
void __tracer_cleanup_trace_bits(void);
void __tracer_fuzz(void);
void __tracer_init_trace_bits(void);
void __tracer_block_hit(int curblkId);
void __tracer_fuzz(void);
void __tracer_init_virgin_bits(void);
void __trace_init_class_lookup16(void);
#endif

#include "data.h"
#include "logger.h"
#include "types.h"
#include "config.h"

#endif