#ifndef CONFIG_H_
#define CONFIG_H_
#define MAP_SIZE (1 << 16)
#define BLOCK_SIZE 300000
#define CAN_TRACE_PIPE "can_trace_pipe"
#define DONE_TRACE_PIPE "done_trace_pipe"
#define PROJECT_NAME 'U'
#define SHM_KEY_FILE_NAME "shm_key_file"
#define SHM_ID_ENV "shm_id_env"
#define SHM_ID_ENV_BLOCKS "shm_id_env_blocks"
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
    "_start",
    "start",
    "__libc_start_main",
    "__libc_start_call_main",
    "_init",
    "_fini",
    "__libc_csu_init",
    "__libc_csu_fini",
    "register_tm_clones",
    "deregister_tm_clones",
    "__do_global_ctors_aux",
    "__do_global_dtors_aux",
    "frame_dummy",
    "__cxa_atexit",
    "__cxa_finalize",
    "__cxa_throw",
    "__cxa_begin_catch",
    "__cxa_end_catch",
    "__cxa_rethrow",
    "__gmon_start__",
    "__assert_fail",
    "_dl_start",
    "_dl_relocate_static_pie",

    // Memory
    "malloc",
    "calloc",
    "realloc",
    "free",

    // I/O and util
    "printf",
    "fnmatch",
    "readlinkat",
    "FATAL",
>>>>>>> 1b5e0e5000e2327bb1e98321ab6f2faac4a4bafa
    "SAY",

    // Argp
    "argp_failure",
    "argp_help",
    "argp_state_help",
    "argp_error",
    "argp_parse",

    // Entry
    "__wrap_main",
    "main",

    // Oracle functions
    "__oracle_init",
    "__oracle_init_shm",
    "__oracle_init@plt",
    "__oracle_trap_hit",
    "__oracle_trap_hit@plt",
    "__oracle_save_rdi",
    "__oracle_save_rdi@plt",
    "__oracle_restore_rdi",
    "__oracle_restore_rdi@plt",
    "__oracle_fuzz",
    "__oracle_apply",
    "__oracle_write_testcase",

    // Tracer internals
    "__tracer_block_hit",
    "__tracer_init_virgin_bits",
    "__tracer_cleanup_trace_bits",
    "__tracer_init_trace_bits",
    "__tracer_init_count_class16",
    "__tracer_init_class_lookup16",
    "__tracer_classify_counts",
    "__tracer_has_bit",
    "__trace_save_rdi",
    "__trace_restore_rdi",

    // AFL / forkserver
    "__afl_maybe_log",
    "__fsrvonly_store",
    "__fsrvonly_return",
    "__fsrvonly_setup",
    "__fsrvonly_setup_first",
    "__fsrvonly_forkserver",
    "__fsrvonly_fork_wait_loop",
    "__fsrvonly_fork_resume",
    "__fsrvonly_die",
    "__fsrvonly_setup_abort",
    ".AFL_SHM_ENV",
};
#endif
#endif