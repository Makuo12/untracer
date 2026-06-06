#include "liboracle.h"

using std::vector;
using std::string;

__attribute__((constructor)) void __oracle_forkserver(int argc, char **argv) {
    std::cout << "Running oracle" << std::endl;
    vector<Entry> entries;
    string input_file(argv[optind]);
    __oracle_init(entries, input_file);
    __oracle_init_dyninst(argc, argv);
    __oracle_fuzz(entries, input_file);
}
