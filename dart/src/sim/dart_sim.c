#include "dart_sim.h"

/**
 * Author: Wei Zhang
 * E-mail: X-Spirit.zhang@ttu.edu
 * 
 * 
 * This is a simulator to simulate the behavior of DART. 
 * In DART, we care more about unevenly distributed workload other than actual search efficiency.
 * We believe the imbalanced workload is the biggest factor that may affect the perfix search efficiency
 * of the distributed system.
 * 
 * 
 */


DART *dart;

/**
 * 
 * This should be a local function for a real client.
 */
void dart_vnode_init(DART *dart, dart_vnode *vnodes, int num_vnode){
    int v = 0;
    for (v = 0 ; v < num_vnode; v++) {
        dart_vnode vnode = vnodes[v];
        vnode.id = 0;
        vnode.num_dst_server = 1;
        // A realloc may be needed for future expansion.
        vnode.dest_servers = (dart_server *)calloc(vnode.num_dst_server, sizeof(dart_server));
        vnode.dest_servers[0] = dart->all_servers[vnode.id % dart->num_server];
    }
}

/**
 * 
 * All variables related to DART and dart_client can be extend to global ones in a real client.
 */
void dart_client_init(DART *dart, int clientId) {
    // Calculate tree height;
    int dart_tree_height = (int)ceil(log_with_base((double)dart->alphabet_size, dart->num_server));
    // calculate number of all leaf nodes
    int num_vnode = (int)pow(dart->alphabet_size, dart_tree_height);
    // each client will have ``num_vnode'' vnodes.
    dart_vnode *vnodes = (dart_vnode *)calloc(num_vnode, sizeof(dart_vnode));

    dart_client *clt = &(dart->all_clients[clientId]);

    clt->id = clientId;
    clt->vnodes = vnodes;
    clt->num_vnode = num_vnode;
    clt->dart_tree_height = dart_tree_height;

    //Initialize all virtual nodes
    dart_vnode_init(dart, clt->vnodes, clt->num_vnode);
}


void dart_space_init(int num_client, int num_server, int alphabet_size){
    dart = (DART *)calloc(1, sizeof(DART));
    dart->alphabet_size = alphabet_size;
    // initialize clients;
    dart->num_client = num_client;
    dart->all_clients = (dart_client*)calloc(num_client, sizeof(dart_client));
    // initialize servers;
    dart->num_server = num_server;
    dart->all_servers = (dart_server*)calloc(num_server, sizeof(dart_server));

    // Initialize vnodes of all clients
    int i = 0;
    for (i = 0 ; i < num_client; i++) {
        dart_client_init(dart, i);
    }  
}


void init(int argc, char **argv){
    srand((unsigned int)time(NULL));
    
    if (argc < 3) {
        printf("Usage: sim.exe <num_client> <num_server> <alphabet_size>\n");
        exit(1);
    }

    int num_client = atoi(argv[1]);
    int num_server = atoi(argv[2]);
    int alphabet_size = atoi(argv[3]);

    dart_space_init(num_client, num_server, alphabet_size);

}



int main(int argc, char **argv){

    init(argc, argv);

    

    
    
    return 0;
}