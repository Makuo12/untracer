#ifndef CONFIG_H_
#define CONFIG_H_
#define MAP_SIZE (1 << 16)
#define CAN_TRACE_PIPE "can_trace_pipe"
#define DONE_TRACE_PIPE "done_trace_pipe"
#define PROJECT_NAME 'U'
#define SHM_KEY_FILE_NAME "shm_key_file"
#define SHM_ID_ENV "shm_id_env"
#define SHM_TRAP_BLOCK_IDS_ENV "shm_trap_block_ids_env"
#define SUCCESS "200"
#define FAILURE "600"
#define IN_DIR_ENV "input_dir"
#define OUT_DIR_ENV "output_dir"
#define INPUT_FILE_ENV "input_file"

#ifdef __cplusplus
#include <set>
#include <string>
const std::set<std::string> skipFunctions = {
    // CRT
    "_start", "__libc_start_main",
    "_init", "_fini",
    "__libc_csu_init", "__libc_csu_fini",
    "register_tm_clones", "deregister_tm_clones",
    "__do_global_ctors_aux", "__do_global_dtors_aux",
    "frame_dummy",
    "__cxa_atexit", "__cxa_finalize",
    "malloc", "calloc", "realloc", "free", "FATAL", "SAY", "__wrap_main"
};
#endif
#endif