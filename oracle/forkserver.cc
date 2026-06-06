


using std::string;
using std::vector;

void __oracle_fuzz(vector<Entry> &entries, string &out_file_path);
void __oracle_init_dyninst(int argc, char ** argv);



__attribute__((constructor)) void __oracle_forkserver(int argc, char **argv) {
    std::cout << "Running oracle" << std::endl;
    __oracle_init_dyninst(argc, argv);
    string in_dir(getenv(IN_DIR) ? getenv(IN_DIR) : "");
    string out_dir(getenv(OUT_DIR) ? getenv(OUT_DIR) : "");
    string input_file(argv[optind]);
    if (in_dir.size() == 0 || out_dir.size() == 0 || input_file.size() == 0) {
        FATAL("Please set all flags -o (output path), -p (path for file), -i (input directory)");
    }
    vector<Entry> entries;
    dirent **items;
    int count = scandir(in_dir.data(), &items, NULL, alphasort);
    if (count < 0) {
        FATAL("Failed to scan input directory");
    }
    if (count == 0) {
        FATAL("No input files found to fuzz");
    }
    struct stat st;
    for (int i = 0; i < count; ++i) {
        if (items[i]->d_type != DT_REG) continue;
        string file_path = in_dir + "/" + items[i]->d_name;
        if (stat(file_path.c_str(), &st) == 0) {
            Entry entry{items[i]->d_name, st.st_size, file_path};
            entries.emplace_back(entry);
        }
        free(items[i]);
    }
    free(items);
    if (entries.empty()) {
        FATAL("No valid input files found to fuzz");
    }
    __oracle_fuzz(entries, input_file);
}
