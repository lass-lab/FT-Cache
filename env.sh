
export FT_Cache_DATA_DIR=/scratch/s5104a21/ftctest/build/src
export PATH=/scratch/s5104a21/lib/mercury2/bin:$PATH
export LD_LIBRARY_PATH=/scratch/s5104a21/lib/mercury2/lib:$LD_LIBRARY_PATH
export PATH=/scratch/s5104a21/lib/log4c/bin:$PATH
export LD_LIBRARY_PATH=/scratch/s5104a21/lib/log4c/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/scratch/s5104a21/lib/mercury/lib/pkgconfig:/scratch/s5104a21/lib/log4c/lib/pkgconfig
export CC=/apps/compiler/gcc/10.2.0/bin/gcc
export CXX=/apps/compiler/gcc/10.2.0/bin/c++
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/apps/compiler/gcc/10.2.0/lib64

export LIBFABRIC_DIR=/scratch/s5104a21/lib/libfabric
export PATH=$LIBFABRIC_DIR/bin:$PATH
export LD_LIBRARY_PATH=$LIBFABRIC_DIR/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=/scratch/s5104a21/lib/libfabric/lib/pkgconfig:$PKG_CONFIG_PATH
export CPATH=/usr/mpi/gcc/openmpi-4.1.2a1/include:$CPATH
export C_INCLUDE_PATH=/usr/mpi/gcc/openmpi-4.1.2a1/include:$C_INCLUDE_PATH
export CPLUS_INCLUDE_PATH=/usr/mpi/gcc/openmpi-4.1.2a1/include:$CPLUS_INCLUDE_PATH

export PATH=/scratch/s5104a21/lib/boost/bin:$PATH
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/scratch/s5104a21/lib/boost/lib:$LD_LIBRARY_PATH


export FT_Cache_SERVER_COUNT=1
export FT_Cache_LOG_LEVEL=800
export RDMAV_FORK_SAFE=1
export VERBS_LOG_LEVEL=4
export BBPATH=/etmp 
export FT_Cache_DATA_DIR=/scratch/s5104a21/ftctest/build/src
