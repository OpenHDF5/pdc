#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>

#include <sys/shm.h>
#include <sys/mman.h>


#include "mpi.h"

#include "mercury.h"
#include "mercury_macros.h"

// Mercury hash table and list
#include "mercury_hash_table.h"
#include "mercury_list.h"


#include "query_utils.h"
#include "timer_utils.h"

#include "dart_server.h"
// Mercury multithread
#include "mercury_thread.h"
#include "mercury_thread_pool.h"
#include "mercury_thread_mutex.h"


int pdc_server_rank_g;
int pdc_server_size_g;

hg_class_t * hg_class_g;
hg_context_t * hg_context_g;



/*
 * Callback function of the server to lookup other servers via Mercury RPC
 *
 * \param  port[IN]        Port number for Mercury to use
 * \param  hg_class[IN]    Pointer to Mercury class
 * \param  hg_context[IN]  Pointer to Mercury context
 *
 * \return Non-negative on success/Negative on failure
 */
int PDC_Server_init(int port, hg_class_t **hg_class, hg_context_t **hg_context)
{
    int ret_value = SUCCEED;
    int i = 0;
    char **all_addr_strings = NULL;
    char self_addr_string[ADDR_MAX];
    char na_info_string[ADDR_MAX];
    char hostname[1024];

    // set server id start
    pdc_id_seq_g = pdc_id_seq_g * (pdc_server_rank_g+1);

    // Create server tmp dir
    pdc_mkdir(pdc_server_tmp_dir_g);

    all_addr_strings_1d_g = (char* )calloc(sizeof(char ), pdc_server_size_g * ADDR_MAX);
    all_addr_strings    = (char**)calloc(sizeof(char*), pdc_server_size_g );
    total_mem_usage_g += (sizeof(char) + sizeof(char*));

    memset(hostname, 0, 1024);
    gethostname(hostname, 1023);
    sprintf(na_info_string, "bmi+tcp://%s:%d", hostname, port);
    /* sprintf(na_info_string, "ofi+gni://%s:%d", hostname, port); */
    /* sprintf(na_info_string, "ofi+tcp://%s:%d", hostname, port); */
    /* sprintf(na_info_string, "cci+tcp://%s:%d", hostname, port); */
    if (pdc_server_rank_g == 0)
        printf("==PDC_SERVER[%d]: using %.7s\n", pdc_server_rank_g, na_info_string);

    // Init server
    *hg_class = HG_Init(na_info_string, NA_TRUE);
    if (*hg_class == NULL) {
        printf("Error with HG_Init()\n");
        return FAIL;
    }

    // Create HG context
    *hg_context = HG_Context_create(*hg_class);
    if (*hg_context == NULL) {
        printf("Error with HG_Context_create()\n");
        return FAIL;
    }

    // Get server address
    PDC_get_self_addr(*hg_class, self_addr_string);
    printf("Server address is: %s\n", self_addr_string);
    fflush(stdout);

    // Init server to server communication.
    pdc_remote_server_info_g = (pdc_remote_server_info_t*)calloc(sizeof(pdc_remote_server_info_t),
                                pdc_server_size_g);

    for (i = 0; i < pdc_server_size_g; i++) {
        ret_value = PDC_Server_remote_server_info_init(&pdc_remote_server_info_g[i]);
        if (ret_value != SUCCEED) {
            printf("==PDC_SERVER[%d]: error with PDC_Server_remote_server_info_init\n", pdc_server_rank_g);
            goto done;
        }
    }

    MPI_Allgather(self_addr_string, ADDR_MAX, MPI_CHAR, all_addr_strings_1d_g, ADDR_MAX, MPI_CHAR, MPI_COMM_WORLD);
    for (i = 0; i < pdc_server_size_g; i++) {
        all_addr_strings[i] = &all_addr_strings_1d_g[i*ADDR_MAX];
        pdc_remote_server_info_g[i].addr_string = &all_addr_strings_1d_g[i*ADDR_MAX];
        /* printf("==PDC_SERVER[%d]: %s\n", pdc_server_rank_g, all_addr_strings[i]); */
    }

    // Rank 0 write all addresses to one file
    if (pdc_server_rank_g == 0) {
        /* printf("========================\n"); */
        /* printf("Server address%s:\n", pdc_server_size_g ==1?"":"es"); */
        /* for (i = 0; i < pdc_server_size_g; i++) */
        /*     printf("%s\n", all_addr_strings[i]); */
        /* printf("========================\n"); */
        PDC_Server_write_addr_to_file(all_addr_strings, pdc_server_size_g);

        // Free
        /* free(all_addr_strings_1d_g); */
        free(all_addr_strings);
    }
    fflush(stdout);


    // We are starting a brand new server
    if (is_hash_table_init_g != 1) {
        // Hash table init
        ret_value = PDC_Server_init_hash_table();
        if (ret_value != SUCCEED) {
            printf("==PDC_SERVER[%d]: error with PDC_Server_init_hash_table\n", pdc_server_rank_g);
            goto done;
        }
    }

    // Data server related init
    pdc_data_server_read_list_head_g = NULL;
    pdc_data_server_write_list_head_g = NULL;

    // Initalize atomic variable to finalize server
    hg_atomic_set32(&close_server_g, 0);

    n_metadata_g = 0;

done:
    return ret_value;
} 


/*
 * Main function of PDC server
 *
 * \param  argc[IN]     Number of command line arguments
 * \param  argv[IN]     Command line arguments
 *
 * \return Non-negative on success/Negative on failure
 */
int main(int argc, char *argv[])
{
    int ret; 
    int port;
    char *tmp_dir;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &pdc_server_rank_g);
    MPI_Comm_size(MPI_COMM_WORLD, &pdc_server_size_g);

    port = pdc_server_rank_g % 32 + 7000 ;
    printf("rank=%d, port=%d\n", pdc_server_rank_g,port);

    // Set up tmp dir
    tmp_dir = getenv("PDC_TMPDIR");
    if (tmp_dir == NULL)
        tmp_dir = "./pdc_tmp";

    sprintf(pdc_server_tmp_dir_g, "%s/", tmp_dir);
    
    if (pdc_server_rank_g == 0) {
        printf("==PDC_SERVER[%d]: using [%s] as tmp dir \n", pdc_server_rank_g, pdc_server_tmp_dir_g);
    }

    ret = PDC_Server_init(port, &hg_class_g, &hg_context_g);
    if (ret != SUCCEED || hg_class_g == NULL || hg_context_g == NULL) {
        printf("Error with Mercury init, exit...\n");
        ret = FAIL;
        goto done;
    }

done:

    MPI_Finalize();
    return 0;
}