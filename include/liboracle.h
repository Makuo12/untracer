#ifndef LIBORACLE_H_
#define LIBORACLE_H_

#include <getopt.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "data.h"
#include "logger.h"
#include "types.h"
#include "config.h"
void __oracle_init(Entry **entries, int *entry_count, const char *input_file);
#ifdef __cplusplus
extern "C"
{
#endif

    void __oracle_write_testcase(u8 *mem, Entry *entry, const char *input_file);
    void __oracle_init_shm(void);
    int __real_main(int, char **);

#ifdef __cplusplus
}
#endif

#endif // u8 is in types