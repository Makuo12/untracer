#ifndef DATA_H_
#define DATA_H_
#include <sys/types.h> // Standard POSIX types
typedef struct {
    char d_name[256];
    off_t st_size; // off_t is the standard POSIX type for file sizes
    char file_path[512];
    int has_issues;
    int single_pass;
    int full_pass;
    int path_found;
    int trace_count;
} Entry;
#ifdef __cplusplus
#include <string>
class EntryClass {
    public:
    std::string filename;
    off_t size;
    std::string file_path;
    bool has_issues = false;
    int single_pass = 0;
    int full_pass = 0;
    int path_found = 0;
    int trace_count = 0;
    EntryClass(std::string filename, off_t size, std::string file_path): filename(filename), size(size), file_path(file_path) {}
};
#endif

#endif 