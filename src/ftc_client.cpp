//Starting to use CPP functionality


#include <map>
#include <string>
#include <filesystem>
#include <iostream>
#include <assert.h>
#include <mutex>

#include "ftc_internal.h"
#include "ftc_logging.h"
#include "ftc_comm.h"

__thread bool tl_disable_redirect = false;
bool g_disable_redirect = true;
bool g_ftc_initialized = false;
bool g_ftc_comm_initialized = false;
bool g_mercury_init=false;

uint32_t g_ftc_server_count = 0;
char *ftc_data_dir = NULL;

pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;

std::map<int,std::string> fd_map;
std::map<int, int > fd_redir_map;
const int TIMEOUT_LIMIT = 3;

/* Devise a way to safely call this and initialize early */
static void __attribute__((constructor)) ftc_client_init()
{	
    pthread_mutex_lock(&init_mutex);
    if (g_ftc_initialized){
        pthread_mutex_unlock(&init_mutex);
        return;
    }
    ftc_init_logging();

    char * ftc_data_dir_c = getenv("FT_Cache_DATA_DIR");

    if (getenv("FT_Cache_SERVER_COUNT") != NULL)
    {
        g_ftc_server_count = atoi(getenv("FT_Cache_SERVER_COUNT"));
    }
    else
    {        
        L4C_FATAL("Please set enviroment variable FT_Cache_SERVER_COUNT\n");
		return;
    }


    if (ftc_data_dir_c != NULL)
    {
		ftc_data_dir = (char *)malloc(strlen(ftc_data_dir_c) + 1);
		snprintf(ftc_data_dir, strlen(ftc_data_dir_c) + 1, "%s", ftc_data_dir_c);
    }
    
	initialize_timeout_counters(g_ftc_server_count); 
    g_ftc_initialized = true;
	ftc_get_addr();
    pthread_mutex_unlock(&init_mutex);
    
	g_disable_redirect = false;
}

static void __attribute((destructor)) ftc_client_shutdown()
{
    ftc_shutdown_comm();
}

/* Initialization function for timeout counter */
void initialize_timeout_counters(int num_nodes) {
    timeout_counters.resize(num_nodes, 0);
}


bool ftc_track_file(const char *path, int flags, int fd)
{       
        if (strstr(path, ".ports.cfg.") != NULL)
        {
            return false;
        }
	/* Always back out of RDONLY */
	bool tracked = false;
	if ((flags & O_ACCMODE) == O_WRONLY) {
		return false;
	}

	if ((flags & O_APPEND)) {
		return false;
	}    

	try {
		std::string ppath = std::filesystem::canonical(path).parent_path();
		/* Check if current file exists in FT_Cache_DATA_DIR */
		if (ftc_data_dir != NULL){
			std::string test = std::filesystem::canonical(ftc_data_dir);
			
			if (ppath.find(test) != std::string::npos)
			{
				fd_map[fd] = std::filesystem::canonical(path);
				tracked = true;
			}		
		}else if (ppath == std::filesystem::current_path()) {       
			fd_map[fd] = std::filesystem::canonical(path);
			tracked = true;
		}
	} catch (...)
	{
		//Need to do something here
	}

    hg_bool_t done = HG_FALSE;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


	/* Send RPC to tell server to open file */
	if (tracked){
		if (!g_mercury_init){
			ftc_init_comm(false);	
			/* I think I only need to do this once */
			ftc_client_comm_register_rpc();
			g_mercury_init = true;
			initialize_timeout_counters(g_ftc_server_count);
			const char *type = "client"; 
			const char *rank_str = getenv("HOROVOD_RANK");
			int client_rank = atoi(rank_str);
			initialize_log(client_rank, type);
			ftc_get_addr();
		}
		ftc_open_state_t *ftc_open_state_p = (ftc_open_state_t *)malloc(sizeof(ftc_open_state_t));
        ftc_open_state_p->done = &done;
        ftc_open_state_p->cond = &cond;
        ftc_open_state_p->mutex = &mutex;	
		int host = std::hash<std::string>{}(fd_map[fd]) % g_ftc_server_count;	
		
		{
            std::lock_guard<std::mutex> lock(timeout_mutex);
			
            if (timeout_counters[host] >= TIMEOUT_LIMIT) {
                L4C_INFO("Host %d reached timeout limit, skipping", host);
                return false; // Skip further processing for this node
            }
        }

		ftc_client_comm_gen_open_rpc(host, fd_map[fd], fd, ftc_open_state_p);
		ftc_client_block(host, &done, &cond, &mutex);
	}


	return tracked;
}

/* Need to clean this up - in theory the RPC should time out if the request hasn't been serviced we'll go to the file-system?
 * Maybe not - we'll roll to another server.
 * For now we return true to keep the good path happy
 */
ssize_t ftc_remote_read(int fd, void *buf, size_t count)
{
	/* FT_Cache Code */
	/* Check the local fd - if it's tracked we pass it to the RPC function
	 * The local FD is converted to the remote FD with the buf and count
	 * We must know the remote FD to avoid collision on the remote side
	 */
	ssize_t bytes_read = -1;
	hg_bool_t done = HG_FALSE;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

	if (ftc_file_tracked(fd)){
		int host = std::hash<std::string>{}(fd_map[fd]) % g_ftc_server_count;	
		{
            std::lock_guard<std::mutex> lock(timeout_mutex);
            if (timeout_counters[host] >= TIMEOUT_LIMIT) {
                L4C_INFO("Host %d reached timeout limit, skipping", host);
                return bytes_read; // Skip further processing for this node
            }
        }	
	
        ftc_rpc_state_t_client *ftc_rpc_state_p = (ftc_rpc_state_t_client *)malloc(sizeof(ftc_rpc_state_t_client));
        ftc_rpc_state_p->bytes_read = &bytes_read;
        ftc_rpc_state_p->done = &done;
        ftc_rpc_state_p->cond = &cond;
        ftc_rpc_state_p->mutex = &mutex;

		ftc_client_comm_gen_read_rpc(host, fd, buf, count, -1, ftc_rpc_state_p);
		bytes_read = ftc_read_block(host, &done, &bytes_read, &cond, &mutex);		
	}
	/* Non-FT_Cache Reads come from base */
	return bytes_read;
}

/* Need to clean this up - in theory the RPC should time out if the request hasn't been serviced we'll go to the file-system?
 * Maybe not - we'll roll to another server.
 * For now we return true to keep the good path happy
 */
ssize_t ftc_remote_pread(int fd, void *buf, size_t count, off_t offset)
{
	/* FT_Cache Code */
	/* Check the local fd - if it's tracked we pass it to the RPC function
	 * The local FD is converted to the remote FD with the buf and count
	 * We must know the remote FD to avoid collision on the remote side
	 */
	ssize_t bytes_read = -1;
	hg_bool_t done = HG_FALSE;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

	if (ftc_file_tracked(fd)){
		int host = std::hash<std::string>{}(fd_map[fd]) % g_ftc_server_count;	
		{
            std::lock_guard<std::mutex> lock(timeout_mutex);
            if (timeout_counters[host] >= TIMEOUT_LIMIT) {
                L4C_INFO("Host %d reached timeout limit, skipping", host);
                return bytes_read; // Skip further processing for this node
            }
        }
        ftc_rpc_state_t_client *ftc_rpc_state_p = (ftc_rpc_state_t_client *)malloc(sizeof(ftc_rpc_state_t_client));
        ftc_rpc_state_p->bytes_read = &bytes_read;
        ftc_rpc_state_p->done = &done;
        ftc_rpc_state_p->cond = &cond;
        ftc_rpc_state_p->mutex = &mutex;

		ftc_client_comm_gen_read_rpc(host, fd, buf, count, offset, ftc_rpc_state_p);
		bytes_read = ftc_read_block(host, &done, &bytes_read, &cond, &mutex);   	
	}
	/* Non-FT_Cache Reads come from base */
	return bytes_read;
}

ssize_t ftc_remote_lseek(int fd, int offset, int whence)
{
		/* FT_Cache Code */
	/* Check the local fd - if it's tracked we pass it to the RPC function
	 * The local FD is converted to the remote FD with the buf and count
	 * We must know the remote FD to avoid collision on the remote side
	 */
	ssize_t bytes_read = -1;
	if (ftc_file_tracked(fd)){
		int host = std::hash<std::string>{}(fd_map[fd]) % g_ftc_server_count;	
		L4C_INFO("Remote seek - Host %d", host);		
		ftc_client_comm_gen_seek_rpc(host, fd, offset, whence);
		bytes_read = ftc_seek_block();   		
		return bytes_read;
	}
	/* Non-FT_Cache Reads come from base */
	return bytes_read;
}

void ftc_remote_close(int fd){
	if (ftc_file_tracked(fd)){
		int host = std::hash<std::string>{}(fd_map[fd]) % g_ftc_server_count;	
		{
            std::lock_guard<std::mutex> lock(timeout_mutex);
            if (timeout_counters[host] >= TIMEOUT_LIMIT) {
                L4C_INFO("Host %d reached timeout limit, skipping", host);
                return; // Skip further processing for this node
            }
        }
		ftc_rpc_state_t_close *rpc_state = (ftc_rpc_state_t_close *)malloc(sizeof(ftc_rpc_state_t_close));
    	rpc_state->done = false;
    	rpc_state->timeout = false;
		rpc_state->host = 0;
		ftc_client_comm_gen_close_rpc(host, fd, rpc_state);             	
	}
}

bool ftc_file_tracked(int fd)
{
	if (fd_map.empty()) { 
        return false;  
    }
	return (fd_map.find(fd) != fd_map.end());
}

const char * ftc_get_path(int fd)
{
	if (fd_map.empty()) { 
        return NULL;
    }
	
	if (fd_map.find(fd) != fd_map.end())
	{
		return fd_map[fd].c_str();
	}
	return NULL;
}

bool ftc_remove_fd(int fd)
{	
	if (fd_map.empty()){ 
		return false;
	}
	ftc_remote_close(fd);	
	return fd_map.erase(fd);
}
