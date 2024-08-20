#!/bin/bash

module reset
module load gcc/9.3.0 e4s/20.10
module load log4c
module load mercury/2.0.0
module load cmake
export CC=$OLCF_GCC_ROOT/bin/gcc
cmake ../FT_Cache/

make -j4

