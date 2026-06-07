
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
#include "BPatch_function.h"

#ifdef __cplusplus
#include <string>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <map>
void __oracle_init(std::vector<Entry> &entries, std::string &input_file);
void __oracle_apply(u8 * mem, int position);
void __oracle_fuzz(std::vector<Entry> &entries, std::string &input_file);
void __oracle_init_dyninst(int argc, char ** argv);
void __oracle_fuzz(int argc, char **argv, std::vector<Entry> &entries, std::string &input_file);
#endif


#endif