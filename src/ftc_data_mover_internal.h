#ifndef __FT_Cache_DATA_MOVER_INTERNAL_H__
#define __FT_Cache_DATA_MOVER_INTERNAL_H__

#include <queue>
#include <map>

using namespace std;
/*Data Mover */

extern pthread_cond_t data_cond;
extern pthread_mutex_t data_mutex;
extern pthread_mutex_t path_map_mutex;
extern queue<string> data_queue;
extern map<int, string> fd_to_path;
extern map<string, string> path_cache_map;


void *ftc_data_mover_fn(void *args);
#endif
