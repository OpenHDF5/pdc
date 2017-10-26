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

#ifndef PDC_CLIENT_SERVER_COMMON_H
#define PDC_CLIENT_SERVER_COMMON_H

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "pdc_linkedlist.h"
#include "pdc_private.h"
#include "mercury.h"
#include "mercury_macros.h"
#include "mercury_proc_string.h"

#include "mercury_thread_pool.h"
#include "mercury_atomic.h"
#include "mercury_thread_mutex.h"
#include "mercury_hash_table.h"
#include "mercury_list.h"
#include "utils/art.h"

#include "pdc_obj_pkg.h"

#define ADDR_MAX 128
#define DIM_MAX  4
#define TAG_LEN_MAX 128
#define PDC_SERVER_ID_INTERVEL 1000000
#define PDC_SERVER_MAX_PROC_PER_NODE 64
#define PDC_SERIALIZE_MAX_SIZE 256

/* #define pdc_server_tmp_dir_g  "./pdc_tmp" */
/* extern char pdc_server_tmp_dir_g[ADDR_MAX]; */
#define pdc_server_cfg_name_g "server.cfg"

extern uint64_t pdc_id_seq_g;
extern int pdc_server_rank_g;

#define    PDC_LOCK_OP_OBTAIN  0
#define    PDC_LOCK_OP_RELEASE 1

#define PDC_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define PDC_MIN(x, y) (((x) < (y)) ? (x) : (y))

typedef enum { POSIX=0, DAOS=1 }       PDC_io_plugin_t;
typedef enum { READ=0, WRITE=1, NA=2 } PDC_access_t;
typedef enum { BLOCK=0, NOBLOCK=1 }    PDC_lock_mode_t;

typedef struct pdc_metadata_t pdc_metadata_t;


#ifdef ENABLE_INDEX

typedef struct {
    hg_const_string_t    key;
    hg_const_string_t    str_value;

    int64_t              int64_val;
    uint64_t             obj_id;
    uint32_t             key_hash;
    uint32_t             server_id;
} metadata_index_create_in_t;

static HG_INLINE hg_return_t
hg_proc_metadata_index_create_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    metadata_index_create_in_t *struct_data = (metadata_index_create_in_t*) data;

    ret = hg_proc_hg_const_string_t(proc, &struct_data->key);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_const_string_t(proc, &struct_data->str_value);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_int64_t(proc, &struct_data->int64_val);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint64_t(proc, &struct_data->obj_id);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint32_t(proc, &struct_data->key_hash);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint32_t(proc, &struct_data->server_id);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }

    return ret;
}

typedef struct {
    uint64_t ret;
} metadata_index_create_out_t;


static HG_INLINE hg_return_t
hg_proc_metadata_index_create_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    metadata_index_create_out_t *struct_data = (metadata_index_create_out_t*) data;

    ret = hg_proc_uint64_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }

    return ret;
}
#endif

typedef struct region_list_t {
    size_t   ndim;
    uint64_t start[DIM_MAX];
    uint64_t count[DIM_MAX];
    /* uint64_t stride[DIM_MAX]; */

    uint32_t client_ids[PDC_SERVER_MAX_PROC_PER_NODE];

    uint64_t data_size;
    int      is_data_ready;
    char     shm_addr[ADDR_MAX];
    int      shm_fd;
    char    *buf;
    char     storage_location[ADDR_MAX];
    uint64_t offset;
    int      reg_dirty;
    PDC_access_t access_type;
    hg_bulk_t bulk_handle;
    hg_addr_t addr;
    uint64_t  obj_id;
    uint64_t  reg_id;
    uint64_t  from_obj_id;
    int32_t   client_id;

    pdc_metadata_t *meta;

    struct region_list_t *prev;
    struct region_list_t *next;
    // 16 attributes, need to match init and deep_cp routines
} region_list_t;

// Similar structure PDC_region_info_t defined in pdc_obj_pkg.h
// TODO: currently only support upto 3D
typedef struct region_info_transfer_t {
    size_t ndim;
    uint64_t start_0, start_1, start_2, start_3;
    uint64_t count_0, count_1, count_2, count_3;
    /* uint64_t stride_0, stride_1, stride_2, stride_3; */
} region_info_transfer_t;

typedef struct pdc_metadata_transfer_t {
    int32_t     user_id;
    const char  *app_name;
    const char  *obj_name;
    int32_t     time_step;

    uint64_t    obj_id;

    int32_t     ndim;
    int32_t     dims0, dims1, dims2, dims3;

    const char  *tags;
    const char  *data_location;
    /* time_t      create_time; */
    /* time_t      last_modified_time; */
} pdc_metadata_transfer_t;

typedef struct metadata_query_transfer_in_t{
    int     is_list_all;

    int     user_id;                // Both server and client gets it and do security check
    const char    *app_name;
    const char    *obj_name;

    int     time_step_from;
    int     time_step_to;

    int     ndim;

    /* time_t  create_time_from; */
    /* time_t  create_time_to; */
    /* time_t  last_modified_time_from; */
    /* time_t  last_modified_time_to; */

    const char    *tags;
} metadata_query_transfer_in_t;

typedef struct {
    uint64_t                    obj_id;
    char                        shm_addr[ADDR_MAX];
} client_read_info_t;

typedef struct {
    int32_t client_id;
    int32_t nclient;
    char client_addr[ADDR_MAX];
} client_test_connect_args;

typedef struct PDC_mapping_info {
    pdcid_t                          remote_obj_id;         /* target of object id */
    pdcid_t                          remote_reg_id;         /* target of region id */
    int32_t                          remote_client_id;
    size_t                           remote_ndim;
    region_info_transfer_t           remote_region;
    hg_bulk_t                        remote_bulk_handle;
    hg_addr_t                        remote_addr;
    pdcid_t                          from_obj_id;
    PDC_LIST_ENTRY(PDC_mapping_info) entry;
} PDC_mapping_info_t;

typedef struct region_map_t {
// if keeping the struct of origin of region is needed?
    pdc_cnt_t                        mapping_count;        /* count the number of mapping of this region */
    pdcid_t                          local_obj_id;         /* origin of object id */
    pdcid_t                          local_reg_id;         /* origin of region id */
    size_t                           local_ndim;
    uint64_t                        *local_reg_size;
    hg_addr_t                        local_addr;
    hg_bulk_t                        local_bulk_handle;
    PDC_var_type_t                   local_data_type;
    PDC_LIST_HEAD(PDC_mapping_info)  ids;                  /* Head of list of IDs */

    struct region_map_t             *prev;
    struct region_map_t             *next;
} region_map_t;

// For storing metadata
typedef struct pdc_metadata_t {
    int     user_id;                // Both server and client gets it and do security check
    char    app_name[ADDR_MAX];
    char    obj_name[ADDR_MAX];
    int     time_step;
    // Above four are the unique identifier for objects

    uint64_t obj_id;
    time_t  create_time;
    time_t  last_modified_time;

    char    tags[TAG_LEN_MAX];
    char    data_location[ADDR_MAX];

    int     ndim;
    int     dims[DIM_MAX];

    // For region storage list
    region_list_t *storage_region_list_head;

    // For region lock list
    region_list_t *region_lock_head;

    // For region map
    region_map_t *region_map_head;

    // For hash table list
    struct pdc_metadata_t *prev;
    struct pdc_metadata_t *next;
    void *bloom;

} pdc_metadata_t;

typedef struct {
    PDC_access_t                io_type;
    uint32_t                    client_id;
    int32_t                     nclient;
    pdc_metadata_t              meta;
    region_list_t               region;
} data_server_io_info_t;

/* #ifdef HG_HAS_BOOST */
/* MERCURY_GEN_STRUCT_PROC( pdc_metadata_transfer_t, ((int32_t)(user_id)) ((int32_t)(time_step)) ((uint64_t)(obj_id)) ((int32_t)(ndim)) ((int32_t)(dims0)) ((int32_t)(dims1)) ((int32_t)(dims2)) ((int32_t)(dims3)) ((hg_const_string_t)(app_name)) ((hg_const_string_t)(obj_name)) ((hg_const_string_t)(data_location)) ((hg_const_string_t)(tags)) ) */

/* MERCURY_GEN_PROC( gen_obj_id_in_t, ((pdc_metadata_transfer_t)(data)) ((uint32_t)(hash_value)) ) */
/* MERCURY_GEN_PROC( gen_obj_id_out_t, ((uint64_t)(obj_id)) ) */

/* /1* MERCURY_GEN_PROC( send_obj_name_marker_in_t, ((hg_const_string_t)(obj_name)) ((uint32_t)(hash_value)) ) *1/ */
/* /1* MERCURY_GEN_PROC( send_obj_name_marker_out_t, ((int32_t)(ret)) ) *1/ */

/* MERCURY_GEN_PROC( client_test_connect_in_t, ((int32_t)(client_id)) ((int32_t)(nclient)) ((hg_const_string_t)(client_addr)) ) */
/* MERCURY_GEN_PROC( client_test_connect_out_t, ((int32_t)(ret)) ) */

/* MERCURY_GEN_PROC( server_lookup_client_in_t, ((int32_t)(server_id)) ((int32_t)(nserver)) ((hg_const_string_t)(server_addr)) ) */
/* MERCURY_GEN_PROC( server_lookup_client_out_t, ((int32_t)(ret)) ) */

/* MERCURY_GEN_PROC( server_lookup_remote_server_in_t, ((int32_t)(server_id)) ) */
/* MERCURY_GEN_PROC( server_lookup_remote_server_out_t, ((int32_t)(ret)) ) */

/* MERCURY_GEN_PROC( notify_io_complete_in_t, ((uint64_t)(obj_id)) ((int32_t)(io_type)) ((hg_const_string_t)(shm_addr)) ) */
/* MERCURY_GEN_PROC( notify_io_complete_out_t, ((int32_t)(ret)) ) */

/* MERCURY_GEN_PROC( close_server_in_t,  ((int32_t)(client_id)) ) */
/* MERCURY_GEN_PROC( close_server_out_t, ((int32_t)(ret)) ) */

/* MERCURY_GEN_PROC( notify_region_update_in_t, ((uint64_t)(obj_id)) ((uint64_t)(reg_id)) ) */
/* MERCURY_GEN_PROC( notify_region_update_out_t, ((int32_t)(ret)) ) */

/* MERCURY_GEN_PROC( close_server_in_t,  ((int32_t)(client_id)) ) */
/* MERCURY_GEN_PROC( close_server_out_t, ((int32_t)(ret)) ) */

/* MERCURY_GEN_STRUCT_PROC( metadata_query_transfer_in_t, ((int32_t)(is_list_all)) ((int32_t)(user_id)) ((hg_const_string_t)(app_name)) ((hg_const_string_t)(obj_name)) ((int32_t)(time_step_from)) ((int32_t)(time_step_to)) ((int32_t)(ndim)) ((hg_const_string_t)(tags)) ) */
/* /1* MERCURY_GEN_STRUCT_PROC( metadata_query_transfer_in_t, ((int32_t)(user_id)) ((hg_const_string_t)(app_name)) ((hg_const_string_t)(obj_name)) ((int32_t)(time_step_from)) ((int32_t)(time_step_to)) ((int32_t)(ndim)) ((int32_t)(create_time_from)) ((int32_t)(create_time_to)) ((int32_t)(last_modified_time_from)) ((int32_t)(last_modified_time_to)) ((hg_const_string_t)(tags)) ) *1/ */
/* MERCURY_GEN_PROC( metadata_query_transfer_out_t, ((hg_bulk_t)(bulk_handle)) ((int32_t)(ret)) ) */

/* MERCURY_GEN_PROC( metadata_query_in_t, ((hg_const_string_t)(obj_name)) ((uint32_t)(hash_value)) ) */
/* MERCURY_GEN_PROC( metadata_query_out_t, ((pdc_metadata_transfer_t)(ret)) ) */

/* MERCURY_GEN_PROC( metadata_delete_by_id_in_t, ((uint64_t)(obj_id)) ) */
/* MERCURY_GEN_PROC( metadata_delete_by_id_out_t, ((int32_t)(ret)) ) */

/* MERCURY_GEN_PROC( metadata_delete_in_t, ((hg_const_string_t)(obj_name)) ((int32_t)(time_step)) ((uint32_t)(hash_value)) ) */
/* MERCURY_GEN_PROC( metadata_delete_out_t, ((int32_t)(ret)) ) */

/* MERCURY_GEN_PROC( metadata_add_tag_in_t, ((uint64_t)(obj_id)) ((uint32_t)(hash_value)) ((hg_const_string_t)(new_tag)) ) */
/* MERCURY_GEN_PROC( metadata_add_tag_out_t, ((int32_t)(ret)) ) */

/* MERCURY_GEN_PROC( metadata_update_in_t, ((uint64_t)(obj_id)) ((uint32_t)(hash_value)) ((pdc_metadata_transfer_t)(new_metadata)) ) */
/* MERCURY_GEN_PROC( metadata_update_out_t, ((int32_t)(ret)) ) */

/* MERCURY_GEN_PROC( gen_obj_unmap_notification_in_t, ((uint64_t)(local_obj_id)) ) */
/* MERCURY_GEN_PROC( gen_obj_unmap_notification_out_t, ((int32_t)(ret)) ) */

/* MERCURY_GEN_PROC( gen_reg_unmap_notification_in_t, ((uint64_t)(local_obj_id)) ((uint64_t)(local_reg_id)) ) */
/* MERCURY_GEN_PROC( gen_reg_unmap_notification_out_t, ((int32_t)(ret)) ) */

/* MERCURY_GEN_PROC( gen_reg_map_notification_in_t, ((uint64_t)(local_obj_id)) ((uint64_t)(local_reg_id)) ((uint64_t)(remote_obj_id)) ((uint64_t)(remote_reg_id)) ((int32_t)(remote_client_id)) ((uint8_t)(local_type)) ((uint8_t)(remote_type)) ((uint32_t)(ndim)) ((hg_bulk_t)(bulk_handle)) ) */
/* MERCURY_GEN_PROC( gen_reg_map_notification_out_t, ((int32_t)(ret)) ) */

/* MERCURY_GEN_STRUCT_PROC( region_info_transfer_t, ((hg_size_t)(ndim)) ((uint64_t)(start_0)) ((uint64_t)(start_1)) ((uint64_t)(start_2)) ((uint64_t)(start_3))  ((uint64_t)(count_0)) ((uint64_t)(count_1)) ((uint64_t)(count_2)) ((uint64_t)(count_3)) ((uint64_t)(stride_0)) ((uint64_t)(stride_1)) ((uint64_t)(stride_2)) ((uint64_t)(stride_3)) ) */

/* MERCURY_GEN_PROC( region_lock_in_t, ((uint64_t)(obj_id)) ((int32_t)(lock_op)) ((int8_t)(access_type)) ((uint64_t)(local_reg_id)) ((region_info_transfer_t)(region)) ((int32_t)(mapping)) ) */
/* MERCURY_GEN_PROC( region_lock_out_t, ((int32_t)(ret)) ) */

/* // Bulk */
/* MERCURY_GEN_PROC(bulk_write_in_t,  ((hg_int32_t)(cnt)) ((hg_bulk_t)(bulk_handle))) */
/* MERCURY_GEN_PROC(bulk_write_out_t, ((hg_uint64_t)(ret)) ) */

/* /1* */
/*  * Data Server */
/*  *1/ */

/* MERCURY_GEN_PROC(data_server_read_in_t, ((int32_t)(client_id)) ((int32_t)(nclient)) ((pdc_metadata_transfer_t)(meta)) ((region_info_transfer_t)(region))) */
/* MERCURY_GEN_PROC(data_server_read_out_t, ((int32_t)(ret)) ) */

/* MERCURY_GEN_PROC(data_server_write_in_t, ((int32_t)(client_id)) ((int32_t)(nclient)) ((hg_const_string_t)(shm_addr)) ((pdc_metadata_transfer_t)(meta)) ((region_info_transfer_t)(region))) */
/* MERCURY_GEN_PROC(data_server_write_out_t, ((int32_t)(ret)) ) */

/* MERCURY_GEN_PROC(data_server_read_check_in_t, ((int32_t)(client_id)) ((pdc_metadata_transfer_t)(meta)) ((region_info_transfer_t)(region))) */
/* MERCURY_GEN_PROC(data_server_read_check_out_t, ((int32_t)(ret)) ((hg_const_string_t)(shm_addr)) ) */

/* MERCURY_GEN_PROC(data_server_write_check_in_t, ((int32_t)(client_id)) ((pdc_metadata_transfer_t)(meta)) ((region_info_transfer_t)(region))) */
/* MERCURY_GEN_PROC(data_server_write_check_out_t, ((int32_t)(ret)) ) */
/* #else */

typedef struct {
    hg_const_string_t    obj_name;
    uint32_t             hash_value;
    uint32_t             time_step;
} metadata_query_in_t;

typedef struct {
    pdc_metadata_transfer_t ret;
} metadata_query_out_t;

typedef struct {
    uint64_t                obj_id;
    uint32_t                hash_value;
    hg_const_string_t       new_tag;
} metadata_add_tag_in_t;

typedef struct {
    int32_t            ret;
} metadata_add_tag_out_t;

typedef struct {
    uint64_t                obj_id;
    uint32_t                hash_value;
    pdc_metadata_transfer_t new_metadata;
} metadata_update_in_t;

typedef struct {
    int32_t            ret;
} metadata_update_out_t;

typedef struct {
    uint64_t           obj_id;
} metadata_delete_by_id_in_t;

typedef struct {
    int32_t            ret;
} metadata_delete_by_id_out_t;

typedef struct {
    uint64_t                    obj_id;
//    int32_t                     lock_op;
    PDC_access_t                access_type;
    pdcid_t                     local_reg_id;
    region_info_transfer_t      region;
    pbool_t                     mapping;
} region_lock_in_t;

typedef struct {
    int32_t            ret;
} region_lock_out_t;

static HG_INLINE hg_return_t
hg_proc_region_info_transfer_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    region_info_transfer_t *struct_data = (region_info_transfer_t*) data;

    ret = hg_proc_hg_size_t(proc, &struct_data->ndim);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }

    ret = hg_proc_uint64_t(proc, &struct_data->start_0);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint64_t(proc, &struct_data->start_1);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint64_t(proc, &struct_data->start_2);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint64_t(proc, &struct_data->start_3);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }

    ret = hg_proc_uint64_t(proc, &struct_data->count_0);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint64_t(proc, &struct_data->count_1);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint64_t(proc, &struct_data->count_2);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint64_t(proc, &struct_data->count_3);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }

    /* ret = hg_proc_uint64_t(proc, &struct_data->stride_0); */
    /* if (ret != HG_SUCCESS) { */
    /*     HG_LOG_ERROR("Proc error"); */
    /*     return ret; */
    /* } */
    /* ret = hg_proc_uint64_t(proc, &struct_data->stride_1); */
    /* if (ret != HG_SUCCESS) { */
    /*     HG_LOG_ERROR("Proc error"); */
    /*     return ret; */
    /* } */
    /* ret = hg_proc_uint64_t(proc, &struct_data->stride_2); */
    /* if (ret != HG_SUCCESS) { */
    /*     HG_LOG_ERROR("Proc error"); */
    /*     return ret; */
    /* } */
    /* ret = hg_proc_uint64_t(proc, &struct_data->stride_3); */
    /* if (ret != HG_SUCCESS) { */
    /*     HG_LOG_ERROR("Proc error"); */
    /*     return ret; */
    /* } */
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_region_lock_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    region_lock_in_t *struct_data = (region_lock_in_t*) data;

    ret = hg_proc_uint64_t(proc, &struct_data->obj_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
    }
/*
    ret = hg_proc_uint32_t(proc, &struct_data->lock_op);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
    }
*/
    ret = hg_proc_uint8_t(proc, &struct_data->access_type);
    if (ret != HG_SUCCESS) {
    HG_LOG_ERROR("Proc error");
    }
    ret = hg_proc_uint64_t(proc, &struct_data->local_reg_id);
    if (ret != HG_SUCCESS) {
    HG_LOG_ERROR("Proc error");
    }
    ret = hg_proc_region_info_transfer_t(proc, &struct_data->region);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint32_t(proc, &struct_data->mapping);
    if (ret != HG_SUCCESS) {
    HG_LOG_ERROR("Proc error");
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_region_lock_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    region_lock_out_t *struct_data = (region_lock_out_t*) data;

    ret = hg_proc_uint32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
    }
    return ret;
}

typedef struct {
    hg_const_string_t    obj_name;
    int32_t              time_step;
    uint32_t             hash_value;
} metadata_delete_in_t;

typedef struct {
    int32_t            ret;
} metadata_delete_out_t;

static HG_INLINE hg_return_t
hg_proc_metadata_query_transfer_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    metadata_query_transfer_in_t *struct_data = (metadata_query_transfer_in_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->is_list_all);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_int32_t(proc, &struct_data->user_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_const_string_t(proc, &struct_data->app_name);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_const_string_t(proc, &struct_data->obj_name);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_int32_t(proc, &struct_data->time_step_from);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_int32_t(proc, &struct_data->time_step_to);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint32_t(proc, &struct_data->ndim);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    /* ret = hg_proc_int32_t(proc, &struct_data->create_time_from); */
    /* if (ret != HG_SUCCESS) { */
	/* HG_LOG_ERROR("Proc error"); */
    /*     return ret; */
    /* } */
    /* ret = hg_proc_int32_t(proc, &struct_data->create_time_to); */
    /* if (ret != HG_SUCCESS) { */
	/* HG_LOG_ERROR("Proc error"); */
    /*     return ret; */
    /* } */
    /* ret = hg_proc_int32_t(proc, &struct_data->last_modified_time_from); */
    /* if (ret != HG_SUCCESS) { */
	/* HG_LOG_ERROR("Proc error"); */
    /*     return ret; */
    /* } */
    /* ret = hg_proc_int32_t(proc, &struct_data->last_modified_time_to); */
    /* if (ret != HG_SUCCESS) { */
	/* HG_LOG_ERROR("Proc error"); */
    /*     return ret; */
    /* } */
    ret = hg_proc_hg_const_string_t(proc, &struct_data->tags);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

typedef struct {
    int32_t     ret;
    hg_bulk_t   bulk_handle;
} metadata_query_transfer_out_t;

static HG_INLINE hg_return_t
hg_proc_metadata_query_transfer_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    metadata_query_transfer_out_t *struct_data = (metadata_query_transfer_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_bulk_t(proc, &struct_data->bulk_handle);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}


static HG_INLINE hg_return_t
hg_proc_pdc_metadata_transfer_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    pdc_metadata_transfer_t *struct_data = (pdc_metadata_transfer_t*) data;

    ret = hg_proc_uint32_t(proc, &struct_data->user_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_const_string_t(proc, &struct_data->app_name);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_const_string_t(proc, &struct_data->obj_name);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_int32_t(proc, &struct_data->time_step);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint32_t(proc, &struct_data->ndim);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_int32_t(proc, &struct_data->dims0);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_int32_t(proc, &struct_data->dims1);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_int32_t(proc, &struct_data->dims2);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_int32_t(proc, &struct_data->dims3);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint64_t(proc, &struct_data->obj_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_const_string_t(proc, &struct_data->data_location);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_const_string_t(proc, &struct_data->tags);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_metadata_add_tag_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    metadata_add_tag_in_t *struct_data = (metadata_add_tag_in_t*) data;

    ret = hg_proc_hg_uint64_t(proc, &struct_data->obj_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint32_t(proc, &struct_data->hash_value);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_const_string_t(proc, &struct_data->new_tag);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_metadata_add_tag_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    metadata_add_tag_out_t *struct_data = (metadata_add_tag_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_metadata_update_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    metadata_update_in_t *struct_data = (metadata_update_in_t*) data;

    ret = hg_proc_hg_uint64_t(proc, &struct_data->obj_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint32_t(proc, &struct_data->hash_value);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_pdc_metadata_transfer_t(proc, &struct_data->new_metadata);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_metadata_update_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    metadata_update_out_t *struct_data = (metadata_update_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_metadata_delete_by_id_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    metadata_delete_by_id_in_t *struct_data = (metadata_delete_by_id_in_t*) data;

    ret = hg_proc_uint64_t(proc, &struct_data->obj_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_metadata_delete_by_id_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    metadata_delete_by_id_out_t *struct_data = (metadata_delete_by_id_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_metadata_delete_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    metadata_delete_in_t *struct_data = (metadata_delete_in_t*) data;

    ret = hg_proc_hg_const_string_t(proc, &struct_data->obj_name);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
    }
    ret = hg_proc_int32_t(proc, &struct_data->time_step);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
    }
    ret = hg_proc_uint32_t(proc, &struct_data->hash_value);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_metadata_delete_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    metadata_delete_out_t *struct_data = (metadata_delete_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_metadata_query_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    metadata_query_in_t *struct_data = (metadata_query_in_t*) data;

    ret = hg_proc_hg_const_string_t(proc, &struct_data->obj_name);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint32_t(proc, &struct_data->time_step);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint32_t(proc, &struct_data->hash_value);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_metadata_query_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret = HG_SUCCESS;
    metadata_query_out_t *struct_data = (metadata_query_out_t*) data;

    ret = hg_proc_pdc_metadata_transfer_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

typedef struct {
    pdc_metadata_transfer_t data;
    uint32_t hash_value;
} gen_obj_id_in_t;

typedef struct {
    uint64_t obj_id;
} gen_obj_id_out_t;


/* typedef struct { */
/*     hg_const_string_t    obj_name; */
/*     uint32_t             hash_value; */
/* } send_obj_name_marker_in_t; */

/* typedef struct { */
/*     int32_t ret; */
/* } send_obj_name_marker_out_t; */

typedef struct {
    int32_t server_id;
    int32_t nserver;
    hg_const_string_t server_addr;
} server_lookup_client_in_t;

typedef struct {
    int32_t ret;
} server_lookup_client_out_t;


typedef struct {
    int32_t server_id;
} server_lookup_remote_server_in_t;

typedef struct {
    int32_t ret;
} server_lookup_remote_server_out_t;


typedef struct {
    uint32_t client_id;
    int32_t nclient;
    hg_const_string_t client_addr;
} client_test_connect_in_t;

typedef struct {
    int32_t ret;
} client_test_connect_out_t;


typedef struct {
    int32_t  io_type;
    uint64_t obj_id;
    hg_const_string_t shm_addr;
} notify_io_complete_in_t;

typedef struct {
    int32_t ret;
} notify_io_complete_out_t;

typedef struct {
    uint64_t obj_id;
    uint64_t reg_id;
} notify_region_update_in_t;

typedef struct {
    int32_t ret;
} notify_region_update_out_t;

typedef struct {
    uint32_t client_id;
} close_server_in_t;

typedef struct {
    int32_t ret;
} close_server_out_t;

typedef struct {
    uint64_t        local_obj_id;
    uint64_t        local_reg_id;
    uint64_t        remote_obj_id;
    uint64_t        remote_reg_id;
    int32_t         remote_client_id;
    PDC_var_type_t  local_type;
    PDC_var_type_t  remote_type;
    size_t          ndim;
    hg_bulk_t       local_bulk_handle;
    hg_bulk_t       remote_bulk_handle;
    region_info_transfer_t      region;
} gen_reg_map_notification_in_t;

typedef struct {
    int32_t ret;
} gen_reg_map_notification_out_t;

typedef struct {
    uint64_t        local_obj_id;
    uint64_t        local_reg_id;
} gen_reg_unmap_notification_in_t;

typedef struct {
    int32_t ret;
} gen_reg_unmap_notification_out_t;

typedef struct {
    uint64_t        local_obj_id;
} gen_obj_unmap_notification_in_t;

typedef struct {
    int32_t ret;
} gen_obj_unmap_notification_out_t;

static HG_INLINE hg_return_t
hg_proc_gen_obj_id_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    gen_obj_id_in_t *struct_data = (gen_obj_id_in_t*) data;

    ret = hg_proc_pdc_metadata_transfer_t(proc, &struct_data->data);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint32_t(proc, &struct_data->hash_value);
    if (ret != HG_SUCCESS) {
	    HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_gen_obj_id_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    gen_obj_id_out_t *struct_data = (gen_obj_id_out_t*) data;

    ret = hg_proc_uint64_t(proc, &struct_data->obj_id);
    if (ret != HG_SUCCESS) {
	    HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

/* static HG_INLINE hg_return_t */
/* hg_proc_send_obj_name_marker_in_t(hg_proc_t proc, void *data) */
/* { */
/*     hg_return_t ret; */
/*     send_obj_name_marker_in_t *struct_data = (send_obj_name_marker_in_t*) data; */

/*     ret = hg_proc_hg_const_string_t(proc, &struct_data->obj_name); */
/*     if (ret != HG_SUCCESS) { */
/* 	HG_LOG_ERROR("Proc error"); */
/*     } */
/*     ret = hg_proc_uint32_t(proc, &struct_data->hash_value); */
/*     if (ret != HG_SUCCESS) { */
/* 	HG_LOG_ERROR("Proc error"); */
/*     } */
/*     return ret; */
/* } */

/* static HG_INLINE hg_return_t */
/* hg_proc_send_obj_name_marker_out_t(hg_proc_t proc, void *data) */
/* { */
/*     hg_return_t ret; */
/*     send_obj_name_marker_out_t *struct_data = (send_obj_name_marker_out_t*) data; */

/*     ret = hg_proc_int32_t(proc, &struct_data->ret); */
/*     if (ret != HG_SUCCESS) { */
/* 	HG_LOG_ERROR("Proc error"); */
/*     } */
/*     return ret; */
/* } */

static HG_INLINE hg_return_t
hg_proc_server_lookup_remote_server_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    server_lookup_remote_server_in_t *struct_data = (server_lookup_remote_server_in_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->server_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_server_lookup_remote_server_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    server_lookup_remote_server_out_t *struct_data = (server_lookup_remote_server_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_server_lookup_client_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    server_lookup_client_in_t *struct_data = (server_lookup_client_in_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->server_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_int32_t(proc, &struct_data->nserver);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_const_string_t(proc, &struct_data->server_addr);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_server_lookup_client_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    server_lookup_client_out_t *struct_data = (server_lookup_client_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
    }
    return ret;
}


static HG_INLINE hg_return_t
hg_proc_client_test_connect_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    client_test_connect_in_t *struct_data = (client_test_connect_in_t*) data;

    ret = hg_proc_uint32_t(proc, &struct_data->client_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_int32_t(proc, &struct_data->nclient);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_const_string_t(proc, &struct_data->client_addr);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_client_test_connect_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    client_test_connect_out_t *struct_data = (client_test_connect_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	    HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_notify_io_complete_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    notify_io_complete_in_t *struct_data = (notify_io_complete_in_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->io_type);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint64_t(proc, &struct_data->obj_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_const_string_t(proc, &struct_data->shm_addr);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_notify_io_complete_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    notify_io_complete_out_t *struct_data = (notify_io_complete_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_notify_region_update_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    notify_region_update_in_t *struct_data = (notify_region_update_in_t*) data;

    ret = hg_proc_uint64_t(proc, &struct_data->obj_id);
    if (ret != HG_SUCCESS) {
    HG_LOG_ERROR("Proc error");
    }
    ret = hg_proc_uint64_t(proc, &struct_data->reg_id);
    if (ret != HG_SUCCESS) {
    HG_LOG_ERROR("Proc error");
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_notify_region_update_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    notify_region_update_out_t *struct_data = (notify_region_update_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
    HG_LOG_ERROR("Proc error");
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_close_server_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    close_server_in_t *struct_data = (close_server_in_t*) data;

    ret = hg_proc_uint32_t(proc, &struct_data->client_id);
    if (ret != HG_SUCCESS) {
	    HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_close_server_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    close_server_out_t *struct_data = (close_server_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	    HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}


// Bulk
/* Define bulk_write_in_t */
typedef struct {
    hg_int32_t cnt;
    hg_bulk_t bulk_handle;
} bulk_write_in_t;

/* Define hg_proc_bulk_write_in_t */
static HG_INLINE hg_return_t
hg_proc_bulk_write_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret = HG_SUCCESS;
    bulk_write_in_t *struct_data = (bulk_write_in_t *) data;

    ret = hg_proc_int32_t(proc, &struct_data->cnt);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_bulk_t(proc, &struct_data->bulk_handle);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

/* Define bulk_write_out_t */
typedef struct {
    hg_uint64_t ret;
} bulk_write_out_t;

/* Define hg_proc_bulk_write_out_t */
static HG_INLINE hg_return_t
hg_proc_bulk_write_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret = HG_SUCCESS;
    bulk_write_out_t *struct_data = (bulk_write_out_t *) data;

    ret = hg_proc_uint64_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}
// End of bulk


static HG_INLINE hg_return_t
hg_proc_gen_reg_map_notification_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    gen_reg_map_notification_in_t *struct_data = (gen_reg_map_notification_in_t *) data;

    ret = hg_proc_uint64_t(proc, &struct_data->local_obj_id);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint64_t(proc, &struct_data->local_reg_id);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint64_t(proc, &struct_data->remote_obj_id);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint64_t(proc, &struct_data->remote_reg_id);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_int32_t(proc, &struct_data->remote_client_id);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint8_t(proc, &struct_data->local_type);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint8_t(proc, &struct_data->remote_type);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint32_t(proc, &struct_data->ndim);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_bulk_t(proc, &struct_data->local_bulk_handle);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_bulk_t(proc, &struct_data->remote_bulk_handle);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_region_info_transfer_t(proc, &struct_data->region);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_gen_reg_map_notification_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    gen_reg_map_notification_out_t *struct_data = (gen_reg_map_notification_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	    HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_gen_reg_unmap_notification_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    gen_reg_unmap_notification_in_t *struct_data = (gen_reg_unmap_notification_in_t *) data;

    ret = hg_proc_uint64_t(proc, &struct_data->local_obj_id);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint64_t(proc, &struct_data->local_reg_id);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_gen_reg_unmap_notification_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    gen_reg_unmap_notification_out_t *struct_data = (gen_reg_unmap_notification_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_gen_obj_unmap_notification_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    gen_obj_unmap_notification_in_t *struct_data = (gen_obj_unmap_notification_in_t *) data;

    ret = hg_proc_uint64_t(proc, &struct_data->local_obj_id);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_gen_obj_unmap_notification_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    gen_obj_unmap_notification_out_t *struct_data = (gen_obj_unmap_notification_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}


/*
 * Data Server related
 */

/* MERCURY_GEN_PROC(data_server_read_in_t,  ((int32_t)(nclient)) ((hg_uint64_t)(meta_id)) ((region_info_transfer_t)(region))) */
/* MERCURY_GEN_PROC(data_server_read_out_t, ((int32_t)(ret)) ) */
typedef struct {
    uint32_t                    client_id;
    int32_t                     nclient;
    pdc_metadata_transfer_t     meta;
    region_info_transfer_t      region;
} data_server_read_in_t;

typedef struct {
    int32_t            ret;
} data_server_read_out_t;

static HG_INLINE hg_return_t
hg_proc_data_server_read_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    data_server_read_in_t *struct_data = (data_server_read_in_t*) data;

    ret = hg_proc_uint32_t(proc, &struct_data->client_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_int32_t(proc, &struct_data->nclient);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_pdc_metadata_transfer_t(proc, &struct_data->meta);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_region_info_transfer_t(proc, &struct_data->region);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_data_server_read_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    data_server_read_out_t *struct_data = (data_server_read_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
    }
    return ret;
}

// Data server write
typedef struct {
    uint32_t                    client_id;
    int32_t                     nclient;
    hg_const_string_t           shm_addr;
    pdc_metadata_transfer_t     meta;
    region_info_transfer_t      region;
} data_server_write_in_t;

typedef struct {
    int32_t            ret;
} data_server_write_out_t;

static HG_INLINE hg_return_t
hg_proc_data_server_write_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    data_server_write_in_t *struct_data = (data_server_write_in_t*) data;

    ret = hg_proc_uint32_t(proc, &struct_data->client_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_int32_t(proc, &struct_data->nclient);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_const_string_t(proc, &struct_data->shm_addr);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_pdc_metadata_transfer_t(proc, &struct_data->meta);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_region_info_transfer_t(proc, &struct_data->region);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_data_server_write_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    data_server_write_out_t *struct_data = (data_server_write_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
    }
    return ret;
}

typedef struct {
    uint32_t                    client_id;
    pdc_metadata_transfer_t     meta;
    region_info_transfer_t      region;
} data_server_read_check_in_t;

typedef struct {
    int32_t            ret;
    hg_string_t  shm_addr;
} data_server_read_check_out_t;

static HG_INLINE hg_return_t
hg_proc_data_server_read_check_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    data_server_read_check_in_t *struct_data = (data_server_read_check_in_t*) data;

    ret = hg_proc_uint32_t(proc, &struct_data->client_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_pdc_metadata_transfer_t(proc, &struct_data->meta);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_region_info_transfer_t(proc, &struct_data->region);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_data_server_read_check_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    data_server_read_check_out_t *struct_data = (data_server_read_check_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_string_t(proc, &struct_data->shm_addr);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

typedef struct {
    uint32_t                     client_id;
    pdc_metadata_transfer_t     meta;
    region_info_transfer_t      region;
} data_server_write_check_in_t;

typedef struct {
    int32_t            ret;
} data_server_write_check_out_t;

static HG_INLINE hg_return_t
hg_proc_data_server_write_check_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    data_server_write_check_in_t *struct_data = (data_server_write_check_in_t*) data;

    ret = hg_proc_uint32_t(proc, &struct_data->client_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_pdc_metadata_transfer_t(proc, &struct_data->meta);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_region_info_transfer_t(proc, &struct_data->region);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_data_server_write_check_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    data_server_write_check_out_t *struct_data = (data_server_write_check_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

typedef struct {
    uint64_t                    obj_id;
    uint64_t                    offset;
    hg_string_t                 storage_location;
    region_info_transfer_t      region;
} update_region_loc_in_t;

typedef struct {
    int32_t            ret;
} update_region_loc_out_t;

static HG_INLINE hg_return_t
hg_proc_update_region_loc_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    update_region_loc_in_t *struct_data = (update_region_loc_in_t*) data;

    ret = hg_proc_uint64_t(proc, &struct_data->obj_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_hg_string_t(proc, &struct_data->storage_location);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_uint64_t(proc, &struct_data->offset);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_region_info_transfer_t(proc, &struct_data->region);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_update_region_loc_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    update_region_loc_out_t *struct_data = (update_region_loc_out_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

typedef struct {
    uint64_t obj_id;
} get_metadata_by_id_in_t;

typedef struct {
    pdc_metadata_transfer_t res_meta;
} get_metadata_by_id_out_t;

static HG_INLINE hg_return_t
hg_proc_get_metadata_by_id_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    get_metadata_by_id_in_t *struct_data = (get_metadata_by_id_in_t*) data;

    ret = hg_proc_uint64_t(proc, &struct_data->obj_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

static HG_INLINE hg_return_t
hg_proc_get_metadata_by_id_out_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    get_metadata_by_id_out_t *struct_data = (get_metadata_by_id_out_t*) data;

    ret = hg_proc_pdc_metadata_transfer_t(proc, &struct_data->res_meta);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

// For generic serialized data transfer
typedef struct {
    hg_string_t buf;
} pdc_serialized_data_t;

static HG_INLINE hg_return_t
hg_proc_pdc_serialized_data_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    pdc_serialized_data_t *struct_data = (pdc_serialized_data_t*) data;

    ret = hg_proc_hg_string_t(proc, &struct_data->buf);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

typedef struct {
    hg_string_t buf;
    pdc_metadata_transfer_t meta;
} pdc_aggregated_io_to_server_t;

static HG_INLINE hg_return_t
hg_proc_pdc_aggregated_io_to_server_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    pdc_aggregated_io_to_server_t *struct_data = (pdc_aggregated_io_to_server_t*) data;

    ret = hg_proc_hg_string_t(proc, &struct_data->buf);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_pdc_metadata_transfer_t(proc, &struct_data->meta);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

typedef struct {
    uint64_t obj_id;
    region_info_transfer_t req_region;
} get_storage_info_in_t;

static HG_INLINE hg_return_t
hg_proc_get_storage_info_in_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    get_storage_info_in_t *struct_data = (get_storage_info_in_t*) data;

    ret = hg_proc_uint64_t(proc, &struct_data->obj_id);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    ret = hg_proc_region_info_transfer_t(proc, &struct_data->req_region);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
        return ret;
    }
    return ret;
}

typedef struct {
    int ret;
} pdc_int_ret_t;

static HG_INLINE hg_return_t
hg_proc_pdc_int_ret_t(hg_proc_t proc, void *data)
{
    hg_return_t ret;
    pdc_int_ret_t *struct_data = (pdc_int_ret_t*) data;

    ret = hg_proc_int32_t(proc, &struct_data->ret);
    if (ret != HG_SUCCESS) {
	HG_LOG_ERROR("Proc error");
    }
    return ret;
}

/* #endif // HAS_BOOST */



hg_id_t gen_obj_id_register(hg_class_t *hg_class);
/* hg_id_t send_obj_name_marker_register(hg_class_t *hg_class); */
hg_id_t client_test_connect_register(hg_class_t *hg_class);
hg_id_t server_lookup_client_register(hg_class_t *hg_class);
hg_id_t close_server_register(hg_class_t *hg_class);
hg_id_t metadata_query_register(hg_class_t *hg_class);
hg_id_t metadata_delete_register(hg_class_t *hg_class);
hg_id_t metadata_delete_by_id_register(hg_class_t *hg_class);
hg_id_t metadata_update_register(hg_class_t *hg_class);
hg_id_t metadata_add_tag_register(hg_class_t *hg_class);
hg_id_t region_lock_register(hg_class_t *hg_class);

hg_id_t gen_reg_unmap_notification_register(hg_class_t *hg_class);
hg_id_t gen_obj_unmap_notification_register(hg_class_t *hg_class);
hg_id_t data_server_write_register(hg_class_t *hg_class);
hg_id_t notify_region_update_register(hg_class_t *hg_class);
hg_id_t region_release_register(hg_class_t *hg_class);

hg_id_t test_bulk_xfer_register(hg_class_t *hg_class);
hg_id_t server_lookup_remote_server_register(hg_class_t *hg_class);
hg_id_t update_region_loc_register(hg_class_t *hg_class);
hg_id_t get_metadata_by_id_register(hg_class_t *hg_class);
hg_id_t get_storage_info_register(hg_class_t *hg_class);

//bulk
hg_id_t query_partial_register(hg_class_t *hg_class);
hg_id_t notify_io_complete_register(hg_class_t *hg_class);
hg_id_t data_server_read_register(hg_class_t *hg_class);

struct hg_test_bulk_args {
    int cnt;
    hg_handle_t handle;
    size_t nbytes;
    hg_atomic_int32_t completed_transfers;
    size_t ret;
    pdc_metadata_t **meta_arr;
    uint32_t        *n_meta;
};

struct lock_bulk_args {
    hg_handle_t handle;
    region_lock_in_t in;
    struct PDC_region_info *server_region;
    void  *data_buf;
    region_map_t *mapping_list;
    hg_addr_t addr;
};

struct region_lock_update_bulk_args {
    hg_handle_t handle;
    region_lock_in_t in;
    pdcid_t remote_obj_id;
    pdcid_t remote_reg_id;
    int32_t remote_client_id;
    void  *data_buf;
    struct PDC_region_info *server_region;
};

struct region_update_bulk_args {
    pdc_cnt_t refcount;   // to track how many unlocked mapped region for data transfer
    hg_handle_t handle;
    hg_bulk_t   bulk_handle;
    pdcid_t remote_obj_id;
    pdcid_t remote_reg_id;
    int32_t remote_client_id;
    struct lock_bulk_args *args;
};

hg_id_t gen_reg_map_notification_register(hg_class_t *hg_class);

perr_t   delete_metadata_from_hash_table(metadata_delete_in_t *in, metadata_delete_out_t *out);
perr_t   PDC_Server_update_metadata(metadata_update_in_t *in, metadata_update_out_t *out);
perr_t   PDC_Server_add_tag_metadata(metadata_add_tag_in_t *in, metadata_add_tag_out_t *out);
perr_t   PDC_get_self_addr(hg_class_t* hg_class, char* self_addr_string);
uint32_t PDC_get_server_by_obj_id(uint64_t obj_id, int n_server);
uint32_t PDC_get_hash_by_name(const char *name);
int      PDC_metadata_cmp(pdc_metadata_t *a, pdc_metadata_t *b);
perr_t   PDC_metadata_init(pdc_metadata_t *a);
void     PDC_print_metadata(pdc_metadata_t *a);
void     PDC_print_region_list(region_list_t *a);
void     PDC_print_storage_region_list(region_list_t *a);
perr_t   PDC_init_region_list(region_list_t *a);
int      PDC_is_same_region_list(region_list_t *a, region_list_t *b);

perr_t pdc_metadata_t_to_transfer_t(pdc_metadata_t *meta, pdc_metadata_transfer_t *transfer);
perr_t pdc_transfer_t_to_metadata_t(pdc_metadata_transfer_t *transfer, pdc_metadata_t *meta);

perr_t pdc_region_info_to_list_t(struct PDC_region_info *region, region_list_t *list);
perr_t pdc_region_transfer_t_to_list_t(region_info_transfer_t *transfer, region_list_t *region);
perr_t pdc_region_list_t_to_transfer(region_list_t *region, region_info_transfer_t *transfer);
perr_t pdc_region_list_t_deep_cp(region_list_t *from, region_list_t *to);

perr_t pdc_region_info_t_to_transfer(struct PDC_region_info *region, region_info_transfer_t *transfer);

perr_t PDC_serialize_regions_lists(region_list_t** regions, uint32_t n_region, void *buf, uint32_t buf_size);
perr_t PDC_unserialize_region_lists(void *buf, region_list_t** regions, uint32_t *n_region);
perr_t PDC_get_serialized_size(region_list_t** regions, uint32_t n_region, uint32_t *len);

perr_t PDC_replace_zero_chars(signed char *buf, uint32_t buf_size);
perr_t PDC_replace_char_fill_values(signed char *buf, uint32_t buf_size);

void pdc_mkdir(const char *dir);

extern hg_hash_table_t   *metadata_hash_table_g;
extern hg_atomic_int32_t  close_server_g;


hg_id_t data_server_write_check_register(hg_class_t *hg_class);
hg_id_t data_server_read_register(hg_class_t *hg_class);

hg_id_t data_server_read_check_register(hg_class_t *hg_class);
hg_id_t data_server_read_register(hg_class_t *hg_class);

#endif /* PDC_CLIENT_SERVER_COMMON_H */
