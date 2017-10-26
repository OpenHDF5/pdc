#ifndef DART_SIM_H
#define DART_SIM_H


#include "dart_utils.h"


typedef struct {
    int id;
    int word_count;
    int request_per_sec;
} dart_server;

typedef struct {
    int id;
    int num_dst_server;
    dart_server *dest_servers;
} dart_vnode;

typedef struct {
    int id;
    int num_vnode;
    int dart_tree_height;
    dart_vnode *vnodes;
} dart_client;

typedef struct {
    dart_client *all_clients;
    dart_server *all_servers;
    int num_client;
    int num_server;
    int alphabet_size;
} DART;

#endif //DART_SIM_H