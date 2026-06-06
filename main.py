import argparse
import subprocess
import random
import string
import os

IN_DIR = "input_dir"
OUT_DIR = "output_dir"

def create_input(size):
    test = "test"
    os.makedirs("test", exist_ok=True)
    os.makedirs("output", exist_ok=True)
    for i in range(size):
        filename = f"{test}_{i}"
        path = os.path.join(test, filename)
        with open(path, "w") as f:
            text = ''.join(random.choices(string.ascii_letters, k=40))
            f.write(text)

def create_archives(compiler, include, forkserver_name, archive_filename, object_filename):
    oracle_o = f"{compiler} {include} ./oracle/{forkserver_name} -c -o oracle_{object_filename}"
    result = subprocess.run(oracle_o, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    tracer_o = f"{compiler} {include} ./tracer/{forkserver_name} -c -o tracer_{object_filename}"
    result = subprocess.run(tracer_o, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    archive = "ar rcs"
    oracle_a = f"{archive} liboracle_{archive_filename} oracle_{object_filename}"
    result = subprocess.run(oracle_a, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    tracer_a = f"{archive} libtracer_{archive_filename} tracer_{object_filename}"
    result = subprocess.run(tracer_a, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    os.remove(f"oracle_{object_filename}")
    os.remove(f"tracer_{object_filename}")

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

def main():
    # create_input(10)
    input_file = create_arguments()
    compiler = "clang++"
    include = "-I ./include"
    forkserver_name = "forkserver.cc"
    object_filename = "forkserver.o"
    archive_filename = "forkserver.a"
    create_archives(compiler, include, forkserver_name, archive_filename, object_filename) 
    target_name = "target.cc"
    oracle_elf = f"{compiler} {target_name} -Wl,-force_load,liboracle_forkserver.a -o oracle.elf"
    result = subprocess.run(oracle_elf, shell=True, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        exit(1)
    result = subprocess.run(["./oracle.elf", input_file])

if __name__ == "__main__":
    main()