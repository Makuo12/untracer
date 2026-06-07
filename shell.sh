
echo "Building xpdf..."
cd /home/makuo12/Documents/forte-research/untracer/xpdf-4.06_2
rm -rf build
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=gcc \
        -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_CXX_FLAGS="/home/makuo12/Documents/forte-research/untracer/libs/liboracle.so -fno-pie" \
        -DCMAKE_C_FLAGS="/home/makuo12/Documents/forte-research/untracer/libs/liboracle.so -fno-pie" \
        -DCMAKE_EXE_LINKER_FLAGS="-L/home/makuo12/Documents/forte-research/untracer/libs -loracle -no-pie" \
        ../
make pdftotext
cp pdftotext /home/makuo12/Documents/forte-research/untracer/target/pdftotext
cd ../..
