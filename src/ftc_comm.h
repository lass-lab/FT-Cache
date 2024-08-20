#ifndef __FT_Cache_RPC_ENGINE_INTERNAL_H__
#define __FT_Cache_RPC_ENGINE_INTERNAL_H__
extern "C" {
#include <mercury.h>
#include <mercury_bulk.h>
#include <mercury_macros.h>
#include <mercury_proc_string.h>
}

#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

using namespace std;


/* Struct used to carry state of overall operation across callbacks */
struct ftc_rpc_state_t_client {
    uint32_t value;
    hg_size_t size;
    void *buffer;
    hg_bulk_t bulk_handle;
    hg_handle_t handle;
    int local_fd; 
    int offset; 
    ssize_t *bytes_read; 
    hg_bool_t *done; 
    pthread_cond_t *cond; 
    pthread_mutex_t *mutex; 
	char filepath[256]; 
	uint32_t svr_hash; 
};

/* Carry CB Information for CB */
struct ftc_open_state_t{
    uint32_t local_fd;
	char filepath[256];
	uint32_t svr_hash;
    hg_bool_t *done;
    pthread_cond_t *cond;
    pthread_mutex_t *mutex;
};

struct ftc_rpc_state_t_close {
    bool done;
    bool timeout;
	int local_fd;
    uint32_t host;
    hg_addr_t addr;
    std::condition_variable cond;
    std::mutex mutex;
    hg_handle_t handle;
};

/* For logging */
typedef struct {
    char filepath[256];
    char request[256];
    int flag;
    int client_rank;
    int server_rank;
    char expn[256];
	int n_epoch;
	int n_batch;
    struct timeval clocktime;
} log_info_t;

/* For detecting the failure */
extern std::vector<int> timeout_counters;
extern mutex timeout_mutex;
/* For logging */
extern hg_addr_t my_address;
extern int client_rank;

/* Visible API for example RPC operation */

/* RPC Open Handler */
MERCURY_GEN_PROC(ftc_open_out_t, ((int32_t)(ret_status)))
MERCURY_GEN_PROC(ftc_open_in_t, ((hg_string_t)(path))((int32_t)(client_rank))((int32_t)(localfd)))

/* BULK Read Handler */
MERCURY_GEN_PROC(ftc_rpc_out_t, ((int32_t)(ret)))
MERCURY_GEN_PROC(ftc_rpc_in_t, ((int32_t)(input_val))((hg_bulk_t)(bulk_handle))((int32_t)(accessfd))((int32_t)(localfd))((int64_t)(offset))((int32_t)(client_rank)))

/* RPC Seek Handler */
MERCURY_GEN_PROC(ftc_seek_out_t, ((int32_t)(ret)))
MERCURY_GEN_PROC(ftc_seek_in_t, ((int32_t)(fd))((int32_t)(offset))((int32_t)(whence)))


/* Close Handler input arg */
MERCURY_GEN_PROC(ftc_close_in_t, ((int32_t)(fd))((int32_t)(client_rank)))

/* General */
void ftc_init_comm(hg_bool_t listen);
void *ftc_progress_fn(void *args);
void ftc_comm_list_addr();
void ftc_comm_create_handle(hg_addr_t addr, hg_id_t id, hg_handle_t *handle);
void ftc_shutdown_comm();
void ftc_comm_free_addr(hg_addr_t addr);

/* Retrieve the static variables */
hg_class_t *ftc_comm_get_class();
hg_context_t *ftc_comm_get_context();

/* Client */
void ftc_client_comm_gen_seek_rpc(uint32_t svr_hash, int fd, int offset, int whence);
void ftc_client_comm_gen_read_rpc(uint32_t svr_hash, int localfd, void* buffer, ssize_t count, off_t offset, ftc_rpc_state_t_client *ftc_rpc_state_p);
void ftc_client_comm_gen_open_rpc(uint32_t svr_hash, string path, int fd, ftc_open_state_t *ftc_open_state_p);
void ftc_client_comm_gen_close_rpc(uint32_t svr_hash, int fd, ftc_rpc_state_t_close* rpc_state);
hg_addr_t ftc_client_comm_lookup_addr(int rank);
void ftc_client_comm_register_rpc();
void ftc_client_block(uint32_t host, hg_bool_t *done, pthread_cond_t *cond, pthread_mutex_t *mutex);
ssize_t ftc_read_block(uint32_t host, hg_bool_t *done, ssize_t *bytes_read, pthread_cond_t *cond, pthread_mutex_t *mutex);
ssize_t ftc_seek_block();

/* For FT */
void initialize_hash_ring(int serverCount, int vnodes);
void extract_ip_portion(const char* full_address, char* ip_portion, size_t max_len);
/* Function for debugging */
char *buffer_to_hex(const void *buf, size_t size);

/* Functions for logging */
void initialize_log(int rank, const char *type);
void logging_info(log_info_t *info, const char *type);
void ftc_get_addr();

/* Mercury common RPC registration */
hg_id_t ftc_rpc_register(void);
hg_id_t ftc_open_rpc_register(void);
hg_id_t ftc_close_rpc_register(void);
hg_id_t ftc_seek_rpc_register(void);
#endif

