
echo "Building xpdf..."
cd /Users/mac/Documents/forte_research/closure_rust/xpdf-4.06_2
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=gcc \
        -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_CXX_FLAGS="/home/makuo12/Documents/forte-research/untracer/libs/liboracle.so" \
        -DCMAKE_C_FLAGS="/home/makuo12/Documents/forte-research/untracer/libs/liboracle.so" \
        ../
make pdftotext
cd ../..
