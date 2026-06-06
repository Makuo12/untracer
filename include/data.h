#ifndef DATA_H_
#define DATA_H_

#ifdef __cplusplus
#include <string>
class Entry {
    public:
    std::string filename;
    off_t size;
    std::string file_path;
    bool has_issues = false;
    int single_pass = 0;
    int full_pass = 0;
    int path_found = 0;
    int trace_count = 0;
    Entry(std::string filename, off_t size, std::string file_path): filename(filename), size(size), file_path(file_path) {}
};
#endif

#endif 