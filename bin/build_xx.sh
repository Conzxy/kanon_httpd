#!/bin/bash
cd ../build && \
  cmake .. -DGEN_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release -G Ninja && \
  cmake --build . --target http_common && \
  cmake --build . --target httpd_kanon && \
  cmake .. -DGEN_PLUGIN=ON
