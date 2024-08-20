# Fault Tolerant AI Cache (FT_Cache)

[![DOI](https://zenodo.org/badge/844795297.svg)](https://zenodo.org/doi/10.5281/zenodo.13347303)

## Overview

FT_Cache is a fault-tolerant distributed cache designed to optimize AI workloads by ensuring data availability and reliability in high-performance computing environments. The system is available in two versions:

- **Ver1**: Implements a Parallel File System (PFS) redirection approach for enhanced fault tolerance.
- **Ver2**: Utilizes an Elastic Recaching approach with a Ring Hash mechanism, improving scalability and cache efficiency.

## Prerequisites

Ensure you have the following dependencies installed:

- **GCC** version 9.1.0:
  ```bash
  wget http://ftp.gnu.org/gnu/gcc/gcc-9.1.0/gcc-9.1.0.tar.gz
  tar xzf gcc-9.1.0.tar.gz
  cd gcc-9.1.0
  ./contrib/download_prerequisites
  cd ..
  mkdir objdir gcc-install
  cd objdir
  ../gcc-9.1.0/configure --disable-multilib --prefix=$PARENT_DIR/gcc-install
  make -j$(nproc)
  make install
  ```

- **Mercury** version 2.0.0 or higher:
  ```bash
  git clone -b <version> http://github.com/mercury-hpc/mercury.git
  ```

- **Libfabric** version 1.15.2.0 or higher

- **CMake** version 3.20.4 or higher

- **Log4c** version 1.2.4:
  - Follow installation instructions at log4c.sourceforge.net.

## Building FT_Cache

### Step 1: Set Up Prerequisites

1. Install and configure the required packages:
   ```bash
   export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$YOUR_MERCURY_PATH/lib/pkg-config
   export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$YOUR_log4c_PATH/install/lib/pkgconfig
   ```

2. Load necessary modules:
   ```bash
   module reset
   module load mercury cmake libfabric
   module unload darshan-runtime
   ```

3. Set the C and C++ compilers:
   ```bash
   export CC=$YOUR_GCC_INSTALL_PATH/bin/gcc
   export CXX=$YOUR_GCC_INSTALL_PATH/bin/g++
   ```

4. Set the library paths:
   ```bash
   export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$YOUR_GCC_INSTALL_PATH/lib64
   export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$YOUR_log4c_PATH/install/lib
   ```

5. Configure FT_Cache-specific variables:
   ```bash
   export FT_Cache_SERVER_COUNT=1
   export FT_Cache_LOG_LEVEL=800
   export RDMAV_FORK_SAFE=1
   export VERBS_LOG_LEVEL=4
   export BBPATH=$YOUR_LOCAL_SSD_MNT_PATH
   ```

### Step 2: Build FT_Cache

1. Create and navigate to the build directory:
   ```bash
   mkdir build
   cd build
   ```

2. Run CMake and compile the project:
   ```bash
   cmake ../
   make -j4
   ```

## Testing and Running FT_Cache

### Step 1: Allocate Resources

Allocate K nodes using salloc:
```bash
salloc -A $ACCOUNT -J $USAGE -t $TIME -p batch -N $K -C nvme
```

### Step 2: Start the FT_Cache Server

Navigate to the server directory and start the server:
```bash
cd $FT_Cache_PATH/build/src
srun -n K -c K ./ftc_server 1 &
```
Ensure FT_Cache_SERVER_COUNT is set to 1.

### Step 3: Set Data Directory for FT_Cache

Set the FT_Cache_DATA_DIR:
```bash
export FT_Cache_DATA_DIR=$FT_Cache_PATH/build/src
```

### Step 4: Test FT_Cache

Run basic tests:
```bash
LD_PRELOAD=$FT_Cache_PATH/build/src/libftc_client.so srun -n K -c K ../tests/basic_test
```

### Step 5: Run Your Application

Run your application with FT_Cache:
```bash
LD_PRELOAD=$FT_Cache_PATH/build/src/libftc_client.so srun -n K -c K $PATH_TO_YOUR_APP
```

Note: If your cluster uses IBM Spectrum MPI, you can use:
```bash
export OMPI_LD_PRELOAD_PREPEND=$FT_Cache_PATH/build/src/libftc_client.so
```

## Troubleshooting

- **Synchronization Issues**: If you encounter errors in basic_test, comment out the cleanup_files() function in basic_test.c and manually delete the files.

- **FT_Cache Configuration**: Modify the info_string in ftc_init_comm() in the ftc_comm.cpp file to match your transport protocol:

  For verbs:
  ```cpp
  info_string = "ofi+verbs://";
  ```

  For TCP:
  ```cpp
  info_string = "ofi+tcp://";
  ```

## Timeout Configuration

To configure timeouts, adjust the following in the source files:

- In ftc_comm_client.cpp:
  ```cpp
  #define TIMEOUT_SECONDS 3
  ```

- In ftc_client.cpp:
  ```cpp
  const int TIMEOUT_LIMIT;
  ```
