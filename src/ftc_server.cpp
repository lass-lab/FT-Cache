#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ftc_comm.h"
#include "ftc_data_mover_internal.h"


#define FT_Cache_SERVER 1

extern "C" {
#include "ftc_logging.h"
}

__thread bool tl_disable_redirect = false;
uint32_t ftc_server_count = 0;

struct ftc_lookup_arg {
	hg_class_t *hg_class;
	hg_context_t *context;
	hg_id_t id;
	hg_addr_t addr;
};

int ftc_start_comm_server(void)
{
    HG_Set_log_level("DEBUG");

    /* Start the data mover before anything else */
    pthread_t ftc_data_mover_tid;
    if (pthread_create(&ftc_data_mover_tid, NULL, ftc_data_mover_fn, NULL) != 0){
		L4C_FATAL("Failed to initialized mecury progress thread\n");
	}

    /* True means we're a listener */
    ftc_init_comm(true);

    /* Post our address */
    ftc_comm_list_addr();

    /* Register basic RPC */
    ftc_rpc_register();
    ftc_open_rpc_register();
    ftc_close_rpc_register();
    ftc_seek_rpc_register();



    while (1)
        sleep(1);

    return EXIT_SUCCESS;
}



int main(int argc, char **argv)
{
    int l_error = 0;

    if (argc < 2)
    {
        fprintf(stderr, "Please supply server count\n");
        exit(-1);
    }

    ftc_server_count = atoi(argv[1]);

    ftc_init_logging();
    L4C_INFO("Server process starting up");
    ftc_start_comm_server();
    L4C_INFO("FT_Cache Server process shutting down");
    return (l_error);
}
