#! /bin/sh
mkdir -p Release
cd Release
cmake -DCMAKE_BUILD_TYPE=Release ..

make pomcpp_bin -j$(nproc)
(./pomcpp)
