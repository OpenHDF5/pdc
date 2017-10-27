#ifndef DART_SIM_H
#define DART_SIM_H


#include "dart_utils.h"


typedef struct {
    int id;
    uint32_t word_count;
    uint32_t request_count;
    uint32_t max_word_count;
    uint32_t max_request_per_sec;
} dart_server;

typedef struct {
    int id;
    int num_dst_server;
    dart_server *dest_servers;
} dart_vnode;

typedef struct {
    int id;
    uint32_t num_vnode;
    int dart_tree_height;
    uint32_t *vnode_key_count;
    uint32_t vnode_constraint;
    dart_vnode *vnodes;
} dart_client;

typedef struct {
    dart_client *all_clients;
    dart_server *all_servers;
    int num_client;
    int num_server;
    int alphabet_size;
    int spanning_factor;
} DART;

#endif //DART_SIM_H