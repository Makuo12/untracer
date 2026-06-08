import argparse
import subprocess
import random
import string
from pathlib import Path
import shutil
import os

SHM_KEY_FILE_NAME = "shm_key_file"
SHM_ID_ENV = "shm_id_env"
SHM_TRAP_BLOCK_IDS_ENV = "shm_trap_block_ids_env"
SUCCESS = "200"
FAILURE = "600"
IN_DIR_ENV = "input_dir"
OUT_DIR_ENV = "output_dir"
INPUT_FILE_ENV = "input_file"


lib_items = []

def create_input(size):
    test = "test"
    shutil.rmtree(test, ignore_errors=True)
    shutil.rmtree("output", ignore_errors=True)
    os.makedirs("test", exist_ok=True)
    os.makedirs("output", exist_ok=True)
    for i in range(size):
        filename = f"{test}_{i}"
        path = os.path.join(test, filename)
        with open(path, "w") as f:
            text = ''.join(random.choices(string.ascii_letters, k=40))
            f.write(text)

            

def create_libs(compiler="g++", include="-I ./include", headers = ""):
    shutil.rmtree("libs", ignore_errors=True)
    main_path = "/home/makuo12/Documents/forte-research/build"
    os.makedirs("libs", exist_ok=True)
    for filename in os.listdir(main_path):
        path = os.path.join(main_path, filename)
        if os.path.isdir(path):
            for subfilename in os.listdir(path):
                if subfilename.endswith(".so"):
                    src = os.path.join(path, subfilename)
                    dst = os.path.join("libs", subfilename)
                    split_name = subfilename.split(".")
                    name = "-l" + split_name[0][3:]
                    lib_items.append(name)
                    os.symlink(src, dst)
                    os.symlink(src, dst + ".13.0")
    tracer_lib = f"{compiler} {include} ./tracer/libtracer.cc -shared -fPIC -o ./libs/libtracer.so"
    result = subprocess.run(tracer_lib, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    oracle_lib = f"{compiler} {include} {headers} ./oracle/liboracle.cc -shared -fPIC -o ./libs/liboracle.so"
    result = subprocess.run(oracle_lib, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    oracle_dyninst_lib = f"{compiler} {include} {headers} ./oracle/oracle_dyninst.cc -shared -fPIC -o ./libs/liboracle_dyninst.so"
    result = subprocess.run(oracle_dyninst_lib, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    # lib_items.append("-ltracer")
    
def get_headers(pattern = "h", path = "/home/makuo12/Documents/forte-research/dyninst"):
    names = []
    for p in Path(path).rglob(pattern):
        if p.is_dir():
            names.append(str(p))
    return set(names)


def create_arguments():
    tmp = "input"
    os.makedirs(tmp, exist_ok=True)
    parser = argparse.ArgumentParser(description="Untracer")
    parser.add_argument("-i", dest="in_dir", required=True, help="Directory holding inputs")
    parser.add_argument("-o", dest="out_dir", required=True, help="Directory for outputs")
    parser.add_argument("input_file", nargs="?", help="Directory holding inputs")
    args = parser.parse_args()
    in_dir        = args.in_dir
    out_dir       = args.out_dir
    input_file    = args.input_file 
    if input_file == "@@":
        input_file = os.path.join(tmp, "cur_input")
        # os.makedirs(out_dir, exist_ok=True)
        open(input_file, "w").close() 
    shm_key_file = os.path.join(tmp, SHM_KEY_FILE_NAME)
    open(shm_key_file, "w").close()
    os.environ[IN_DIR_ENV] = in_dir
    os.environ[OUT_DIR_ENV] = out_dir
    os.environ[SHM_KEY_FILE_NAME] = shm_key_file
    os.environ[INPUT_FILE_ENV] = input_file
    return input_file

def create_elf(compiler, target_name):
    oracle_elf = f"{compiler} {target_name} -Wl,-force_load,liboracle_forkserver.a -o oracle.elf"
    result = subprocess.run(oracle_elf, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    trace_elf = f"{compiler} {target_name} -Wl,-force_load,libtracer_forkserver.a -o tracer.elf"
    result = subprocess.run(trace_elf, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)

def setup_tracer(compiler="g++", include="-I ./include", headers = ""):

    forkserver = "./tracer/forkserver.c"
    object_file = "./build/tracer_forkserver.o"
    tracer_o = f"gcc {include} {forkserver} -c -o {object_file}"
    result = subprocess.run(tracer_o, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    # tracer_a = f"ar rcs {a_file} {object_file}"
    # result = subprocess.run(tracer_a, shell=True, stderr=subprocess.PIPE, text=True)
    # if result.returncode != 0:
    #     print(f"Compilation failed:\n{result.stderr}")
    #     exit(1)
    tracer_elf = f"{compiler} {include} target.cc {object_file} -Wl,--wrap=main -L./libs -Wl,-rpath,./libs -no-pie -ltracer -o ./build/tracer.elf"
    print(tracer_elf)
    result = subprocess.run(tracer_elf, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    names = " ".join(item for item in lib_items)
    dyninst_elf = f"{compiler} {include} {headers} ./tracer/tracer_dyninst.cc -L./libs -Wl,-rpath,./libs -Wl,--start-group {names} -ltracer -Wl,--end-group -o ./build/tracer_dyninst.elf"
    print(dyninst_elf)
    result = subprocess.run(dyninst_elf, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    result = subprocess.run(["./build/tracer_dyninst.elf", "./build/tracer.elf"])
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)

def setup_oracle(compiler="g++", include="-I ./include", headers = ""):
    forkserver = "./oracle/forkserver.c"
    object_file = "./build/oracle_forkserver.o"
    oracle_o = f"gcc {include} {headers} {forkserver} -c -o {object_file}"
    result = subprocess.run(oracle_o, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    # oracle_a = f"ar rcs {a_file} {object_file}"
    # result = subprocess.run(oracle_a, shell=True, stderr=subprocess.PIPE, text=True)
    # if result.returncode != 0:
    #     print(f"Compilation failed:\n{result.stderr}")
    #     exit(1)
    oracle_elf = f"{compiler} {include} target.cc {object_file} -Wl,--wrap=main -L./libs -Wl,-rpath,./libs -no-pie -loracle -o ./build/oracle.elf"
    print(oracle_elf)
    result = subprocess.run(oracle_elf, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    names = " ".join(item for item in lib_items)
    dyninst_elf = f"{compiler} {include} {headers} ./oracle/oracle_dyninst.cc -L./libs -Wl,-rpath,./libs -Wl,--start-group {names} -Wl,--end-group -o ./build/oracle_dyninst.elf"
    print(dyninst_elf)
    result = subprocess.run(dyninst_elf, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    result = subprocess.run(["./build/oracle_dyninst.elf", "./build/oracle.elf"])
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)

def setup_oracle_pdftotext(include="-I ./include"):
    xpdf_dir = "/home/makuo12/Documents/forte-research/untracer/xpdf-4.06_2"
    libs_dir = "/home/makuo12/Documents/forte-research/untracer/libs"
    obj_dir = "/home/makuo12/Documents/forte-research/untracer/build"
    output = "/home/makuo12/Documents/forte-research/untracer/target/pdftotext.oracle"

    # Step 1: compile forkserver
    forkserver = "./oracle/forkserver.c"
    object_file = f"{obj_dir}/oracle_forkserver.o"
    oracle_o = f"gcc {include} {forkserver} -c -o {object_file}"
    result = subprocess.run(oracle_o, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Oracle forkserver compilation failed:\n{result.stderr}")
        exit(1)

    # Step 2: cmake configure
    build_dir = f"{xpdf_dir}/build"
    os.makedirs(build_dir, exist_ok=True)

    cmake_cmd = (
        f"cmake -DCMAKE_BUILD_TYPE=Release "
        f"-DCMAKE_C_COMPILER=gcc "
        f"-DCMAKE_CXX_COMPILER=g++ "
        f"-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY "
        f"-DCMAKE_CXX_FLAGS=\"-fno-pie\" "
        f"-DCMAKE_C_FLAGS=\"-fno-pie\" "
        f"-DCMAKE_EXE_LINKER_FLAGS=\"-L{libs_dir} -loracle -no-pie -Wl,--wrap=main {object_file}\" "
        f"{xpdf_dir}/"
    )
    print(cmake_cmd)
    result = subprocess.run(cmake_cmd, shell=True, cwd=build_dir, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"CMake configuration failed:\n{result.stderr}")
        exit(1)

    # Step 3: make pdftotext
    result = subprocess.run("make pdftotext", shell=True, cwd=build_dir, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Make failed:\n{result.stderr}")
        exit(1)

    # Step 4: copy output
    result = subprocess.run(f"cp {build_dir}/xpdf/pdftotext {output}", shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Copy failed:\n{result.stderr}")
        exit(1)

    print(f"Oracle pdftotext built -> {output}")


def setup_tracer_pdftotext(include="-I ./include"):
    xpdf_dir = "/home/makuo12/Documents/forte-research/untracer/xpdf-4.06_2"
    libs_dir = "/home/makuo12/Documents/forte-research/untracer/libs"
    obj_dir = "/home/makuo12/Documents/forte-research/untracer/build"
    output = "/home/makuo12/Documents/forte-research/untracer/target/pdftotext.trace"

    # Step 1: compile forkserver
    forkserver = "./tracer/forkserver.c"
    object_file = f"{obj_dir}/tracer_forkserver.o"
    tracer_o = f"gcc {include} {forkserver} -c -o {object_file}"
    result = subprocess.run(tracer_o, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Tracer forkserver compilation failed:\n{result.stderr}")
        exit(1)

    # Step 2: cmake configure
    build_dir = f"{xpdf_dir}/build"
    os.makedirs(build_dir, exist_ok=True)

    cmake_cmd = (
        f"cmake -DCMAKE_BUILD_TYPE=Release "
        f"-DCMAKE_C_COMPILER=gcc "
        f"-DCMAKE_CXX_COMPILER=g++ "
        f"-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY "
        f"-DCMAKE_CXX_FLAGS=\"-fno-pie\" "
        f"-DCMAKE_C_FLAGS=\"-fno-pie\" "
        f"-DCMAKE_EXE_LINKER_FLAGS=\"-L{libs_dir} -ltracer -no-pie -Wl,--wrap=main {object_file}\" "
        f"{xpdf_dir}/"
    )
    print(cmake_cmd)
    result = subprocess.run(cmake_cmd, shell=True, cwd=build_dir, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"CMake configuration failed:\n{result.stderr}")
        exit(1)

    # Step 3: make pdftotext
    result = subprocess.run("make pdftotext", shell=True, cwd=build_dir, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Make failed:\n{result.stderr}")
        exit(1)

    # Step 4: copy output
    result = subprocess.run(f"cp {build_dir}/xpdf/pdftotext {output}", shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Copy failed:\n{result.stderr}")
        exit(1)

    print(f"Tracer pdftotext built -> {output}")

def setup_untracer(compiler="g++", include="-I ./include"):
    untracer_elf = f"{compiler} {include} untracer.cc -o untracer.elf"
    result = subprocess.run(untracer_elf, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
        
def run_untracer():
    # cerr << "Usage: " << argv[0] << " -o <oracle> -t <trace> -b <bblock> -i <input>" << endl;
    result = subprocess.run(["./untracer.elf", "-t", "./output/tracer_instrumented.elf", "-b", "./output/.bblist", "-o", "./build/oracle.elf"])
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)


def main():
    print("Setting up untracer...")
    headers = " -I ".join(head for head in get_headers())
    if len(headers) > 0:
        headers = "-I " + headers
    create_input(10)
    create_libs(headers = headers)
    setup_tracer(headers = headers)
    setup_oracle(headers = headers) 
    input_file = create_arguments()
    setup_untracer()
    run_untracer()
    #create_archives(compiler, include, forkserver_name, archive_filename, object_filename) 
    # create_elf(compiler, target_name)
    # result = subprocess.run(["./oracle.elf", input_file])



if __name__ == "__main__":
    main()