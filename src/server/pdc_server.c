/*
 * Copyright Notice for
 * Proactive Data Containers (PDC) Software Library and Utilities
 * -----------------------------------------------------------------------------

 *** Copyright Notice ***

 * Proactive Data Containers (PDC) Copyright (c) 2017, The Regents of the
 * University of California, through Lawrence Berkeley National Laboratory,
 * UChicago Argonne, LLC, operator of Argonne National Laboratory, and The HDF
 * Group (subject to receipt of any required approvals from the U.S. Dept. of
 * Energy).  All rights reserved.

 * If you have questions about your rights to use or distribute this software,
 * please contact Berkeley Lab's Innovation & Partnerships Office at  IPO@lbl.gov.

 * NOTICE.  This Software was developed under funding from the U.S. Department of
 * Energy and the U.S. Government consequently retains certain rights. As such, the
 * U.S. Government has been granted for itself and others acting on its behalf a
 * paid-up, nonexclusive, irrevocable, worldwide license in the Software to
 * reproduce, distribute copies to the public, prepare derivative works, and
 * perform publicly and display publicly, and to permit other to do so.
 */

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

#ifdef ENABLE_MPI
    #include "mpi.h"
#endif

#include "utlist.h"
#include "dablooms.h"

#include "mercury.h"
#include "mercury_macros.h"

// Mercury hash table and list
#include "mercury_hash_table.h"
#include "mercury_list.h"

#include "pdc_interface.h"
#include "pdc_client_server_common.h"
#include "pdc_server.h"

#include "query_utils.h"
#include "timer_utils.h"

#ifdef ENABLE_MULTITHREAD
// Mercury multithread
#include "mercury_thread.h"
#include "mercury_thread_pool.h"
#include "mercury_thread_mutex.h"

hg_thread_mutex_t pdc_client_addr_metex_g;
hg_thread_mutex_t pdc_metadata_hash_table_mutex_g;
/* hg_thread_mutex_t pdc_metadata_name_mark_hash_table_mutex_g; */
hg_thread_mutex_t pdc_time_mutex_g;
hg_thread_mutex_t pdc_bloom_time_mutex_g;
hg_thread_mutex_t n_metadata_mutex_g;
hg_thread_mutex_t data_read_list_mutex_g;
hg_thread_mutex_t data_write_list_mutex_g;
#endif

#define BLOOM_TYPE_T counting_bloom_t
#define BLOOM_NEW    new_counting_bloom
#define BLOOM_CHECK  counting_bloom_check
#define BLOOM_ADD    counting_bloom_add
#define BLOOM_REMOVE counting_bloom_remove
#define BLOOM_FREE   free_counting_bloom

// Global debug variable to control debug printfs
int is_debug_g = 0;
int pdc_client_num_g = 0;

hg_class_t   *hg_class_g   = NULL;
hg_context_t *hg_context_g = NULL;

int           work_todo_g = 0;

/* hg_context_t *pdc_client_context_g = NULL; */
int           pdc_to_client_work_todo_g = 0;

pdc_client_info_t        *pdc_client_info_g        = NULL;
pdc_remote_server_info_t *pdc_remote_server_info_g = NULL;
char                     *all_addr_strings_1d_g    = NULL;
int                       is_all_client_connected  = 0;

static hg_id_t    server_lookup_client_register_id_g;
static hg_id_t    server_lookup_remote_server_register_id_g;
static hg_id_t    notify_io_complete_register_id_g;
static hg_id_t    update_region_loc_register_id_g;
static hg_id_t    notify_region_update_register_id_g;
static hg_id_t    get_metadata_by_id_register_id_g;
static hg_id_t    get_storage_info_register_id_g;

// Global thread pool
hg_thread_pool_t *hg_test_thread_pool_g = NULL;

// Global hash table for storing metadata
hg_hash_table_t *metadata_hash_table_g = NULL;
/* hg_hash_table_t *metadata_name_mark_hash_table_g = NULL; */

int is_hash_table_init_g = 0;
int is_restart_g = 0;

int pdc_server_rank_g = 0;
int pdc_server_size_g = 1;

char pdc_server_tmp_dir_g[ADDR_MAX];

// Debug statistics var
int n_bloom_total_g;
int n_bloom_maybe_g;
double server_bloom_check_time_g  = 0.0;
double server_bloom_insert_time_g = 0.0;
double server_insert_time_g       = 0.0;
double server_delete_time_g       = 0.0;
double server_update_time_g       = 0.0;
double server_hash_insert_time_g  = 0.0;
double server_bloom_init_time_g   = 0.0;

double total_mem_usage_g          = 0.0;

uint32_t n_metadata_g             = 0;


// Data server related
pdc_data_server_io_list_t *pdc_data_server_read_list_head_g = NULL;
pdc_data_server_io_list_t *pdc_data_server_write_list_head_g = NULL;


/*
 * Check if two hash keys are equal
 *
 * \param vlocation1 [IN]       Hash table key
 * \param vlocation2 [IN]       Hash table key
 *
 * \return 1 if two keys are equal, 0 otherwise
 */
static int
PDC_Server_metadata_int_equal(hg_hash_table_key_t vlocation1, hg_hash_table_key_t vlocation2)
{
    return *((uint32_t *) vlocation1) == *((uint32_t *) vlocation2);
}

/*
 * Get hash key's location in hash table
 *
 * \param vlocation [IN]        Hash table key
 *
 * \return the location of hash key in the table
 */
static unsigned int
PDC_Server_metadata_int_hash(hg_hash_table_key_t vlocation)
{
    return *((uint32_t *) vlocation);
}

/*
 * Free the hash key
 *
 * \param  key [IN]        Hash table key
 *
 * \return void
 */
static void
PDC_Server_metadata_int_hash_key_free(hg_hash_table_key_t key)
{
    free((uint32_t *) key);
}

/* static void */
/* PDC_Server_metadata_name_mark_hash_value_free(hg_hash_table_value_t value) */
/* { */
/*     pdc_metadata_name_mark_t *elt, *tmp, *head; */

/*     head = (pdc_metadata_name_mark_t *) value; */

/*     // Free metadata list */
/*     DL_FOREACH_SAFE(head,elt,tmp) { */
/*       /1* DL_DELETE(head,elt); *1/ */
/*       free(elt); */
/*     } */
/* } */


/*
 * Free metadata hash value
 *
 * \param  value [IN]        Hash table value
 *
 * \return void
 */
static void
PDC_Server_metadata_hash_value_free(hg_hash_table_value_t value)
{
    pdc_metadata_t *elt, *tmp;
    pdc_hash_table_entry_head *head;

    FUNC_ENTER(NULL);

    head = (pdc_hash_table_entry_head*) value;

    // Free bloom filter
    if (head->bloom != NULL) {
        BLOOM_FREE(head->bloom);
    }

    // Free metadata list
    if (is_restart_g == 0) {
        /* printf("freeing individual head->metadata\n"); */
        /* fflush(stdout); */
        DL_FOREACH_SAFE(head->metadata,elt,tmp) {
          /* DL_DELETE(head,elt); */
          free(elt);
        }
    }
    /* else { */
        /* printf("freeing head->metadata\n"); */
        /* fflush(stdout); */
        /* if (head->metadata != NULL) { */
        /*     free(head->metadata); */
        /* } */
    /* } */
}

/*
 * Init the remote server info structure
 *
 * \param  info [IN]        PDC remote server info
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_remote_server_info_init(pdc_remote_server_info_t *info)
{
    perr_t ret_value = SUCCEED;

    FUNC_ENTER(NULL);

    if (info == NULL) {
        ret_value = FAIL;
        printf("==PDC_SERVER: NULL info, unable to init pdc_remote_server_info_t!\n");
        goto done;
    }

    info->addr_string = NULL;
    info->addr_valid  = 0;
    info->addr        = 0;
    info->server_lookup_remote_server_handle_valid = 0;
    info->update_region_loc_handle_valid           = 0;
    info->get_metadata_by_id_handle_valid          = 0;
    info->get_storage_info_handle_valid            = 0;

done:
    FUNC_LEAVE(ret_value);
}

/*
 * Init the PDC metadata structure
 *
 * \param  a [IN]        PDC metadata structure
 *
 * \return void
 */
void PDC_Server_metadata_init(pdc_metadata_t* a)
{
    int i;

    FUNC_ENTER(NULL);

    a->user_id              = 0;
    a->time_step            = 0;
    a->app_name[0]          = 0;
    a->obj_name[0]          = 0;

    a->obj_id               = 0;
    a->ndim                 = 0;
    for (i = 0; i < DIM_MAX; i++)
        a->dims[i] = 0;

    a->create_time          = 0;
    a->last_modified_time   = 0;
    a->tags[0]              = 0;
    a->data_location[0]     = 0;

    a->region_lock_head     = NULL;
    a->region_map_head      = NULL;
    a->prev                 = NULL;
    a->next                 = NULL;
}
// ^ hash table

/*
 * Concatenate the metadata's obj_name and time_step to one char array
 *
 * \param  metadata [IN]        PDC metadata structure pointer
 * \param  output   [OUT]       Concatenated char array pointer
 *
 * \return void
 */
static inline void combine_obj_info_to_string(pdc_metadata_t *metadata, char *output)
{
    /* sprintf(output, "%d%s%s%d", metadata->user_id, metadata->app_name, metadata->obj_name, metadata->time_step); */
    FUNC_ENTER(NULL);
    sprintf(output, "%s%d", metadata->obj_name, metadata->time_step);
}

/* static int find_identical_namemark(pdc_metadata_name_mark_t *mlist, pdc_metadata_name_mark_t *a) */
/* { */
/*     FUNC_ENTER(NULL); */

/*     perr_t ret_value; */

/*     pdc_metadata_name_mark_t *elt; */
/*     DL_FOREACH(mlist, elt) { */
/*         if (strcmp(elt->obj_name, a->obj_name) == 0) { */
/*             /1* printf("Identical namemark with name [%s] already exist in current Metadata store!\n", ); *1/ */
/*             ret_value = 1; */
/*             goto done; */
/*         } */
/*     } */

/*     ret_value = 0; */

/* done: */
/*     FUNC_LEAVE(ret_value); */
/* } */

/*
 * Trigger and progress the Mercury queue, execute callback function
 *
 * \param  hg_context[IN]        Mercury context pointer
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_check_client_response(hg_context_t **hg_context)
{
    perr_t ret_value;
    hg_return_t hg_ret;
    unsigned int actual_count;

    FUNC_ENTER(NULL);

    do {
        actual_count = 0;
        do {
            /* hg_ret = HG_Trigger(*hg_context, 100/1* timeout *1/, 1/1* max count *1/, &actual_count); */
            hg_ret = HG_Trigger(*hg_context, 0/* timeout */, 1/* max count */, &actual_count);
        } while ((hg_ret == HG_SUCCESS) && actual_count && pdc_to_client_work_todo_g);

        if (pdc_to_client_work_todo_g == 0)  break;

        /* printf("========Before Progress\n"); */
        hg_ret = HG_Progress(*hg_context, HG_MAX_IDLE_TIME);
        /* hg_ret = HG_Progress(*hg_context, 1000); */
        /* printf(" "); */
        /* printf("========After  Progress\n"); */
    } while (hg_ret == HG_SUCCESS);

    ret_value = SUCCEED;

    FUNC_LEAVE(ret_value);
}

/*
 * Trigger and execute callback function only
 *
 * \param  hg_context[IN]        Mercury context pointer
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_trigger(hg_context_t **hg_context)
{
    perr_t ret_value;
    hg_return_t hg_ret;
    unsigned int actual_count;

    FUNC_ENTER(NULL);

    do {
        actual_count = 0;
        do {
            hg_ret = HG_Trigger(*hg_context, 0/* timeout */, 1/* max count */, &actual_count);
        } while ((hg_ret == HG_SUCCESS) && actual_count && work_todo_g);

        if (work_todo_g <= 0)  break;

    } while (hg_ret == HG_SUCCESS);

    ret_value = SUCCEED;

    FUNC_LEAVE(ret_value);
}
/*
 * Trigger and progress the Mercury queue, execute callback function
 *
 * \param  hg_context[IN]        Mercury context pointer
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_check_response(hg_context_t **hg_context)
{
    perr_t ret_value;
    hg_return_t hg_ret;
    unsigned int actual_count;

    FUNC_ENTER(NULL);

    do {
        actual_count = 0;
        do {
            hg_ret = HG_Trigger(*hg_context, 0/* timeout */, 1/* max count */, &actual_count);
        } while ((hg_ret == HG_SUCCESS) && actual_count && work_todo_g);

        /* printf("actual_count=%d\n",actual_count); */
        /* Do not try to make progress anymore if we're done */
        if (work_todo_g <= 0)  break;

        hg_ret = HG_Progress(*hg_context, 1000);
        /* hg_ret = HG_Progress(*hg_context, HG_MAX_IDLE_TIME); */
        /* printf("==PDC_SERVER: PDC_Server_check_response after  HG_Progress \n"); */
        /* fflush(stdout); */
    } while (hg_ret == HG_SUCCESS);

    ret_value = SUCCEED;

    FUNC_LEAVE(ret_value);
}

/*
 * Destroy the client info structures, free the allocated space
 *
 * \param  info[IN]        Pointer to the client info structures
 *
 * \return Non-negative on success/Negative on failure
 */
static perr_t PDC_Server_destroy_client_info(pdc_client_info_t *info)
{
    int i;
    perr_t ret_value = SUCCEED;
    hg_return_t hg_ret;

    FUNC_ENTER(NULL);

    if (is_debug_g == 1) {
        printf("==PDC_SERVER[%d]: Destryoing %d client info\n", pdc_server_rank_g, pdc_client_num_g);
    }

    // Destroy addr and handle
    for (i = 0; i < pdc_client_num_g ; i++) {
        /* if (info[i].server_lookup_client_handle_valid == 1) { */

        /*     hg_ret = HG_Destroy(info[i].server_lookup_client_handle); */
        /*     if (hg_ret != HG_SUCCESS) { */
        /*         printf("==PDC_SERVER: PDC_Server_destroy_client_info() error with HG_Destroy\n"); */
        /*         ret_value = FAIL; */
        /*         goto done; */
        /*     } */
        /*     info[i].server_lookup_client_handle_valid = 0; */
        /* } */

        /* if (info[i].notify_io_complete_handle_valid == 1) { */
        /*     hg_ret = HG_Destroy(info[i].notify_io_complete_handle); */
        /*     if (hg_ret != HG_SUCCESS) { */
        /*         printf("==PDC_SERVER: PDC_Server_destroy_client_info() error with HG_Destroy\n"); */
        /*         ret_value = FAIL; */
        /*         goto done; */
        /*     } */
        /*     info[i].notify_io_complete_handle_valid = 0; */
        /* } */

        if (info[i].notify_region_update_handle_valid == 1) {
            hg_ret = HG_Destroy(info[i].notify_region_update_handle);
            if (hg_ret != HG_SUCCESS) {
                printf("==PDC_SERVER: PDC_Server_destroy_client_info() error with HG_Destroy\n");
                ret_value = FAIL;
                goto done;
            }
            info[i].notify_region_update_handle_valid = 0;
        }
        if (info[i].addr_valid == 1) {
            hg_ret = HG_Addr_free(hg_class_g, info[i].addr);
            if (hg_ret != HG_SUCCESS) {
                printf("==PDC_SERVER: PDC_Server_destroy_client_info() error with HG_Addr_free\n");
                ret_value = FAIL;
                goto done;
            }
            info[i].addr_valid = 0;
        }


    } // end of for

    free(info);
done:
    FUNC_LEAVE(ret_value);
} // PDC_Server_init


/*
 * Callback function of the server to lookup clients, also gets the confirm message from client.
 *
 * \param  callback_info[IN]        Mercury callback info pointer
 *
 * \return Non-negative on success/Negative on failure
 */
static hg_return_t
server_lookup_client_rpc_cb(const struct hg_cb_info *callback_info)
{
    hg_return_t ret_value = HG_SUCCESS;
    server_lookup_client_out_t output;
    server_lookup_args_t *server_lookup_args = (server_lookup_args_t*) callback_info->arg;
    hg_handle_t handle = callback_info->info.forward.handle;

    FUNC_ENTER(NULL);

    /* Get output from client */
    ret_value = HG_Get_output(handle, &output);
    if (ret_value != HG_SUCCESS) {
        printf("==PDC_SERVER[%d]: PDC_Server_get_storage_info_cb: error HG_Get_output\n", pdc_server_rank_g);
        server_lookup_args->ret_int = -1;
        goto done;
    }
    if (is_debug_g == 1) {
        printf("==PDC_SERVER[%d]: server_lookup_client_rpc_cb,  got output from client=%d\n",
                pdc_server_rank_g, output.ret);
    }
    server_lookup_args->ret_int = output.ret;

done:
    pdc_to_client_work_todo_g = 0;
    HG_Free_output(handle, &output);
    FUNC_LEAVE(ret_value);
}

/*
 * Callback function for HG_Addr_lookup(), creates a Mercury handle then forward the RPC message
 * to the client
 *
 * \param  callback_info[IN]        Mercury callback info pointer
 *
 * \return Non-negative on success/Negative on failure
 */
static hg_return_t
PDC_Server_lookup_client_cb(const struct hg_cb_info *callback_info)
{
    hg_return_t ret_value = HG_SUCCESS;
    uint32_t client_id;
    server_lookup_args_t *server_lookup_args;
    server_lookup_client_in_t in;
    hg_handle_t server_lookup_client_handle;

    FUNC_ENTER(NULL);

    server_lookup_args = (server_lookup_args_t*) callback_info->arg;
    client_id = server_lookup_args->client_id;

    pdc_client_info_g[client_id].addr = callback_info->info.lookup.addr;
    pdc_client_info_g[client_id].addr_valid = 1;

    // Create HG handle if needed
    /* if (pdc_client_info_g[client_id].server_lookup_client_handle_valid!= 1) { */
        /* HG_Create(pdc_client_context_g, pdc_client_info_g[client_id].addr, server_lookup_client_register_id_g, */
        /*           &server_lookup_client_handle); */
        HG_Create(hg_context_g, pdc_client_info_g[client_id].addr, server_lookup_client_register_id_g,
                  &server_lookup_client_handle);
        /* pdc_client_info_g[client_id].server_lookup_client_handle_valid= 1; */
    /* } */

    // Fill input structure
    in.server_id   = server_lookup_args->server_id;
    in.server_addr = server_lookup_args->server_addr;
    in.nserver     = pdc_server_rank_g;

    ret_value = HG_Forward(server_lookup_client_handle, server_lookup_client_rpc_cb, server_lookup_args, &in);
    if (ret_value != HG_SUCCESS) {
        fprintf(stderr, "server_lookup_client__cb(): Could not start HG_Forward()\n");
        return EXIT_FAILURE;
    }

    if (is_debug_g == 1) {
        printf("==PDC_SERVER[%d]: PDC_Server_lookup_client_cb() - forwarded to client %d\n",
                pdc_server_rank_g, client_id);
        fflush(stdout);
    }

    ret_value = HG_Destroy(server_lookup_client_handle);
    if (ret_value != HG_SUCCESS) {
        printf("==PDC_SERVER[%d]: PDC_Server_lookup_client_cb() - "
                "HG_Destroy(server_lookup_client_handle) error!\n", pdc_server_rank_g);
        fflush(stdout);
    }

    FUNC_LEAVE(ret_value);
}

/*
 * Lookup the available clients to obtain proper address of them for future communication
 * via Mercury.
 *
 * \param  client_id[IN]        Client's MPI rank
 *
 * \return Non-negative on success/Negative on failure
 */
static perr_t PDC_Server_lookup_client(uint32_t client_id)
{
    perr_t ret_value = SUCCEED;
    hg_return_t hg_ret;

    FUNC_ENTER(NULL);

    if (pdc_client_num_g <= 0) {
        printf("==PDC_SERVER: PDC_Server_lookup_client() - number of client <= 0!\n");
        ret_value = FAIL;
        goto done;
    }

    if (pdc_client_info_g[client_id].addr_valid == 1)
        goto done;

    // Lookup and fill the client info
    server_lookup_args_t lookup_args;
    char *target_addr_string;

    lookup_args.server_id = pdc_server_rank_g;
    lookup_args.client_id = client_id;
    lookup_args.server_addr = pdc_client_info_g[client_id].addr_string;
    target_addr_string = pdc_client_info_g[client_id].addr_string;

    if (is_debug_g == 1) {
        printf("==PDC_SERVER[%d]: Testing connection to client %d: %s\n",
                pdc_server_rank_g, client_id, target_addr_string);
        fflush(stdout);
    }
    /* hg_ret = HG_Addr_lookup(pdc_client_context_g, PDC_Server_lookup_client_cb, */
    /*                         &lookup_args, target_addr_string, HG_OP_ID_IGNORE); */
    hg_ret = HG_Addr_lookup(hg_context_g, PDC_Server_lookup_client_cb,
                            &lookup_args, target_addr_string, HG_OP_ID_IGNORE);
    if (hg_ret != HG_SUCCESS ) {
        printf("==PDC_SERVER: Connection to client %d FAILED!\n", client_id);
        ret_value = FAIL;
        goto done;
    }

    if (is_debug_g == 1) {
        printf("==PDC_SERVER[%d]: waiting for client %d\n", pdc_server_rank_g, client_id);
        fflush(stdout);
    }

    // Wait for response from client
    work_todo_g = 1;
    PDC_Server_check_response(&hg_context_g);
    /* PDC_Server_trigger(&hg_context_g); */
    /* pdc_to_client_work_todo_g = 1; */
    /* PDC_Server_check_client_response(&pdc_client_context_g); */

    if (is_debug_g) {
        printf("==PDC_SERVER[%d]: Received response from client %d\n", pdc_server_rank_g, client_id);
        fflush(stdout);
    }

    /* if (pdc_server_rank_g == 0) { */
    /*     printf("==PDC_SERVER[%d]: Finished connection test to all clients\n", pdc_server_rank_g); */
    /* } */

done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
}

/*
 * Init the client info structure
 *
 * \param  a[IN]        PDC client info structure pointer
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_client_info_init(pdc_client_info_t* a)
{
    perr_t ret_value = SUCCEED;

    FUNC_ENTER(NULL);
    if (a == NULL) {
        printf("==PDC_SERVER: PDC_client_info_init() NULL input!\n");
        ret_value = FAIL;
        goto done;
    }
    else if (pdc_client_num_g != 0) {
        printf("==PDC_SERVER: PDC_client_info_init() - pdc_client_num_g is not 0!\n");
        ret_value = FAIL;
        goto done;
    }

    memset(a->addr_string, 0, ADDR_MAX);
    a->addr_valid = 0;
    a->server_lookup_client_handle_valid = 0;
    a->notify_io_complete_handle_valid   = 0;
    a->notify_region_update_handle_valid = 0;
done:
    FUNC_LEAVE(ret_value);
}

/*
 * Callback function, allocates the client info structure with the first connectiong from all clients,
 * copies the client's address, and when received all clients' test connection message, start the lookup
 * process to test connection to all clients.
 *
 * \param  callback_info[IN]        Mercury callback info pointer
 *
 * \return Non-negative on success/Negative on failure
 */
hg_return_t PDC_Server_get_client_addr(const struct hg_cb_info *callback_info)
{
    int i;
    hg_return_t ret_value = HG_SUCCESS;

    FUNC_ENTER(NULL);

    client_test_connect_args *in= (client_test_connect_args*) callback_info->arg;
#ifdef ENABLE_MULTITHREAD
        hg_thread_mutex_lock(&pdc_client_addr_metex_g);
#endif


    if (is_all_client_connected  == 1) {
        if (is_debug_g == 1) {
            printf("==PDC_SERVER[%d]: new application run detected\n", pdc_server_rank_g);
            fflush(stdout);
        }

        PDC_Server_destroy_client_info(pdc_client_info_g);
        pdc_client_info_g = NULL;
        is_all_client_connected = 0;
    }


    if (pdc_client_info_g == NULL) {
        pdc_client_info_g = (pdc_client_info_t*)calloc(sizeof(pdc_client_info_t), in->nclient+1);
        if (pdc_client_info_g == NULL) {
            printf("==PDC_SERVER: PDC_Server_get_client_addr - unable to allocate space\n");
            ret_value = FAIL;
            goto done;
        }
        pdc_client_num_g = 0;
        for (i = 0; i < in->nclient + 1; i++)
            PDC_client_info_init(&pdc_client_info_g[i]);
        if (is_debug_g == 1) {
            printf("==PDC_SERVER[%d]: finished init %d client info\n", pdc_server_rank_g, in->nclient);
            fflush(stdout);
        }

    }

    pdc_client_num_g++;

    strcpy(pdc_client_info_g[in->client_id].addr_string, in->client_addr);

    if (is_debug_g) {
        printf("==PDC_SERVER: got client addr: %s from client[%d], total: %d\n",
                pdc_client_info_g[in->client_id].addr_string, in->client_id, pdc_client_num_g);
        fflush(stdout);
    }

    if (pdc_client_num_g >= in->nclient) {
        printf("==PDC_SERVER[%d]: got the last connection request from client[%d]\n",
                pdc_server_rank_g, in->client_id);
        fflush(stdout);
        for (i = 0; i < in->nclient; i++)
            ret_value = PDC_Server_lookup_client(i);

        is_all_client_connected = 1;

        if (is_debug_g) {
            printf("==PDC_SERVER[%d]: PDC_Server_get_client_addr - Finished PDC_Server_lookup_client()\n",
                    pdc_server_rank_g);
            fflush(stdout);
        }
    }
#ifdef ENABLE_MULTITHREAD
        hg_thread_mutex_lock(&pdc_client_addr_metex_g);
#endif

done:
    FUNC_LEAVE(ret_value);
}

/*
 * Get the metadata with obj ID and hash key
 *
 * \param  obj_id[IN]        Object ID
 * \param  hash_key[IN]      Hash value of the ID attributes
 *
 * \return NULL if no match is found/pointer to the metadata structure otherwise
 */
/* static pdc_metadata_t * find_metadata_by_id_and_hash_key(uint64_t obj_id, uint32_t hash_key) */
/* { */
/*     pdc_metadata_t *ret_value = NULL; */
/*     pdc_metadata_t *elt; */
/*     pdc_hash_table_entry_head *lookup_value = NULL; */

/*     FUNC_ENTER(NULL); */

/*     if (metadata_hash_table_g != NULL) { */
/*         lookup_value = hg_hash_table_lookup(metadata_hash_table_g, &hash_key); */

/*         if (lookup_value == NULL) { */
/*             ret_value = NULL; */
/*             goto done; */
/*         } */

/*         DL_FOREACH(lookup_value->metadata, elt) { */
/*             if (elt->obj_id == obj_id) { */
/*                 ret_value = elt; */
/*                 goto done; */
/*             } */
/*         } */

/*     }  // if (metadata_hash_table_g != NULL) */
/*     else { */
/*         printf("==PDC_SERVER: metadata_hash_table_g not initilized!\n"); */
/*         ret_value = NULL; */
/*         goto done; */
/*     } */
/* done: */
/*     FUNC_LEAVE(ret_value); */
/* } */

/*
 * Get the metadata with obj ID from the metadata list
 *
 * \param  mlist[IN]         Metadata list head
 * \param  obj_id[IN]        Object ID
 *
 * \return NULL if no match is found/pointer to the found metadata otherwise
 */
static pdc_metadata_t * find_metadata_by_id_from_list(pdc_metadata_t *mlist, uint64_t obj_id)
{
    pdc_metadata_t *ret_value, *elt;

    FUNC_ENTER(NULL);

    ret_value = NULL;
    if (mlist == NULL) {
        ret_value = NULL;
        goto done;
    }

    DL_FOREACH(mlist, elt) {
        if (elt->obj_id == obj_id) {
            ret_value = elt;
            goto done;
        }
    }

done:
    FUNC_LEAVE(ret_value);
}

/*
 * Get the metadata with the specified object ID by iteration of all metadata in the hash table
 *
 * \param  obj_id[IN]        Object ID
 *
 * \return NULL if no match is found/pointer to the found metadata otherwise
 */
static pdc_metadata_t * find_metadata_by_id(uint64_t obj_id)
{
    pdc_metadata_t *ret_value;
    pdc_hash_table_entry_head *head;
    pdc_metadata_t *elt;
    hg_hash_table_iter_t hash_table_iter;
    int n_entry;

    FUNC_ENTER(NULL);

    if (metadata_hash_table_g != NULL) {
        // Since we only have the obj id, need to iterate the entire hash table
        n_entry = hg_hash_table_num_entries(metadata_hash_table_g);
        hg_hash_table_iterate(metadata_hash_table_g, &hash_table_iter);

        while (n_entry != 0 && hg_hash_table_iter_has_more(&hash_table_iter)) {

            head = hg_hash_table_iter_next(&hash_table_iter);
            // Now iterate the list under this entry
            DL_FOREACH(head->metadata, elt) {
                if (elt->obj_id == obj_id) {
                    return elt;
                }
            }
        }
    }  // if (metadata_hash_table_g != NULL)
    else {
        printf("==PDC_SERVER: metadata_hash_table_g not initialized!\n");
        ret_value = NULL;
        goto done;
    }

done:
    FUNC_LEAVE(ret_value);
}

/*
 * Wrapper function of find_metadata_by_id().
 *
 * \param  obj_id[IN]        Object ID
 *
 * \return NULL if no match is found/pointer to the found metadata otherwise
 */
pdc_metadata_t *PDC_Server_get_obj_metadata(pdcid_t obj_id)
{
    pdc_metadata_t *ret_value = NULL;

    FUNC_ENTER(NULL);

    ret_value = find_metadata_by_id(obj_id);
    if (ret_value == NULL) {
        printf("==PDC_SERVER[%d]: PDC_Server_get_obj_metadata() - cannot find meta with id %" PRIu64 "\n",
                pdc_server_rank_g, obj_id);
        goto done;
    }

done:
    FUNC_LEAVE(ret_value);
}

/*
 * Find if there is identical metadata exist in hash table
 *
 * \param  entry[IN]        Hash table entry of metadata
 * \param  a[IN]            Pointer to metadata to be checked against
 *
 * \return NULL if no match is found/pointer to the found metadata otherwise
 */
static pdc_metadata_t * find_identical_metadata(pdc_hash_table_entry_head *entry, pdc_metadata_t *a)
{
    pdc_metadata_t *ret_value = NULL;
    BLOOM_TYPE_T *bloom;
    int bloom_check;

    FUNC_ENTER(NULL);

/*     printf("==PDC_SERVER: querying with:\n"); */
/*     PDC_print_metadata(a); */
/*     fflush(stdout); */

    // Use bloom filter to quick check if current metadata is in the list
    if (entry->bloom != NULL && a->user_id != 0 && a->app_name[0] != 0) {
        /* printf("==PDC_SERVER: using bloom filter\n"); */
        /* fflush(stdout); */

        bloom = entry->bloom;

        char combined_string[ADDR_MAX];
        combine_obj_info_to_string(a, combined_string);
        /* printf("bloom_check: Combined string: %s\n", combined_string); */
        /* fflush(stdout); */

#ifdef ENABLE_TIMING
        // Timing
        gettimeofday(&ht_total_start, 0);
#endif

        bloom_check = BLOOM_CHECK(bloom, combined_string, strlen(combined_string));

#ifdef ENABLE_TIMING
        // Timing
        struct timeval  ht_total_start;
        struct timeval  ht_total_end;
        long long ht_total_elapsed;
        double ht_total_sec;

        gettimeofday(&ht_total_end, 0);
        ht_total_elapsed    = (ht_total_end.tv_sec-ht_total_start.tv_sec)*1000000LL + ht_total_end.tv_usec-ht_total_start.tv_usec;
        ht_total_sec        = ht_total_elapsed / 1000000.0;
#endif

#ifdef ENABLE_MULTITHREAD
        hg_thread_mutex_lock(&pdc_bloom_time_mutex_g);
#endif

#ifdef ENABLE_TIMING
        server_bloom_check_time_g += ht_total_sec;
#endif

#ifdef ENABLE_MULTITHREAD
        hg_thread_mutex_unlock(&pdc_bloom_time_mutex_g);
#endif

        n_bloom_total_g++;
        if (bloom_check == 0) {
            /* printf("==PDC_SERVER[%d]: Bloom filter says definitely not %s!\n", pdc_server_rank_g, combined_string); */
            /* fflush(stdout); */
            ret_value = NULL;
            goto done;
        }
        else {
            // bloom filter says maybe, so need to check entire list
            /* printf("==PDC_SERVER[%d]: Bloom filter says maybe for %s!\n", pdc_server_rank_g, combined_string); */
            /* fflush(stdout); */
            n_bloom_maybe_g++;
            pdc_metadata_t *elt;
            DL_FOREACH(entry->metadata, elt) {
                if (PDC_metadata_cmp(elt, a) == 0) {
                    /* printf("Identical metadata exist in Metadata store!\n"); */
                    /* PDC_print_metadata(a); */
                    ret_value = elt;
                    goto done;
                }
            }
        }

    }
    else {
        // Bloom has not been created
        /* printf("Bloom filter not created yet!\n"); */
        /* fflush(stdout); */
        pdc_metadata_t *elt;
        DL_FOREACH(entry->metadata, elt) {
            if (PDC_metadata_cmp(elt, a) == 0) {
                /* printf("Identical metadata exist in Metadata store!\n"); */
                /* PDC_print_metadata(a); */
                ret_value = elt;
                goto done;
            }
        }
    } // if bloom==NULL

done:
    /* int count; */
    /* DL_COUNT(lookup_value, elt, count); */
    /* printf("%d item(s) in list\n", count); */

    FUNC_LEAVE(ret_value);
}

/*
 * Print the Mercury version
 *
 * \return void
 */
void PDC_Server_print_version()
{
    unsigned major, minor, patch;

    FUNC_ENTER(NULL);

    HG_Version_get(&major, &minor, &patch);
    printf("Server running mercury version %u.%u-%u\n", major, minor, patch);
    return;
}

/*
 * Allocate a new object ID
 *
 * \return 64-bit integer of object ID
 */
static uint64_t PDC_Server_gen_obj_id()
{
    uint64_t ret_value;

    FUNC_ENTER(NULL);

    ret_value = pdc_id_seq_g++;

    FUNC_LEAVE(ret_value);
}

/*
 * Write the servers' addresses to file, so that client can read the file and
 * get all the servers' addresses.
 *
 * \param  addr_strings[IN]     2D char array of all servers' network address
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_write_addr_to_file(char** addr_strings, int n)
{
    perr_t ret_value = SUCCEED;
    char config_fname[ADDR_MAX];
    int i;

    FUNC_ENTER(NULL);

    // write to file

    sprintf(config_fname, "%s%s", pdc_server_tmp_dir_g, pdc_server_cfg_name_g);
    FILE *na_config = fopen(config_fname, "w+");
    if (!na_config) {
        fprintf(stderr, "Could not open config file from: %s\n", config_fname);
        goto done;
    }

    fprintf(na_config, "%d\n", n);
    for (i = 0; i < n; i++) {
        fprintf(na_config, "%s\n", addr_strings[i]);
    }
    fclose(na_config);
    na_config = NULL;

done:
    FUNC_LEAVE(ret_value);
}

/*
 * Init the hash table for metadata storage
 *
 * \return Non-negative on success/Negative on failure
 */
static perr_t PDC_Server_init_hash_table()
{
    perr_t ret_value = SUCCEED;

    FUNC_ENTER(NULL);

    metadata_hash_table_g = hg_hash_table_new(PDC_Server_metadata_int_hash, PDC_Server_metadata_int_equal);
    if (metadata_hash_table_g == NULL) {
        printf("==PDC_SERVER: metadata_hash_table_g init error! Exit...\n");
        goto done;
    }
    hg_hash_table_register_free_functions(metadata_hash_table_g, PDC_Server_metadata_int_hash_key_free, PDC_Server_metadata_hash_value_free);

    /* // Name marker hash table, reuse some functions from metadata_hash_table */
    /* metadata_name_mark_hash_table_g = hg_hash_table_new(PDC_Server_metadata_int_hash, PDC_Server_metadata_int_equal); */
    /* if (metadata_name_mark_hash_table_g == NULL) { */
    /*     printf("==PDC_SERVER: metadata_name_mark_hash_table_g init error! Exit...\n"); */
    /*     exit(-1); */
    /* } */
    /* hg_hash_table_register_free_functions(metadata_name_mark_hash_table_g, PDC_Server_metadata_int_hash_key_free, PDC_Server_metadata_name_mark_hash_value_free); */

    is_hash_table_init_g = 1;

done:
    FUNC_LEAVE(ret_value);
}

/*
 * Remove a metadata from bloom filter
 *
 * \param  metadata[IN]     Metadata pointer of the remove target
 * \param  bloom[IN]        Bloom filter's pointer
 *
 * \return Non-negative on success/Negative on failure
 */
static perr_t PDC_Server_remove_from_bloom(pdc_metadata_t *metadata, BLOOM_TYPE_T *bloom)
{
    perr_t ret_value = SUCCEED;

    FUNC_ENTER(NULL);

    if (bloom == NULL) {
        printf("==PDC_SERVER: PDC_Server_remove_from_bloom(): bloom pointer is NULL\n");
        ret_value = FAIL;
        goto done;
    }

    char combined_string[ADDR_MAX];
    combine_obj_info_to_string(metadata, combined_string);
    /* printf("==PDC_SERVER: PDC_Server_remove_from_bloom(): Combined string: %s\n", combined_string); */

    ret_value = BLOOM_REMOVE(bloom, combined_string, strlen(combined_string));
    if (ret_value != SUCCEED) {
        printf("==PDC_SERVER[%d]: PDC_Server_remove_from_bloom() - error\n",
                pdc_server_rank_g);
        goto done;
    }


done:
    FUNC_LEAVE(ret_value);
}

/*
 * Add a metadata to bloom filter
 *
 * \param  metadata[IN]     Metadata pointer of the target
 * \param  bloom[IN]        Bloom filter's pointer
 *
 * \return Non-negative on success/Negative on failure
 */
static perr_t PDC_Server_add_to_bloom(pdc_metadata_t *metadata, BLOOM_TYPE_T *bloom)
{
    perr_t ret_value = SUCCEED;
    char combined_string[ADDR_MAX];

    FUNC_ENTER(NULL);

    if (bloom == NULL) {
        /* printf("==PDC_SERVER: PDC_Server_add_to_bloom(): bloom pointer is NULL\n"); */
        /* ret_value = FAIL; */
        goto done;
    }

    combine_obj_info_to_string(metadata, combined_string);
    /* printf("==PDC_SERVER[%d]: PDC_Server_add_to_bloom(): Combined string: %s\n", pdc_server_rank_g, combined_string); */
    /* fflush(stdout); */

    // Only add to bloom filter if it's definately not
    /* int bloom_check; */
    /* bloom_check = BLOOM_CHECK(bloom, combined_string, strlen(combined_string)); */
    /* if (bloom_check == 0) { */
        ret_value = BLOOM_ADD(bloom, combined_string, strlen(combined_string));
    /* } */
    if (ret_value != SUCCEED) {
        printf("==PDC_SERVER[%d]: PDC_Server_add_to_bloom() - error \n",
                pdc_server_rank_g);
        goto done;
    }


done:
    FUNC_LEAVE(ret_value);
}

/*
 * Init a bloom filter
 *
 * \param  entry[IN]     Entry of the metadata hash table
 *
 * \return Non-negative on success/Negative on failure
 */
static perr_t PDC_Server_bloom_init(pdc_hash_table_entry_head *entry)
{
    perr_t      ret_value = 0;
    int capacity = 500000;
    double error_rate = 0.05;

    FUNC_ENTER(NULL);

    // Init bloom filter
    /* int capacity = 100000; */
    n_bloom_maybe_g = 0;
    n_bloom_total_g = 0;

#ifdef ENABLE_TIMING
    // Timing
    struct timeval  ht_total_start;
    struct timeval  ht_total_end;
    long long ht_total_elapsed;
    double ht_total_sec;

    gettimeofday(&ht_total_start, 0);
#endif

    entry->bloom = (BLOOM_TYPE_T*)BLOOM_NEW(capacity, error_rate);
    if (!entry->bloom) {
        fprintf(stderr, "ERROR: Could not create bloom filter\n");
        ret_value = -1;
        goto done;
    }

    /* PDC_Server_add_to_bloom(entry, bloom); */

#ifdef ENABLE_TIMING
    // Timing
    gettimeofday(&ht_total_end, 0);
    ht_total_elapsed    = (ht_total_end.tv_sec-ht_total_start.tv_sec)*1000000LL + ht_total_end.tv_usec-ht_total_start.tv_usec;
    ht_total_sec        = ht_total_elapsed / 1000000.0;

    server_bloom_init_time_g += ht_total_sec;
#endif

done:
    FUNC_LEAVE(ret_value);
}

/*
 * Insert a metadata to the metadata hash table
 *
 * \param  head[IN]      Head of the hash table
 * \param  new[IN]       Metadata pointer to be inserted
 *
 * \return Non-negative on success/Negative on failure
 */
static perr_t PDC_Server_hash_table_list_insert(pdc_hash_table_entry_head *head, pdc_metadata_t *new)
{
    perr_t ret_value = SUCCEED;
    pdc_metadata_t *elt;

    FUNC_ENTER(NULL);

    // add to bloom filter
    if (head->n_obj == CREATE_BLOOM_THRESHOLD) {
        /* printf("==PDC_SERVER:Init bloom\n"); */
        /* fflush(stdout); */
        PDC_Server_bloom_init(head);
        DL_FOREACH(head->metadata, elt) {
            PDC_Server_add_to_bloom(elt, head->bloom);
        }
        ret_value = PDC_Server_add_to_bloom(new, head->bloom);
        if (ret_value != SUCCEED) {
            printf("==PDC_SERVER[%d]: PDC_Server_hash_table_list_insert() - error add to bloom\n",
                    pdc_server_rank_g);
            goto done;
        }
    }
    else if (head->n_obj >= CREATE_BLOOM_THRESHOLD || head->bloom != NULL) {
        /* printf("==PDC_SERVER: Adding to bloom filter, %d existed\n", head->n_obj); */
        /* fflush(stdout); */
        ret_value = PDC_Server_add_to_bloom(new, head->bloom);
        if (ret_value != SUCCEED) {
            printf("==PDC_SERVER[%d]: PDC_Server_hash_table_list_insert() - error add to bloom\n",
                    pdc_server_rank_g);
            goto done;
        }

    }

    /* printf("Adding to linked list\n"); */
    // Currently $metadata is unique, insert to linked list
    DL_APPEND(head->metadata, new);
    head->n_obj++;

    /* // Debug print */
    /* int count; */
    /* pdc_metadata_t *elt; */
    /* DL_COUNT(head, elt, count); */
    /* printf("Append one metadata, total=%d\n", count); */
done:

    FUNC_LEAVE(ret_value);
}

/*
 * Init a metadata list (doubly linked) under the given hash table entry
 *
 * \param  entry[IN]        An entry pointer of the hash table
 * \param  hash_key[IN]     Hash key of the entry
 *
 * \return Non-negative on success/Negative on failure
 */
static perr_t PDC_Server_hash_table_list_init(pdc_hash_table_entry_head *entry, uint32_t *hash_key)
{

    perr_t      ret_value = SUCCEED;
    hg_return_t ret;

    FUNC_ENTER(NULL);

    /* printf("==PDC_SERVER[%d]: hash entry init for hash key [%u]\n", pdc_server_rank_g, *hash_key); */

/*     // Init head of linked list */
/*     metadata->prev = metadata;                                                                   \ */
/*     metadata->next = NULL; */

#ifdef ENABLE_TIMING
    // Timing
    struct timeval  ht_total_start;
    struct timeval  ht_total_end;
    long long ht_total_elapsed;
    double ht_total_sec;

    gettimeofday(&ht_total_start, 0);
#endif

    // Insert to hash table
    ret = hg_hash_table_insert(metadata_hash_table_g, hash_key, entry);
    if (ret != 1) {
        fprintf(stderr, "PDC_Server_hash_table_list_init(): Error with hash table insert!\n");
        ret_value = FAIL;
        goto done;
    }

    /* PDC_print_metadata(entry->metadata); */
    /* printf("entry n_obj=%d\n", entry->n_obj); */

#ifdef ENABLE_TIMING
    // Timing
    gettimeofday(&ht_total_end, 0);
    ht_total_elapsed    = (ht_total_end.tv_sec-ht_total_start.tv_sec)*1000000LL + ht_total_end.tv_usec-ht_total_start.tv_usec;
    ht_total_sec        = ht_total_elapsed / 1000000.0;

    server_hash_insert_time_g += ht_total_sec;
#endif

    /* PDC_Server_bloom_init(new); */

done:
    FUNC_LEAVE(ret_value);
}

/* perr_t insert_obj_name_marker(send_obj_name_marker_in_t *in, send_obj_name_marker_out_t *out) { */

/*     FUNC_ENTER(NULL); */

/*     perr_t ret_value = SUCCEED; */
/*     hg_return_t hg_ret; */


/*     /1* printf("==PDC_SERVER: Insert obj name marker [%s]\n", in->obj_name); *1/ */

/*     uint32_t *hash_key = (uint32_t*)malloc(sizeof(uint32_t)); */
/*     if (hash_key == NULL) { */
/*         printf("Cannot allocate hash_key!\n"); */
/*         goto done; */
/*     } */
/*     total_mem_usage_g += sizeof(uint32_t); */

/*     *hash_key = in->hash_value; */

/*     pdc_metadata_name_mark_t *namemark= (pdc_metadata_name_mark_t*)malloc(sizeof(pdc_metadata_name_mark_t)); */
/*     if (namemark == NULL) { */
/*         printf("==PDC_SERVER: ERROR - Cannot allocate pdc_metadata_name_mark_t!\n"); */
/*         goto done; */
/*     } */
/*     total_mem_usage_g += sizeof(pdc_metadata_name_mark_t); */
/*     strcpy(namemark->obj_name, in->obj_name); */

/*     pdc_metadata_name_mark_t *lookup_value; */
/*     pdc_metadata_name_mark_t *elt; */

/* #ifdef ENABLE_MULTITHREAD */
/*     // Obtain lock for hash table */
/*     int unlocked = 0; */
/*     hg_thread_mutex_lock(&pdc_metadata_name_mark_hash_table_mutex_g); */
/* #endif */

/*     if (metadata_name_mark_hash_table_g != NULL) { */
/*         // lookup */
/*         /1* printf("checking hash table with key=%d\n", *hash_key); *1/ */
/*         lookup_value = hg_hash_table_lookup(metadata_name_mark_hash_table_g, hash_key); */

/*         // Is this hash value exist in the Hash table? */
/*         if (lookup_value != NULL) { */
/*             // Check if there exist namemark identical to current one */
/*             if (find_identical_namemark(lookup_value, namemark) == 1) { */
/*                 // If find same one, do nothing */
/*                 /1* printf("==PDC_SERVER: marker exist for [%s]\n", namemark->obj_name); *1/ */
/*                 ret_value = 0; */
/*                 free(namemark); */
/*             } */
/*             else { */
/*                 // Currently namemark is unique, insert to linked list */
/*                 DL_APPEND(lookup_value, namemark); */
/*             } */

/*             /1* free(hash_key); *1/ */
/*         } */
/*         else { */
/*             /1* printf("lookup_value is NULL!\n"); *1/ */
/*             // First entry for current hasy_key, init linked list */
/*             namemark->prev = namemark;                                                                   \ */
/*             namemark->next = NULL; */

/*             // Insert to hash table */
/*             hg_ret = hg_hash_table_insert(metadata_name_mark_hash_table_g, hash_key, namemark); */
/*             if (hg_ret != 1) { */
/*                 fprintf(stderr, "==PDC_SERVER: ERROR - insert_obj_name_marker() error with hash table insert!\n"); */
/*                 ret_value = -1; */
/*                 goto done; */
/*             } */
/*         } */
/*     } */
/*     else { */
/*         printf("metadata_hash_table_g not initialized!\n"); */
/*         ret_value = -1; */
/*         goto done; */
/*     } */

/* #ifdef ENABLE_MULTITHREAD */
/*     // ^ Release hash table lock */
/*     hg_thread_mutex_unlock(&pdc_metadata_name_mark_hash_table_mutex_g); */
/* #endif */

/* done: */
/*     out->ret = 1; */
/*     FUNC_LEAVE(ret_value); */
/* } */

/*
 * Add the tag received from one client to the corresponding metadata structure
 *
 * \param  in[IN]       Input structure received from client
 * \param  out[OUT]     Output structure to be sent back to the client
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_add_tag_metadata(metadata_add_tag_in_t *in, metadata_add_tag_out_t *out)
{

    FUNC_ENTER(NULL);

    perr_t ret_value;

#ifdef ENABLE_TIMING
    // Timing
    struct timeval  ht_total_start;
    struct timeval  ht_total_end;
    long long ht_total_elapsed;
    double ht_total_sec;

    gettimeofday(&ht_total_start, 0);
#endif

    /* printf("==PDC_SERVER: Got add_tag request: hash=%u, obj_id=%" PRIu64 "\n", in->hash_value, in->obj_id); */

    uint32_t *hash_key = (uint32_t*)malloc(sizeof(uint32_t));
    if (hash_key == NULL) {
        printf("==PDC_SERVER: Cannot allocate hash_key!\n");
        goto done;
    }
    total_mem_usage_g += sizeof(uint32_t);
    *hash_key = in->hash_value;
    uint64_t obj_id = in->obj_id;

    pdc_hash_table_entry_head *lookup_value;

#ifdef ENABLE_MULTITHREAD
    // Obtain lock for hash table
    int unlocked = 0;
    hg_thread_mutex_lock(&pdc_metadata_hash_table_mutex_g);
#endif

    if (metadata_hash_table_g != NULL) {
        // lookup
        /* printf("==PDC_SERVER: checking hash table with key=%d\n", *hash_key); */
        lookup_value = hg_hash_table_lookup(metadata_hash_table_g, hash_key);

        // Is this hash value exist in the Hash table?
        if (lookup_value != NULL) {

            /* printf("==PDC_SERVER: lookup_value not NULL!\n"); */
            // Check if there exist metadata identical to current one
            pdc_metadata_t *target;
            target = find_metadata_by_id_from_list(lookup_value->metadata, obj_id);
            if (target != NULL) {
                /* printf("==PDC_SERVER: Found add_tag target!\n"); */
                /* printf("Received add_tag info:\n"); */
                /* PDC_print_metadata(&in->new_metadata); */

                // Check and find valid add_tag fields
                // Currently user_id, obj_name are not supported to be updated in this way
                // obj_name change is done through client with delete and add operation.
                if (in->new_tag != NULL && in->new_tag[0] != 0 &&
                        !(in->new_tag[0] == ' ' && in->new_tag[1] == 0)) {
                    // add a ',' to separate different tags
                    /* printf("Previous tags: %s\n", target->tags); */
                    /* printf("Adding tags: %s\n", in->new_metadata.tags); */
                    target->tags[strlen(target->tags)+1] = 0;
                    target->tags[strlen(target->tags)] = ',';
                    strcat(target->tags, in->new_tag);
                    /* printf("Final tags: %s\n", target->tags); */
                }

                out->ret  = 1;

            } // if (lookup_value != NULL)
            else {
                // Object not found for deletion request
                /* printf("==PDC_SERVER: add tag target not found!\n"); */
                ret_value = FAIL;
                out->ret  = -1;
            }

        } // if lookup_value != NULL
        else {
            /* printf("==PDC_SERVER: add tag target not found!\n"); */
            ret_value = FAIL;
            out->ret = -1;
        }

    } // if (metadata_hash_table_g != NULL)
    else {
        printf("==PDC_SERVER: metadata_hash_table_g not initilized!\n");
        ret_value = FAIL;
        out->ret = -1;
        goto done;
    }

    if (ret_value != SUCCEED) {
        printf("==PDC_SERVER[%d]: PDC_Server_add_tag_metadata() - error \n",
                pdc_server_rank_g);
        goto done;
    }


#ifdef ENABLE_MULTITHREAD
    // ^ Release hash table lock
    hg_thread_mutex_unlock(&pdc_metadata_hash_table_mutex_g);
    unlocked = 1;
#endif

#ifdef ENABLE_TIMING
    // Timing
    gettimeofday(&ht_total_end, 0);
    ht_total_elapsed    = (ht_total_end.tv_sec-ht_total_start.tv_sec)*1000000LL + ht_total_end.tv_usec-ht_total_start.tv_usec;
    ht_total_sec        = ht_total_elapsed / 1000000.0;
#endif

#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_lock(&pdc_time_mutex_g);
#endif

#ifdef ENABLE_TIMING
    server_update_time_g += ht_total_sec;
#endif

#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_unlock(&pdc_time_mutex_g);
#endif


done:
#ifdef ENABLE_MULTITHREAD
    if (unlocked == 0)
        hg_thread_mutex_unlock(&pdc_metadata_hash_table_mutex_g);
#endif
    /* if (hash_key != NULL) */
    /*     free(hash_key); */
    fflush(stdout);
    FUNC_LEAVE(ret_value);
} // end of add_tag_metadata_from_hash_table

/*
 * Update the metadata received from one client to the corresponding metadata structure
 *
 * \param  in[IN]       Input structure received from client
 * \param  out[OUT]     Output structure to be sent back to the client
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_update_metadata(metadata_update_in_t *in, metadata_update_out_t *out)
{
    perr_t ret_value;
    uint64_t obj_id;
    pdc_hash_table_entry_head *lookup_value;
    /* pdc_metadata_t *elt; */
    /* int unlocked = 0; */
    uint32_t *hash_key;
    pdc_metadata_t *target;

    FUNC_ENTER(NULL);

#ifdef ENABLE_TIMING
    // Timing
    struct timeval  ht_total_start;
    struct timeval  ht_total_end;
    long long ht_total_elapsed;
    double ht_total_sec;

    gettimeofday(&ht_total_start, 0);
#endif

    /* printf("==PDC_SERVER: Got update request: hash=%u, obj_id=%" PRIu64 "\n", in->hash_value, in->obj_id); */

    hash_key = (uint32_t*)malloc(sizeof(uint32_t));
    if (hash_key == NULL) {
        printf("==PDC_SERVER: Cannot allocate hash_key!\n");
        goto done;
    }
    total_mem_usage_g += sizeof(uint32_t);
    *hash_key = in->hash_value;
    obj_id = in->obj_id;

#ifdef ENABLE_MULTITHREAD
    // Obtain lock for hash table
    hg_thread_mutex_lock(&pdc_metadata_hash_table_mutex_g);
#endif

    if (metadata_hash_table_g != NULL) {
        // lookup
        /* printf("==PDC_SERVER: checking hash table with key=%d\n", *hash_key); */
        lookup_value = hg_hash_table_lookup(metadata_hash_table_g, hash_key);

        // Is this hash value exist in the Hash table?
        if (lookup_value != NULL) {

            /* printf("==PDC_SERVER: lookup_value not NULL!\n"); */
            // Check if there exist metadata identical to current one
            target = find_metadata_by_id_from_list(lookup_value->metadata, obj_id);
            if (target != NULL) {
                /* printf("==PDC_SERVER: Found update target!\n"); */
                /* printf("Received update info:\n"); */
                /* PDC_print_metadata(&in->new_metadata); */
                /* printf("==PDC_SERVER: new dataloc: [%s]\n", in->new_metadata.data_location); */

                // Check and find valid update fields
                // Currently user_id, obj_name are not supported to be updated in this way
                // obj_name change is done through client with delete and add operation.
                if (in->new_metadata.time_step != -1)
                    target->time_step = in->new_metadata.time_step;
                if (in->new_metadata.app_name[0] != 0 &&
                        !(in->new_metadata.app_name[0] == ' ' && in->new_metadata.app_name[1] == 0))
                    strcpy(target->app_name,      in->new_metadata.app_name);
                if (in->new_metadata.data_location[0] != 0 &&
                        !(in->new_metadata.data_location[0] == ' ' && in->new_metadata.data_location[1] == 0))
                    strcpy(target->data_location, in->new_metadata.data_location);
                if (in->new_metadata.tags[0] != 0 &&
                        !(in->new_metadata.tags[0] == ' ' && in->new_metadata.tags[1] == 0)) {
                    // add a ',' to separate different tags
                    /* printf("Previous tags: %s\n", target->tags); */
                    /* printf("Adding tags: %s\n", in->new_metadata.tags); */
                    target->tags[strlen(target->tags)+1] = 0;
                    target->tags[strlen(target->tags)] = ',';
                    strcat(target->tags, in->new_metadata.tags);
                    /* printf("Final tags: %s\n", target->tags); */
                }

                out->ret  = 1;
            } // if (lookup_value != NULL)
            else {
                // Object not found for deletion request
                /* printf("==PDC_SERVER: update target not found!\n"); */
                ret_value = -1;
                out->ret  = -1;
            }

        } // if lookup_value != NULL
        else {
            /* printf("==PDC_SERVER: update target not found!\n"); */
            ret_value = -1;
            out->ret = -1;
        }

    } // if (metadata_hash_table_g != NULL)
    else {
        printf("==PDC_SERVER: metadata_hash_table_g not initialized!\n");
        ret_value = -1;
        out->ret = -1;
        goto done;
    }

#ifdef ENABLE_MULTITHREAD
    // ^ Release hash table lock
    hg_thread_mutex_unlock(&pdc_metadata_hash_table_mutex_g);
    unlocked = 1;
#endif

#ifdef ENABLE_TIMING
    // Timing
    gettimeofday(&ht_total_end, 0);
    ht_total_elapsed    = (ht_total_end.tv_sec-ht_total_start.tv_sec)*1000000LL + ht_total_end.tv_usec-ht_total_start.tv_usec;
    ht_total_sec        = ht_total_elapsed / 1000000.0;
#endif

#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_lock(&pdc_time_mutex_g);
#endif

#ifdef ENABLE_TIMING
    server_update_time_g += ht_total_sec;
#endif

#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_unlock(&pdc_time_mutex_g);
#endif


done:
#ifdef ENABLE_MULTITHREAD
    if (unlocked == 0)
        hg_thread_mutex_unlock(&pdc_metadata_hash_table_mutex_g);
#endif
    fflush(stdout);
    FUNC_LEAVE(ret_value);
} // end of update_metadata_from_hash_table

/*
 * Delete metdata with the ID received from a client
 *
 * \param  in[IN]       Input structure received from client, conatins object ID
 * \param  out[OUT]     Output structure to be sent back to the client
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t delete_metadata_by_id(metadata_delete_by_id_in_t *in, metadata_delete_by_id_out_t *out)
{
    perr_t ret_value = FAIL;
    pdc_metadata_t *elt;
    hg_hash_table_iter_t hash_table_iter;
    uint64_t target_obj_id;
    int n_entry;

    FUNC_ENTER(NULL);

    out->ret = -1;

#ifdef ENABLE_TIMING
    // Timing
    struct timeval  ht_total_start;
    struct timeval  ht_total_end;
    long long ht_total_elapsed;
    double ht_total_sec;

    gettimeofday(&ht_total_start, 0);
#endif


    /* printf("==PDC_SERVER[%d]: Got delete by id request: obj_id=%" PRIu64 "\n", pdc_server_rank_g, in->obj_id); */
    /* fflush(stdout); */

    target_obj_id = in->obj_id;

    /* printf("==PDC_SERVER: delete request name:%s ts=%d hash=%u\n", in->obj_name, in->time_step, in->hash_value); */

#ifdef ENABLE_MULTITHREAD
    // Obtain lock for hash table
    int unlocked = 0;
    hg_thread_mutex_lock(&pdc_metadata_hash_table_mutex_g);
#endif

    if (metadata_hash_table_g != NULL) {

        // Since we only have the obj id, need to iterate the entire hash table
        pdc_hash_table_entry_head *head;

        n_entry = hg_hash_table_num_entries(metadata_hash_table_g);
        hg_hash_table_iterate(metadata_hash_table_g, &hash_table_iter);

        while (n_entry != 0 && hg_hash_table_iter_has_more(&hash_table_iter)) {

            head = hg_hash_table_iter_next(&hash_table_iter);
            // Now iterate the list under this entry
            DL_FOREACH(head->metadata, elt) {

                if (elt->obj_id == target_obj_id) {
                    // We found the delete target
                    // Check if there are more objects in this list
                    if (head->n_obj > 1) {
                        // Remove from bloom filter
                        if (head->bloom != NULL) {
                            PDC_Server_remove_from_bloom(elt, head->bloom);
                        }

                        // Remove from linked list
                        DL_DELETE(head->metadata, elt);
                        head->n_obj--;
                        /* printf("==PDC_SERVER: delete from DL!\n"); */
                    }
                    else {
                        // This is the last item under the current entry, remove the hash entry
                        /* printf("==PDC_SERVER: delete from hash table!\n"); */
                        uint32_t hash_key = PDC_get_hash_by_name(elt->obj_name);
                        hg_hash_table_remove(metadata_hash_table_g, &hash_key);

                        // Free this item and delete hash table entry
                        /* if(is_restart_g != 1) { */
                        /*     free(elt); */
                        /* } */
                    }
                    out->ret  = 1;
                    ret_value = SUCCEED;
                }
            } // DL_FOREACH
        }  // while
    } // if (metadata_hash_table_g != NULL)
    else {
        printf("==PDC_SERVER: metadata_hash_table_g not initialized!\n");
        ret_value = FAIL;
        out->ret = -1;
        goto done;
    }

#ifdef ENABLE_MULTITHREAD
    // ^ Release hash table lock
    hg_thread_mutex_unlock(&pdc_metadata_hash_table_mutex_g);
    unlocked = 1;
#endif

#ifdef ENABLE_TIMING
    // Timing
    gettimeofday(&ht_total_end, 0);
    ht_total_elapsed    = (ht_total_end.tv_sec-ht_total_start.tv_sec)*1000000LL + ht_total_end.tv_usec-ht_total_start.tv_usec;
    ht_total_sec        = ht_total_elapsed / 1000000.0;
#endif

#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_lock(&pdc_time_mutex_g);
#endif

#ifdef ENABLE_TIMING
    server_delete_time_g += ht_total_sec;
#endif

#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_unlock(&pdc_time_mutex_g);
#endif


    // Decrement total metadata count
#ifdef ENABLE_MULTITHREAD
        hg_thread_mutex_lock(&n_metadata_mutex_g);
#endif
        n_metadata_g-- ;
#ifdef ENABLE_MULTITHREAD
        hg_thread_mutex_unlock(&n_metadata_mutex_g);
#endif


done:
    /* printf("==PDC_SERVER[%d]: Finished delete by id request: obj_id=%" PRIu64 "\n", pdc_server_rank_g, in->obj_id); */
    /* fflush(stdout); */
#ifdef ENABLE_MULTITHREAD
    if (unlocked == 0)
        hg_thread_mutex_unlock(&pdc_metadata_hash_table_mutex_g);
#endif
    FUNC_LEAVE(ret_value);
} // end of delete_metadata_by_id


/*
 * Delete metdata from hash table with the ID received from a client
 *
 * \param  in[IN]       Input structure received from client, conatins object ID
 * \param  out[OUT]     Output structure to be sent back to the client
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t delete_metadata_from_hash_table(metadata_delete_in_t *in, metadata_delete_out_t *out)
{
    perr_t ret_value;
    uint32_t *hash_key;
    pdc_metadata_t *target;

    FUNC_ENTER(NULL);

#ifdef ENABLE_TIMING
    // Timing
    struct timeval  ht_total_start;
    struct timeval  ht_total_end;
    long long ht_total_elapsed;
    double ht_total_sec;

    gettimeofday(&ht_total_start, 0);
#endif

    /* printf("==PDC_SERVER[%d]: Got delete request: hash=%d, obj_id=%" PRIu64 "\n", pdc_server_rank_g, in->hash_value, in->obj_id); */
    /* fflush(stdout); */

    hash_key = (uint32_t*)malloc(sizeof(uint32_t));
    if (hash_key == NULL) {
        printf("==PDC_SERVER: Cannot allocate hash_key!\n");
        goto done;
    }
    total_mem_usage_g += sizeof(uint32_t);
    *hash_key = in->hash_value;
    /* uint64_t obj_id = in->obj_id; */

    pdc_hash_table_entry_head *lookup_value;
    pdc_metadata_t metadata;
    strcpy(metadata.obj_name, in->obj_name);
    metadata.time_step = in->time_step;
    metadata.app_name[0] = 0;
    metadata.user_id = -1;
    metadata.obj_id = 0;

    /* printf("==PDC_SERVER: delete request name:%s ts=%d hash=%u\n", in->obj_name, in->time_step, in->hash_value); */

#ifdef ENABLE_MULTITHREAD
    // Obtain lock for hash table
    int unlocked = 0;
    hg_thread_mutex_lock(&pdc_metadata_hash_table_mutex_g);
#endif

    if (metadata_hash_table_g != NULL) {
        // lookup
        /* printf("==PDC_SERVER: checking hash table with key=%d\n", *hash_key); */
        lookup_value = hg_hash_table_lookup(metadata_hash_table_g, hash_key);

        // Is this hash value exist in the Hash table?
        if (lookup_value != NULL) {

            /* printf("==PDC_SERVER: lookup_value not NULL!\n"); */
            // Check if there exist metadata identical to current one
            target = find_identical_metadata(lookup_value, &metadata);
            if (target != NULL) {
                /* printf("==PDC_SERVER: Found delete target!\n"); */

                // Check if target is the only item in this linked list
                /* int curr_list_size; */
                /* DL_COUNT(lookup_value, elt, curr_list_size); */

                /* printf("==PDC_SERVER: still %d objects in current list\n", curr_list_size); */

                /* if (curr_list_size > 1) { */
                if (lookup_value->n_obj > 1) {
                    // Remove from bloom filter
                    if (lookup_value->bloom != NULL) {
                        PDC_Server_remove_from_bloom(target, lookup_value->bloom);
                    }

                    // Remove from linked list
                    DL_DELETE(lookup_value->metadata, target);
                    lookup_value->n_obj--;
                    /* printf("==PDC_SERVER: delete from DL!\n"); */
                }
                else {
                    // Remove from hash
                    /* printf("==PDC_SERVER: delete from hash table!\n"); */
                    hg_hash_table_remove(metadata_hash_table_g, hash_key);

                    // Free this item and delete hash table entry
                    /* if(is_restart_g != 1) { */
                    /*     free(target); */
                    /* } */
                }
                out->ret  = 1;

            } // if (lookup_value != NULL)
            else {
                // Object not found for deletion request
                printf("==PDC_SERVER: delete target not found!\n");
                ret_value = -1;
                out->ret  = -1;
            }

        } // if lookup_value != NULL
        else {
            printf("==PDC_SERVER: delete target not found!\n");
            ret_value = -1;
            out->ret = -1;
        }

    } // if (metadata_hash_table_g != NULL)
    else {
        printf("==PDC_SERVER: metadata_hash_table_g not initialized!\n");
        ret_value = -1;
        out->ret = -1;
        goto done;
    }

#ifdef ENABLE_MULTITHREAD
    // ^ Release hash table lock
    hg_thread_mutex_unlock(&pdc_metadata_hash_table_mutex_g);
    unlocked = 1;
#endif

#ifdef ENABLE_TIMING
    // Timing
    gettimeofday(&ht_total_end, 0);
    ht_total_elapsed    = (ht_total_end.tv_sec-ht_total_start.tv_sec)*1000000LL + ht_total_end.tv_usec-ht_total_start.tv_usec;
    ht_total_sec        = ht_total_elapsed / 1000000.0;
#endif

#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_lock(&pdc_time_mutex_g);
#endif

#ifdef ENABLE_TIMING
    server_delete_time_g += ht_total_sec;
#endif

#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_unlock(&pdc_time_mutex_g);
#endif


    // Decrement total metadata count
#ifdef ENABLE_MULTITHREAD
        hg_thread_mutex_lock(&n_metadata_mutex_g);
#endif
        n_metadata_g-- ;
#ifdef ENABLE_MULTITHREAD
        hg_thread_mutex_unlock(&n_metadata_mutex_g);
#endif

done:
    /* printf("==PDC_SERVER[%d]: Finished delete request: hash=%u, obj_id=%" PRIu64 "\n", pdc_server_rank_g, in->hash_value, in->obj_id); */
    /* fflush(stdout); */
#ifdef ENABLE_MULTITHREAD
    if (unlocked == 0)
        hg_thread_mutex_unlock(&pdc_metadata_hash_table_mutex_g);
#endif
    /* if (hash_key != NULL) */
    /*     free(hash_key); */

    FUNC_LEAVE(ret_value);
} // end of delete_metadata_from_hash_table

/*
 * Insert the metdata received from client to the hash table
 *
 * \param  in[IN]       Input structure received from client, conatins metadata
 * \param  out[OUT]     Output structure to be sent back to the client
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t insert_metadata_to_hash_table(gen_obj_id_in_t *in, gen_obj_id_out_t *out)
{
    perr_t ret_value = SUCCEED;
    pdc_metadata_t *metadata;
    uint32_t *hash_key, i;


    FUNC_ENTER(NULL);

#ifdef ENABLE_TIMING
    // Timing
    struct timeval  ht_total_start;
    struct timeval  ht_total_end;
    long long ht_total_elapsed;
    double ht_total_sec;

    gettimeofday(&ht_total_start, 0);
#endif

    /* printf("Got object creation request with name: %s\tHash=%u\n", in->data.obj_name, in->hash_value); */
    /* printf("Full name check: %s\n", &in->obj_name[507]); */

    metadata = (pdc_metadata_t*)malloc(sizeof(pdc_metadata_t));
    if (metadata == NULL) {
        printf("Cannot allocate pdc_metadata_t!\n");
        goto done;
    }
    total_mem_usage_g += sizeof(pdc_metadata_t);

    PDC_metadata_init(metadata);

    metadata->user_id   = in->data.user_id;
    metadata->time_step = in->data.time_step;
    metadata->ndim      = in->data.ndim;
    metadata->dims[0]   = in->data.dims0;
    metadata->dims[1]   = in->data.dims1;
    metadata->dims[2]   = in->data.dims2;
    metadata->dims[3]   = in->data.dims3;
    for (i = metadata->ndim; i < DIM_MAX; i++)
        metadata->dims[i] = 0;


    strcpy(metadata->obj_name,      in->data.obj_name);
    strcpy(metadata->app_name,      in->data.app_name);
    strcpy(metadata->tags,          in->data.tags);
    strcpy(metadata->data_location, in->data.data_location);

    // DEBUG
    int debug_flag = 0;
    /* PDC_print_metadata(metadata); */

    /* create_time              =; */
    /* last_modified_time       =; */

    hash_key = (uint32_t*)malloc(sizeof(uint32_t));
    if (hash_key == NULL) {
        printf("Cannot allocate hash_key!\n");
        goto done;
    }
    total_mem_usage_g += sizeof(uint32_t);
    *hash_key = in->hash_value;

    pdc_hash_table_entry_head *lookup_value;
    pdc_metadata_t *found_identical;

#ifdef ENABLE_MULTITHREAD
    // Obtain lock for hash table
    int unlocked = 0;
    hg_thread_mutex_lock(&pdc_metadata_hash_table_mutex_g);
#endif

    if (debug_flag == 1)
        printf("checking hash table with key=%d\n", *hash_key);

    if (metadata_hash_table_g != NULL) {
        // lookup
        lookup_value = hg_hash_table_lookup(metadata_hash_table_g, hash_key);

        // Is this hash value exist in the Hash table?
        if (lookup_value != NULL) {

            if (debug_flag == 1)
                printf("lookup_value not NULL!\n");
            // Check if there exist metadata identical to current one
            /* found_identical = NULL; */
            found_identical = find_identical_metadata(lookup_value, metadata);
            if ( found_identical != NULL) {
                printf("==PDC_SERVER[%d]: Found identical metadata with name %s!\n",
                        pdc_server_rank_g, metadata->obj_name);

                if (debug_flag == 1) {
                    /* PDC_print_metadata(metadata); */
                    /* PDC_print_metadata(found_identical); */
                }
                out->obj_id = 0;
                free(metadata);
                goto done;
            }
            else {
                PDC_Server_hash_table_list_insert(lookup_value, metadata);
            }

        }
        else {
            // First entry for current hasy_key, init linked list, and insert to hash table
            if (debug_flag == 1) {
                printf("lookup_value is NULL! Init linked list\n");
            }
            fflush(stdout);

            pdc_hash_table_entry_head *entry = (pdc_hash_table_entry_head*)malloc(sizeof(pdc_hash_table_entry_head));
            entry->bloom    = NULL;
            entry->metadata = NULL;
            entry->n_obj    = 0;
            total_mem_usage_g += sizeof(pdc_hash_table_entry_head);

            PDC_Server_hash_table_list_init(entry, hash_key);
            PDC_Server_hash_table_list_insert(entry, metadata);
        }

    }
    else {
        printf("metadata_hash_table_g not initialized!\n");
        goto done;
    }

    // Generate object id (uint64_t)
    metadata->obj_id = PDC_Server_gen_obj_id();

#ifdef ENABLE_MULTITHREAD
    // ^ Release hash table lock
    hg_thread_mutex_unlock(&pdc_metadata_hash_table_mutex_g);
    unlocked = 1;
#endif


#ifdef ENABLE_MULTITHREAD
        hg_thread_mutex_lock(&n_metadata_mutex_g);
#endif
        n_metadata_g++ ;
#ifdef ENABLE_MULTITHREAD
        hg_thread_mutex_unlock(&n_metadata_mutex_g);
#endif


    // Fill $out structure for returning the generated obj_id to client
    out->obj_id = metadata->obj_id;

    // Debug print metadata info
    /* PDC_print_metadata(metadata); */

#ifdef ENABLE_TIMING
    // Timing
    gettimeofday(&ht_total_end, 0);
    ht_total_elapsed    = (ht_total_end.tv_sec-ht_total_start.tv_sec)*1000000LL + ht_total_end.tv_usec-ht_total_start.tv_usec;
    ht_total_sec        = ht_total_elapsed / 1000000.0;
#endif

#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_lock(&pdc_time_mutex_g);
#endif

#ifdef ENABLE_TIMING
    server_insert_time_g += ht_total_sec;
#endif

#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_unlock(&pdc_time_mutex_g);
#endif


done:
#ifdef ENABLE_MULTITHREAD
    if (unlocked == 0)
        hg_thread_mutex_unlock(&pdc_metadata_hash_table_mutex_g);
#endif
    /* printf("==PDC_SERVER[%d]: inserted name %s hash key %u to hash table\n", pdc_server_rank_g, in->data.obj_name, *hash_key); */
    /* fflush(stdout); */
    /* if (hash_key != NULL) */
    /*     free(hash_key); */
    FUNC_LEAVE(ret_value);
} // end of insert_metadata_to_hash_table

/*
 * Print all existing metadata in the hash table
 *
 * \return Non-negative on success/Negative on failure
 */
static perr_t PDC_Server_print_all_metadata()
{
    perr_t ret_value = SUCCEED;
    hg_hash_table_iter_t hash_table_iter;
    pdc_metadata_t *elt;
    pdc_hash_table_entry_head *head;

    FUNC_ENTER(NULL);

    hg_hash_table_iterate(metadata_hash_table_g, &hash_table_iter);
    while (hg_hash_table_iter_has_more(&hash_table_iter)) {
        head = hg_hash_table_iter_next(&hash_table_iter);
        DL_FOREACH(head->metadata, elt) {
            PDC_print_metadata(elt);
        }
    }

    FUNC_LEAVE(ret_value);
}

/*
 * Check for duplicates in the hash table
 *
 * \return Non-negative on success/Negative on failure
 */
static perr_t PDC_Server_metadata_duplicate_check()
{
    perr_t ret_value = SUCCEED;
    hg_hash_table_iter_t hash_table_iter;
    int n_entry, count = 0;
    int all_maybe, all_total, all_entry;
    int has_dup_obj = 0;
    int all_dup_obj = 0;
    pdc_metadata_t *elt, *elt_next;
    pdc_hash_table_entry_head *head;

    FUNC_ENTER(NULL);

    n_entry = hg_hash_table_num_entries(metadata_hash_table_g);

    #ifdef ENABLE_MPI
        MPI_Reduce(&n_bloom_maybe_g, &all_maybe, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(&n_bloom_total_g, &all_total, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(&n_entry,         &all_entry, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    #else
        all_maybe = n_bloom_maybe_g;
        all_total = n_bloom_total_g;
        all_entry = n_entry;
    #endif

    if (pdc_server_rank_g == 0) {
        printf("==PDC_SERVER: Bloom filter says maybe %d times out of %d\n", all_maybe, all_total);
        printf("==PDC_SERVER: Metadata duplicate check with %d hash entries ", all_entry);
    }

    fflush(stdout);

    hg_hash_table_iterate(metadata_hash_table_g, &hash_table_iter);

    while (n_entry != 0 && hg_hash_table_iter_has_more(&hash_table_iter)) {
        head = hg_hash_table_iter_next(&hash_table_iter);
        /* DL_COUNT(head, elt, dl_count); */
        /* if (pdc_server_rank_g == 0) { */
        /*     printf("  Hash entry[%d], with %d items\n", count, dl_count); */
        /* } */
        DL_SORT(head->metadata, PDC_metadata_cmp);
        // With sorted list, just compare each one with its next
        DL_FOREACH(head->metadata, elt) {
            elt_next = elt->next;
            if (elt_next != NULL) {
                if (PDC_metadata_cmp(elt, elt_next) == 0) {
                    /* PDC_print_metadata(elt); */
                    has_dup_obj = 1;
                    ret_value = FAIL;
                    goto done;
                }
            }
        }
        count++;
    }

    fflush(stdout);

done:
    #ifdef ENABLE_MPI
        MPI_Reduce(&has_dup_obj, &all_dup_obj, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    #else
        all_dup_obj = has_dup_obj;
    #endif
    if (pdc_server_rank_g == 0) {
        if (all_dup_obj > 0) {
            printf("  ...Found duplicates!\n");
        }
        else {
            printf("  ...No duplicates found!\n");
        }
    }

    FUNC_LEAVE(ret_value);
}

/*
 * Callback function of the server to lookup clients, also gets the confirm message from client.
 *
 * \param  callback_info[IN]        Mercury callback info pointer
 *
 * \return Non-negative on success/Negative on failure
 */
static hg_return_t
lookup_remote_server_rpc_cb(const struct hg_cb_info *callback_info)
{
    hg_return_t ret_value = HG_SUCCESS;
    server_lookup_remote_server_out_t output;

    FUNC_ENTER(NULL);

    /* printf("Entered lookup_remote_server_rpc_cb()\n"); */
    /* server_lookup_args_t *lookup_args = (server_lookup_args_t*) callback_info->arg; */
    hg_handle_t handle = callback_info->info.forward.handle;

    /* Get output from server*/
    ret_value = HG_Get_output(handle, &output);
    if (ret_value != HG_SUCCESS) {
        printf("==PDC_SERVER[%d]: lookup_remote_server_rpc_cb() error getting output\n", pdc_server_rank_g);
        goto done;
    }

    /* printf("Return value=%" PRIu64 "\n", output.ret); */

done:
    work_todo_g--;
    HG_Free_output(handle, &output);
    FUNC_LEAVE(ret_value);
}

/*
 * Callback function of the server to lookup other servers via Mercury RPC
 *
 * \param  callback_info[IN]        Mercury callback info pointer
 *
 * \return Non-negative on success/Negative on failure
 */
static hg_return_t
lookup_remote_server_cb(const struct hg_cb_info *callback_info)
{
    hg_return_t ret_value = HG_SUCCESS;
    uint32_t server_id;
    server_lookup_args_t *lookup_args;
    server_lookup_remote_server_in_t in;

    FUNC_ENTER(NULL);

    lookup_args = (server_lookup_args_t*) callback_info->arg;
    server_id = lookup_args->server_id;

    /* printf("lookup_remote_server_cb(): server ID=%d\n", server_id); */
    /* fflush(stdout); */

    pdc_remote_server_info_g[server_id].addr = callback_info->info.lookup.addr;
    pdc_remote_server_info_g[server_id].addr_valid = 1;

    // Create HG handle if needed
    /* if (pdc_remote_server_info_g[server_id].server_lookup_remote_server_handle_valid != 1) { */
        HG_Create(hg_context_g, pdc_remote_server_info_g[server_id].addr,
                  server_lookup_remote_server_register_id_g,
                  &pdc_remote_server_info_g[server_id].server_lookup_remote_server_handle);
        /* pdc_remote_server_info_g[server_id].server_lookup_remote_server_handle_valid= 1; */
    /* } */

    // Fill input structure
    in.server_id = pdc_server_rank_g;

    /* printf("Sending input to target\n"); */
    ret_value = HG_Forward(pdc_remote_server_info_g[server_id].server_lookup_remote_server_handle, lookup_remote_server_rpc_cb, lookup_args, &in);
    if (ret_value != HG_SUCCESS) {
        fprintf(stderr, "lookup_remote_server_cb(): Could not start HG_Forward()\n");
        goto done;
    }

done:
    HG_Destroy(pdc_remote_server_info_g[server_id].server_lookup_remote_server_handle);
    FUNC_LEAVE(ret_value);
}

/*
 * Test connection to other servers
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_lookup_remote_server()
{
    int i;
    perr_t ret_value      = SUCCEED;
    hg_return_t hg_ret    = HG_SUCCESS;
    server_lookup_args_t lookup_args;

    FUNC_ENTER(NULL);

    // Lookup and fill the remote server info
    for (i = 0; i < pdc_server_size_g; i++) {

        if (i == pdc_server_rank_g) continue;

        lookup_args.server_id = pdc_server_rank_g;
        /* printf("==PDC_SERVER[%d]: Testing connection to remote server %d: %s\n", pdc_server_rank_g, i, pdc_remote_server_info_g[i].addr_string); */
        /* fflush(stdout); */

        hg_ret = HG_Addr_lookup(hg_context_g, lookup_remote_server_cb, &lookup_args, pdc_remote_server_info_g[i].addr_string, HG_OP_ID_IGNORE);
        if (hg_ret != HG_SUCCESS ) {
            printf("==PDC_SERVER: Connection to remote server FAILED!\n");
            ret_value = FAIL;
            goto done;
        }

        work_todo_g = 1;
        PDC_Server_check_response(&hg_context_g);
    }

    if (pdc_server_rank_g == 0) {
        printf("==PDC_SERVER[%d]: Successfully established connection to %d other PDC servers\n",
                pdc_server_rank_g, pdc_server_size_g- 1);
        fflush(stdout);
    }

done:
    FUNC_LEAVE(ret_value);
} // PDC_Server_lookup_remote_server

/*
 * Callback function of the server to lookup other servers via Mercury RPC
 *
 * \param  port[IN]        Port number for Mercury to use
 * \param  hg_class[IN]    Pointer to Mercury class
 * \param  hg_context[IN]  Pointer to Mercury context
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_init(int port, hg_class_t **hg_class, hg_context_t **hg_context)
{
    perr_t ret_value = SUCCEED;
    int i = 0;
    char **all_addr_strings = NULL;
    char self_addr_string[ADDR_MAX];
    char na_info_string[ADDR_MAX];
    char hostname[1024];

    FUNC_ENTER(NULL);

    /* PDC_Server_print_version(); */

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

    /* pdc_client_context_g = HG_Context_create(*hg_class); */
    /* if (pdc_client_context_g == NULL) { */
    /*     printf("Error with HG_Context_create()\n"); */
    /*     return FAIL; */
    /* } */


    // Get server address
    PDC_get_self_addr(*hg_class, self_addr_string);
    /* printf("Server address is: %s\n", self_addr_string); */
    /* fflush(stdout); */

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

    // Gather addresses
#ifdef ENABLE_MPI
    MPI_Allgather(self_addr_string, ADDR_MAX, MPI_CHAR, all_addr_strings_1d_g, ADDR_MAX, MPI_CHAR, MPI_COMM_WORLD);
    for (i = 0; i < pdc_server_size_g; i++) {
        all_addr_strings[i] = &all_addr_strings_1d_g[i*ADDR_MAX];
        pdc_remote_server_info_g[i].addr_string = &all_addr_strings_1d_g[i*ADDR_MAX];
        /* printf("==PDC_SERVER[%d]: %s\n", pdc_server_rank_g, all_addr_strings[i]); */
    }
#else
    all_addr_strings[0] = self_addr_string;
#endif
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

#ifdef ENABLE_MULTITHREAD
    // Init threadpool
    char *nthread_env = getenv("PDC_SERVER_NTHREAD");
    int n_thread;
    if (nthread_env != NULL)
        n_thread = atoi(nthread_env);

    if (n_thread < 1)
        n_thread = 2;
    hg_thread_pool_init(n_thread, &hg_test_thread_pool_g);
    if (pdc_server_rank_g == 0) {
        printf("\n==PDC_SERVER[d]: Starting server with %d threads...\n", pdc_server_rank_g, n_thread);
        fflush(stdout);
    }
    hg_thread_mutex_init(&pdc_client_addr_metex_g);
    hg_thread_mutex_init(&pdc_time_mutex_g);
    hg_thread_mutex_init(&pdc_bloom_time_mutex_g);
    hg_thread_mutex_init(&n_metadata_mutex_g);
    hg_thread_mutex_init(&data_read_list_mutex_g);
    hg_thread_mutex_init(&data_write_list_mutex_g);
#else
    if (pdc_server_rank_g == 0) {
        printf("==PDC_SERVER[%d]: without multi-thread!\n", pdc_server_rank_g);
        fflush(stdout);
    }
#endif

    // TODO: support restart with different number of servers than previous run
    char checkpoint_file[ADDR_MAX];
    if (is_restart_g == 1) {
        sprintf(checkpoint_file, "%s%s%d", pdc_server_tmp_dir_g, "metadata_checkpoint.", pdc_server_rank_g);

#ifdef ENABLE_TIMING
        // Timing
        struct timeval  ht_total_start;
        struct timeval  ht_total_end;
        long long ht_total_elapsed;
        double restart_time, all_restart_time;
        gettimeofday(&ht_total_start, 0);
#endif

        ret_value = PDC_Server_restart(checkpoint_file);
        if (ret_value != SUCCEED) {
            printf("==PDC_SERVER[%d]: error with PDC_Server_restart\n", pdc_server_rank_g);
            goto done;
        }
#ifdef ENABLE_TIMING
        // Timing
        gettimeofday(&ht_total_end, 0);
        ht_total_elapsed = (ht_total_end.tv_sec-ht_total_start.tv_sec)*1000000LL + ht_total_end.tv_usec-ht_total_start.tv_usec;
        restart_time = ht_total_elapsed / 1000000.0;

    #ifdef ENABLE_MPI
        MPI_Reduce(&restart_time, &all_restart_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    #else
        all_restart_time = restart_time;
    #endif
        if (pdc_server_rank_g == 0)
            printf("==PDC_SERVER: total restart time = %.6f\n", all_restart_time);
#endif
    }
    else {
        // We are starting a brand new server
        if (is_hash_table_init_g != 1) {
            // Hash table init
            ret_value = PDC_Server_init_hash_table();
            if (ret_value != SUCCEED) {
                printf("==PDC_SERVER[%d]: error with PDC_Server_init_hash_table\n", pdc_server_rank_g);
                goto done;
            }
        }
    }



    // Data server related init
    pdc_data_server_read_list_head_g = NULL;
    pdc_data_server_write_list_head_g = NULL;

    // Initalize atomic variable to finalize server
    hg_atomic_set32(&close_server_g, 0);

    n_metadata_g = 0;

done:
    FUNC_LEAVE(ret_value);
} // PDC_Server_init


perr_t PDC_Server_metadata_index_create(metadata_index_create_in_t *in, metadata_index_create_out_t *out){return SUCCEED;}

/*
 * Destroy the remote server info structures, free the allocated space
 *
 * \param  info[IN]        Pointer to the remote server info structures
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_destroy_remote_server_info(pdc_remote_server_info_t *info)
{
    int i;
    hg_return_t hg_ret;
    perr_t ret_value = SUCCEED;

    FUNC_ENTER(NULL);

    // Destroy addr and handle
    for (i = 0; i <pdc_server_size_g; i++) {
        if (i == pdc_server_rank_g)
            continue;
        /* if (info[i].server_lookup_remote_server_handle_valid == 1) { */

        /*     info[i].server_lookup_remote_server_handle_valid = 0; */
        /*     hg_ret = HG_Destroy(info[i].server_lookup_remote_server_handle); */
        /*     if (hg_ret != HG_SUCCESS) { */
        /*         printf("==PDC_SERVER: PDC_Server_destroy_remote_server_info() error with HG_Destroy\n"); */
        /*         ret_value = FAIL; */
        /*         goto done; */
        /*     } */
        /* } */
        /* if (info[i].update_region_loc_handle_valid == 1) { */

        /*     info[i].update_region_loc_handle_valid = 0; */
        /*     hg_ret = HG_Destroy(info[i].update_region_loc_handle); */
        /*     if (hg_ret != HG_SUCCESS) { */
        /*         printf("==PDC_SERVER: PDC_Server_destroy_remote_server_info() error with HG_Destroy\n"); */
        /*         ret_value = FAIL; */
        /*         goto done; */
        /*     } */
        /* } */
        /* if (info[i].get_metadata_by_id_handle_valid == 1) { */
        /*     info[i].get_metadata_by_id_handle= 0; */
        /*     hg_ret = HG_Destroy(info[i].get_metadata_by_id_handle); */
        /*     if (hg_ret != HG_SUCCESS) { */
        /*         printf("==PDC_SERVER: PDC_Server_destroy_remote_server_info() error with HG_Destroy\n"); */
        /*         ret_value = FAIL; */
        /*         goto done; */
        /*     } */
        /* } */
        /* if (info[i].get_storage_info_handle_valid == 1) { */

        /*     info[i].get_storage_info_handle= 0; */
        /*     hg_ret = HG_Destroy(info[i].get_storage_info_handle); */
        /*     if (hg_ret != HG_SUCCESS) { */
        /*         printf("==PDC_SERVER: PDC_Server_destroy_remote_server_info() error with HG_Destroy\n"); */
        /*         ret_value = FAIL; */
        /*         goto done; */
        /*     } */
        /* } */

        if (info[i].addr_valid == 1) {
            info[i].addr_valid = 0;
            hg_ret = HG_Addr_free(hg_class_g, info[i].addr);
            if (hg_ret != HG_SUCCESS) {
                printf("==PDC_SERVER: PDC_Server_destroy_remote_server_info() error with HG_Addr_free\n");
                ret_value = FAIL;
                goto done;
            }
        }

        /* if (info[i].notify_region_update_handle_valid == 1) { */
        /*     info[i].notify_region_update_handle_valid = 0; */
        /*     HG_Destroy(info[i].notify_region_update_handle); */
        /* } */
    }

done:
    FUNC_LEAVE(ret_value);
} // PDC_Server_destroy_remote_server_info

/*
 * Finalize the server, free allocated spaces
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_finalize()
{
    perr_t ret_value = SUCCEED;

    FUNC_ENTER(NULL);

    // Debug: print all metadata
    /* PDC_Server_print_all_metadata(); */

    // Debug: check duplicates
    /* PDC_Server_metadata_duplicate_check(); */
    /* fflush(stdout); */

    // Free hash table
    if(metadata_hash_table_g != NULL)
        hg_hash_table_free(metadata_hash_table_g);

/* printf("hash table freed!\n"); */
/* fflush(stdout); */

    ret_value = PDC_Server_destroy_client_info(pdc_client_info_g);
    if (ret_value != SUCCEED) {
        printf("==PDC_SERVER: Error with PDC_Server_destroy_client_info\n");
        goto done;
    }

/* printf("client info destroyed!\n"); */
/* fflush(stdout); */

    ret_value = PDC_Server_destroy_remote_server_info(pdc_remote_server_info_g);
    if (ret_value != SUCCEED) {
        printf("==PDC_SERVER: Error with PDC_Server_destroy_client_info\n");
        goto done;
    }

/* printf("server info destroyed!"\n); */
/* fflush(stdout); */

/*     if(metadata_name_mark_hash_table_g != NULL) */
/*         hg_hash_table_free(metadata_name_mark_hash_table_g); */
#ifdef ENABLE_TIMING

    double all_bloom_check_time_max, all_bloom_check_time_min, all_insert_time_max, all_insert_time_min;
    double all_server_bloom_init_time_min,  all_server_bloom_init_time_max;
    double all_server_bloom_insert_time_min,  all_server_bloom_insert_time_max;
    double all_server_hash_insert_time_min, all_server_hash_insert_time_max;

    #ifdef ENABLE_MPI
    MPI_Reduce(&server_bloom_check_time_g, &all_bloom_check_time_max,        1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&server_bloom_check_time_g, &all_bloom_check_time_min,        1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&server_insert_time_g,      &all_insert_time_max,             1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&server_insert_time_g,      &all_insert_time_min,             1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);

    MPI_Reduce(&server_bloom_init_time_g,  &all_server_bloom_init_time_max,  1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&server_bloom_init_time_g,  &all_server_bloom_init_time_min,  1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&server_hash_insert_time_g, &all_server_hash_insert_time_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&server_hash_insert_time_g, &all_server_hash_insert_time_min, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);

    #else
    all_bloom_check_time_min        = server_bloom_check_time_g;
    all_bloom_check_time_max        = server_bloom_check_time_g;
    all_insert_time_max             = server_insert_time_g;
    all_insert_time_min             = server_insert_time_g;
    all_server_bloom_init_time_min  = server_bloom_init_time_g;
    all_server_bloom_init_time_max  = server_bloom_init_time_g;
    all_server_hash_insert_time_max = server_hash_insert_time_g;
    all_server_hash_insert_time_min = server_hash_insert_time_g;
    #endif
    if (pdc_server_rank_g == 0) {
        printf("==PDC_SERVER: total bloom check time = %.6f, %.6f\n", all_bloom_check_time_min, all_bloom_check_time_max);
        printf("==PDC_SERVER: total insert      time = %.6f, %.6f\n", all_insert_time_min, all_insert_time_max);
        printf("==PDC_SERVER: total hash insert time = %.6f, %.6f\n", all_server_hash_insert_time_min, all_server_hash_insert_time_max);
        printf("==PDC_SERVER: total bloom init  time = %.6f, %.6f\n", all_server_bloom_init_time_min, all_server_bloom_init_time_max);
        printf("==PDC_SERVER: total memory usage     = %.2f MB\n", total_mem_usage_g/1048576.0);
        fflush(stdout);
    }
    // TODO: remove server tmp dir?

#endif

#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_destroy(&pdc_time_mutex_g      );
    hg_thread_mutex_destroy(&pdc_client_addr_metex_g);
    hg_thread_mutex_destroy(&pdc_bloom_time_mutex_g);
    hg_thread_mutex_destroy(&n_metadata_mutex_g    );
    hg_thread_mutex_destroy(&data_read_list_mutex_g       );
    hg_thread_mutex_destroy(&data_write_list_mutex_g       );
#endif

done:
    free(all_addr_strings_1d_g);
    FUNC_LEAVE(ret_value);
}

#ifdef ENABLE_MULTITHREAD

/*
 * Multi-thread Mercury progess
 *
 * \return Non-negative on success/Negative on failure
 */
static HG_THREAD_RETURN_TYPE
hg_progress_thread(void *arg)
{
    /* pthread_t tid = pthread_self(); */
    /* pid_t tid; */
    /* tid = syscall(SYS_gettid); */
    hg_context_t *context = (hg_context_t*)arg;
    HG_THREAD_RETURN_TYPE tret = (HG_THREAD_RETURN_TYPE) 0;
    hg_return_t ret = HG_SUCCESS;

    FUNC_ENTER(NULL);

    do {
        if (hg_atomic_get32(&close_server_g)) break;

        ret = HG_Progress(context, 100);
        /* printf("thread [%d]\n", tid); */
    } while (ret == HG_SUCCESS || ret == HG_TIMEOUT);

    hg_thread_exit(tret);

    return tret;
}
#endif

/*
 * Checkpoint in-memory metadata to persistant storage, each server writes to one file
 *
 * \param  filename[IN]     Checkpoint file name
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_checkpoint(char *filename)
{
    perr_t ret_value = SUCCEED;
    pdc_metadata_t *elt;
    region_list_t  *region_elt;
    pdc_hash_table_entry_head *head;
    int n_entry, checkpoint_count = 0, n_region;
    uint32_t hash_key;

    FUNC_ENTER(NULL);

    if (pdc_server_rank_g == 0) {
        printf("\n\n==PDC_SERVER: Start checkpoint process [%s]\n", filename);
    }

    FILE *file = fopen(filename, "w+");
    if (file==NULL) {
        printf("==PDC_SERVER: PDC_Server_checkpoint() - Checkpoint file open error");
        ret_value = FAIL;
        goto done;
    }

    // DHT
    n_entry = hg_hash_table_num_entries(metadata_hash_table_g);
    /* printf("%d entries\n", n_entry); */
    fwrite(&n_entry, sizeof(int), 1, file);

    hg_hash_table_iter_t hash_table_iter;
    hg_hash_table_iterate(metadata_hash_table_g, &hash_table_iter);

    while (n_entry != 0 && hg_hash_table_iter_has_more(&hash_table_iter)) {
        head = hg_hash_table_iter_next(&hash_table_iter);
        /* printf("count=%d\n", head->n_obj); */
        /* fflush(stdout); */

        fwrite(&head->n_obj, sizeof(int), 1, file);
        // TODO: find a way to get hash_key from hash table istead of calculating again.
        hash_key = PDC_get_hash_by_name(head->metadata->obj_name);
        fwrite(&hash_key, sizeof(uint32_t), 1, file);
        // Iterate every metadata structure in current entry
        DL_FOREACH(head->metadata, elt) {
            /* printf("==PDC_SERVER: Writing one metadata...\n"); */
            /* PDC_print_metadata(elt); */
            fwrite(elt, sizeof(pdc_metadata_t), 1, file);

            // Write region info
            DL_COUNT(elt->storage_region_list_head, region_elt, n_region);
            fwrite(&n_region, sizeof(int), 1, file);
            DL_FOREACH(elt->storage_region_list_head, region_elt) {
                fwrite(region_elt, sizeof(region_list_t), 1, file);
            }

            checkpoint_count++;
        }
    }

    fclose(file);
    file = NULL;

    int all_checkpoint_count;
#ifdef ENABLE_MPI
    MPI_Reduce(&checkpoint_count, &all_checkpoint_count, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
#else
    all_checkpoint_count = checkpoint_count;
#endif
    if (pdc_server_rank_g == 0) {
        printf("==PDC_SERVER: checkpointed %d objects\n", all_checkpoint_count);
    }

done:
    FUNC_LEAVE(ret_value);
}

/*
 * Load metadata from checkpoint file in persistant storage
 *
 * \param  filename[IN]     Checkpoint file name
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_restart(char *filename)
{
    perr_t ret_value = 1;
    int n_entry, count, i, j, nobj = 0, all_nobj = 0, n_region;
    pdc_metadata_t *metadata, *elt;
    region_list_t *region_list;
    pdc_hash_table_entry_head *entry;
    uint32_t *hash_key;

    FUNC_ENTER(NULL);

    FILE *file = fopen(filename, "r");
    if (file==NULL) {
        printf("==PDC_SERVER: PDC_Server_checkpoint() - Checkpoint file open error");
        ret_value = FAIL;
        goto done;
    }

    // init hash table
    PDC_Server_init_hash_table();
    fread(&n_entry, sizeof(int), 1, file);
    /* printf("%d entries\n", n_entry); */

    while (n_entry>0) {
        fread(&count, sizeof(int), 1, file);
        /* printf("Count:%d\n", count); */

        hash_key = (uint32_t *)malloc(sizeof(uint32_t));
        fread(hash_key, sizeof(uint32_t), 1, file);
        /* printf("Hash key is %u\n", *hash_key); */
        total_mem_usage_g += sizeof(uint32_t);

        // Reconstruct hash table
        entry = (pdc_hash_table_entry_head*)malloc(sizeof(pdc_hash_table_entry_head));
        entry->n_obj = 0;
        entry->bloom = NULL;
        entry->metadata = NULL;
        // Init hash table metadata (w/ bloom) with first obj
        PDC_Server_hash_table_list_init(entry, hash_key);


        metadata = (pdc_metadata_t*)calloc(sizeof(pdc_metadata_t), count);

        // TODO: test
        for (i = 0; i < count; i++) {
            fread(metadata+i, sizeof(pdc_metadata_t), 1, file);

            (metadata+i)->storage_region_list_head = NULL;
            (metadata+i)->region_lock_head         = NULL;
            (metadata+i)->region_map_head          = NULL;
            (metadata+i)->bloom                    = NULL;
            (metadata+i)->prev                     = NULL;
            (metadata+i)->next                     = NULL;

            fread(&n_region, sizeof(int), 1, file);
            if (n_region < 0) {
                printf("==PDC_SERVER: PDC_Server_restart - error with checkpoint file region number\n");
                ret_value = FAIL;
                goto done;
            }
            region_list = (region_list_t*)calloc(sizeof(region_list_t), n_region);

            fread(region_list, sizeof(region_list_t), n_region, file);

            for (j = 0; j < n_region; j++) {
                (region_list+j)->buf           = NULL;
                (region_list+j)->is_data_ready = 0;
                (region_list+j)->shm_fd        = 0;
                (region_list+j)->meta          = (metadata+i);
                (region_list+j)->prev          = NULL;
                (region_list+j)->next          = NULL;
                memset((region_list+j)->shm_addr, 0, ADDR_MAX);

                DL_APPEND((metadata+i)->storage_region_list_head, region_list+j);
            }
        }

        nobj += count;
        total_mem_usage_g += sizeof(pdc_hash_table_entry_head);
        total_mem_usage_g += (sizeof(pdc_metadata_t)*count);

        // Debug print for loaded metadata from checkpoint file
        /* for (i = 0; i < count; i++) { */
        /*     elt = metadata + i; */
        /*     PDC_print_metadata(elt); */
        /* } */


        entry->metadata = NULL;

        // Insert the rest objs to the linked list
        for (i = 0; i < count; i++) {
            elt = metadata + i;
            // Add to hash list and bloom filter
            ret_value = PDC_Server_hash_table_list_insert(entry, elt);
            if (ret_value != SUCCEED) {
                printf("==PDC_SERVER: error with hash table recovering from checkpoint file\n");
                goto done;
            }
        }
        n_entry--;

        /* free(metadata); */
    }

    fclose(file);
    file = NULL;

#ifdef ENABLE_MPI
    MPI_Reduce(&nobj, &all_nobj, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
#else
    all_nobj = nobj;
#endif
    if (pdc_server_rank_g == 0) {
        printf("==PDC_SERVER: Server restarted from saved session, successfully loaded %d objects...\n", all_nobj);
    }

    // debug
    /* PDC_Server_print_all_metadata(); */

done:
    /* if (hash_key != NULL) */
    /*     free(hash_key); */
    FUNC_LEAVE(ret_value);
}

#ifdef ENABLE_MULTITHREAD

/*
 * Multithread Mercury server to trigger and progress
 *
 * \return Non-negative on success/Negative on failure
 */
static perr_t PDC_Server_multithread_loop(hg_context_t *context)
{
    perr_t ret_value = SUCCEED;
    hg_thread_t progress_thread;
    hg_return_t ret = HG_SUCCESS;

    FUNC_ENTER(NULL);

    hg_thread_create(&progress_thread, hg_progress_thread, context);

    do {
        if (hg_atomic_get32(&close_server_g)) break;

        ret = HG_Trigger(context, 0, 1, NULL);
    } while (ret == HG_SUCCESS || ret == HG_TIMEOUT);

    hg_thread_join(progress_thread);

    // Destory pool
    hg_thread_pool_destroy(hg_test_thread_pool_g);
    /* hg_thread_mutex_destroy(&close_server_g); */

    FUNC_LEAVE(ret_value);
}
#endif

/*
 * Single-thread Mercury server to trigger and progress
 *
 * \return Non-negative on success/Negative on failure
 */
static perr_t PDC_Server_loop(hg_context_t *hg_context)
{
    perr_t ret_value = SUCCEED;;
    hg_return_t hg_ret;
    unsigned int actual_count;

    FUNC_ENTER(NULL);

    /* Poke progress engine and check for events */
    do {
        actual_count = 0;
        do {
            /* hg_ret = HG_Trigger(hg_context, 1024/1* timeout *1/, 4096/1* max count *1/, &actual_count); */
/* printf("==PDC_SERVER[%d]: before HG_Trigger()\n", pdc_server_rank_g); */
/* fflush(stdout); */
            hg_ret = HG_Trigger(hg_context, 0/* timeout */, 1 /* max count */, &actual_count);
/* printf("==PDC_SERVER[%d]: after HG_Trigger()\n", pdc_server_rank_g); */
/* fflush(stdout); */
        } while ((hg_ret == HG_SUCCESS) && actual_count);

        /* Do not try to make progress anymore if we're done */
        if (hg_atomic_cas32(&close_server_g, 1, 1)) break;
/* printf("==PDC_SERVER[%d]: before HG_Progress()\n", pdc_server_rank_g); */
/* fflush(stdout); */
        hg_ret = HG_Progress(hg_context, HG_MAX_IDLE_TIME);
        /* hg_ret = HG_Progress(hg_context, 1000); */
/* printf("==PDC_SERVER[%d]: after HG_Progress()\n", pdc_server_rank_g); */
/* fflush(stdout); */

    } while (hg_ret == HG_SUCCESS || close_server_g != 1);

    if (hg_ret == HG_SUCCESS)
        ret_value = SUCCEED;
    else
        ret_value = FAIL;

    FUNC_LEAVE(ret_value);
}

/* For 1D boxes (intervals) we have: */
/* box1 = (xmin1, xmax1) */
/* box2 = (xmin2, xmax2) */
/* overlapping1D(box1,box2) = xmax1 >= xmin2 and xmax2 >= xmin1 */

/* For 2D boxes (rectangles) we have: */
/* box1 = (x:(xmin1,xmax1),y:(ymin1,ymax1)) */
/* box2 = (x:(xmin2,xmax2),y:(ymin2,ymax2)) */
/* overlapping2D(box1,box2) = overlapping1D(box1.x, box2.x) and */
/*                            overlapping1D(box1.y, box2.y) */

/* For 3D boxes we have: */
/* box1 = (x:(xmin1,xmax1),y:(ymin1,ymax1),z:(zmin1,zmax1)) */
/* box2 = (x:(xmin2,xmax2),y:(ymin2,ymax2),z:(zmin2,zmax2)) */
/* overlapping3D(box1,box2) = overlapping1D(box1.x, box2.x) and */
/*                            overlapping1D(box1.y, box2.y) and */
/*                            overlapping1D(box1.z, box2.z) */

/*
 * Check if two 1D segments overlaps
 *
 * \param  xmin1[IN]        Start offset of first segment
 * \param  xmax1[IN]        End offset of first segment
 * \param  xmin2[IN]        Start offset of second segment
 * \param  xmax2[IN]        End offset of second segment
 *
 * \return 1 if they overlap/-1 otherwise
 */
static int is_overlap_1D(uint64_t xmin1, uint64_t xmax1, uint64_t xmin2, uint64_t xmax2)
{
    int ret_value = -1;

    if (xmax1 >= xmin2 && xmax2 >= xmin1) {
        ret_value = 1;
    }

    return ret_value;
}

/*
 * Check if two 2D box overlaps
 *
 * \param  xmin1[IN]        Start offset (x-axis) of first  box
 * \param  xmax1[IN]        End   offset (x-axis) of first  box
 * \param  ymin1[IN]        Start offset (y-axis) of first  box
 * \param  ymax1[IN]        End   offset (y-axis) of first  box
 * \param  xmin2[IN]        Start offset (x-axis) of second box
 * \param  xmax2[IN]        End   offset (x-axis) of second box
 * \param  ymin2[IN]        Start offset (y-axis) of second box
 * \param  ymax2[IN]        End   offset (y-axis) of second box
 *
 * \return 1 if they overlap/-1 otherwise
 */
static int is_overlap_2D(uint64_t xmin1, uint64_t xmax1, uint64_t ymin1, uint64_t ymax1,
                         uint64_t xmin2, uint64_t xmax2, uint64_t ymin2, uint64_t ymax2)
{
    int ret_value = -1;

    /* if (is_overlap_1D(box1.x, box2.x) == 1 && is_overlap_1D(box1.y, box2.y) == 1) { */
    if (is_overlap_1D(xmin1, xmax1, xmin2, xmax2 ) == 1 &&
        is_overlap_1D(ymin1, ymax1, ymin2, ymax2) == 1) {
        ret_value = 1;
    }

    return ret_value;
}

/*
 * Check if two 3D box overlaps
 *
 * \param  xmin1[IN]        Start offset (x-axis) of first  box
 * \param  xmax1[IN]        End   offset (x-axis) of first  box
 * \param  ymin1[IN]        Start offset (y-axis) of first  box
 * \param  ymax1[IN]        End   offset (y-axis) of first  box
 * \param  zmin2[IN]        Start offset (z-axis) of first  box
 * \param  zmax2[IN]        End   offset (z-axis) of first  box
 * \param  xmin2[IN]        Start offset (x-axis) of second box
 * \param  xmax2[IN]        End   offset (x-axis) of second box
 * \param  ymin2[IN]        Start offset (y-axis) of second box
 * \param  ymax2[IN]        End   offset (y-axis) of second box
 * \param  zmin2[IN]        Start offset (z-axis) of second box
 * \param  zmax2[IN]        End   offset (z-axis) of second box
 *
 * \return 1 if they overlap/-1 otherwise
 */
static int is_overlap_3D(uint64_t xmin1, uint64_t xmax1, uint64_t ymin1, uint64_t ymax1, uint64_t zmin1, uint64_t zmax1,
                         uint64_t xmin2, uint64_t xmax2, uint64_t ymin2, uint64_t ymax2, uint64_t zmin2, uint64_t zmax2)
{
    int ret_value = -1;

    /* if (is_overlap_1D(box1.x, box2.x) == 1 && is_overlap_1D(box1.y, box2.y) == 1) { */
    if (is_overlap_1D(xmin1, xmax1, xmin2, xmax2) == 1 &&
        is_overlap_1D(ymin1, ymax1, ymin2, ymax2) == 1 &&
        is_overlap_1D(zmin1, zmax1, zmin2, zmax2) == 1 )
    {
        ret_value = 1;
    }

    return ret_value;
}

/* static int is_overlap_4D(uint64_t xmin1, uint64_t xmax1, uint64_t ymin1, uint64_t ymax1, uint64_t zmin1, uint64_t zmax1, */
/*                          uint64_t mmin1, uint64_t mmax1, */
/*                          uint64_t xmin2, uint64_t xmax2, uint64_t ymin2, uint64_t ymax2, uint64_t zmin2, uint64_t zmax2, */
/*                          uint64_t mmin2, uint64_t mmax2 ) */
/* { */
/*     int ret_value = -1; */

/*     /1* if (is_overlap_1D(box1.x, box2.x) == 1 && is_overlap_1D(box1.y, box2.y) == 1) { *1/ */
/*     if (is_overlap_1D(xmin1, xmax1, xmin2, xmax2) == 1 && */
/*         is_overlap_1D(ymin1, ymax1, ymin2, ymax2) == 1 && */
/*         is_overlap_1D(zmin1, zmax1, zmin2, zmax2) == 1 && */
/*         is_overlap_1D(mmin1, mmax1, mmin2, mmax2) == 1 ) */
/*     { */
/*         ret_value = 1; */
/*     } */

/*     return ret_value; */
/* } */

/*
 * Check if two regions overlap
 *
 * \param  a[IN]     Pointer to first region
 * \param  b[IN]     Pointer to second region
 *
 * \return 1 if they overlap/-1 otherwise
 */
static int is_contiguous_region_overlap(region_list_t *a, region_list_t *b)
{
    int ret_value = 1;

    if (a == NULL || b == NULL) {
        printf("==PDC_SERVER: is_region_identical() - passed NULL value!\n");
        ret_value = -1;
        goto done;
    }

    /* printf("==PDC_SERVER: is_contiguous_region_overlap adim=%d, bdim=%d\n", a->ndim, b->ndim); */
    if (a->ndim != b->ndim || a->ndim <= 0 || b->ndim <= 0) {
        ret_value = -1;
        goto done;
    }

    uint64_t xmin1 = 0, xmin2 = 0, xmax1 = 0, xmax2 = 0;
    uint64_t ymin1 = 0, ymin2 = 0, ymax1 = 0, ymax2 = 0;
    uint64_t zmin1 = 0, zmin2 = 0, zmax1 = 0, zmax2 = 0;
    /* uint64_t mmin1 = 0, mmin2 = 0, mmax1 = 0, mmax2 = 0; */

    if (a->ndim >= 1) {
        xmin1 = a->start[0];
        xmax1 = a->start[0] + a->count[0] - 1;
        xmin2 = b->start[0];
        xmax2 = b->start[0] + b->count[0] - 1;
        /* printf("xmin1, xmax1, xmin2, xmax2: %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n", xmin1, xmax1, xmin2, xmax2); */
    }
    if (a->ndim >= 2) {
        ymin1 = a->start[1];
        ymax1 = a->start[1] + a->count[1] - 1;
        ymin2 = b->start[1];
        ymax2 = b->start[1] + b->count[1] - 1;
        /* printf("ymin1, ymax1, ymin2, ymax2: %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n", ymin1, ymax1, ymin2, ymax2); */
    }
    if (a->ndim >= 3) {
        zmin1 = a->start[2];
        zmax1 = a->start[2] + a->count[2] - 1;
        zmin2 = b->start[2];
        zmax2 = b->start[2] + b->count[2] - 1;
        /* printf("zmin1, zmax1, zmin2, zmax2: %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n", zmin1, zmax1, zmin2, zmax2); */
    }
    /* if (a->ndim >= 4) { */
    /*     mmin1 = a->start[3]; */
    /*     mmax1 = a->start[3] + a->count[3] - 1; */
    /*     mmin2 = b->start[3]; */
    /*     mmax2 = b->start[3] + b->count[3] - 1; */
    /* } */

    if (a->ndim == 1) {
        ret_value = is_overlap_1D(xmin1, xmax1, xmin2, xmax2);
    }
    else if (a->ndim == 2) {
        ret_value = is_overlap_2D(xmin1, xmax1, ymin1, ymax1, xmin2, xmax2, ymin2, ymax2);
    }
    else if (a->ndim == 3) {
        ret_value = is_overlap_3D(xmin1, xmax1, ymin1, ymax1, zmin1, zmax1, xmin2, xmax2, ymin2, ymax2, zmin2, zmax2);
    }
    /* else if (a->ndim == 4) { */
    /*     ret_value = is_overlap_4D(xmin1, xmax1, ymin1, ymax1, zmin1, zmax1, mmin1, mmax1, xmin2, xmax2, ymin2, ymax2, zmin2, zmax2, mmin2, mmax2); */
    /* } */

done:
    /* printf("is overlap: %d\n", ret_value); */
    FUNC_LEAVE(ret_value);
}

/*
 * Check if two regions overlap
 *
 * \param  ndim[IN]        Dimension of the two region
 * \param  a_start[IN]     Start offsets of the the first region
 * \param  a_count[IN]     Size of the the first region
 * \param  b_start[IN]     Start offsets of the the second region
 * \param  b_count[IN]     Size of the the second region
 *
 * \return 1 if they overlap/-1 otherwise
 */
static int is_contiguous_start_count_overlap(uint32_t ndim, uint64_t *a_start, uint64_t *a_count, uint64_t *b_start, uint64_t *b_count)
{
    int ret_value = 1;

    if (ndim > DIM_MAX || NULL == a_start || NULL == a_count ||NULL == b_start ||NULL == b_count) {
        printf("is_contiguous_start_count_overlap: invalid input !\n");
        ret_value = -1;
        goto done;
    }

    uint64_t xmin1 = 0, xmin2 = 0, xmax1 = 0, xmax2 = 0;
    uint64_t ymin1 = 0, ymin2 = 0, ymax1 = 0, ymax2 = 0;
    uint64_t zmin1 = 0, zmin2 = 0, zmax1 = 0, zmax2 = 0;
    /* uint64_t mmin1 = 0, mmin2 = 0, mmax1 = 0, mmax2 = 0; */

    if (ndim >= 1) {
        xmin1 = a_start[0];
        xmax1 = a_start[0] + a_count[0] - 1;
        xmin2 = b_start[0];
        xmax2 = b_start[0] + b_count[0] - 1;
        /* printf("xmin1, xmax1, xmin2, xmax2: %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n", xmin1, xmax1, xmin2, xmax2); */
    }
    if (ndim >= 2) {
        ymin1 = a_start[1];
        ymax1 = a_start[1] + a_count[1] - 1;
        ymin2 = b_start[1];
        ymax2 = b_start[1] + b_count[1] - 1;
        /* printf("ymin1, ymax1, ymin2, ymax2: %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n", ymin1, ymax1, ymin2, ymax2); */
    }
    if (ndim >= 3) {
        zmin1 = a_start[2];
        zmax1 = a_start[2] + a_count[2] - 1;
        zmin2 = b_start[2];
        zmax2 = b_start[2] + b_count[2] - 1;
        /* printf("zmin1, zmax1, zmin2, zmax2: %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n", zmin1, zmax1, zmin2, zmax2); */
    }
    /* if (ndim >= 4) { */
    /*     mmin1 = a_start[3]; */
    /*     mmax1 = a_start[3] + a_count[3] - 1; */
    /*     mmin2 = b_start[3]; */
    /*     mmax2 = b_start[3] + b_count[3] - 1; */
    /* } */

    if (ndim == 1)
        ret_value = is_overlap_1D(xmin1, xmax1, xmin2, xmax2);
    else if (ndim == 2)
        ret_value = is_overlap_2D(xmin1, xmax1, ymin1, ymax1,
                                  xmin2, xmax2, ymin2, ymax2);
    else if (ndim == 3)
        ret_value = is_overlap_3D(xmin1, xmax1, ymin1, ymax1, zmin1, zmax1,
                                  xmin2, xmax2, ymin2, ymax2, zmin2, zmax2);
    /* else if (ndim == 4) */
    /*     ret_value = is_overlap_4D(xmin1, xmax1, ymin1, ymax1, zmin1, zmax1, mmin1, mmax1, */
    /*                               xmin2, xmax2, ymin2, ymax2, zmin2, zmax2, mmin2, mmax2); */

done:
    FUNC_LEAVE(ret_value);
}

/*
 * Get the overlapping region's information of two regions
 *
 * \param  ndim[IN]             Dimension of the two region
 * \param  start1[IN]           Start offsets of the the first region
 * \param  count1[IN]           Sizes of the the first region
 * \param  start2[IN]           Start offsets of the the second region
 * \param  count2[IN]           Sizes of the the second region
 * \param  overlap_start[IN]    Start offsets of the the overlapping region
 * \param  overlap_size[IN]     Sizes of the the overlapping region
 *
 * \return Non-negative on success/Negative on failure
 */
static perr_t get_overlap_start_count(uint32_t ndim, uint64_t *start1, uint64_t *count1,
                                                     uint64_t *start2, uint64_t *count2,
                                       uint64_t *overlap_start, uint64_t *overlap_count)
{
    perr_t ret_value = SUCCEED;
    uint64_t i;

    if (NULL == start1 || NULL == count1 || NULL == start2 || NULL == count2 ||
            NULL == overlap_start || NULL == overlap_count) {

        printf("get_overlap NULL input!\n");
        ret_value = FAIL;
        return ret_value;
    }

    // Check if they are truly overlapping regions
    if (is_contiguous_start_count_overlap(ndim, start1, count1, start2, count2) != 1) {
        printf("get_overlap_start_count: non-overlap regions!\n");
        for (i = 0; i < ndim; i++) {
            printf("dim%" PRIu64 " - start1: %" PRIu64 " count1: %" PRIu64 ", start2:%" PRIu64 " count2:%" PRIu64 "\n",
                    i, start1[i], count1[i], start2[i], count2[i]);
        }
        ret_value = FAIL;
        goto done;
    }

    for (i = 0; i < ndim; i++) {
        overlap_start[i] = PDC_MAX(start1[i], start2[i]);
        /* end = max(xmax2, xmax1); */
        overlap_count[i] = PDC_MIN(start1[i]+count1[i], start2[i]+count2[i]) - overlap_start[i];
    }

done:
    if (ret_value == FAIL) {
        for (i = 0; i < ndim; i++) {
            overlap_start[i] = 0;
            overlap_count[i] = 0;
        }
    }
    return ret_value;
}

/*
 * Check if two regions are the same
 *
 * \param  a[IN]     Pointer to the first region
 * \param  b[IN]     Pointer to the second region
 *
 * \return 1 if they are the same/-1 otherwise
 */
static int is_region_identical(region_list_t *a, region_list_t *b)
{
    int ret_value = -1;
    uint32_t i;

    FUNC_ENTER(NULL);

    if (a == NULL || b == NULL) {
        printf("==PDC_SERVER: is_region_identical() - passed NULL value!\n");
        ret_value = -1;
        goto done;
    }

    if (a->ndim != b->ndim) {
        ret_value = -1;
        goto done;
    }

    for (i = 0; i < a->ndim; i++) {
        if (a->start[i] != b->start[i] || a->count[i] != b->count[i] ) {
        /* if (a->start[i] != b->start[i] || a->count[i] != b->count[i] || a->stride[i] != b->stride[i] ) { */
            ret_value = -1;
            goto done;
        }
    }

    ret_value = 1;

done:
    FUNC_LEAVE(ret_value);
}

//perr_t PDC_Server_region_lock_status(pdcid_t obj_id, region_info_transfer_t *region, int *lock_status)
perr_t PDC_Server_region_lock_status(PDC_mapping_info_t *mapped_region, int *lock_status)
{
    perr_t ret_value = SUCCEED;
    pdc_metadata_t *res_meta;
    region_list_t *elt, *request_region;

    *lock_status = 0;
    request_region = (region_list_t *)malloc(sizeof(region_list_t));
    pdc_region_transfer_t_to_list_t(&(mapped_region->remote_region), request_region);
    //PDC_Server_get_metadata_by_id(obj_id, res_meta);
    res_meta = find_metadata_by_id(mapped_region->remote_obj_id);
    /*
    printf("requested region: \n");
    printf("offset is %lld, %lld\n", (request_region->start)[0], (request_region->start)[1]);
    printf("size is %lld, %lld\n", (request_region->count)[0], (request_region->count)[1]);
    */
    // iterate the target metadata's region_lock_head (linked list) to search for queried region
    DL_FOREACH(res_meta->region_lock_head, elt) {
        /*
        printf("region in lock list: \n");
        printf("offset is %lld, %lld\n", (elt->start)[0], (elt->start)[1]);
        printf("size is %lld, %lld\n", (elt->count)[0], (elt->count)[1]);
        */
        if (is_region_identical(request_region, elt) == 1) {
            *lock_status = 1;
            elt->reg_dirty = 1;
            elt->bulk_handle = mapped_region->remote_bulk_handle;
            elt->addr = mapped_region->remote_addr;
            elt->from_obj_id = mapped_region->from_obj_id;
            elt->obj_id = mapped_region->remote_obj_id;
            elt->reg_id = mapped_region->remote_reg_id;
            elt->client_id = mapped_region->remote_client_id;
        }
    }

    FUNC_LEAVE(ret_value);
}

/*
 * Lock a reigon.
 *
 * \param  in[IN]       Lock region information received from the client
 * \param  out[OUT]     Output stucture to be sent back to the client
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_region_lock(region_lock_in_t *in, region_lock_out_t *out)
{
    perr_t ret_value;
    uint64_t target_obj_id;
    int ndim;
//    int lock_op;
    region_list_t *request_region;
    pdc_metadata_t *target_obj;
    region_list_t *elt, *tmp;

    FUNC_ENTER(NULL);

    /* printf("==PDC_SERVER: received lock request,                                \ */
    /*         obj_id=%" PRIu64 ", op=%d, ndim=%d, start=%" PRIu64 " count=%" PRIu64 " stride=%d\n", */
    /*         in->obj_id, in->lock_op, in->region.ndim, */
    /*         in->region.start_0, in->region.count_0, in->region.stride_0); */

    target_obj_id = in->obj_id;
    ndim = in->region.ndim;
//    lock_op = in->lock_op;

    // Convert transferred lock region to structure
    request_region = (region_list_t *)malloc(sizeof(region_list_t));
    PDC_init_region_list(request_region);
    request_region->ndim = ndim;

    if (ndim >=1) {
        request_region->start[0]  = in->region.start_0;
        request_region->count[0]  = in->region.count_0;
        /* request_region->stride[0] = in->region.stride_0; */
    }
    if (ndim >=2) {
        request_region->start[1]  = in->region.start_1;
        request_region->count[1]  = in->region.count_1;
        /* request_region->stride[1] = in->region.stride_1; */
    }
    if (ndim >=3) {
        request_region->start[2]  = in->region.start_2;
        request_region->count[2]  = in->region.count_2;
        /* request_region->stride[2] = in->region.stride_2; */
    }
    if (ndim >=4) {
        request_region->start[3]  = in->region.start_3;
        request_region->count[3]  = in->region.count_3;
        /* request_region->stride[3] = in->region.stride_3; */
    }


    // Locate target metadata structure
    target_obj = find_metadata_by_id(target_obj_id);
    if (target_obj == NULL) {
        printf("==PDC_SERVER: PDC_Server_region_lock - requested object (id=%" PRIu64 ") does not exist\n", in->obj_id);
        ret_value = -1;
        out->ret = -1;
        goto done;
    }

    request_region->meta = target_obj;

    /* printf("==PDC_SERVER: obtaining lock ... "); */
    // Go through all existing locks to check for overlapping
    // Note: currently only assumes contiguous region
    DL_FOREACH(target_obj->region_lock_head, elt) {
        if (is_contiguous_region_overlap(elt, request_region) == 1) {
            /* printf("rejected! (found overlapping regions)\n"); */
            out->ret = -1;
            goto done;
        }
    }
    // No overlaps found
    DL_APPEND(target_obj->region_lock_head, request_region);
    out->ret = 1;
    /* printf("granted\n"); */


done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
}

perr_t PDC_Server_region_release(region_lock_in_t *in, region_lock_out_t *out)
{
    perr_t ret_value;
    uint64_t target_obj_id;
    int ndim;
//    int lock_op;
    region_list_t *request_region;
    pdc_metadata_t *target_obj;
    region_list_t *elt, *tmp;
    int found = 0;

    FUNC_ENTER(NULL);

    /* printf("==PDC_SERVER: received lock request,                                \ */
    /*         obj_id=%" PRIu64 ", op=%d, ndim=%d, start=%" PRIu64 " count=%" PRIu64 " stride=%d\n", */
    /*         in->obj_id, in->lock_op, in->region.ndim, */
    /*         in->region.start_0, in->region.count_0, in->region.stride_0); */

    target_obj_id = in->obj_id;
    ndim = in->region.ndim;
//    lock_op = in->lock_op;

    // Convert transferred lock region to structure
    request_region = (region_list_t *)malloc(sizeof(region_list_t));
    PDC_init_region_list(request_region);
    request_region->ndim = ndim;

    if (ndim >=1) {
        request_region->start[0]  = in->region.start_0;
        request_region->count[0]  = in->region.count_0;
        /* request_region->stride[0] = in->region.stride_0; */
    }
    if (ndim >=2) {
        request_region->start[1]  = in->region.start_1;
        request_region->count[1]  = in->region.count_1;
        /* request_region->stride[1] = in->region.stride_1; */
    }
    if (ndim >=3) {
        request_region->start[2]  = in->region.start_2;
        request_region->count[2]  = in->region.count_2;
        /* request_region->stride[2] = in->region.stride_2; */
    }
    if (ndim >=4) {
        request_region->start[3]  = in->region.start_3;
        request_region->count[3]  = in->region.count_3;
        /* request_region->stride[3] = in->region.stride_3; */
    }


    // Locate target metadata structure
    target_obj = find_metadata_by_id(target_obj_id);
    if (target_obj == NULL) {
        printf("==PDC_SERVER: PDC_Server_region_lock - requested object (id=%" PRIu64 ") does not exist\n", in->obj_id);
        ret_value = -1;
        out->ret = -1;
        goto done;
    }

    request_region->meta = target_obj;

    /* printf("==PDC_SERVER: releasing lock ... "); */
    // Find the lock region in the list and remove it
    DL_FOREACH_SAFE(target_obj->region_lock_head, elt, tmp) {
        if (is_region_identical(request_region, elt) == 1) {
            // Found the requested region lock, remove from the linked list
            found = 1;
            DL_DELETE(target_obj->region_lock_head, elt);
            free(request_region);
            free(elt);
            out->ret = 1;
            /* printf("released!\n"); */
            goto done;
        }
    }
        // Request release lock region not found
        /* printf("requested release region/object does not exist\n"); */

    out->ret = 1;

done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
}

/*
 * Check if the metadata satisfies the constraint received from client
 *
 * \param  metadata[IN]       Metadata pointer
 * \param  constraints[IN]    Constraints received from client
 *
 * \return 1 if the metadata satisfies the constratins/-1 otherwise
 */
static int is_metadata_satisfy_constraint(pdc_metadata_t *metadata, metadata_query_transfer_in_t *constraints,
    const char *k_query, const char *vfrom_query,
        const char *vto_query)
{
    int ret_value = 1;

    FUNC_ENTER(NULL);

    /* int     user_id; */
    /* char    *app_name; */
    /* char    *obj_name; */
    /* int     time_step_from; */
    /* int     time_step_to; */
    /* int     ndim; */
    /* char    *tags; */
    if (constraints->user_id > 0 && constraints->user_id != metadata->user_id) {
        ret_value = -1;
        goto done;
    }
    if (strcmp(constraints->app_name, " ") != 0 && strcmp(metadata->app_name, constraints->app_name) != 0) {
        ret_value = -1;
        goto done;
    }
    if (strcmp(constraints->obj_name, " ") != 0 && strcmp(metadata->obj_name, constraints->obj_name) != 0) {
        ret_value = -1;
        goto done;
    }
    if (constraints->time_step_from > 0 && constraints->time_step_to > 0 &&
        (metadata->time_step < constraints->time_step_from || metadata->time_step > constraints->time_step_to)
       ) {
        ret_value = -1;
        goto done;
    }
    if (constraints->ndim > 0 && metadata->ndim != constraints->ndim ) {
        ret_value = -1;
        goto done;
    }
    // TODO: Currently only supports searching with one tag
    if (strcmp(constraints->tags, " ") != 0) {
        //println("query string: %s", constraints->tags);
        int start_with_select = startsWith(constraints->tags, "select");
        if (start_with_select) { // to be compatible with previous h5boss test
            if (strstr(metadata->tags, constraints->tags) == NULL) {
                ret_value = -1;
            }
            goto done;
        }else {
            println("pattern query starts ... ");
            // parse query conditions from constraints->tags,
            char * qdelim = NULL;
            // for non range queries, we use ':' as delimiter
            qdelim = strchr(constraints->tags, ':');
            if (qdelim) {
                println("k_query %s, v_query %s", k_query, vfrom_query);
                int rst = 0;
                if (strlen(vfrom_query)<=0) {
                    rst = k_v_matches_p(metadata->tags, k_query, NULL);
                } else {
                    rst = k_v_matches_p(metadata->tags, k_query, vfrom_query);
                }
                println("k_query %s, v_query %s, result %d", k_query, vfrom_query, rst);
                // determine results
                ret_value = (rst? 1 : -1);
                goto done;
            }
            // for range queries, we use '~' for delimiter
            qdelim = NULL;
            qdelim = strchr(constraints->tags, '~');
            if (qdelim) {
                int from = atoi(vfrom_query);
                int to = atoi(vto_query);
                int rst = is_value_in_range(metadata->tags, k_query, from, to);
                ret_value = (rst ? 1 : -1);
                goto done;
            }
        }
    }

done:
    FUNC_LEAVE(ret_value);
}

/*
 * Get the metadata that satisfies the query constraint
 *
 * \param  in[IN]           Input structure from client that contains the query constraint
 * \param  n_meta[OUT]      Number of metadata that satisfies the query constraint
 * \param  buf_ptrs[OUT]    Pointers to the found metadata
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_get_partial_query_result(metadata_query_transfer_in_t *in, uint32_t *n_meta, void ***buf_ptrs)
{
    perr_t ret_value = FAIL;
    uint32_t i;
    uint32_t n_buf, iter = 0;
    pdc_hash_table_entry_head *head;
    pdc_metadata_t *elt;
    hg_hash_table_iter_t hash_table_iter;
    int n_entry;

    // add timer
    stopwatch_t timer;

    FUNC_ENTER(NULL);

    // n_buf = n_metadata_g + 1 for potential padding array
    n_buf = n_metadata_g + 1;
    *buf_ptrs = (void**)calloc(n_buf, sizeof(void*));
    for (i = 0; i < n_buf; i++) {
        (*buf_ptrs)[i] = (void*)calloc(1, sizeof(void*));
    }
    timer_start(&timer);
    println("tags in query = %s", in->tags);
    fflush(stdout);
    char *query = in->tags;
    char *k_query = get_key(query, ':');
    char *vfrom_query = get_value(query, ':');
    char *vto_query = "";
    if (strchr(in->tags, '~')) {
        vto_query = get_value(vfrom_query, '-');
        vfrom_query = get_key(vfrom_query, '-');
    }

    // TODO: free buf_ptrs
    if (metadata_hash_table_g != NULL) {

        n_entry = hg_hash_table_num_entries(metadata_hash_table_g);
        hg_hash_table_iterate(metadata_hash_table_g, &hash_table_iter);


        while (n_entry != 0 && hg_hash_table_iter_has_more(&hash_table_iter)) {
            head = hg_hash_table_iter_next(&hash_table_iter);
            DL_FOREACH(head->metadata, elt) {
                // List all objects, no need to check other constraints
                if (in->is_list_all == 1) {
                    (*buf_ptrs)[iter++] = elt;
                }
                // check if current metadata matches search constraint
                else if (is_metadata_satisfy_constraint(elt, in, k_query, vfrom_query, vto_query) == 1) {
                    (*buf_ptrs)[iter++] = elt;
                }
            }
        }
        *n_meta = iter;

        printf("PDC_Server_get_partial_query_result: Total matching results: %d\n", *n_meta);

    }  // if (metadata_hash_table_g != NULL)
    else {
        printf("==PDC_SERVER: metadata_hash_table_g not initialized!\n");
        ret_value = FAIL;
        goto done;
    }

    timer_pause(&timer);

    println("Time to address query %s on server = %ld , with %d metadata objects obtained.", in->tags, timer_delta_us(&timer), *n_meta);

    ret_value = SUCCEED;

done:
    FUNC_LEAVE(ret_value);
}

/*
 * Seach the hash table with object name and hash key
 *
 * \param  obj_name[IN]     Name of the object to be searched
 * \param  hash_key[IN]     Hash value of the name string
 * \param  out[OUT]         Pointers to the found metadata
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_search_with_name_hash(const char *obj_name, uint32_t hash_key, pdc_metadata_t** out)
{
    perr_t ret_value = SUCCEED;
    pdc_hash_table_entry_head *lookup_value;
    pdc_metadata_t metadata;
    const char *name;

    FUNC_ENTER(NULL);

    *out = NULL;

    // Set up a metadata struct to query
    PDC_Server_metadata_init(&metadata);

    name = obj_name;

    strcpy(metadata.obj_name, name);
    /* metadata.time_step = tmp_time_step; */
    // TODO: currently PDC_Client_query_metadata_name_timestep is not taking timestep for querying
    metadata.time_step = 0;

    /* printf("==PDC_SERVER[%d]: search with name [%s], hash key %u\n", pdc_server_rank_g, name, hash_key); */

    if (metadata_hash_table_g != NULL) {
        // lookup
        /* printf("checking hash table with key=%d\n", hash_key); */
        lookup_value = hg_hash_table_lookup(metadata_hash_table_g, &hash_key);

        // Is this hash value exist in the Hash table?
        if (lookup_value != NULL) {
            /* printf("==PDC_SERVER: PDC_Server_search_with_name_hash(): lookup_value not NULL!\n"); */
            // Check if there exist metadata identical to current one
            /* PDC_print_metadata(lookup_value->metadata); */
            /* if (lookup_value->bloom == NULL) { */
            /*     printf("bloom is NULL\n"); */
            /* } */
            *out = find_identical_metadata(lookup_value, &metadata);

            if (*out == NULL) {
                 /* printf("==PDC_SERVER[%d]: Queried object with name [%s] has no full match!\n", */
                 /* pdc_server_rank_g, obj_name); */
                /* fflush(stdout); */
                ret_value = FAIL;
                goto done;
            }
            /* else { */
                /* printf("==PDC_SERVER[%d]: name %s found in hash table \n", pdc_server_rank_g, name); */
                /* fflush(stdout); */
                /* PDC_print_metadata(*out); */
            /* } */
        }
        else {
            *out = NULL;
        }

    }
    else {
        printf("metadata_hash_table_g not initialized!\n");
        ret_value = -1;
        goto done;
    }

    if (*out == NULL)
        printf("==PDC_SERVER[%d]: Queried object with name [%s] not found! \n", pdc_server_rank_g, name);

    /* PDC_print_metadata(*out); */

done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
}

/* void dstnode ( MySKL_n n ) */
/* { */
/*     pdc_metadata_t *item = MySKLgetEntry ( n, pdc_metadata_t, node ); */
/*     free(item); */
/* } */

/* int test_skl(int maxlev) */
/* { */
/*     MySKL_e err; */
/*     MySKL_t l; */
/*     MySKL_i it1; */
/*     MySKL_n n; */

/*     pdc_metadata_t *item1 = (pdc_metadata_t*)malloc(sizeof(pdc_metadata_t)); */
/*     pdc_metadata_t *item2 = (pdc_metadata_t*)malloc(sizeof(pdc_metadata_t)); */
/*     pdc_metadata_t *item3 = (pdc_metadata_t*)malloc(sizeof(pdc_metadata_t)); */
/*     item1->obj_id = 1; */
/*     item2->obj_id = 2; */
/*     item3->obj_id = 3; */

/*     l = MySKLinit( maxlev, PDC_metadata_cmp, dstnode, &err ); */
/*     if ( err == MYSKL_STATUS_OK ) { */
/*         printf("Inserting data to skl\n"); */
/*         MySKLinsertAD( l, &(item1->node) ); */
/*         MySKLinsertAD( l, &(item2->node) ); */
/*         MySKLinsertAD( l, &(item3->node) ); */

/*         MySKLsetIterator ( l, &it1, NULL ); */
/*         while ( ( n = MySKLgetNextNode ( &it1, 1 ) ) ) { */
/*             printf("%" PRIu64 "", MySKLgetEntry( n, LN_s, node )->obj_id); */
/*         } */

/*         MySKLdestroyIterator( &it1 ); */

/*         /1* MySKLdeleteNF( l, &tofound.node, NULL ); *1/ */
/*     } */
/*     else printf ( "Error with skiplist init!"); */

/*     return 0; */
/* } */

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
    int port;
    char *tmp_dir;
    perr_t ret;
    hg_return_t hg_ret;

    FUNC_ENTER(NULL);

#ifdef ENABLE_MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &pdc_server_rank_g);
    MPI_Comm_size(MPI_COMM_WORLD, &pdc_server_size_g);
#else
    pdc_server_rank_g = 0;
    pdc_server_size_g = 1;
#endif

    // Init rand seed
    srand(time(NULL));

    is_restart_g = 0;
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

#ifdef ENABLE_TIMING
    // Timing
    struct timeval  start;
    struct timeval  end;
    long long elapsed;
    double server_init_time, all_server_init_time;
    gettimeofday(&start, 0);
#endif

    if (argc > 1) {
        if (strcmp(argv[1], "restart") == 0)
            is_restart_g = 1;
    }

    ret = PDC_Server_init(port, &hg_class_g, &hg_context_g);
    if (ret != SUCCEED || hg_class_g == NULL || hg_context_g == NULL) {
        printf("Error with Mercury init, exit...\n");
        ret = FAIL;
        goto done;
    }

    // Get debug environment var
    char *is_debug_env = getenv("PDC_DEBUG");
    if (is_debug_env != NULL) {
        is_debug_g = atoi(is_debug_env);
        printf("==PDC_SERVER: PDC_DEBUG set to %d!\n", is_debug_g);
    }

    // Register RPC, metadata related
    client_test_connect_register(hg_class_g);
    gen_obj_id_register(hg_class_g);
    close_server_register(hg_class_g);
    /* send_obj_name_marker_register(hg_class_g); */
    metadata_index_create_register(hg_class_g);
    metadata_query_register(hg_class_g);
    metadata_delete_register(hg_class_g);
    metadata_delete_by_id_register(hg_class_g);
    metadata_update_register(hg_class_g);
    metadata_add_tag_register(hg_class_g);
    region_lock_register(hg_class_g);
    region_release_register(hg_class_g);
    // bulk
    query_partial_register(hg_class_g);

    // Mapping
    gen_reg_map_notification_register(hg_class_g);
    gen_reg_unmap_notification_register(hg_class_g);
    gen_obj_unmap_notification_register(hg_class_g);

    // Data server
    data_server_read_register(hg_class_g);
    data_server_write_register(hg_class_g);
    data_server_read_check_register(hg_class_g);
    data_server_write_check_register(hg_class_g);

    // Server to client RPC
    server_lookup_client_register_id_g = server_lookup_client_register(hg_class_g);
    notify_io_complete_register_id_g   = notify_io_complete_register(hg_class_g);

    // Server to server RPC
    server_lookup_remote_server_register_id_g = server_lookup_remote_server_register(hg_class_g);
    update_region_loc_register_id_g    = update_region_loc_register(hg_class_g);
    notify_region_update_register_id_g = notify_region_update_register(hg_class_g);
    get_metadata_by_id_register_id_g   = get_metadata_by_id_register(hg_class_g);
    get_storage_info_register_id_g     = get_storage_info_register(hg_class_g);

    if (PDC_Server_lookup_remote_server() != SUCCEED) {
        printf("==PDC_SERVER[%d]: unable to lookup_remote_server, exiting...\n", pdc_server_rank_g);
        goto done;
    }

#ifdef ENABLE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif

#ifdef ENABLE_TIMING
    // Timing
    gettimeofday(&end, 0);
    elapsed = (end.tv_sec-start.tv_sec)*1000000LL + end.tv_usec-start.tv_usec;
    server_init_time = elapsed / 1000000.0;
#endif


    /* // Debug */
    /* test_serialize(); */



    if (pdc_server_rank_g == 0) {
#ifdef ENABLE_TIMING
        printf("==PDC_SERVER[%d]: total startup time = %.6f\n", pdc_server_rank_g, server_init_time);
#endif
        printf("==PDC_SERVER[%d]: Server ready!\n\n\n", pdc_server_rank_g);
    }
    fflush(stdout);

#ifdef ENABLE_MULTITHREAD
    PDC_Server_multithread_loop(hg_context_g);
#else
    PDC_Server_loop(hg_context_g);
#endif

    printf("==PDC_SERVER[%d]: All work done, finalizing\n", pdc_server_rank_g);

    /* if (pdc_server_rank_g == 0) { */
    /*     printf("==PDC_SERVER: All work done, finalizing\n"); */
    /*     fflush(stdout); */
    /* } */
#ifdef ENABLE_CHECKPOINT
    // TODO: instead of checkpoint at app finalize time, try checkpoint with a time countdown or # of objects
    char checkpoint_file[ADDR_MAX];
    sprintf(checkpoint_file, "%s%s%d", pdc_server_tmp_dir_g, "metadata_checkpoint.", pdc_server_rank_g);

    #ifdef ENABLE_TIMING
    // Timing
    struct timeval  ht_total_start;
    struct timeval  ht_total_end;
    long long ht_total_elapsed;
    double checkpoint_time, all_checkpoint_time;
    gettimeofday(&ht_total_start, 0);
    #endif

    PDC_Server_checkpoint(checkpoint_file);

    #ifdef ENABLE_TIMING
    // Timing
    gettimeofday(&ht_total_end, 0);
    ht_total_elapsed = (ht_total_end.tv_sec-ht_total_start.tv_sec)*1000000LL + ht_total_end.tv_usec-ht_total_start.tv_usec;
    checkpoint_time = ht_total_elapsed / 1000000.0;

    #ifdef ENABLE_MPI
    MPI_Reduce(&checkpoint_time, &all_checkpoint_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    #else
    all_checkpoint_time = checkpoint_time;
    #endif

    if (pdc_server_rank_g == 0) {
        printf("==PDC_SERVER: total checkpoint  time = %.6f\n", all_checkpoint_time);
    }

    #endif
#endif

done:
    if (pdc_server_rank_g == 0) {
        printf("==PDC_SERVER: start finalizing ... ");
        /* printf("==PDC_SERVER: [%d] exiting...\n", pdc_server_rank_g); */
        fflush(stdout);
    }
    PDC_Server_finalize();

    // Finalize
    /* hg_ret = HG_Context_destroy(pdc_client_context_g); */
    /* if (hg_ret != HG_SUCCESS) */
    /*     printf("error with HG_Context_destroy(pdc_client_context_g)\n"); */

    hg_ret = HG_Context_destroy(hg_context_g);
    if (hg_ret != HG_SUCCESS)
        printf("error with HG_Context_destroy(hg_context_g)\n");

    hg_ret = HG_Finalize(hg_class_g);
    if (hg_ret != HG_SUCCESS)
        printf("error with HG_Finalize\n");

    if (pdc_server_rank_g == 0) {
        printf("done.\n");
        /* printf("==PDC_SERVER: [%d] exiting...\n", pdc_server_rank_g); */
        fflush(stdout);
    }

#ifdef ENABLE_MPI
    MPI_Finalize();
#endif
    return 0;
}


/*
 * Data Server related
 */

/*
 * Check if two region are the same
 *
 * \param  a[IN]     Pointer of the first region
 * \param  b[IN]     Pointer of the second region
 *
 * \return 1 if same/0 otherwise
 */
int region_list_cmp(region_list_t *a, region_list_t *b)
{
    if (a->ndim != b->ndim) {
        printf("  region_list_cmp(): not equal ndim! \n");
        return -1;
    }

    uint32_t i;
    uint64_t tmp;
    for (i = 0; i < a->ndim; i++) {
        tmp = a->start[i] - b->start[i];
        if (tmp != 0)
            return tmp;
    }
    return 0;
}

/*
 * Check if two region are from the same client
 *
 * \param  a[IN]     Pointer of the first region
 * \param  b[IN]     Pointer of the second region
 *
 * \return 1 if same/0 otherwise
 */
int region_list_cmp_by_client_id(region_list_t *a, region_list_t *b)
{
    if (a->ndim != b->ndim) {
        printf("  region_list_cmp_by_client_id(): not equal ndim! \n");
        return -1;
    }

    return (a->client_ids[0] - b->client_ids[0]);
}


// TODO: currently only support merging regions that are cut in one dimension
/*
 * Merge multiple region to contiguous ones
 *
 * \param  list[IN]         Pointer of the regions in a list
 * \param  merged[OUT]      Merged list (new)
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_merge_region_list_naive(region_list_t *list, region_list_t **merged)
{
    perr_t ret_value = FAIL;


    // print all regions
    region_list_t *elt, *elt_elt;
    region_list_t *tmp_merge;
    uint32_t i;
    int count, pos, pos_pos, tmp_pos;
    int *is_merged;

    DL_SORT(list, region_list_cmp);
    DL_COUNT(list, elt, count);

    is_merged = (int*)calloc(sizeof(int), count);

    DL_FOREACH(list, elt) {
        PDC_print_region_list(elt);
    }

    // Init merged head
    pos = 0;
    DL_FOREACH(list, elt) {
        if (is_merged[pos] != 0) {
            pos++;
            continue;
        }

        // First region that has not been merged
        tmp_merge = (region_list_t*)malloc(sizeof(region_list_t));
        if (NULL == tmp_merge) {
            printf("==PDC_SERVER: ERROR allocating for region_list_t!\n");
            ret_value = FAIL;
            goto done;
        }
        PDC_init_region_list(tmp_merge);

        // Add the client id to the client_ids[] arrary
        tmp_pos = 0;
        tmp_merge->client_ids[tmp_pos] = elt->client_ids[0];
        tmp_pos++;

        tmp_merge->ndim = list->ndim;
        for (i = 0; i < list->ndim; i++) {
            tmp_merge->start[i]  = elt->start[i];
            /* tmp_merge->stride[i] = elt->stride[i]; */
            tmp_merge->count[i]  = elt->count[i];
        }
        is_merged[pos] = 1;

        DL_APPEND(*merged, tmp_merge);

        // Check for all other regions in the list and see it any can be merged
        pos_pos = 0;
        DL_FOREACH(list, elt_elt) {
            if (is_merged[pos_pos] != 0) {
                pos_pos++;
                continue;
            }

            // check if current elt_elt can be merged to elt
            for (i = 0; i < list->ndim; i++) {
                if (elt_elt->start[i] == tmp_merge->start[i] + tmp_merge->count[i]) {
                    tmp_merge->count[i] += elt_elt->count[i];
                    is_merged[pos_pos] = 1;
                    tmp_merge->client_ids[tmp_pos] = elt_elt->client_ids[0];
                    tmp_pos++;
                    break;
                }
            }
            pos_pos++;
        }

        pos++;
    }

    ret_value = SUCCEED;

done:
    fflush(stdout);
    free(is_merged);
    FUNC_LEAVE(ret_value);
}

/*
 * Callback function for the region update, gets output from client
 *
 * \param  callback_info[OUT]      Mercury callback info
 *
 * \return Non-negative on success/Negative on failure
 */
static hg_return_t PDC_Server_notify_region_update_cb(const struct hg_cb_info *callback_info)
{
    hg_return_t ret_value = HG_SUCCESS;
    hg_handle_t handle;
    notify_region_update_out_t output;
    struct server_region_update_args *update_args;

    FUNC_ENTER(NULL);

    update_args = (server_lookup_args_t*) callback_info->arg;
    handle = callback_info->info.forward.handle;

    /* Get output from client */
    ret_value = HG_Get_output(handle, &output);
    if (ret_value != HG_SUCCESS) {
        printf("==PDC_SERVER[%d]: PDC_Server_notify_region_update_cb - error with HG_Get_output\n",
                pdc_server_rank_g);
        update_args->ret = -1;
        goto done;
    }
    //printf("==PDC_SERVER[%d]: PDC_Server_notify_region_update_cb - received from client with %d\n", pdc_server_rank_g, output.ret);
    update_args->ret = output.ret;

done:
    work_todo_g--;
    HG_Free_output(handle, &output);
    FUNC_LEAVE(ret_value);
}

/*
 * Callback function for the notify region update
 *
 * \param  meta_id[OUT]      Metadata ID
 * \param  reg_id[OUT]       Object ID
 * \param  client_id[OUT]    Client's MPI rank
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_SERVER_notify_region_update_to_client(uint64_t obj_id, uint64_t reg_id, int32_t client_id)
{
    perr_t ret_value = SUCCEED;
    hg_return_t hg_ret;
    struct server_region_update_args update_args;

    FUNC_ENTER(NULL);

    if (pdc_client_info_g[client_id].addr_valid == 0) {
        ret_value = PDC_Server_lookup_client(client_id);
        if (ret_value != SUCCEED) {
            fprintf(stderr, "==PDC_SERVER: PDC_Server_notify_region_update_to_client() - \
                    PDC_Server_lookup_client failed)\n");
            return FAIL;
        }
    }

    if (pdc_client_info_g[client_id].notify_region_update_handle_valid != 1) {
        hg_ret = HG_Create(hg_context_g, pdc_client_info_g[client_id].addr, notify_region_update_register_id_g,
                            &pdc_client_info_g[client_id].notify_region_update_handle);
        if (hg_ret != HG_SUCCESS) {
            fprintf(stderr, "PDC_Server_notify_region_update_to_client(): Could not HG_Create()\n");
            return FAIL;
        }
        pdc_client_info_g[client_id].notify_region_update_handle_valid = 1;
    }

    // Fill input structure
    notify_region_update_in_t in;
    in.obj_id    = obj_id;
    in.reg_id    = reg_id;

    /* printf("Sending input to target\n"); */
    hg_ret = HG_Forward(pdc_client_info_g[client_id].notify_region_update_handle, PDC_Server_notify_region_update_cb, &update_args, &in);
    if (hg_ret != HG_SUCCESS) {
        fprintf(stderr, "PDC_Server_notify_region_update_to_client(): Could not start HG_Forward()\n");
        return FAIL;
    }

done:
    FUNC_LEAVE(ret_value);
}

/*
perr_t PDC_SERVER_notify_region_update_to_client(pdcid_t meta_id, pdcid_t reg_id, int32_t client_id)
{
    perr_t ret_value = SUCCEED;
    hg_return_t hg_ret;

    FUNC_ENTER(NULL);

    if (pdc_client_info_g[client_id].addr_valid == 0) {
        ret_value = PDC_Server_lookup_client(client_id);
        if (ret_value != SUCCEED) {
            fprintf(stderr, "==PDC_SERVER: PDC_SERVER_notify_region_update() - \
                    PDC_Server_lookup_client failed)\n");
            return FAIL;
        }
    }
    if (pdc_client_info_g[client_id].notify_region_update_handle_valid != 1) {
        hg_ret = HG_Create(hg_context_g, pdc_client_info_g[client_id].addr, notify_region_update_register_id_g,
                           &pdc_client_info_g[client_id].notify_region_update_handle);
        if (hg_ret != HG_SUCCESS) {
            fprintf(stderr, "PDC_Server_notify_region_update_to_client(): Could not HG_Create()\n");
            return FAIL;
        }
        pdc_client_info_g[client_id].notify_region_update_handle_valid = 1;
    }

    // Fill input structure
    notify_region_update_in_t in;
    in.obj_id    = meta_id;
    in.reg_id    = reg_id;

    server_lookup_args_t lookup_args;
    hg_ret = HG_Forward(pdc_client_info_g[client_id].notify_region_update_handle, PDC_Server_notify_region_update_cb, &lookup_args, &in);
    if (hg_ret != HG_SUCCESS) {
        fprintf(stderr, "PDC_Server_notify_region_update_to_client(): Could not start HG_Forward()\n");
        return FAIL;
    }
    if(lookup_args.ret_int != 1)
        ret_value = FAIL;

    work_todo_g = 1;
    PDC_Server_check_response(&hg_context_g);

    FUNC_LEAVE(ret_value);
}
*/

/*
 * Close the shared memory
 *
 * \param  region[OUT]    Pointer to region
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_close_shm(region_list_t *region)
{
    perr_t ret_value = SUCCEED;

    FUNC_ENTER(NULL);

    if (region->buf == NULL)
        goto done;

    /* remove the mapped memory segment from the address space of the process */
    if (munmap(region->buf, region->data_size) == -1) {
        printf("==PDC_SERVER: Unmap failed\n");
        return FAIL;
    }

    /* close the shared memory segment as if it was a file */
    if (close(region->shm_fd) == -1) {
        printf("==PDC_SERVER: close shm_fd failed\n");
        return FAIL;
    }

done:
    FUNC_LEAVE(ret_value);
}

/*
 * Callback function for IO complete notification send to client, gets output from client
 *
 * \param  callback_info[IN]    Mercury callback info
 *
 * \return Non-negative on success/Negative on failure
 */
static hg_return_t PDC_Server_notify_io_complete_cb(const struct hg_cb_info *callback_info)
{
    hg_return_t ret_value;

    FUNC_ENTER(NULL);

    server_lookup_args_t *lookup_args = (server_lookup_args_t*) callback_info->arg;
    hg_handle_t handle = callback_info->info.forward.handle;

    /* Get output from server*/
    notify_io_complete_out_t output;

    ret_value = HG_Get_output(handle, &output);
    if (ret_value != HG_SUCCESS) {
        printf("==PDC_SERVER[%d]: PDC_Server_notify_io_complete_cb to client %"PRIu32 " "
                "- error with HG_Get_output\n", pdc_server_rank_g, lookup_args->client_id);
        lookup_args->ret_int = -1;
        goto done;
    }

    if (is_debug_g ) {
        printf("==PDC_SERVER[%d]: PDC_Server_notify_io_complete_cb - received from client %d with %d\n",
                pdc_server_rank_g, lookup_args->client_id, output.ret);
    }
    lookup_args->ret_int = output.ret;

done:
    pdc_to_client_work_todo_g = 0;
    HG_Free_output(handle, &output);
    FUNC_LEAVE(ret_value);
}

/*
 * Callback function for IO complete notification send to client
 *
 * \param  client_id[IN]    Target client's MPI rank
 * \param  obj_id[IN]       Object ID
 * \param  shm_addr[IN]     Server's shared memory address
 * \param  io_typ[IN]       IO type (read/write)
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_notify_io_complete_to_client(uint32_t client_id, uint64_t obj_id,
        char* shm_addr, PDC_access_t io_type)
{
    char tmp_shm[50];
    perr_t ret_value   = SUCCEED;
    hg_return_t hg_ret = HG_SUCCESS;
    server_lookup_args_t lookup_args;
    hg_handle_t notify_io_complete_handle;

    FUNC_ENTER(NULL);

    if (client_id >= (uint32_t)pdc_client_num_g) {
        printf("==PDC_SERVER[%d]: PDC_Server_notify_io_complete_to_client() - "
                "client_id %d invalid)\n", pdc_server_rank_g, client_id);
        ret_value = FAIL;
        goto done;
    }

    if (pdc_client_info_g[client_id].addr_valid != 1) {
        ret_value = PDC_Server_lookup_client(client_id);
        if (ret_value != SUCCEED) {
            printf("==PDC_SERVER[%d]: PDC_Server_notify_io_complete_to_client() - "
                    "PDC_Server_lookup_client failed)\n", pdc_server_rank_g);
            goto done;
        }
    }

    /* if (pdc_client_info_g[client_id].notify_io_complete_handle_valid != 1) { */
        /* printf("==PDC_SERVER[%d]: PDC_Server_notify_io_complete_to_client() - Create a handle to client %d\n", */
        /*         pdc_server_rank_g, client_id); */

        hg_ret = HG_Create(hg_context_g, pdc_client_info_g[client_id].addr,
                    notify_io_complete_register_id_g, &notify_io_complete_handle);
        /* hg_ret = HG_Create(pdc_client_context_g, pdc_client_info_g[client_id].addr, */
        /*             notify_io_complete_register_id_g, &notify_io_complete_handle); */
        if (hg_ret != HG_SUCCESS) {
            printf("==PDC_SERVER[%d]: PDC_Server_notify_io_complete_to_client() - "
                    "HG_Create failed)\n", pdc_server_rank_g);
            ret_value = FAIL;
            goto done;
        }
        /* pdc_client_info_g[client_id].notify_io_complete_handle_valid = 1; */
    /* } */

    // Fill input structure
    notify_io_complete_in_t in;
    in.obj_id     = obj_id;
    in.io_type    = io_type;
    if (shm_addr[0] == 0) {
        sprintf(tmp_shm, "%d", client_id * 10);
        in.shm_addr   = tmp_shm;
        /* in.shm_addr   = " "; */
    }
    else
        in.shm_addr   = shm_addr;

    if (is_debug_g ) {
        /* printf("==PDC_SERVER: PDC_Server_notify_io_complete_to_client shm_addr = [%s]\n", in.shm_addr); */
        printf("==PDC_SERVER[%d]: PDC_Server_notify_io_complete_to_client %d \n", pdc_server_rank_g, client_id);
        fflush(stdout);
    }

    /* printf("Sending input to target\n"); */
    lookup_args.client_id = client_id;
    hg_ret = HG_Forward(notify_io_complete_handle, PDC_Server_notify_io_complete_cb, &lookup_args, &in);

    if (hg_ret != HG_SUCCESS) {
        fprintf(stderr, "PDC_Server_notify_io_complete_to_client(): Could not start HG_Forward()\n");
        ret_value = FAIL;
        goto done;
    }

    /* if (is_debug_g) { */
    /*     printf("==PDC_SERVER[%d]: forwarded to client %d!\n", pdc_server_rank_g, client_id); */
    /* } */

    work_todo_g = 1;
    PDC_Server_check_response(&hg_context_g);
    /* PDC_Server_trigger(&hg_context_g); */
    /* pdc_to_client_work_todo_g = 1; */
    /* PDC_Server_check_client_response(&pdc_client_context_g); */

done:
    fflush(stdout);
    hg_ret = HG_Destroy(notify_io_complete_handle);
    if (hg_ret != HG_SUCCESS) {
        printf("==PDC_SERVER[%d]: PDC_Server_notify_io_complete_to_client() - "
                "HG_Destroy(notify_io_complete_handle) error!\n", pdc_server_rank_g);
    }
    FUNC_LEAVE(ret_value);
}

/*
 * Check if a previous read request has been completed
 *
 * \param  in[IN]       Input structure received from client containing the IO request info
 * \param  out[OUT]     Output structure to be sent back to the client
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_read_check(data_server_read_check_in_t *in, data_server_read_check_out_t *out)
{
    perr_t ret_value = SUCCEED;

    FUNC_ENTER(NULL);

    uint32_t i;
    pdc_metadata_t meta;
    PDC_metadata_init(&meta);
    pdc_transfer_t_to_metadata_t(&in->meta, &meta);

    pdc_data_server_io_list_t *io_elt = NULL, *io_target = NULL;
    region_list_t *r_elt = NULL;

    region_list_t r_target;
    PDC_init_region_list(&r_target);
    pdc_region_transfer_t_to_list_t(&(in->region), &r_target);

#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_lock(&data_read_list_mutex_g);
#endif
    // Iterate io list, find current request
    DL_FOREACH(pdc_data_server_read_list_head_g, io_elt) {
        if (meta.obj_id == io_elt->obj_id) {
            io_target = io_elt;
            break;
        }
    }
#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_unlock(&data_read_list_mutex_g);
#endif

    // If not found, create and insert one to the list
    if (NULL == io_target) {
        printf("==PDC_SERVER: No existing io request with same obj_id found, create a new one!\n");
        out->ret = -1;
        out->shm_addr = " ";
        goto done;
    }

    /* printf("%d region: start(%" PRIu64 ", %" PRIu64 ") size(%" PRIu64 ", %" PRIu64 ") \n", r_target.start[0], r_target.start[1], r_target.count[0], r_target.count[1]); */
    int found_region = 0;
    DL_FOREACH(io_target->region_list_head, r_elt) {
        /* if (region_list_cmp(r_elt, &r_target) == 0) { */
        for (i = 0; i < PDC_SERVER_MAX_PROC_PER_NODE; i++) {
            if (r_elt->client_ids[i] == in->client_id) {
                // Found io list
                found_region = 1;
                out->ret = r_elt->is_data_ready;
                out->shm_addr = r_elt->shm_addr;
                goto done;
            }
        }
    }

    if (found_region == 0) {
        printf("==PDC_SERVER: No existing io request with same region found!\n");
        out->ret = -1;
        out->shm_addr = " ";
        goto done;
    }

    // TODO remove the item in pdc_data_server_read_list_head_g after the request is fulfilled
    //      at object close time?
done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
}

/*
 * Check if a previous write request has been completed
 *
 * \param  in[IN]       Input structure received from client containing the IO request info
 * \param  out[OUT]     Output structure to be sent back to the client
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_write_check(data_server_write_check_in_t *in, data_server_write_check_out_t *out)
{
    perr_t ret_value = FAIL;

    FUNC_ENTER(NULL);

    int i;
    pdc_metadata_t meta;
    PDC_metadata_init(&meta);
    pdc_transfer_t_to_metadata_t(&in->meta, &meta);

    pdc_data_server_io_list_t *io_elt = NULL, *io_target = NULL;
    region_list_t *r_elt = NULL;

    region_list_t r_target;
    PDC_init_region_list(&r_target);
    pdc_region_transfer_t_to_list_t(&(in->region), &r_target);

#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_lock(&data_write_list_mutex_g);
#endif
    // Iterate io list, find current request
    DL_FOREACH(pdc_data_server_write_list_head_g, io_elt) {
        if (meta.obj_id == io_elt->obj_id) {
            io_target = io_elt;
            break;
        }
    }
#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_unlock(&data_write_list_mutex_g);
#endif

    // If not found, create and insert one to the list
    if (NULL == io_target) {
        printf("==PDC_SERVER: No existing io request with same obj_id found, create a new one!\n");
        out->ret = -1;
        ret_value = SUCCEED;
        goto done;
    }

    /* printf("%d region: start(%" PRIu64 ", %" PRIu64 ") size(%" PRIu64 ", %" PRIu64 ") \n", r_target.start[0], r_target.start[1], r_target.count[0], r_target.count[1]); */
    int found_region = 0;
    DL_FOREACH(io_target->region_list_head, r_elt) {
        /* if (region_list_cmp(r_elt, &r_target) == 0) { */
        for (i = 0; i < PDC_SERVER_MAX_PROC_PER_NODE; i++) {
            if (r_elt->client_ids[i] == in->client_id) {
                // Found io list
                found_region = 1;
                out->ret = r_elt->is_data_ready;
                ret_value = SUCCEED;
                goto done;
            }
        }
    }

    if (found_region == 0) {
        printf("==PDC_SERVER: No existing io request with same region found!\n");
        out->ret = -1;
        ret_value = SUCCEED;
        goto done;
    }

    ret_value = SUCCEED;
    // TODO remove the item in pdc_data_server_write_list_head_g after the request is fulfilled
    //      at object close? time

done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
} //PDC_Server_write_check

/*
 * Read the requested data to shared memory address
 *
 * \param  region_list_head[IN]       List of IO request to be performed
 * \param  obj_id[IN]                 Object ID of the IO request
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_data_read_to_shm(region_list_t *region_list_head, uint64_t obj_id)
{
    perr_t ret_value = FAIL;
    region_list_t *elt;
    /* region_list_t *tmp; */
    /* region_list_t *merged_list = NULL; */

    FUNC_ENTER(NULL);

    // TODO: merge regions for aggregated read
    // Merge regions
    /* PDC_Server_merge_region_list_naive(region_list_head, &merged_list); */

    // Replace region_list with merged list
    DL_SORT(region_list_head, region_list_cmp_by_client_id);
    /* DL_FOREACH_SAFE(region_list_head, elt, tmp) { */
    /*     DL_DELETE(region_list_head, elt); */
    /*     free(elt); */
    /* } */
    /* region_list_head = merged_list; */

    /* printf("==PDC_SERVER: after merge\n"); */
    /* DL_FOREACH(merged_list, elt) { */
    /*     PDC_print_region_list(elt); */
    /* } */
    /* fflush(stdout); */

    // Now we have a -merged- list of regions to be read,
    // so just read one by one
    DL_FOREACH(region_list_head, elt) {

        elt->data_size = elt->count[0];
        size_t i = 0;
        for (i = 1; i < elt->ndim; i++)
            elt->data_size *= elt->count[i];

        // Get min max client ID
        uint32_t client_id_min = elt->client_ids[0];
        uint32_t client_id_max = elt->client_ids[0];
        for (i = 1; i < PDC_SERVER_MAX_PROC_PER_NODE; i++) {
            if (elt->client_ids[i] == 0)
                break;
            client_id_min = elt->client_ids[i] < client_id_min ? elt->client_ids[i] : client_id_min;
            client_id_max = elt->client_ids[i] > client_id_max ? elt->client_ids[i] : client_id_max;
        }

        int rnd = rand();
        // Shared memory address is /objID_ServerID_ClientIDmin_to_ClientIDmax_rand
        sprintf(elt->shm_addr, "/%" PRIu64 "_s%d_c%dto%d_%d", obj_id, pdc_server_rank_g,
                                             client_id_min, client_id_max, rnd);

        /* create the shared memory segment as if it was a file */
        /* printf("==PDC_SERVER: creating share memory segment with address [%s]\n", elt->shm_addr); */
        elt->shm_fd = shm_open(elt->shm_addr, O_CREAT | O_RDWR, 0666);
        if (elt->shm_fd == -1) {
            printf("==PDC_SERVER: Shared memory shm_open failed\n");
            ret_value = FAIL;
            goto done;
        }

        /* configure the size of the shared memory segment */
        ftruncate(elt->shm_fd, elt->data_size);

        /* map the shared memory segment to the address space of the process */
        elt->buf = mmap(0, elt->data_size, PROT_READ | PROT_WRITE, MAP_SHARED, elt->shm_fd, 0);
        if (elt->buf == MAP_FAILED) {
            printf("==PDC_SERVER: Shared memory mmap failed\n");
            // close and shm_unlink?
            ret_value = FAIL;
            goto done;
        }
    } // DL_FOREACH

    // POSIX read for now
    ret_value = PDC_Server_regions_io(region_list_head, POSIX);
            /* status = PDC_Server_data_write_real(io_list_target); */
    if (ret_value != SUCCEED) {
        printf("==PDC_SERVER: error reading data from storage and create shared memory\n");
        goto done;
    }

    ret_value = SUCCEED;

done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
} //PDC_Server_data_read_to_shm

/*
 * Get the storage location of a region from local metadata hash table
 *
 * \param  obj_id[IN]               Object ID of the request
 * \param  region[IN]               Request region
 * \param  n_loc[OUT]               Number of storage locations of the target region
 * \param  overlap_region_loc[OUT]  List of region locations
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_get_local_storage_location_of_region(uint64_t obj_id, region_list_t *region,
        uint32_t *n_loc, region_list_t **overlap_region_loc)
{
    perr_t ret_value = SUCCEED;
    pdc_metadata_t *target_meta = NULL;
    region_list_t  *region_elt = NULL;

    FUNC_ENTER(NULL);

    // Find object metadata
    *n_loc = 0;
    target_meta = PDC_Server_get_obj_metadata(obj_id);
    if (target_meta == NULL) {
        printf("==PDC_SERVER[%d]: PDC_Server_get_obj_metadata FAILED!\n", pdc_server_rank_g);
        ret_value = FAIL;
        goto done;
    }
    DL_FOREACH(target_meta->storage_region_list_head, region_elt) {
        if (is_contiguous_region_overlap(region_elt, region) == 1) {
            PDC_init_region_list(overlap_region_loc[*n_loc]);
            pdc_region_list_t_deep_cp(region_elt, overlap_region_loc[*n_loc]);
            /* overlap_region_loc[*n_loc] = region_elt; */
            *n_loc += 1;
        }
        /* PDC_print_storage_region_list(region_elt); */
        if (*n_loc > PDC_MAX_OVERLAP_REGION_NUM) {
            printf("==PDC_SERVER: PDC_Server_get_local_storage_location_of_region - \
                    exceeding PDC_MAX_OVERLAP_REGION_NUM regions!\n");
            ret_value = FAIL;
            goto done;
        }
    } // DL_FOREACH

    if (*n_loc == 0) {
        printf("==PDC_SERVER: PDC_Server_get_local_storage_location_of_region - \
                no overlapping region found\n");
        ret_value = FAIL;
        goto done;
    }


done:
    FUNC_LEAVE(ret_value);
} // PDC_Server_get_local_storage_location_of_region

/*
 * Callback function for get storage info.
 *
 * \param  callback_info[IN]         Mercury callback info
 *
 * \return Non-negative on success/Negative on failure
 */
static hg_return_t
PDC_Server_get_storage_info_cb (const struct hg_cb_info *callback_info)
{
    hg_return_t ret_value;
    server_lookup_args_t *lookup_args;
    hg_handle_t handle;
    pdc_serialized_data_t output;

    FUNC_ENTER(NULL);

    lookup_args = (server_lookup_args_t*) callback_info->arg;
    handle = callback_info->info.forward.handle;

    /* Get output from server*/
    ret_value = HG_Get_output(handle, &output);
    if (ret_value != HG_SUCCESS) {
        printf("==PDC_SERVER[%d]: PDC_Server_get_storage_info_cb: error HG_Get_output\n", pdc_server_rank_g);
        lookup_args->void_buf = NULL;
        goto done;
    }

    /* printf("PDC_Server_get_storage_info_cb: ret=%d\n", output.ret); */
    lookup_args->void_buf = output.buf;

done:
    work_todo_g--;
    HG_Free_output(handle, &output);
    FUNC_LEAVE(ret_value);
}

/*
 * Get the storage location of a region from (possiblly remote) metadata hash table
 *
 * \param  obj_id[IN]               Object ID of the request
 * \param  region[IN]               Request region
 * \param  n_loc[OUT]               Number of storage locations of the target region
 * \param  overlap_region_loc[OUT]  List of region locations
 *
 * \return Non-negative on success/Negative on failure
 */
// Note: one request region can spread across multiple regions in storage
// Need to allocate **overlap_region_loc with PDC_MAX_OVERLAP_REGION_NUM before calling this
perr_t PDC_Server_get_storage_location_of_region(region_list_t *request_region, uint32_t *n_loc,
                                                 region_list_t **overlap_region_loc)
{
    perr_t ret_value = SUCCEED;
    hg_return_t hg_ret;
    uint32_t server_id = 0;
    pdc_metadata_t *region_meta = NULL;
    get_storage_info_in_t in;

    FUNC_ENTER(NULL);

    if (request_region == NULL || overlap_region_loc == NULL || overlap_region_loc[0] == NULL || n_loc == NULL) {
        printf("==PDC_SERVER[%d]: PDC_Server_get_storage_location_of_region() input has NULL value!\n",
                pdc_server_rank_g);
        ret_value = FAIL;
        goto done;
    }

    region_meta = request_region->meta;
    if (region_meta == NULL) {
        printf("PDC_SERVER[%d]: PDC_Server_get_storage_location_of_region - request region has NULL metadata\n",
                pdc_server_rank_g);
        ret_value = FAIL;
        goto done;
    }
    server_id = PDC_get_server_by_obj_id(region_meta->obj_id, pdc_server_size_g);
    if (server_id == (uint32_t)pdc_server_rank_g) {
        // Metadata object is local, no need to send update RPC
        ret_value = PDC_Server_get_local_storage_location_of_region(region_meta->obj_id, request_region, n_loc, overlap_region_loc);
        if (ret_value != SUCCEED) {
            printf("==PDC_SERVER[%d]: PDC_Server_get_storage_location_of_region()"
                    "unable to get local storage location!\n", pdc_server_rank_g);
            goto done;
        }
    }
    else {
        /* if (pdc_remote_server_info_g[server_id].get_storage_info_handle_valid!= 1) { */
            HG_Create(hg_context_g, pdc_remote_server_info_g[server_id].addr, get_storage_info_register_id_g,
                      &pdc_remote_server_info_g[server_id].get_storage_info_handle);
            /* pdc_remote_server_info_g[server_id].get_storage_info_handle_valid = 1; */
        /* } */

        /* printf("Sending updated region loc to target\n"); */
        server_lookup_args_t lookup_args;

        if (request_region->meta == NULL) {
            printf("==PDC_SERVER: NULL request_region->meta");
            ret_value = FAIL;
            goto done;
        }
        in.obj_id = request_region->meta->obj_id;
        pdc_region_list_t_to_transfer(request_region, &in.req_region);

        hg_ret = HG_Forward(pdc_remote_server_info_g[server_id].get_storage_info_handle,
                            PDC_Server_get_storage_info_cb, &lookup_args, &in);
        if (hg_ret != HG_SUCCESS) {
            fprintf(stderr, "PDC_Client_update_metadata_with_name(): Could not start HG_Forward()\n");
            HG_Destroy(pdc_remote_server_info_g[server_id].get_storage_info_handle);
            return FAIL;
        }

        // Wait for response from remote metadata server
        work_todo_g = 1;
        PDC_Server_check_response(&hg_context_g);

        *n_loc = 0;
        if (PDC_Server_unserialize_regions_info(&lookup_args.void_buf, overlap_region_loc, n_loc) != SUCCEED ) {
            printf("==PDC_SERVER: unable to unserialize_regions_info");
            *n_loc = 0;
            ret_value = FAIL;
            goto done;
        }
        HG_Destroy(pdc_remote_server_info_g[server_id].get_storage_info_handle);
    }

done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
} // PDC_Server_get_storage_location_of_region

/*
 * Set the Lustre stripe count/size of a given path
 *
 * \param  path[IN]             Directory to be set with Lustre stripe/count
 * \param  stripe_count[IN]     Stripe count
 * \param  stripe_size_MB[IN]   Stripe size in MB
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_set_lustre_stripe(char *path, int stripe_count, int stripe_size_MB)
{
    perr_t ret_value = SUCCEED;
    size_t len;
    int i;
    char tmp[ADDR_MAX];
    char cmd[ADDR_MAX];

    FUNC_ENTER(NULL);

    snprintf(tmp, sizeof(tmp),"%s",path);

    len = strlen(tmp);
    for (i = len-1; i >= 0; i--)
        if (path[i] == '/') {
            tmp[i] = 0;
            break;
        }

    sprintf(cmd, "lfs setstripe -S %dm -c %d %s", stripe_count, stripe_size_MB, tmp);

    if (system(cmd) < 0) {
        printf("==PDC_SERVER: Fail to set Lustre stripe parameters [%s]\n", tmp);
        ret_value = FAIL;
        goto done;
    }

done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
}

/*
 * Perform the IO request with different IO system
 *
 * \param  region_list_head[IN]     List of IO requests
 * \param  plugin[IN]               IO system to be used
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_regions_io(region_list_t *region_list_head, PDC_io_plugin_t plugin)
{
    perr_t ret_value = SUCCEED;

    FUNC_ENTER(NULL);

    // If read, need to get locations from metadata server

    if (plugin == POSIX) {
        ret_value = PDC_Server_posix_one_file_io(region_list_head);
        if (ret_value !=  SUCCEED) {
            printf("==PDC_SERVER[%d]: PDC_Server_regions_io - error with PDC_Server_posix_one_file_io\n",
                    pdc_server_rank_g);
            goto done;
        }
    }
    else if (plugin == DAOS) {
        printf("DAOS plugin in under development, switch to POSIX instead.\n");
        ret_value = PDC_Server_posix_one_file_io(region_list_head);
    }
    else {
        printf("==PDC_SERVER: unsupported IO plugin!\n");
        ret_value = FAIL;
        goto done;
    }

done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
}

/*
 * Write the data from clients' shared memory to persistant storage
 *
 * \param  region_list_head[IN]     List of IO requests
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_data_write_from_shm(region_list_t *region_list_head)
{
    perr_t ret_value = SUCCEED;
    /* region_list_t *merged_list = NULL; */
    region_list_t *elt;

    FUNC_ENTER(NULL);

    // Sort the list so it is ordered by client id
    /* DL_SORT(region_list_head, region_list_cmp_by_client_id); */

    // TODO: Merge regions
    /* PDC_Server_merge_region_list_naive(io_list->region_list_head, &merged_list); */
    /* printf("==PDC_SERVER: write regions after merge\n"); */
    /* DL_FOREACH(io_list->region_list_head, elt) { */
    /*     PDC_print_region_list(elt); */
    /* } */

    // Now we have a merged list of regions to be read,
    // so just write one by one
    DL_FOREACH(region_list_head, elt) {

        // Calculate io size
        size_t i = 0;
        if (elt->data_size == 0) {
            elt->data_size = elt->count[0];
            for (i = 1; i < elt->ndim; i++)
                elt->data_size *= elt->count[i];
        }

        // Open shared memory and map to data buf
        elt->shm_fd = shm_open(elt->shm_addr, O_RDONLY, 0666);
        if (elt->shm_fd == -1) {
            printf("==PDC_SERVER: Shared memory open failed [%s]!\n", elt->shm_addr);
            ret_value = FAIL;
            goto done;
        }

        elt->buf= mmap(0, elt->data_size, PROT_READ, MAP_SHARED, elt->shm_fd, 0);
        if (elt->buf== MAP_FAILED) {
            printf("==PDC_CLIENT: Map failed: %s\n", strerror(errno));
            // close and unlink?
            ret_value = FAIL;
            goto done;
        }
        /* printf("==PDC_SERVER[%d]: PDC_Server_data_write_from_shm buf [%.*s]\n", */
        /*         pdc_server_rank_g, elt->data_size, elt->buf); */
    }

    // POSIX write
    ret_value = PDC_Server_regions_io(region_list_head, POSIX);
    if (ret_value != SUCCEED) {
        printf("==PDC_SERVER: PDC_Server_regions_io ERROR!\n");
        goto done;
    }

done:
    // Close all opened shared memory
    DL_FOREACH(region_list_head, elt) {
        ret_value = PDC_Server_close_shm(elt);
        if (ret_value != SUCCEED) {
            printf("==PDC_SERVER: error closing shared memory\n");
            goto done;
        }
    }

    fflush(stdout);
    FUNC_LEAVE(ret_value);
} // PDC_Server_data_write_from_shm

/*
 * Perform the IO request via shared memory
 *
 * \param  callback_info[IN]     Mercury callback info
 *
 * \return Non-negative on success/Negative on failure
 */
hg_return_t PDC_Server_data_io_via_shm(const struct hg_cb_info *callback_info)
{
    perr_t ret_value = SUCCEED;
    perr_t status    = SUCCEED;

    int i, count;
    uint32_t client_id;
    pdc_data_server_io_list_t *io_list_elt = NULL, *io_list = NULL, *io_list_target = NULL;
    region_list_t *region_elt = NULL, *region_tmp = NULL;

    FUNC_ENTER(NULL);

    data_server_io_info_t *io_info = (data_server_io_info_t*) callback_info->arg;

    if (io_info->io_type == WRITE)
        io_list = pdc_data_server_write_list_head_g;
    else if (io_info->io_type == READ)
        io_list = pdc_data_server_read_list_head_g;
    else {
        printf("==PDC_SERVER: PDC_Server_data_io_via_shm - invalid IO type received from client!\n");
        ret_value = FAIL;
        goto done;
    }

#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_lock(&data_write_list_mutex_g);
#endif
    // Iterate io list, find the IO list and region of current request
    DL_FOREACH(io_list, io_list_elt) {
        /* printf("io_list_elt obj id: %" PRIu64 "\n", io_list_elt->obj_id); */
        /* fflush(stdout); */
        if (io_info->meta.obj_id == io_list_elt->obj_id) {
            io_list_target = io_list_elt;
            break;
        }
    }
#ifdef ENABLE_MULTITHREAD
    hg_thread_mutex_unlock(&data_write_list_mutex_g);
#endif

    // If not found, create and insert one to the list
    if (NULL == io_list_target) {

        if (is_debug_g == 1) {
            printf("==PDC_SERVER: No existing io request with same obj_id %" PRIu64 " found!\n",
                                                                        io_info->meta.obj_id);
        }
        io_list_target = (pdc_data_server_io_list_t*)calloc(1, sizeof(pdc_data_server_io_list_t));
        if (NULL == io_list_target) {
            printf("==PDC_SERVER: ERROR allocating pdc_data_server_io_list_t!\n");
            ret_value = FAIL;
            goto done;
        }
        io_list_target->obj_id = io_info->meta.obj_id;
        io_list_target->total  = io_info->nclient;
        io_list_target->count  = 0;
        io_list_target->ndim   = io_info->meta.ndim;
        for (i = 0; i < io_info->meta.ndim; i++)
            io_list_target->dims[i] = io_info->meta.dims[i];

        io_list_target->total_size  = 0;

        // TODO: store on BB and Lustre
        // Auto generate a data location path for storing the data
        strcpy(io_list_target->path, io_info->meta.data_location);
        io_list_target->region_list_head = NULL;

        // Only add to the io list if there are more clients participating in this io request
        if (io_info->nclient > 1) {
            DL_APPEND(io_list, io_list_target);
            if (io_info->io_type == WRITE && pdc_data_server_write_list_head_g == NULL)
                pdc_data_server_write_list_head_g = io_list_target;
            else if (io_info->io_type == READ && pdc_data_server_read_list_head_g == NULL)
                pdc_data_server_read_list_head_g  = io_list_target;
        }
    }

    io_list_target->count++;
    if (is_debug_g == 1) {
        printf("==PDC_SERVER[%d]: received %d/%d data %s requests of [%s]\n",
                pdc_server_rank_g, io_list_target->count, io_list_target->total,
                io_info->io_type == READ? "read": "write", io_info->meta.obj_name);
        fflush(stdout);
    }

    // Init current request region
    region_list_t *new_region = (region_list_t*)calloc(1, sizeof(region_list_t));
    if (new_region == NULL) {
        printf("==PDC_SERVER: ERROR allocating new_region!\n");
        ret_value = FAIL;
        goto done;
    }
    pdc_region_list_t_deep_cp(&(io_info->region), new_region);

    // Calculate size
    uint64_t tmp_total_size = 1;
    for (i = 0; i < io_info->meta.ndim; i++)
        tmp_total_size *= new_region->count[i];
    io_list_target->total_size += tmp_total_size;

    // Add current request region to it the io list
    DL_APPEND(io_list_target->region_list_head, new_region);

    /* DL_COUNT(io_list_target->region_list_head, region_elt, count); */
    /* printf("Added 1 to region_list_head, obj_id=%" PRIu64 ", %d total\n", new_region->meta->obj_id, count); */
    /* PDC_print_region_list(new_region); */

    // Check if we have received all requests
    if (io_list_target->count == io_list_target->total) {

        /* printf("\n\n\n\n"); */
        DL_FOREACH(io_list_target->region_list_head, region_elt) {
            /* printf("Region objid after append: %" PRIu64 "\n", region_elt->meta->obj_id); */
            // FIXME: there is something wrong with the obj_id, when 4 clients send 1 request collectively,
            // one of the request region's metadata's obj_id becomes 0
            if (region_elt->meta->obj_id != io_list_target->obj_id) {
                PDC_print_region_list(region_elt);
                region_elt->meta->obj_id = io_list_target->obj_id;
            }
        }

        if (is_debug_g) {
            printf("==PDC_SERVER[%d]: received all %d requests, starts %s [%s]\n",
                    pdc_server_rank_g, io_list_target->total,
                    io_info->io_type == READ? "reading from ": "writing to ", io_list_target->path);
        }

        if (io_info->io_type == READ) {

            /* DL_FOREACH(io_list_target->region_list_head, region_elt) */
            /*     sprintf(region_elt->storage_location, "%s/s%03d.bin", io_list->path, pdc_server_rank_g); */
            // Storage location is obtained later
            status = PDC_Server_data_read_to_shm(io_list_target->region_list_head, io_list_target->obj_id);
            if (status != SUCCEED) {
                printf("==PDC_SERVER[%d]: PDC_Server_data_read_to_shm FAILED!\n", pdc_server_rank_g);
                ret_value = FAIL;
                goto done;
            }
        }
        else if (io_info->io_type == WRITE) {

            // Specify the location of data to be written to
            DL_FOREACH(io_list_target->region_list_head, region_elt) {
                sprintf(region_elt->storage_location, "%s/s%03d.bin", io_list_target->path, pdc_server_rank_g);
                /* printf("region to write obj_id: %" PRIu64 "\n", region_elt->meta->obj_id); */
                /* PDC_print_region_list(region_elt); */
            }

            status = PDC_Server_data_write_from_shm(io_list_target->region_list_head);
            if (status != SUCCEED) {
                printf("==PDC_SERVER[%d]: PDC_Server_data_write_from_shm FAILED!\n", pdc_server_rank_g);
                ret_value = FAIL;
                goto done;
            }
        }
        else {
            printf("==PDC_SERVER: PDC_Server_data_io_via_shm - invalid IO type received from client!\n");
            ret_value = FAIL;
            goto done;
        }

        if (status != SUCCEED) {
            printf("==PDC_SERVER[%d]: ERROR %s [%s]!\n", pdc_server_rank_g,
                    io_info->io_type == WRITE? "writing to": "reading from", io_list_target->path);
            ret_value = FAIL;
            goto done;
        }

        /* sleep(10); */

        // IO is done, notify all clients
        region_elt = NULL;

        /* DL_COUNT(io_list_target->region_list_head, region_elt, count); */
        /* printf("region_list_head: %d total\n", count); */
        DL_FOREACH(io_list_target->region_list_head, region_elt) {
            /* PDC_print_region_list(region_elt); */
            for (i = 0; i < PDC_SERVER_MAX_PROC_PER_NODE; i++) {
                if (i != 0 && region_elt->client_ids[i] == 0)
                    break;

                client_id = region_elt->client_ids[i];
                if (client_id >= (uint32_t)pdc_client_num_g) {
                    printf("==PDC_SERVER[%d]: PDC_Server_data_io_via_shm - error with client_id=%u/%d notify!\n",
                           pdc_server_rank_g, client_id, pdc_client_num_g);
                    break;
                }

                if (is_debug_g ) {
                    printf("==PDC_SERVER[%d]: Finished %s request, notify client %u\n",
                            pdc_server_rank_g, io_info->io_type == READ? "read": "write", client_id);
                    fflush(stdout);
                }

                if (io_info->io_type == WRITE)
                    region_elt->shm_addr[0] = 0;

                // TODO: currently assumes each region is for one client only!
                //       if one region is merged from the requests from multiple clients
                //       the shm_addr needs to be properly offseted when returning to client
                ret_value = PDC_Server_notify_io_complete_to_client(client_id, io_list_target->obj_id,
                                                    region_elt->shm_addr, io_info->io_type);
                if (ret_value != SUCCEED) {
                    printf("PDC_SERVER[%d]: PDC_Server_notify_io_complete_to_client FAILED!\n",
                            pdc_server_rank_g);
                    /* goto done; */
                }

            } // End of for
        } // End of DL_FOREACH

        if (is_debug_g == 1) {
            printf("==PDC_SERVER[%d]: finished writing to storage, and notified all clients\n",
                    pdc_server_rank_g);
        }
        // Remove all regions of the finished request
        region_elt = NULL;
        DL_FOREACH_SAFE(io_list_target->region_list_head, region_elt, region_tmp) {
            /* PDC_print_region_list(region_elt); */
            DL_DELETE(io_list_target->region_list_head, region_elt);
            free(region_elt);
            printf("Deleted one region\n");
            fflush(stdout);
        }

        // Check if this is the last one from the list
        DL_COUNT(io_list, io_list_elt, count);
        if (count == 1) {
            free(io_list);
            if (io_info->io_type == WRITE)
                pdc_data_server_write_list_head_g = NULL;
            else if (io_info->io_type == READ)
                pdc_data_server_read_list_head_g = NULL;
        }
        else {
            /* DL_DELETE(io_list, io_list_target); */
            /* free(io_list_target); */
        }

    } // end of if (io_list_target->count == io_list_target->total)
    else if (io_list_target->count > io_list_target->total) {
        printf("==PDC_SERVER[%d]: received more requested than requested, %d/%d!\n",
                pdc_server_rank_g, io_list_target->count, io_list_target->total);
        ret_value = FAIL;
        goto done;
    }


    ret_value = SUCCEED;

done:
    /* free(io_info); */
    fflush(stdout);
    FUNC_LEAVE(ret_value);
} // end of PDC_Server_data_write

/*
 * Update the storage location information of the corresponding metadata that is stored locally
 *
 * \param  region[IN]     Region info of the data that's been written by server
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_update_local_region_storage_loc(region_list_t *region)
{
    perr_t ret_value = SUCCEED;
    pdc_metadata_t *target_meta = NULL, *region_meta = NULL;
    region_list_t  *region_elt = NULL, *new_region = NULL;
    int update_success = -1;
    uint32_t i = 0;

    FUNC_ENTER(NULL);

/* printf("==PDC_SERVER: update region storage location\n"); */
    region_meta = region->meta;
    if (region->meta == NULL) {
        printf("==PDC_SERVER[%d] PDC_Server_update_local_region_storage_loc FAIL to get request metadata\n",
                pdc_server_rank_g);
        ret_value = FAIL;
        goto done;
    }

    // Find object metadata
    target_meta = PDC_Server_get_obj_metadata(region_meta->obj_id);
    if (target_meta == NULL) {
        printf("==PDC_SERVER[%d] PDC_Server_update_local_region_storage_loc FAIL to get storage metadata\n",
                pdc_server_rank_g);
        ret_value = FAIL;
        goto done;
    }

    DL_FOREACH(target_meta->storage_region_list_head, region_elt) {
        if (PDC_is_same_region_list(region_elt, region) == 1) {
            strcpy(region_elt->storage_location, region->storage_location);
            region_elt->offset = region->offset;
            update_success = 1;
            break;
        }
        /* PDC_print_storage_region_list(region_elt); */
    } // DL_FOREACH

    if (update_success == -1) {
        /* printf("==PDC_SERVER: create new region location/offset\n"); */
        // Create the region list
        new_region = (region_list_t*)malloc(sizeof(region_list_t));
        PDC_init_region_list(new_region);

        // Only copy the ndim, start, and count is sufficient
        new_region->ndim = region->ndim;
        for (i = 0; i < new_region->ndim; i++) {
            new_region->start[i] = region->start[i];
            new_region->count[i] = region->count[i];
            /* new_region->stride[i] = region->stride[i]; */
        }
        strcpy(new_region->storage_location, region->storage_location);
        new_region->offset = region->offset;

        DL_APPEND(target_meta->storage_region_list_head, new_region);
        /* PDC_print_storage_region_list(new_region); */
    }

done:
    FUNC_LEAVE(ret_value);
}

/*
 * Callback function for the region location info update
 *
 * \param  callback_info[IN]     Mercury callback info
 *
 * \return Non-negative on success/Negative on failure
 */
static hg_return_t
PDC_Server_update_region_loc_cb(const struct hg_cb_info *callback_info)
{
    hg_return_t ret_value;
    server_lookup_args_t *lookup_args;
    hg_handle_t handle;
    metadata_update_out_t output;

    FUNC_ENTER(NULL);

    lookup_args = (server_lookup_args_t*) callback_info->arg;
    handle = callback_info->info.forward.handle;

    /* Get output from server*/
    ret_value = HG_Get_output(handle, &output);
    if (ret_value != HG_SUCCESS) {
        printf("==PDC_SERVER[%d]: PDC_Server_get_storage_info_cb: error HG_Get_output\n", pdc_server_rank_g);
        lookup_args->ret_int = -1;
        goto done;
    }

    if (is_debug_g ) {
        printf("PDC_Server_update_region_loc_cb: ret=%d\n", output.ret);
    }
    lookup_args->ret_int = output.ret;

done:
    work_todo_g--;
    HG_Free_output(handle, &output);
    FUNC_LEAVE(ret_value);
}

/*
 * Update the storage location information of the corresponding metadata that may be stored in a
 * remote server.
 *
 * \param  callback_info[IN]     Mercury callback info
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_update_region_storagelocation_offset(region_list_t *region)
{
    hg_return_t hg_ret;
    perr_t ret_value = SUCCEED;
    uint32_t server_id = 0;
    pdc_metadata_t *region_meta = NULL;

    FUNC_ENTER(NULL);


    if (region->storage_location == NULL) {
        printf("==PDC_SERVER: PDC_Server_update_region_storagelocation_offset() \
                cannot update storage location with NULL!\n");
        ret_value = FAIL;
        goto done;
    }

    region_meta = region->meta;
    if (region_meta == NULL) {
        printf("==PDC_SERVER: PDC_Server_update_region_storagelocation_offset() \
                cannot update storage location, region meta is NULL!\n");
        ret_value = FAIL;
        goto done;
    }

    server_id = PDC_get_server_by_obj_id(region_meta->obj_id, pdc_server_size_g);
    if (server_id == (uint32_t)pdc_server_rank_g) {
        // Metadata object is local, no need to send update RPC
        ret_value = PDC_Server_update_local_region_storage_loc(region);
        if (ret_value != SUCCEED)
            goto done;
    }
    else {
        /* if (pdc_remote_server_info_g[server_id].update_region_loc_handle_valid != 1) { */
            HG_Create(hg_context_g, pdc_remote_server_info_g[server_id].addr, update_region_loc_register_id_g,
                    &pdc_remote_server_info_g[server_id].update_region_loc_handle);
            /* pdc_remote_server_info_g[server_id].update_region_loc_handle_valid = 1; */
        /* } */

        /* printf("Sending updated region loc to target\n"); */
        server_lookup_args_t lookup_args;

        update_region_loc_in_t in;
        in.obj_id = region->meta->obj_id;
        in.storage_location = region->storage_location;
        in.offset = region->offset;
        pdc_region_list_t_to_transfer(region, &in.region);

        hg_ret = HG_Forward(pdc_remote_server_info_g[server_id].update_region_loc_handle,
                            PDC_Server_update_region_loc_cb, &lookup_args, &in);

        if (hg_ret != HG_SUCCESS) {
            fprintf(stderr, "PDC_Client_update_metadata_with_name(): Could not start HG_Forward()\n");
            return FAIL;
        }

        // Wait for response from server
        work_todo_g = 1;
        PDC_Server_check_response(&hg_context_g);

        HG_Destroy(pdc_remote_server_info_g[server_id].update_region_loc_handle);
    }

done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
} // end of PDC_Server_update_region_storagelocation_offset

/*
 * Callback function for get the metadata by ID
 *
 * \param  callback_info[IN]     Mercury callback info
 *
 * \return Non-negative on success/Negative on failure
 */
static hg_return_t
PDC_Server_get_metadata_by_id_cb(const struct hg_cb_info *callback_info)
{
    hg_return_t ret_value;
    pdc_metadata_t *meta = NULL;
    server_lookup_args_t *lookup_args;
    hg_handle_t handle;
    get_metadata_by_id_out_t output;

    FUNC_ENTER(NULL);

    lookup_args = (server_lookup_args_t*) callback_info->arg;
    handle = callback_info->info.forward.handle;

    ret_value = HG_Get_output(handle, &output);
    if (ret_value != HG_SUCCESS) {
        printf("==PDC_SERVER[%d]: PDC_Server_get_storage_info_cb: error HG_Get_output\n", pdc_server_rank_g);
        lookup_args->meta = NULL;
        goto done;
    }

    if (output.res_meta.obj_id != 0) {
        // TODO free metdata
        meta = (pdc_metadata_t*)malloc(sizeof(pdc_metadata_t));
        pdc_transfer_t_to_metadata_t(&output.res_meta, meta);
    }
    else {
        lookup_args->meta = NULL;
        printf("PDC_Server_get_metadata_by_id_cb: no valid metadata is retrieved\n");
    }

    lookup_args->meta = meta;

done:
    work_todo_g--;
    HG_Free_output(handle, &output);
    FUNC_LEAVE(ret_value);
} // PDC_Server_get_metadata_by_id_cb

/*
 * Get metadata of the object ID received from client from local metadata hash table
 *
 * \param  obj_id[IN]           Object ID
 * \param  res_metadata[IN]     Pointer of metadata of the specified object ID
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_get_local_metadata_by_id(uint64_t obj_id, pdc_metadata_t **res_meta)
{
    perr_t ret_value = SUCCEED;

    pdc_hash_table_entry_head *head;
    pdc_metadata_t *elt;
    hg_hash_table_iter_t hash_table_iter;
    int n_entry;

    FUNC_ENTER(NULL);

    *res_meta = NULL;

    if (metadata_hash_table_g != NULL) {
        // Since we only have the obj id, need to iterate the entire hash table
        n_entry = hg_hash_table_num_entries(metadata_hash_table_g);
        hg_hash_table_iterate(metadata_hash_table_g, &hash_table_iter);

        while (n_entry != 0 && hg_hash_table_iter_has_more(&hash_table_iter)) {
            head = hg_hash_table_iter_next(&hash_table_iter);
            // Now iterate the list under this entry
            DL_FOREACH(head->metadata, elt) {
                if (elt->obj_id == obj_id) {
                    *res_meta = elt;
                    goto done;
                }
            }
        }
    }
    else {
        printf("==PDC_SERVER: metadata_hash_table_g not initialized!\n");
        ret_value = FAIL;
        *res_meta = NULL;
        goto done;
    }

done:
    FUNC_LEAVE(ret_value);
} // PDC_Server_get_local_metadata_by_id

/*
 * Get metadata of the object ID received from client from (possibly remtoe) metadata hash table
 *
 * \param  obj_id[IN]           Object ID
 * \param  res_metadata[IN]     Pointer of metadata of the specified object ID
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_get_metadata_by_id(uint64_t obj_id, pdc_metadata_t **res_meta)
{
    hg_return_t hg_ret;
    perr_t ret_value = SUCCEED;
    uint32_t server_id = 0;

    FUNC_ENTER(NULL);

    server_id = PDC_get_server_by_obj_id(obj_id, pdc_server_size_g);
    if (server_id == (uint32_t)pdc_server_rank_g) {
        // Metadata object is local, no need to send update RPC
        ret_value = PDC_Server_get_local_metadata_by_id(obj_id, res_meta);
        if (ret_value != SUCCEED) {
            printf("==PDC_SERVER[%d]: PDC_Server_get_metadata_by_id failed!\n", pdc_server_rank_g);
            goto done;
        }
    }
    else {
        /* if (pdc_remote_server_info_g[server_id].get_metadata_by_id_handle_valid != 1) { */
            HG_Create(hg_context_g, pdc_remote_server_info_g[server_id].addr, get_metadata_by_id_register_id_g,
                    &pdc_remote_server_info_g[server_id].get_metadata_by_id_handle);
            /* pdc_remote_server_info_g[server_id].get_metadata_by_id_handle_valid = 1; */
        /* } */

        /* printf("Sending updated region loc to target\n"); */
        server_lookup_args_t lookup_args;

        get_metadata_by_id_in_t in;
        in.obj_id = obj_id;

        hg_ret = HG_Forward(pdc_remote_server_info_g[server_id].get_metadata_by_id_handle,
                PDC_Server_get_metadata_by_id_cb, &lookup_args, &in);

        if (hg_ret != HG_SUCCESS) {
            fprintf(stderr, "PDC_Server_get_metadata_by_id(): Could not start HG_Forward()\n");
            return FAIL;
        }

        // Wait for response from server
        work_todo_g = 1;
        PDC_Server_check_response(&hg_context_g);

        // Retrieved metadata is stored in lookup_args
        *res_meta = lookup_args.meta;

        HG_Destroy(pdc_remote_server_info_g[server_id].get_metadata_by_id_handle);
    }

done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
} // end of PDC_Server_get_metadata_by_id

/*
 * Serialize the region info structure for network transfer,
 * including ndim, start[], count[], storage loc
 *
 * \param  regions[IN]       List of region info to be serialized
 * \param  n_region[IN]      Number of regions in the list
 * \param  buf[OUT]          Serialized data
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_serialize_regions_info(region_list_t** regions, uint32_t n_region, void *buf)
{
    perr_t ret_value = SUCCEED;
    uint32_t i, j;
    uint32_t ndim, loc_len;
    uint32_t *uint32_ptr = NULL;
    uint64_t *uint64_ptr = NULL;
    char     *char_ptr   = NULL;

    FUNC_ENTER(NULL);

    if (regions == NULL || regions[0] == NULL) {
        printf("==PDC_SERVER: PDC_Server_serialize_regions_info NULL input!\n");
        ret_value = FAIL;
        goto done;
    }

    ndim = regions[0]->ndim;

    uint32_ptr  = (uint32_t*)buf;
    *uint32_ptr = n_region;

    uint32_ptr++;
    *uint32_ptr = ndim;

    uint32_ptr++;
    uint64_ptr = (uint64_t*)uint32_ptr;

    for (i = 0; i < n_region; i++) {
        if (regions[i] == NULL) {
            printf("==PDC_SERVER: PDC_Server_serialize_regions_info NULL input!\n");
            ret_value = FAIL;
            goto done;
        }
        for (j = 0; j < ndim; j++) {
            *uint64_ptr = regions[i]->start[j];
            uint64_ptr++;
            *uint64_ptr = regions[i]->count[j];
            uint64_ptr++;
        }

        loc_len = strlen(regions[i]->storage_location);
        uint32_ptr  = (uint32_t*)uint64_ptr;
        *uint32_ptr = loc_len;
        uint32_ptr++;

        char_ptr = (char*)uint32_ptr;
        if (loc_len <= 0) {
            printf("==PDC_SERVER: PDC_Server_serialize_regions_info invalid storage location [%s]!\n",
                                regions[i]->storage_location);
            ret_value = FAIL;
            goto done;
        }
        strcpy(char_ptr, regions[i]->storage_location);
        char_ptr[loc_len] = PDC_STR_DELIM;  // Delim to replace 0
        char_ptr += (loc_len + 1);

        uint64_ptr = (uint64_t*)char_ptr;
    }

done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
} // PDC_Server_serialize_regions_info

/*
 * Un-serialize the region info structure from network transfer,
 * including ndim, start[], count[], storage loc
 *
 * \param  buf[IN]            Serialized data
 * \param  regions[OUT]       List of region info that are un-serialized
 * \param  n_region[OUT]      Number of regions in the list
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_unserialize_regions_info(void *buf, region_list_t** regions, uint32_t *n_region)
{
    perr_t ret_value = SUCCEED;
    uint32_t i, j;
    uint32_t ndim, loc_len;
    uint32_t *uint32_ptr = NULL;
    uint64_t *uint64_ptr = NULL;
    char     *char_ptr   = NULL;

    FUNC_ENTER(NULL);

    if (buf == NULL || regions == NULL || n_region == NULL) {
        printf("==PDC_SERVER: PDC_Server_unserialize_regions_info NULL input!\n");
        ret_value = FAIL;
        goto done;
    }

    /* total_len = strlen((char*)buf); */

    uint32_ptr = (uint32_t*)buf;
    *n_region = *uint32_ptr;

    uint32_ptr++;
    ndim = *uint32_ptr;

    uint32_ptr++;
    uint64_ptr = (uint64_t*)uint32_ptr;

    for (i = 0; i < *n_region; i++) {
        if (regions[i] == NULL) {
            printf("==PDC_SERVER: PDC_Server_unserialize_regions_info NULL input!\n");
            ret_value = FAIL;
            goto done;
        }
        for (j = 0; j < ndim; j++) {
            regions[i]->start[j] = *uint64_ptr;
            uint64_ptr++;
            regions[i]->count[j] = *uint64_ptr;
            uint64_ptr++;
        }

        uint32_ptr = (uint32_t*)uint64_ptr;
        loc_len = *uint32_ptr;

        uint32_ptr++;

        char_ptr = (char*)uint32_ptr;
        // Verify delimiter
        if (char_ptr[loc_len] != PDC_STR_DELIM) {
            printf("==PDC_SERVER: PDC_Server_unserialize_regions_info delim error!\n");
            ret_value = FAIL;
            goto done;
        }

        strncpy(regions[i]->storage_location, char_ptr, loc_len);
        // Add end of string
        regions[i]->storage_location[loc_len] = 0;

        char_ptr += (loc_len + 1);
        uint64_ptr = (uint64_t*)char_ptr;
// n_region|ndim|start00|count00|...|start0n|count0n|loc_len|loc_str|...
    }

done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
} // PDC_Server_unserialize_regions_info

/*
 * Calculate the total string length of the regions to be serialized
 *
 * \param  regions[IN]       List of region info that are un-serialized
 * \param  n_region[IN]      Number of regions in the list
 * \param  len[OUT]          Length of the serialized string
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_get_total_str_len(region_list_t** regions, uint32_t n_region, uint32_t *len)
{
    perr_t ret_value = SUCCEED;
    uint32_t i;

    FUNC_ENTER(NULL);

    if (regions == NULL || n_region == 0 || len == NULL || regions[0] == NULL) {
        printf("==PDC_SERVER: PDC_Server_get_total_str_len NULL input!\n");
        ret_value = FAIL;
        goto done;
    }

    *len = 0;
    for (i = 0; i < n_region; i++) {
        if (regions[i] == NULL) {
            printf("==PDC_SERVER: PDC_Server_get_total_str_len NULL input in regions!\n");
            ret_value = FAIL;
            goto done;
        }
        *len += (strlen(regions[i]->storage_location) + 1);
    }

    // n_region | ndim | start00 | count00 | ... | startndim0 | countndim0 | loc_len | loc | delim |
     *len += ( sizeof(uint32_t)*2 + sizeof(uint64_t)*regions[0]->ndim*2*n_region + sizeof(uint32_t)*n_region);

done:
    FUNC_LEAVE(ret_value);
}

/*
 * Test serialize/un-serialized code
 *
 * \return void
 */
void test_serialize()
{
    region_list_t **head = NULL, *a, *b, *c, *d;
    head = (region_list_t**)malloc(sizeof(region_list_t*) * 4);
    a = (region_list_t*)malloc(sizeof(region_list_t));
    b = (region_list_t*)malloc(sizeof(region_list_t));
    c = (region_list_t*)malloc(sizeof(region_list_t));
    d = (region_list_t*)malloc(sizeof(region_list_t));

    head[0] = a;
    head[1] = b;
    head[2] = c;
    head[3] = d;

    PDC_init_region_list(a);
    PDC_init_region_list(b);
    PDC_init_region_list(c);
    PDC_init_region_list(d);

    a->ndim = 2;
    a->start[0] = 0;
    a->start[1] = 4;
    a->count[0] = 10;
    a->count[1] = 14;
    sprintf(a->storage_location, "%s", "/path/to/a/a/a/a/a");

    b->ndim = 2;
    b->start[0] = 10;
    b->start[1] = 14;
    b->count[0] = 100;
    b->count[1] = 104;
    sprintf(b->storage_location, "%s", "/path/to/b/b");


    c->ndim = 2;
    c->start[0] = 20;
    c->start[1] = 21;
    c->count[0] = 23;
    c->count[1] = 24;
    sprintf(c->storage_location, "%s", "/path/to/c/c/c/c");


    d->ndim = 2;
    d->start[0] = 110;
    d->start[1] = 111;
    d->count[0] = 70;
    d->count[1] = 71;
    sprintf(d->storage_location, "%s", "/path/to/d");

    uint32_t total_str_len = 0;
    uint32_t n_region = 4;
    PDC_Server_get_total_str_len(head, n_region, &total_str_len);

    void *buf = (void*)malloc(total_str_len);

    PDC_Server_serialize_regions_info(head, n_region, buf);

    region_list_t **regions = (region_list_t**)malloc(sizeof(region_list_t*) * PDC_MAX_OVERLAP_REGION_NUM);
    uint32_t i;
    for (i = 0; i < n_region; i++) {
        regions[i] = (region_list_t*)malloc(sizeof(region_list_t));
        PDC_init_region_list(regions[i]);
    }

    PDC_Server_unserialize_regions_info(buf, regions, &n_region);

}

/*
 * Perform the POSIX read of multiple storage regions that overlap with the read request
 *
 * \param  ndim[IN]                 Number of dimension
 * \param  req_start[IN]            Start offsets of the request
 * \param  req_count[IN]            Counts of the request
 * \param  storage_start[IN]        Start offsets of the storage region
 * \param  storage_count[IN]        Counts of the storage region
 * \param  fp[IN]                   File pointer
 * \param  storage region[IN]       File offset of the first storage region
 * \param  buf[OUT]                 Data buffer to be read to
 * \param  total_read_bytes[OUT]    Total number of bytes read
 *
 * \return Non-negative on success/Negative on failure
 */
// For each intersecteed region in storage, calculate the actual overlapping regions'
// start[] and count[], then read into the buffer with correct offset
perr_t PDC_Server_read_overlap_regions(uint32_t ndim, uint64_t *req_start, uint64_t *req_count,
                                       uint64_t *storage_start, uint64_t *storage_count,
                                       FILE *fp, uint64_t file_offset, void *buf,  size_t *total_read_bytes)
{
    perr_t ret_value = SUCCEED;
    uint64_t overlap_start[DIM_MAX], overlap_count[DIM_MAX];
    uint64_t buf_start[DIM_MAX];
    uint64_t storage_start_physical[DIM_MAX];
    uint64_t buf_offset = 0, storage_offset = file_offset, total_bytes = 0, read_bytes = 0, row_offset = 0;
    uint64_t i = 0, j = 0;
    int is_all_selected = 0;


    FUNC_ENTER(NULL);

    *total_read_bytes = 0;
    if (ndim > 3 || ndim <= 0) {
        printf("==PDC_SERVER[%d]: dim=%" PRIu32 " unsupported yet!", pdc_server_rank_g, ndim);
        ret_value = FAIL;
        goto done;
    }

    // Get the actual start and count of region in storage
    get_overlap_start_count(ndim, req_start, req_count, storage_start, storage_count,
                            overlap_start, overlap_count);

    total_bytes = 1;
    for (i = 0; i < ndim; i++) {
        total_bytes              *= overlap_count[i];
        buf_start[i]              = overlap_start[i] - req_start[i];
        storage_start_physical[i] = overlap_start[i] - storage_start[i];
        if (i == 0) {
            buf_offset = buf_start[0];
            storage_offset += storage_start_physical[0];
        }
        else if (i == 1)  {
            buf_offset += buf_start[1] * req_count[i-1];
            storage_offset += storage_start_physical[1] * storage_count[0];
        }
        else if (i == 2)  {
            buf_offset += buf_start[2] * req_count[0] * req_count[1];
            storage_offset += storage_start_physical[2] * storage_count[0] * storage_count[1];
        }
    }

    if (is_debug_g == 1) {
        for (i = 0; i < ndim; i++) {
            printf("==PDC_SERVER[%d]: overlap start %" PRIu64 ", "
                   "storage_start  %" PRIu64 ", req_start %" PRIu64 " \n",
                   pdc_server_rank_g, overlap_start[i], storage_start[i], req_start[i]);
        }

        for (i = 0; i < ndim; i++) {
            printf("==PDC_SERVER[%d]: dim=%" PRIu32 ", read with storage start %" PRIu64 ","
                    " to buffer offset %" PRIu64 ", of size %" PRIu64 " \n",
                    pdc_server_rank_g, ndim, storage_start_physical[i], buf_start[i], overlap_count[i]);
        }
        fflush(stdout);
    }

    // Check if the entire storage region is selected
    is_all_selected = 1;
    for (i = 0; i < ndim; i++) {
        if (overlap_start[i] != storage_start[i] || overlap_count[i] != storage_count[i]) {
            is_all_selected = -1;
            break;
        }
    }

    /* printf("ndim = %" PRIu64 ", is_all_selected=%d\n", ndim, is_all_selected); */

    // TODO: additional optimization to check if any dimension is entirely selected
    if (ndim == 1 || is_all_selected == 1) {
        fseek (fp, storage_offset, SEEK_SET);

        if (is_debug_g == 1) {
            printf("==PDC_SERVER[%d]: read storage offset %" PRIu64 ", buf_offset  %" PRIu64 "\n",
                    pdc_server_rank_g, storage_offset, buf_offset);
        }

        // Can read the entire storage region at once
        read_bytes = fread(buf+buf_offset, 1, total_bytes, fp);
        if (read_bytes != total_bytes) {
            printf("==PDC_SERVER[%d]: PDC_Server_read_overlap_regions() fread failed!\n", pdc_server_rank_g);
            ret_value= FAIL;
            goto done;
        }
        *total_read_bytes += read_bytes;
        /* printf("Read entire storage region, size=%" PRIu64 ": [%.*s]\n", read_bytes, */
        /*                                 read_bytes, buf+buf_offset); */
    } // end if
    else {
        // NOTE: assuming row major, read overlapping region row by row
        if (ndim == 2) {
            row_offset = 0;
            fseek (fp, storage_offset, SEEK_SET);
            for (i = 0; i < overlap_count[1]; i++) {
                // Move to next row's begining position
                if (i != 0) {
                    fseek (fp, storage_count[0] - overlap_count[0], SEEK_CUR);
                    row_offset = i * req_count[0];
                }
                read_bytes = fread(buf+buf_offset+row_offset, 1, overlap_count[0], fp);
                if (read_bytes != overlap_count[0]) {
                    if (is_debug_g == 1) {
                        printf("==PDC_SERVER[%d]: PDC_Server_read_overlap_regions() fread failed!\n",
                               pdc_server_rank_g);
                    }
                    ret_value= FAIL;
                    goto done;
                }
                *total_read_bytes += read_bytes;
                /* printf("Row %" PRIu64 ", Read data size=%" PRIu64 ": [%.*s]\n", i, overlap_count[0], */
                /*                                 overlap_count[0], buf+buf_offset+row_offset); */
            } // for each row
        } // ndim=2
        else if (ndim == 3) {

            if (is_debug_g == 1) {
                printf("read count: %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n",
                        overlap_count[0], overlap_count[1], overlap_count[2]);
            }

            uint64_t buf_serialize_offset;
            /* fseek (fp, storage_offset, SEEK_SET); */
            for (j = 0; j < overlap_count[2]; j++) {

                fseek (fp, storage_offset + j*storage_count[0]*storage_count[1], SEEK_SET);
                /* printf("seek offset: %" PRIu64 "\n", storage_offset + j*storage_count[0]*storage_count[1]); */

                for (i = 0; i < overlap_count[1]; i++) {
                    // Move to next row's begining position
                    if (i != 0)
                        fseek (fp, storage_count[0] - overlap_count[0], SEEK_CUR);

                    buf_serialize_offset = buf_offset + i*req_count[0] + j*req_count[0]*req_count[1];
                    if (is_debug_g == 1) {
                        printf("Read to buf offset: %" PRIu64 "\n", buf_serialize_offset);
                    }

                    read_bytes = fread(buf+buf_serialize_offset, 1, overlap_count[0], fp);
                    if (read_bytes != overlap_count[0]) {
                        printf("==PDC_SERVER[%d]: PDC_Server_read_overlap_regions() fread failed!\n",
                               pdc_server_rank_g);
                        ret_value= FAIL;
                        goto done;
                    }
                    *total_read_bytes += read_bytes;
                    if (is_debug_g == 1) {
                        printf("z: %" PRIu64 ", j: %" PRIu64 ", Read data size=%" PRIu64 ": [%.*s]\n",
                                j, i, overlap_count[0], (int)overlap_count[0], (char*)buf+buf_serialize_offset);
                    }
                } // for each row
            }

        }

    } // end else (ndim != 1 && !is_all_selected);


    if (total_bytes != *total_read_bytes) {
        printf("==PDC_SERVER[%d]: PDC_Server_read_overlap_regions() read size error!\n", pdc_server_rank_g);
        ret_value = FAIL;
        goto done;
    }


done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
}

/*
 * Read with POSIX within one file
 *
 * \param  region[IN]       Region info of IO request
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_posix_one_file_io(region_list_t* region)
{
    perr_t ret_value = SUCCEED;
    size_t read_bytes = 0, write_bytes = 0;
    size_t total_read_bytes = 0;
    uint32_t offset = 0;
    uint32_t n_storage_regions = 0, i = 0;
    region_list_t *region_elt = NULL, *previous_region = NULL;
    region_list_t **overlap_regions = NULL;
    FILE *fp_read = NULL, *fp_write = NULL;
    char *prev_path = NULL;

    FUNC_ENTER(NULL);

    if (NULL == region) {
        printf("==PDC_SERVER[%d]: PDC_Server_posix_one_file_io NULL input!\n", pdc_server_rank_g);
        ret_value = FAIL;
        goto done;
    }

    // Allocate for temporary overlap region storage info if there is a read in list
    DL_FOREACH(region, region_elt) {
        if (region_elt->access_type == READ) {
            overlap_regions = (region_list_t**)calloc(sizeof(region_list_t*), PDC_MAX_OVERLAP_REGION_NUM);
            for (i = 0; i < PDC_MAX_OVERLAP_REGION_NUM; i++) {
                overlap_regions[i] = (region_list_t*)malloc(sizeof(region_list_t));
                PDC_init_region_list(overlap_regions[i]);
            }
            break;
        }
    }

    // Iterate over all region IO requests
    DL_FOREACH(region, region_elt) {

        if (region_elt->access_type == READ) {

            // For each region, need to contact metadata server to get its
            // storage location (may span multiple storage regions/files)
            // and offsets
            ret_value = PDC_Server_get_storage_location_of_region(region_elt, &n_storage_regions,
                    overlap_regions);
            if (ret_value != SUCCEED) {
                printf("==PDC_SERVER[%d]: PDC_Server_get_storage_location_of_region failed!\n",
                                                                            pdc_server_rank_g);
                goto done;
            }

            /* printf("PDC_SERVER: found %d overlap storage regions.\n", n_storage_regions); */

            // Now for each storage region that overlaps with request region,
            // read with the corresponding offset and size
            for (i = 0; i < n_storage_regions; i++) {

                /* printf("==PDC_SERVER: overlapping storage regions %d\n", n_storage_regions); */
                /* printf("=========================================\n"); */
                /* PDC_print_storage_region_list(overlap_regions[i]); */
                /* printf("=========================================\n"); */

                // If a new file needs to be opened
                if (prev_path == NULL || strcmp(overlap_regions[i]->storage_location, prev_path) != 0) {

                    if (fp_read != NULL)  {
                        fclose(fp_read);
                        fp_read = NULL;
                    }

                    fp_read = fopen(overlap_regions[i]->storage_location, "rb");
                    if (fp_read == NULL) {
                        printf("==PDC_SERVER: fopen failed [%s]\n", region->storage_location);
                        ret_value = FAIL;
                        goto done;
                    }
                }

                // Request: elt->start/count
                // Storage: overlap_regions[i]->start/count
                ret_value = PDC_Server_read_overlap_regions(region_elt->ndim, region_elt->start,
                            region_elt->count, overlap_regions[i]->start, overlap_regions[i]->count,
                            fp_read, overlap_regions[i]->offset, region_elt->buf, &read_bytes);

                if (ret_value != SUCCEED) {
                    printf("==PDC_SERVER[%d]: error with PDC_Server_read_overlap_regions\n",
                            pdc_server_rank_g);
                    fclose(fp_read);
                    fp_read = NULL;

                    goto done;
                }
                total_read_bytes += read_bytes;

                prev_path = overlap_regions[i]->storage_location;
            } // end of for all overlapping storage regions for one request region

            /* printf("Write iteration: size %" PRIu64 "\n", region_elt->data_size); */
            region_elt->is_data_ready = 1;
            region_elt->offset = offset;
            offset += total_read_bytes;

        } // end of READ
        else if (region_elt->access_type == WRITE) {

            // Assumes all regions are written to one file
            if (region_elt->storage_location == NULL) {
                printf("==PDC_SERVER: PDC_Server_posix_one_file_io - storage_location is NULL!\n");
                ret_value = FAIL;
                goto done;
            }

            if (previous_region == NULL
                    || strcmp(region_elt->storage_location, previous_region->storage_location) != 0) {

                // mkdir and set lustre premeters
                pdc_mkdir(region_elt->storage_location);
#ifdef ENABLE_LUSTRE
                PDC_Server_set_lustre_stripe(region_elt->storage_location, 248, 16);
#endif
                if (fp_write != NULL) {
                    fclose(fp_write);
                    fp_write = NULL;
                }
                // Append the current write data
                // TODO: need to recycle file space in cases of data update and delete
                fp_write = fopen(region_elt->storage_location, "ab");
                if (NULL == fp_write) {
                    printf("==PDC_SERVER: fopen failed [%s]\n", region_elt->storage_location);
                    ret_value = FAIL;
                    goto done;
                }
                /* printf("write location is %s\n", region_elt->storage_location); */
            }

            // Get the current write offset
            offset = ftell(fp_write);
            write_bytes = fwrite(region_elt->buf, 1, region_elt->data_size, fp_write);
            if (write_bytes != region_elt->data_size) {
                printf("==PDC_SERVER[%d]: fwrite ERROR!\n", pdc_server_rank_g);
                ret_value= FAIL;
                goto done;
            }

            /* fclose(fp_write); */

            /* printf("Write data offset: %" PRIu64 ", size %" PRIu64 "\n", offset, region_elt->data_size); */
            /* printf("Write data buf: [%.*s]\n", region_elt->data_size, (char*)region_elt->buf); */
            region_elt->is_data_ready = 1;
            region_elt->offset = offset;

            // Update metadata with the location and offset
            /* printf("obj_id before update region storage offset: %" PRIu64 "\n", region_elt->meta->obj_id); */
            ret_value = PDC_Server_update_region_storagelocation_offset(region_elt);
            if (ret_value != SUCCEED) {
                printf("==PDC_SERVER[%d]: failed to update region storage info!\n", pdc_server_rank_g);
                goto done;
            }

            previous_region = region_elt;

        } // end of WRITE
        else {
            printf("==PDC_SERVER: PDC_Server_posix_one_file_io - unsupported access type\n");
            ret_value = FAIL;
            goto done;
        }
    } // end DL_FOREACH

done:
    if (overlap_regions != NULL) {
        for (i = 0; i < PDC_MAX_OVERLAP_REGION_NUM; i++)
            if (overlap_regions[i] != NULL)
                free(overlap_regions[i]);
        free(overlap_regions);
    }

    if (fp_write != NULL) {
        fclose(fp_write);
        fp_write = NULL;
    }

    if (fp_read != NULL) {
        fclose(fp_read);
        fp_write = NULL;
    }

    fflush(stdout);
    FUNC_LEAVE(ret_value);
} // PDC_Server_posix_one_file_io

// Insert the write request to a queue(list) for aggregation
/* perr_t PDC_Server_add_io_request(PDC_access_t io_type, pdc_metadata_t *meta, struct PDC_region_info *region_info, void *buf, uint32_t client_id) */
/* { */
/*     perr_t ret_value = SUCCEED; */
/*     pdc_data_server_io_list_t *elt = NULL, *io_list = NULL, *io_list_target = NULL; */
/*     region_list_t *region_elt = NULL; */

/*     FUNC_ENTER(NULL); */

/*     if (io_type == WRITE) */
/*         io_list = pdc_data_server_write_list_head_g; */
/*     else if (io_type == READ) */
/*         io_list = pdc_data_server_read_list_head_g; */
/*     else { */
/*         printf("==PDC_SERVER: PDC_Server_add_io_request_to_queue - invalid IO type!\n"); */
/*         ret_value = FAIL; */
/*         goto done; */
/*     } */

/* #ifdef ENABLE_MULTITHREAD */
/*     hg_thread_mutex_lock(&data_write_list_mutex_g); */
/* #endif */
/*     // Iterate io list, find the IO list and region of current request */
/*     DL_FOREACH(io_list, elt) { */
/*         if (meta->obj_id == elt->obj_id) { */
/*             io_list_target = elt; */
/*             break; */
/*         } */
/*     } */
/* #ifdef ENABLE_MULTITHREAD */
/*     hg_thread_mutex_unlock(&data_write_list_mutex_g); */
/* #endif */

/*     // If there is no IO list created for current obj_id, create one and insert it to the global list */
/*     if (NULL == io_list_target) { */

/*         /1* printf("==PDC_SERVER: No existing io request with same obj_id found!\n"); *1/ */
/*         io_list_target = (pdc_data_server_io_list_t*)malloc(sizeof(pdc_data_server_io_list_t)); */
/*         if (NULL == io_list_target) { */
/*             printf("==PDC_SERVER: ERROR allocating pdc_data_server_io_list_t!\n"); */
/*             ret_value = FAIL; */
/*             goto done; */
/*         } */
/*         io_list_target->obj_id = meta->obj_id; */
/*         io_list_target->total  = -1; */
/*         io_list_target->count  = 0; */
/*         io_list_target->ndim   = meta->ndim; */
/*         for (i = 0; i < meta->ndim; i++) */
/*             io_list_target->dims[i] = meta->dims[i]; */

/*         io_list_target->total_size  = 0; */

/*         // Auto generate a data location path for storing the data */
/*         strcpy(io_list_target->path, meta->data_location); */
/*         io_list_target->region_list_head = NULL; */

/*         DL_APPEND(io_list, io_list_target); */
/*     } */

/*     /1* printf("==PDC_SERVER[%d]: received %d/%d data %s requests of [%s]\n", pdc_server_rank_g, io_list_target->count, io_list_target->total, io_type == READ? "read": "write", meta->obj_name); *1/ */
/*     region_list_t *new_region = (region_list_t*)malloc(sizeof(region_list_t)); */
/*     if (new_region == NULL) { */
/*         printf("==PDC_SERVER: ERROR allocating new_region!\n"); */
/*         ret_value = FAIL; */
/*         goto done; */
/*     } */

/*     PDC_init_region_list(new_region); */
/*     perr_t pdc_region_info_to_list_t(region_info, new_region); */

/*     new_region->client_ids[0] = client_id; */

/*     // Calculate size */
/*     new_region->data_size = 1; */
/*     for (i = 0; i < new_region->ndim; i++) */
/*         new_region->data_size *= new_region->count[i]; */

/*     io_list_target->total_size += new_region->data_size; */
/*     io_list_targeta->count++; */

/*     // Insert current request to the IO list's region list head */
/*     DL_APPEND(io_list_target->region_list_head, new_region); */

/* done: */
/*     fflush(stdout); */
/*     FUNC_LEAVE(ret_value); */
/* } // end of PDC_Server_add_io_request */

/*
 * Directly server read/write buffer from/to storage of one region
 * Read with POSIX within one file
 *
 * \param  io_type[IN]           IO type (read/write)
 * \param  obj_id[IN]            Object ID
 * \param  region_info[IN]       Region info of IO request
 * \param  buf[IN/OUT]           Data buffer
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_data_io_direct(PDC_access_t io_type, uint64_t obj_id, struct PDC_region_info *region_info, void *buf)
{
    perr_t ret_value = SUCCEED;
    region_list_t *io_region = NULL;
    pdc_metadata_t *meta = NULL;
    size_t i;

    FUNC_ENTER(NULL);

    io_region = (region_list_t*)malloc(sizeof(region_list_t));
    PDC_init_region_list(io_region);

    pdc_region_info_to_list_t(region_info, io_region);

    // Generate a location for data storage for data server to write
    char *data_path = NULL;
    char *user_specified_data_path = getenv("PDC_DATA_LOC");
    if (user_specified_data_path != NULL)
        data_path = user_specified_data_path;
    else {
        data_path = getenv("SCRATCH");
        if (data_path == NULL)
            data_path = ".";
    }

    // Data path prefix will be $SCRATCH/pdc_data/$obj_id/
    sprintf(io_region->storage_location, "%s/pdc_data/%" PRIu64 "/s%03d.bin", data_path, obj_id, pdc_server_rank_g);
    pdc_mkdir(io_region->storage_location);
    printf("storage_location is %s\n", io_region->storage_location);
#ifdef ENABLE_LUSTRE
    PDC_Server_set_lustre_stripe(io_region->storage_location, 248, 16);
printf("lustre is enabled");
#endif
    io_region->access_type = io_type;

    io_region->data_size = io_region->count[0];
    for (i = 1; i < io_region->ndim; i++)
        io_region->data_size *= io_region->count[i];

    io_region->buf = buf;

    // Need to get the metadata
    meta = (pdc_metadata_t*)malloc(sizeof(pdc_metadata_t));
    ret_value = PDC_Server_get_metadata_by_id(obj_id, &meta);
    if (ret_value != SUCCEED) {
        printf("PDC_SERVER: PDC_Server_data_io_direct - unable to get metadata of object\n");
        goto done;
    }
    io_region->meta = meta;

    // Call the actual IO routine
    ret_value = PDC_Server_regions_io(io_region, POSIX);
    if (ret_value != SUCCEED) {
        printf("PDC_SERVER: PDC_Server_data_io_direct - unable perform server direct IO\n");
        goto done;
    }

done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
}

/*
 * Server writes buffer to storage of one region without client involvement
 * Read with POSIX within one file
 *
 * \param  obj_id[IN]            Object ID
 * \param  region_info[IN]       Region info of IO request
 * \param  buf[IN]               Data buffer
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_data_write_direct(uint64_t obj_id, struct PDC_region_info *region_info, void *buf)
{
    perr_t ret_value = SUCCEED;
    FUNC_ENTER(NULL);

    ret_value = PDC_Server_data_io_direct(WRITE, obj_id, region_info, buf);
    if (ret_value != SUCCEED) {
        printf("==PDC_SERVER[%d]: PDC_Server_data_write_direct() "
                "error with PDC_Server_data_io_direct()\n", pdc_server_rank_g);
        goto done;
    }

done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
}

/*
 * Server reads buffer from storage of one region without client involvement
 *
 * \param  obj_id[IN]            Object ID
 * \param  region_info[IN]       Region info of IO request
 * \param  buf[OUT]              Data buffer
 *
 * \return Non-negative on success/Negative on failure
 */
perr_t PDC_Server_data_read_direct(uint64_t obj_id, struct PDC_region_info *region_info, void *buf)
{
    perr_t ret_value = SUCCEED;
    FUNC_ENTER(NULL);

    ret_value = PDC_Server_data_io_direct(READ, obj_id, region_info, buf);
    if (ret_value != SUCCEED) {
        printf("==PDC_SERVER[%d]: PDC_Server_data_write_direct() "
                "error with PDC_Server_data_io_direct()\n", pdc_server_rank_g);
        goto done;
    }

done:
    fflush(stdout);
    FUNC_LEAVE(ret_value);
}
