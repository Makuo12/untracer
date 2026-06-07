import argparse
import subprocess
import random
import string
from pathlib import Path
import shutil
import os

IN_DIR = "input_dir"
OUT_DIR = "output_dir"

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
    parser = argparse.ArgumentParser(description="Untracer")
    parser.add_argument("-i", dest="in_dir", required=True, help="Directory holding inputs")
    parser.add_argument("-o", dest="out_dir", required=True, help="Directory for outputs")
    parser.add_argument("input_file", nargs="?", help="Directory holding inputs")
    args = parser.parse_args()
    in_dir        = args.in_dir
    out_dir       = args.out_dir
    input_file    = args.input_file 
    if input_file == "@@":
        tmp = "input"
        os.makedirs(tmp, exist_ok=True)
        input_file = os.path.join(tmp, "cur_input")
        # os.makedirs(out_dir, exist_ok=True)
        open(input_file, "w").close() 
    os.environ[IN_DIR] = in_dir
    os.environ[OUT_DIR] = out_dir
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
    forkserver = "./tracer/forkserver.cc"
    object_file = "tracer_forkserver.o"
    a_file = "tracer_forkserver.a"
    tracer_o = f"{compiler} {include} {forkserver} -c -o {object_file}"
    result = subprocess.run(tracer_o, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    tracer_a = f"ar rcs {a_file} {object_file}"
    result = subprocess.run(tracer_a, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    tracer_elf = f"{compiler} {include} -L./libs -Wl,-rpath,./libs -no-pie -Wl,--whole-archive {a_file} -Wl,--no-whole-archive -ltracer target.cc -o tracer.elf"
    print(tracer_elf)
    result = subprocess.run(tracer_elf, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    names = " ".join(item for item in lib_items)
    dyninst_elf = f"{compiler} {include} {headers}  ./tracer/tracer_dyninst.cc -L./libs -Wl,-rpath,./libs -Wl,--start-group {names} -Wl,--end-group -o tracer_dyninst.elf"
    print(dyninst_elf)
    result = subprocess.run(dyninst_elf, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)

def setup_oracle(compiler="g++", include="-I ./include", headers = ""):
    # forkserver = "./oracle/forkserver.cc"
    # object_file = "oracle_forkserver.o"
    # a_file = "oracle_forkserver.a"
    # oracle_o = f"{compiler} {include} {headers} {forkserver} -g -c -o {object_file}"
    # result = subprocess.run(oracle_o, shell=True, stderr=subprocess.PIPE, text=True)
    # if result.returncode != 0:
    #     print(f"Compilation failed:\n{result.stderr}")
    #     exit(1)
    # oracle_a = f"ar rcs {a_file} {object_file}"
    # result = subprocess.run(oracle_a, shell=True, stderr=subprocess.PIPE, text=True)
    # if result.returncode != 0:
    #     print(f"Compilation failed:\n{result.stderr}")
    #     exit(1)
    oracle_elf = f"{compiler} {include} -L./libs -Wl,-rpath,./libs -no-pie -loracle target.cc -o oracle.elf"
    print(oracle_elf)
    result = subprocess.run(oracle_elf, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    names = " ".join(item for item in lib_items)
    dyninst_elf = f"{compiler} {include} {headers} ./oracle/oracle_dyninst.cc -L./libs -Wl,-rpath,./libs -Wl,--start-group {names} -Wl,--end-group -o oracle_dyninst.elf"
    print(dyninst_elf)
    result = subprocess.run(dyninst_elf, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)

def main():
    print("Setting up untracer...")
    headers = " -I ".join(head for head in get_headers())
    if len(headers) > 0:
        headers = "-I " + headers
    create_input(10)
    input_file = create_arguments()
    create_libs(headers = headers)
    # setup_tracer(headers = headers)
    setup_oracle(headers = headers) 
    #create_archives(compiler, include, forkserver_name, archive_filename, object_filename) 
    # create_elf(compiler, target_name)
    # result = subprocess.run(["./oracle.elf", input_file])



if __name__ == "__main__":
    main()