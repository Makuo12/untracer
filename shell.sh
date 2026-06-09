
# echo "Building xpdf..."
# gcc -I ./include ./oracle/forkserver.c -c -o ./build/oracle_forkserver.o
# gcc -I ./include ./tracer/forkserver.c -c -o ./build/tracer_forkserver.o
# cd /home/makuo12/Documents/forte-research/untracer/xpdf-4.06_2
# rm -rf build
# mkdir -p build && cd build
# cmake -DCMAKE_BUILD_TYPE=Release \
#         -DCMAKE_C_COMPILER=gcc \
#         -DCMAKE_CXX_COMPILER=g++ \
#         -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
#         -DCMAKE_CXX_FLAGS="/home/makuo12/Documents/forte-research/untracer/libs/liboracle.so -fno-pie" \
#         -DCMAKE_C_FLAGS="/home/makuo12/Documents/forte-research/untracer/libs/liboracle.so -fno-pie" \
#         -DCMAKE_EXE_LINKER_FLAGS="-L/home/makuo12/Documents/forte-research/untracer/libs -loracle -no-pie " \
#         ../
# make pdftotext
# cp ./xpdf/pdttotext /home/makuo12/Documents/forte-research/untracer/pdftotext.trace
# cd ../..
# Unset the external one first to confirm it's the in-code setenv doing the work
unset DYNINSTAPI_RT_LIB
./build/oracle_dyninst.elf /home/makuo12/Documents/forte-research/untracer/target.elf
DYNINST_DEBUG_STARTUP=1 \
DYNINST_DEBUG_BOOTSTRAP=1 \
DYNINSTAPI_RT_LIB=/home/makuo12/Documents/forte-research/build/dyninstAPI_RT/libdyninstAPI_RT.so \
./build/oracle_dyninst.elf /home/makuo12/Documents/forte-research/untracer/target.elf 2>&1 | tee dyninst_debug.txt | head -80