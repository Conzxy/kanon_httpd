mkdir build && cd build && \
# Options:
# -DBUILD_ALL_TESTS=ON(default: OFF)
# -DBUILD_STATIC_LIBS=ON(default: OFF)
# -G Ninja(default: Unix Makefiles), ninja need install
# -DCMAKE_BUILD_TYPE=Debug/...(default: Release)
cmake .. && \
cmake --build . -j 2 --target http_common && \
cmake --build . -j 2 --target httpd_kanon && \
cmake -DGEN_PLUGIN=ON ..
# Then, you can check ./lib/ if has three *.so files
# , ../resources/contents/adder and ../bin/httpd_kanon
# if does exists, then run the ../bin/httpd_kanon
# (You should can the working directory to ../bin and 
#  modify the ../bin/kanon_httpd.conf)