#ifndef __FT_Cache_FAULT_H__
#define __FT_Cache_FAULT_H__
extern "C" {
#include <mercury.h>
#include <mercury_bulk.h>
#include <mercury_macros.h>
#include <mercury_proc_string.h>
}

#include <unordered_map>
#include <vector>
#include <map>
#include <string>

struct Data {
    char file_path[256]; 
    void *value;
    ssize_t size;        
};

extern std::unordered_map<hg_addr_t, std::vector<Data>> data_storage;
extern std::map<std::string, std::string> path_cache_map;
extern hg_addr_t my_address;

void storeData(hg_addr_t node, const char* path, void *buffer, ssize_t size);
void writeToFile(hg_addr_t node);

#endif
