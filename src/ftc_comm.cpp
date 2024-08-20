#include "ftc_comm.h"
#include "ftc_data_mover_internal.h"

extern "C" {
#include "ftc_logging.h"
#include <fcntl.h>
#include <cassert>
#include <unistd.h>
}


#include <string>
#include <iostream>
#include <map>	

static hg_class_t *hg_class = NULL;
static hg_context_t *hg_context = NULL;
static int ftc_progress_thread_shutdown_flags = 0;
static int ftc_server_rank = -1;
static int server_rank = -1;

char server_addr_str[128]; 

/* Struct used to carry state of overall operation across callbacks */
struct ftc_rpc_state {
    hg_size_t size;
    void *buffer;
    hg_bulk_t bulk_handle;
    hg_handle_t handle;
    ftc_rpc_in_t in;
};

/* Extract IP address for node checking */
void extract_ip_portion(const char* full_address, char* ip_portion, size_t max_len) {
    const char* pos = strchr(full_address, ':');
    if (pos) {
        pos = strchr(pos + 1, ':');
    }
    if (pos) {
        size_t len = pos - full_address + 1;
        if (len < max_len) {
            strncpy(ip_portion, full_address, len);
            ip_portion[len] = '\0';
        } else {
            strncpy(ip_portion, full_address, max_len - 1);
            ip_portion[max_len - 1] = '\0';
        }
    } else {
        strncpy(ip_portion, full_address, max_len - 1);
        ip_portion[max_len - 1] = '\0';
    }
}


/* Logging initialization function */
void initialize_log(int rank, const char *type) {
    char log_filename[64];
	const char *logdir = getenv("FT_Cache_LOG_DIR");
    snprintf(log_filename, sizeof(log_filename), "%s/%s_node_%d.log", logdir, type, rank);

    FILE *log_file = fopen(log_filename, "w");
    if (log_file == NULL) {
        perror("Failed to create log file");
        exit(EXIT_FAILURE);
    }

    fprintf(log_file, "Log file for %s rank %d\n", type, rank);
    fclose(log_file);
}

/* Logging function */
void logging_info(log_info_t *info, const char *type) {
    FILE *log_file;
    char log_filename[64];
	const char *logdir = getenv("FT_Cache_LOG_DIR");
	snprintf(log_filename, sizeof(log_filename), "%s/%s_node_%d.log", logdir, type, info->server_rank);

    log_file = fopen(log_filename, "a");
    if (log_file == NULL) {
        perror("Failed to open log file");
        return;
    }

    fprintf(log_file, "[%s][%s][%d][%d][%d][%s][%d][%d][%ld.%06ld]\n",
            info->filepath,
            info->request,
            info->flag,
            info->client_rank,
            info->server_rank,
            info->expn,
			info->n_epoch,
			info->n_batch,
            (long)info->clocktime.tv_sec, (long)info->clocktime.tv_usec);

    fclose(log_file);
}



/* Initialize communication for both the client and server
	processes
	This is based on the rpc_engine template provided by the mercury lib */
void ftc_init_comm(hg_bool_t listen)
{
	const char *info_string = "ofi+tcp://";  
    pthread_t ftc_progress_tid;

    HG_Set_log_level("DEBUG");

    /* Initialize Mercury with the desired network abstraction class */
    hg_class = HG_Init(info_string, listen);
	if (hg_class == NULL){
		L4C_FATAL("Failed to initialize HG_CLASS Listen Mode : %d\n", listen);
	}

    /* Create HG context */
    hg_context = HG_Context_create(hg_class);
	if (hg_context == NULL){
		L4C_FATAL("Failed to initialize HG_CONTEXT\n");
	}

	/* Only for server processes */
	if (listen)
	{
		char *rank_str = getenv("PMIX_RANK");  
    	server_rank = atoi(rank_str);
		if (rank_str != NULL){
			ftc_server_rank = atoi(rank_str);
			const char *type = "server";
			initialize_log(ftc_server_rank, type);
		}else
		{
			L4C_FATAL("Failed to extract rank\n");
		}
	}

	L4C_INFO("Mecury initialized");
	//TODO The engine creates a pthread here to do the listening and progress work
	//I need to understand this better I don't want to create unecessary work for the client
	if (pthread_create(&ftc_progress_tid, NULL, ftc_progress_fn, NULL) != 0){
		L4C_FATAL("Failed to initialized mecury progress thread\n");
	}
}

void ftc_shutdown_comm()
{
    int ret = -1;

    ftc_progress_thread_shutdown_flags = true;

	if (hg_context == NULL)
		return;

//    ret = HG_Context_destroy(hg_context);
//    assert(ret == HG_SUCCESS);

//    ret = HG_Finalize(hg_class);
//    assert(ret == HG_SUCCESS);


}

void *ftc_progress_fn(void *args)
{
	hg_return_t ret;
	unsigned int actual_count = 0;

	while (!ftc_progress_thread_shutdown_flags){
		do{
			ret = HG_Trigger(hg_context, 0, 1, &actual_count);
		} while (
			(ret == HG_SUCCESS) && actual_count && !ftc_progress_thread_shutdown_flags);
		if (!ftc_progress_thread_shutdown_flags)
			HG_Progress(hg_context,100);
	}
	
	return NULL;
}

/* I think only servers need to post their addresses. */
/* There is an expectation that the server will be started in 
 * advance of the clients. Should the servers be started with an
 * argument regarding the number of servers? */
void ftc_comm_list_addr()
{
	char self_addr_string[PATH_MAX];
	char filename[PATH_MAX];
    hg_addr_t self_addr;
	FILE *na_config = NULL;
	hg_size_t self_addr_string_size = PATH_MAX;
//	char *stepid = getenv("PMIX_NAMESPACE");
//	char *jobid = getenv("SLURM_JOBID");
	char *jobid = getenv("MY_JOBID");

	sprintf(filename, "./.ports.cfg.%s", jobid);
	/* Get self addr to tell client about */
    HG_Addr_self(hg_class, &self_addr);
    HG_Addr_to_string(
        hg_class, self_addr_string, &self_addr_string_size, self_addr);
    HG_Addr_free(hg_class, self_addr);
    
	extract_ip_portion(self_addr_string, server_addr_str, sizeof(server_addr_str));

    /* Write addr to a file */
    na_config = fopen(filename, "a+");
    if (!na_config) {
        L4C_ERR("Could not open config file from: %s\n",
            filename);
        exit(0);
    }
    fprintf(na_config, "%d %s\n", ftc_server_rank, self_addr_string);
    fclose(na_config);
}


char *buffer_to_hex(const void *buf, size_t size) {
    const char *hex_digits = "0123456789ABCDEF";
    const unsigned char *buffer = (const unsigned char *)buf;

    char *hex_str = (char *)malloc(size * 2 + 1); // 2 hex chars per byte + null terminator
    if (!hex_str) {
        perror("malloc");
        return NULL;
    }
    for (size_t i = 0; i < size; ++i) {
        hex_str[i * 2] = hex_digits[(buffer[i] >> 4) & 0xF];
        hex_str[i * 2 + 1] = hex_digits[buffer[i] & 0xF];
    }
    hex_str[size * 2] = '\0'; // Null terminator
    return hex_str;
}

/* Callback triggered upon completion of bulk transfer */
static hg_return_t
ftc_rpc_handler_bulk_cb(const struct hg_cb_info *info)
{
    struct ftc_rpc_state *ftc_rpc_state_p = (struct ftc_rpc_state*)info->arg;
    int ret;
    ftc_rpc_out_t out;
    out.ret = ftc_rpc_state_p->size;

	if (info->ret != 0) {
        L4C_DEBUG("Callback info contains an error: %d\n", info->ret);
        /* Free resources and return the error */
        HG_Bulk_free(ftc_rpc_state_p->bulk_handle);
        HG_Destroy(ftc_rpc_state_p->handle);
        free(ftc_rpc_state_p->buffer);
        free(ftc_rpc_state_p);
        return (hg_return_t)info->ret;
    }

    ret = HG_Respond(ftc_rpc_state_p->handle, NULL, NULL, &out);
//    assert(ret == HG_SUCCESS);        

	if (ret != HG_SUCCESS) {
        L4C_DEBUG("Failed to send response: %d\n", ret);
        /* Free resources and return the error */
        HG_Bulk_free(ftc_rpc_state_p->bulk_handle);
        HG_Destroy(ftc_rpc_state_p->handle);
        free(ftc_rpc_state_p->buffer);
        free(ftc_rpc_state_p);
        return (hg_return_t)ret;
    }

    HG_Bulk_free(ftc_rpc_state_p->bulk_handle);
    HG_Destroy(ftc_rpc_state_p->handle);
    free(ftc_rpc_state_p->buffer);
    free(ftc_rpc_state_p);
    return (hg_return_t)0;
}


static hg_return_t
ftc_rpc_handler(hg_handle_t handle)
{
    int ret;
    struct ftc_rpc_state *ftc_rpc_state_p;
    const struct hg_info *hgi;
    ssize_t readbytes;
	log_info_t log_info;
	struct timeval tmp_time; 

    ftc_rpc_state_p = (struct ftc_rpc_state*)malloc(sizeof(*ftc_rpc_state_p));

    /* Decode input */
    ret = HG_Get_input(handle, &ftc_rpc_state_p->in);   
	if (ret != HG_SUCCESS) {
        L4C_DEBUG("HG_Get_input failed with error code %d\n", ret);
        free(ftc_rpc_state_p);
        return (hg_return_t)ret;
    }
    gettimeofday(&log_info.clocktime, NULL);
    
    /* This includes allocating a target buffer for bulk transfer */
    ftc_rpc_state_p->buffer = calloc(1, ftc_rpc_state_p->in.input_val);
    assert(ftc_rpc_state_p->buffer);

    ftc_rpc_state_p->size = ftc_rpc_state_p->in.input_val;
    ftc_rpc_state_p->handle = handle;

    /* Register local target buffer for bulk access */

    hgi = HG_Get_info(handle);
	if (!hgi) {
        L4C_DEBUG("HG_Get_info failed\n");
        return (hg_return_t)ret;
    }
    ret = HG_Bulk_create(hgi->hg_class, 1, &ftc_rpc_state_p->buffer,
        &ftc_rpc_state_p->size, HG_BULK_READ_ONLY,
        &ftc_rpc_state_p->bulk_handle);
    assert(ret == 0);

	/* Logging code */
	snprintf(log_info.filepath, sizeof(log_info.filepath), "fd_%d", ftc_rpc_state_p->in.localfd); 
    log_info.filepath[sizeof(log_info.filepath) - 1] = '\0';
    strncpy(log_info.request, "read", sizeof(log_info.request) - 1);
    log_info.request[sizeof(log_info.request) - 1] = '\0';

	char client_addr_str[128];
    size_t client_addr_str_size = sizeof(client_addr_str);
    ret = HG_Addr_to_string(ftc_comm_get_class(), client_addr_str, &client_addr_str_size, hgi->addr);
	char client_ip[128];
	extract_ip_portion(client_addr_str, client_ip, sizeof(client_ip));

	log_info.flag = (strcmp(server_addr_str, client_ip) == 0) ? 1 : 0;

    log_info.client_rank = ftc_rpc_state_p->in.client_rank;
    log_info.server_rank = server_rank;
    strncpy(log_info.expn, "SReceive", sizeof(log_info.expn) - 1);
    log_info.expn[sizeof(log_info.expn) - 1] = '\0';
    log_info.n_epoch = -1;
    log_info.n_batch = -1;
    logging_info(&log_info, "server");

	ftc_rpc_out_t out;

    if (ftc_rpc_state_p->in.offset == -1){
        readbytes = read(ftc_rpc_state_p->in.accessfd, ftc_rpc_state_p->buffer, ftc_rpc_state_p->size);
    }else
    {
		gettimeofday(&log_info.clocktime, NULL);
		strncpy(log_info.expn, "SSNVMeRequest", sizeof(log_info.expn) - 1);
    	log_info.expn[sizeof(log_info.expn) - 1] = '\0';
        readbytes = pread(ftc_rpc_state_p->in.accessfd, ftc_rpc_state_p->buffer, ftc_rpc_state_p->size, ftc_rpc_state_p->in.offset);
		gettimeofday(&tmp_time, NULL);	

		if (readbytes < 0) { 
			strncpy(log_info.expn, "Fail", sizeof(log_info.expn) - 1);
            log_info.expn[sizeof(log_info.expn) - 1] = '\0';
            logging_info(&log_info, "server");
                HG_Bulk_free(ftc_rpc_state_p->bulk_handle);
                free(ftc_rpc_state_p->buffer);
                L4C_DEBUG("server read failed -1\n");
                out.ret = -1;  // Indicate failure
                HG_Respond(handle, NULL, NULL, &out);
                free(ftc_rpc_state_p);
                return HG_SUCCESS;
    	}
	}
	
    /* Reduce size of transfer to what was actually read */
    // We may need to revisit this.
    ftc_rpc_state_p->size = readbytes;
    /* initiate bulk transfer from client to server */
    ret = HG_Bulk_transfer(hgi->context, ftc_rpc_handler_bulk_cb, ftc_rpc_state_p,
        HG_BULK_PUSH, hgi->addr, ftc_rpc_state_p->in.bulk_handle, 0,
        ftc_rpc_state_p->bulk_handle, 0, ftc_rpc_state_p->size, HG_OP_ID_IGNORE);
 
    assert(ret == 0);

    (void) ret;

    return (hg_return_t)ret;
}




static hg_return_t
ftc_open_rpc_handler(hg_handle_t handle)
{
    ftc_open_in_t in;
    ftc_open_out_t out;    
	const struct hg_info *hgi;
	int nvme_flag = 0;
	
    int ret = HG_Get_input(handle, &in);
    assert(ret == 0);
    string redir_path = in.path;

	/* For logging */
	hgi = HG_Get_info(handle);
    if (!hgi) {
        L4C_DEBUG("HG_Get_info failed\n");
        return (hg_return_t)ret;
    }
	log_info_t log_info;
    strncpy(log_info.filepath, in.path, sizeof(log_info.filepath) - 1);
    log_info.filepath[sizeof(log_info.filepath) - 1] = '\0';
    strncpy(log_info.request, "open", sizeof(log_info.request) - 1);
    log_info.request[sizeof(log_info.request) - 1] = '\0';

	char client_addr_str[128];
    size_t client_addr_str_size = sizeof(client_addr_str);
    ret = HG_Addr_to_string(ftc_comm_get_class(), client_addr_str, &client_addr_str_size, hgi->addr);
    char client_ip[128];
    extract_ip_portion(client_addr_str, client_ip, sizeof(client_ip));

    log_info.flag = (strcmp(server_addr_str, client_ip) == 0) ? 1 : 0;
	
    log_info.client_rank = in.client_rank;
    log_info.server_rank = server_rank;
    strncpy(log_info.expn, "SReceive", sizeof(log_info.expn) - 1);
    log_info.expn[sizeof(log_info.expn) - 1] = '\0';
    log_info.n_epoch = in.localfd;
    log_info.n_batch = -1;
    gettimeofday(&log_info.clocktime, NULL);
    logging_info(&log_info, "server");

	strncpy(log_info.expn, "SPFSRequest", sizeof(log_info.expn) - 1);
    log_info.expn[sizeof(log_info.expn) - 1] = '\0';
	
	pthread_mutex_lock(&path_map_mutex); 
    if (path_cache_map.find(redir_path) != path_cache_map.end())
    {
        redir_path = path_cache_map[redir_path];
		strncpy(log_info.expn, "SNVMeRequest", sizeof(log_info.expn) - 1);
        log_info.expn[sizeof(log_info.expn) - 1] = '\0';
        nvme_flag = 1;
    }
	pthread_mutex_unlock(&path_map_mutex); 	
	gettimeofday(&log_info.clocktime, NULL);
    logging_info(&log_info, "server");
    out.ret_status = open(redir_path.c_str(),O_RDONLY);  
	gettimeofday(&log_info.clocktime, NULL);
    if (nvme_flag) {
        strncpy(log_info.expn, "SNVMeReceive", sizeof(log_info.expn) - 1);
        log_info.expn[sizeof(log_info.expn) - 1] = '\0';
    } else {
        strncpy(log_info.expn, "SPFSReceive", sizeof(log_info.expn) - 1);
        log_info.expn[sizeof(log_info.expn) - 1] = '\0';
    }
    logging_info(&log_info, "server");

    fd_to_path[out.ret_status] = in.path;  
    HG_Respond(handle,NULL,NULL,&out);

    return (hg_return_t)ret;

}

static hg_return_t
ftc_close_rpc_handler(hg_handle_t handle)
{
    ftc_close_in_t in;
	const struct hg_info *hgi;
	int nvme_flag = 0;
	struct timeval tmp_time;
	log_info_t log_info;

    int ret = HG_Get_input(handle, &in);
    assert(ret == HG_SUCCESS);
	gettimeofday(&log_info.clocktime, NULL);
    ret = close(in.fd);
//    assert(ret == 0);
	

	/* Logging code */
	hgi = HG_Get_info(handle);
    if (!hgi) {
        L4C_DEBUG("HG_Get_info failed\n");
		fd_to_path.erase(in.fd);
       	return (hg_return_t)ret;
	}
	snprintf(log_info.filepath, sizeof(log_info.filepath), "fd_%d", in.fd);    
    strncpy(log_info.request, "close", sizeof(log_info.request) - 1);
    log_info.request[sizeof(log_info.request) - 1] = '\0';


	char client_addr_str[128];
    size_t client_addr_str_size = sizeof(client_addr_str);
    ret = HG_Addr_to_string(ftc_comm_get_class(), client_addr_str, &client_addr_str_size, hgi->addr);
    char client_ip[128];
    extract_ip_portion(client_addr_str, client_ip, sizeof(client_ip));

    log_info.flag = (strcmp(server_addr_str, client_ip) == 0) ? 1 : 0;

    log_info.client_rank = in.client_rank;
    log_info.server_rank = server_rank;
    strncpy(log_info.expn, "SReceive", sizeof(log_info.expn) - 1);
    log_info.expn[sizeof(log_info.expn) - 1] = '\0';
    log_info.n_epoch = -1;
    log_info.n_batch = -1;
    logging_info(&log_info, "server");

	gettimeofday(&log_info.clocktime, NULL);
	strncpy(log_info.expn, "SReceive", sizeof(log_info.expn) - 1);
    log_info.expn[sizeof(log_info.expn) - 1] = '\0';

    /* Signal to the data mover to copy the file */
	
	pthread_mutex_lock(&path_map_mutex); 
    if (path_cache_map.find(fd_to_path[in.fd]) == path_cache_map.end())
    {
		strncpy(log_info.expn, "SNVMeRequest", sizeof(log_info.expn) - 1);
    	log_info.expn[sizeof(log_info.expn) - 1] = '\0';
        pthread_mutex_lock(&data_mutex);
        data_queue.push(fd_to_path[in.fd]);
        pthread_cond_signal(&data_cond);
        pthread_mutex_unlock(&data_mutex);
		nvme_flag = 1;
    }
	pthread_mutex_unlock(&path_map_mutex); 
	if (nvme_flag) {
		logging_info(&log_info, "server");
		strncpy(log_info.expn, "SNVMeReceive", sizeof(log_info.expn) - 1);
        log_info.expn[sizeof(log_info.expn) - 1] = '\0';
	}		
	else{
		strncpy(log_info.expn, "SPFSRequest", sizeof(log_info.expn) - 1);
        log_info.expn[sizeof(log_info.expn) - 1] = '\0';
		logging_info(&log_info, "server");
        strncpy(log_info.expn, "SPFSReceive", sizeof(log_info.expn) - 1);
        log_info.expn[sizeof(log_info.expn) - 1] = '\0';

	}	
	log_info.clocktime = tmp_time;
    logging_info(&log_info, "server");	
	
	fd_to_path.erase(in.fd);
    return (hg_return_t)ret;
}

static hg_return_t
ftc_seek_rpc_handler(hg_handle_t handle)
{
    ftc_seek_in_t in;
    ftc_seek_out_t out;    
    int ret = HG_Get_input(handle, &in);
    assert(ret == 0);

    out.ret = lseek64(in.fd, in.offset, in.whence);

    HG_Respond(handle,NULL,NULL,&out);

    return (hg_return_t)ret;
}


/* Register this particular rpc type with Mercury */
hg_id_t
ftc_rpc_register(void)
{
    hg_id_t tmp;

    tmp = MERCURY_REGISTER(
        hg_class, "ftc_base_rpc", ftc_rpc_in_t, ftc_rpc_out_t, ftc_rpc_handler);

    return tmp;
}

hg_id_t
ftc_open_rpc_register(void)
{
    hg_id_t tmp;

    tmp = MERCURY_REGISTER(
        hg_class, "ftc_open_rpc", ftc_open_in_t, ftc_open_out_t, ftc_open_rpc_handler);

    return tmp;
}

hg_id_t
ftc_close_rpc_register(void)
{
    hg_id_t tmp;

    tmp = MERCURY_REGISTER(
        hg_class, "ftc_close_rpc", ftc_close_in_t, void, ftc_close_rpc_handler);
    

    int ret =  HG_Registered_disable_response(hg_class, tmp,
                                           HG_TRUE);                        
    assert(ret == HG_SUCCESS);

    return tmp;
}

/* Register this particular rpc type with Mercury */
hg_id_t
ftc_seek_rpc_register(void)
{
    hg_id_t tmp;

    tmp = MERCURY_REGISTER(
        hg_class, "ftc_seek_rpc", ftc_seek_in_t, ftc_seek_out_t, ftc_seek_rpc_handler);

    return tmp;
}

/* Create context even for client */
void
ftc_comm_create_handle(hg_addr_t addr, hg_id_t id, hg_handle_t *handle)
{    
    hg_return_t ret = HG_Create(hg_context, addr, id, handle);
    assert(ret==HG_SUCCESS);    
}

/* Free the addr */
void 
ftc_comm_free_addr(hg_addr_t addr)
{
    hg_return_t ret = HG_Addr_free(hg_class,addr);
    assert(ret==HG_SUCCESS);
}

hg_class_t *ftc_comm_get_class()
{
    return hg_class;
}

hg_context_t *ftc_comm_get_context()
{
    return hg_context;
}
