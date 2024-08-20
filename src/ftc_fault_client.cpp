#include <iostream>
#include <vector>
#include <cstring>
#include <cstdio> 
#include <cassert>
#include <cstdlib>
#include <filesystem> 

#include "ftc_logging.h"
#include "ftc_fault.h"
#include "ftc_comm.h"

namespace fs = std::filesystem;
std::unordered_map<hg_addr_t, std::vector<Data>> data_storage;

void storeData(hg_addr_t node, const char* path, void *buffer, ssize_t size) {
    void *data_copy = malloc(size);
    if(data_copy == nullptr){
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    memcpy(data_copy, buffer, size);
    
    Data data;
    strncpy(data.file_path, path, sizeof(data.file_path) - 1);
    data.file_path[sizeof(data.file_path) - 1] = '\0'; // Ensure null terminated
    data.value = data_copy;
    data.size = size;
    
    data_storage[node].push_back(data);

}

void writeToFile(hg_addr_t node) {
    if (data_storage.find(node) == data_storage.end()) {
        fprintf(stderr, "Error: Node data not found.\n");
        return;
    }

    if (getenv("BBPATH") == NULL){
        L4C_ERR("Set BBPATH Prior to using FT_Cache");        
    }
    std::string nvmepath = std::string(getenv("BBPATH")) + "/XXXXXX"; 
    
    for (const auto& data : data_storage[node]) {
        char *newdir = (char *)malloc(strlen(nvmepath.c_str())+1);
        strcpy(newdir,nvmepath.c_str());
        mkdtemp(newdir);
        std::string dirpath = newdir;
        std::string filename = dirpath + "/" + fs::path(data.file_path).filename().string();

        FILE* file = fopen(filename.c_str(), "wb");
        if (!file) {
            fprintf(stderr, "Error opening file %s for writing.\n", filename.c_str());
            free(newdir);
            continue;
        }

        fwrite(data.value, 1, data.size, file);
        fclose(file);

        path_cache_map[data.file_path] = filename;

        free(newdir);
    }
    ftc_client_comm_gen_update_rpc(1, path_cache_map);
	L4C_INFO("after fault, memory files written\n");
}

void emptyStore() {
    data_storage.clear();
}

