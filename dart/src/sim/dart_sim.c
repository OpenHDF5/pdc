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

#define REHASHING 1

const int INPUT_RANDOM_STRING =0;
const int INPUT_UUID          =1;
const int INPUT_DICTIONARY    =2;
const int INPUT_WIKI_KEYWORD  =3;

const int HASH_PREFIX_ONE = 0;
const int HASH_PREFIX_TWO = 1;
const int HASH_PREFIX_THREE = 2;
const int HASH_DART_INIT = 3;

const int EXTRA_TREE_HEIGHT = 0;

DART *dart;

/**
 * 
 * This should be a local function for a real client.
 */
void dart_vnode_init(DART *dart, dart_vnode *vnodes, int num_vnode){
    int v = 0;
    for (v = 0 ; v < num_vnode; v++) {
        dart_vnode *vnode = &vnodes[v];
        vnode->id = 0;
        vnode->num_dst_server = 1;

        // A realloc may be needed for future expansion.
        vnode->dest_servers = (dart_server *)calloc(vnode->num_dst_server, sizeof(dart_server));
        vnode->dest_servers[0] = dart->all_servers[vnode->id % dart->num_server];
    }
}

/**
 * 
 * All variables related to DART and dart_client can be extend to global ones in a real client.
 */
void dart_client_init(DART *dart, int clientId) {
    // Calculate tree height;
    int dart_tree_height = (int)ceil(log_with_base((double)dart->alphabet_size, (double)dart->num_server)) +
            1 + EXTRA_TREE_HEIGHT;
    // calculate number of all leaf nodes
    int num_vnode = (int)pow(dart->alphabet_size, dart_tree_height);
    // each client will have ``num_vnode'' vnodes.
    // dart_vnode *vnodes = (dart_vnode *)calloc(num_vnode, sizeof(dart_vnode));

    dart_client *clt = &(dart->all_clients[clientId]);

    clt->id = clientId;
    //clt->vnodes = vnodes;
    clt->num_vnode = num_vnode;
    clt->vnode_constraint=2;
    clt->dart_tree_height = dart_tree_height;
    clt->vnode_key_count = (uint32_t *)calloc(num_vnode, sizeof(uint32_t));

    //Initialize all virtual nodes
    printf("Initializing dart vnode for client %d... \n", clientId);
    //dart_vnode_init(dart, clt->vnodes, clt->num_vnode);
}


void dart_server_init(DART *dart, int serverId){
    dart_server *server = &dart->all_servers[serverId];
    server->id = serverId;
    server->word_count=0;
    server->request_count=0;
    server->max_word_count=10;
    server->max_request_per_sec=100000;
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
    printf("Initializing dart clients... \n");
    int i = 0;
    for (i = 0 ; i < num_client; i++) {
        dart_client_init(dart, i);
    }

    // Initialize all servers
    printf("Initializing dart servers... \n");
    for (i = 0; i < num_server; i++) {
        printf("Initializing dart server %d... \n", i);
        dart_server_init(dart, i);
    }
}

int md5_hash(int prefix_len, char *word){

    char *prefix = (char *)calloc(prefix_len+1, sizeof(char));
    memcpy(prefix, word, prefix_len);
    uint8_t digest;
    md5((uint8_t *)prefix, strlen(prefix), &digest);
    uint32_t inthash = to_int32(&digest);
    return inthash % dart->num_server;
}

int regular_hash(int prefix_len, char *str){
    unsigned long str_hash = hash(str, prefix_len);
    return str_hash % dart->num_server;
}


uint32_t uint32_pow(uint32_t base, uint32_t pow) {
    uint32_t p = 0;
    uint32_t rst = 1;
    for (p = 0; p < pow; p++) {
        rst = base * rst;
    }
    return rst;
}

int int_abs(int a){
    if (a < 0) {
        return 0-a;
    }
    return a;
}

uint32_t dart_init_hash(int tree_height, char *str){
//    char *substr = (char *)calloc(tree_height+1, sizeof(char));
//    memcpy(substr, str, tree_height);
    int n = 0;
    uint32_t c;
    uint32_t rst = 0;
    uint32_t i_t_n;
    int met_end = 0;
    for (n = 1; n <= tree_height; n++) {
        if (str[n-1] == '\0') { met_end = 1; }
        if (str[n-1] != '\0' && met_end==0) {
            i_t_n = ((uint32_t)str[n-1])%dart->alphabet_size + 1;
        }
        c= (i_t_n-1) * ((uint32_t)uint32_pow(dart->alphabet_size, tree_height - n));
        rst += c;
    }
    return rst;
}

void insert_word_to_server(int serverId, char *word){
    dart_server *server = &dart->all_servers[serverId];
    server->word_count = server->word_count + 1;
    //printf("server word count %d\n", server->word_count);
}

int virtual_node_average_rehashing(uint32_t vnode_idx, char *word, dart_client *client){
    int tree_height = client->dart_tree_height;
    uint32_t reconciled_vnode_idx = vnode_idx;
    printf("vnode_idx = %d\n", reconciled_vnode_idx);
    int serverId = 0;
    int i = 0;
    for (serverId = (int)reconciled_vnode_idx % dart->num_server;
         dart->all_servers[serverId].word_count >= dart->all_servers[serverId].max_word_count; i++){

        int index_to_char_to_hash = (int)word[tree_height] % dart->alphabet_size;
        uint32_t vnode_distance = client->num_vnode / dart->alphabet_size;
        reconciled_vnode_idx = (reconciled_vnode_idx + index_to_char_to_hash * vnode_distance) % client->num_vnode;

        reconciled_vnode_idx++;
        serverId = (int)reconciled_vnode_idx % dart->num_server;

        printf("Reconcile happened. from %d to %d\n", vnode_idx , reconciled_vnode_idx);
        if (reconciled_vnode_idx == vnode_idx) {
            for (i=0; i < dart->num_server; i++) {
                dart->all_servers[i].max_word_count = dart->all_servers[serverId].max_word_count * 2;
            }
            //dart->all_servers[serverId].max_word_count = dart->all_servers[serverId].max_word_count * 2;
            break;
        }
    }
    return serverId;
}

int virtual_node_power_of_two_choice_rehashing(uint32_t vnode_idx, char *word, dart_client *client, char *op_type){
    int tree_height = client->dart_tree_height;

    printf("vnode_idx = %d\n", vnode_idx);
    int serverId = vnode_idx % dart->num_server;


    uint32_t reconciled_vnode_idx = vnode_idx;


    int leaf_index = tree_height;
    if (strlen(word)< leaf_index) {
        leaf_index = strlen(word)-1;
    }

    int index_to_char_to_hash = 0;

    uint32_t vnode_distance = client->num_vnode / dart->alphabet_size;

    int word_len = strlen(word);
    for (; leaf_index < word_len; leaf_index++){
        index_to_char_to_hash = (int)word[leaf_index] % dart->alphabet_size;
        reconciled_vnode_idx = (reconciled_vnode_idx + (index_to_char_to_hash) * vnode_distance) % client->num_vnode;
    }

    int reconcile_serverId = reconciled_vnode_idx % dart->num_server;
//    reconcile_serverId = reconciled_vnode_idx%dart->num_server;


//    int i = 0;
//    uint32_t min_word_count = UINT32_MAX;
//    for (i = 0; i < dart->alphabet_size; i++) {
//        reconciled_vnode_idx = (vnode_idx + (index_to_char_to_hash+i) * vnode_distance) % client->num_vnode;
//        if (dart->all_servers[reconciled_vnode_idx%dart->num_server].word_count < min_word_count){
//            min_word_count = dart->all_servers[reconciled_vnode_idx%dart->num_server].word_count;
//            reconcile_serverId = reconciled_vnode_idx%dart->num_server;
//        }
//    }

    //reconcile_serverId = rand() % dart->num_server;

    if (strcmp(op_type, "query")){
        return reconcile_serverId;
    }

    if (dart->all_servers[serverId].word_count > dart->all_servers[reconcile_serverId].word_count) {
        printf("Reconcile happened. from %d to %d\n", vnode_idx , reconciled_vnode_idx);
        return reconcile_serverId;
    }
    return serverId;
}

void insert_word_to_vnode(dart_client *client, char *word) {
    int tree_height = client->dart_tree_height;
    uint32_t vnode_idx = dart_init_hash(tree_height, word);

    int server_idx = vnode_idx % dart->num_server;

#ifdef REHASHING
    //server_idx = virtual_node_average_rehashing(vnode_idx, word, client);
    server_idx = virtual_node_power_of_two_choice_rehashing(vnode_idx, word, client, "insert");
#endif

    printf("Inserting term %s from client %d to server %d\n", word, client->id, server_idx);
    insert_word_to_server((int)(server_idx), word);
}


void lookup_word_from_vnode(dart_client *client, char *word) {
    int tree_height = client->dart_tree_height;
    uint32_t vnode_idx = dart_init_hash(tree_height, word);

    int dest_server_Id = vnode_idx % dart->num_server;
    int reconciled_server_Id = virtual_node_power_of_two_choice_rehashing(vnode_idx, word, client, "query");


}

void init_input_type(int *INPUT_TYPE, char *type_str){
    if (strcmp("rand", type_str)==0){
        *INPUT_TYPE = INPUT_RANDOM_STRING;
    } else if(strcmp("uuid", type_str)==0){
        *INPUT_TYPE = INPUT_UUID;
    } else if(strcmp("dict", type_str)==0){
        *INPUT_TYPE = INPUT_DICTIONARY;
    } else if(strcmp("wiki", type_str)==0){
        *INPUT_TYPE = INPUT_WIKI_KEYWORD;
    }
}

void init_hash_type(int *hash_type, char *type_str){
    if (strcmp("one", type_str)==0){
        *hash_type = HASH_PREFIX_ONE;
    } else if(strcmp("two", type_str)==0){
        *hash_type = HASH_PREFIX_TWO;
    } else if(strcmp("three", type_str)==0){
        *hash_type = HASH_PREFIX_THREE;
    } else if(strcmp("dart", type_str)==0){
        *hash_type = HASH_DART_INIT;
    }
}

void init(int argc, char **argv, int *INPUT_TYPE, int *HASH_TYPE){

    if (argc < 6) {
        printf("Usage: sim.exe <input_type> <hash_type> <num_client> <num_server> <alphabet_size> \n");
        exit(1);
    }

    init_input_type(INPUT_TYPE, argv[1]);
    init_hash_type(HASH_TYPE, argv[2]);

    int num_client = atoi(argv[3]);
    int num_server = atoi(argv[4]);
    int alphabet_size = atoi(argv[5]);

    random_seed(0);

    printf("Initializing dart space... \n");

    dart_space_init(num_client, num_server, alphabet_size);
}

int main(int argc, char **argv){


    int INPUT_TYPE = INPUT_UUID;
    int HASH_TYPE = HASH_DART_INIT;

    init(argc, argv, &INPUT_TYPE, &HASH_TYPE);

    char **input_word_list = NULL;
    int word_count_per_server = 1000;
    int word_count = word_count_per_server * dart->num_server;

    if (INPUT_TYPE == INPUT_DICTIONARY) {
        input_word_list = read_words_from_text(argv[6], &word_count);
    } else if (INPUT_TYPE == INPUT_RANDOM_STRING){
        input_word_list = gen_random_strings(word_count, 16, dart->alphabet_size);
    } else if (INPUT_TYPE == INPUT_UUID) {
        input_word_list = gen_uuids(word_count);
    } else if (INPUT_TYPE == INPUT_WIKI_KEYWORD) {
        input_word_list = read_words_from_text(argv[6], &word_count);
    }

    printf("word list generated %s\n", input_word_list[1]);
    //exit(0);

    double start = get_current_time();

    // create index
    int w = 0; // counter for iterating words
    for (w = 0; w < word_count; w++) {
        int clt_idx = w % dart->num_client;
        dart_client *clt = &dart->all_clients[clt_idx];
        char *word = input_word_list[w];

        if (clt_idx == 0) {
            global_tick(get_comm_time());
        }

        if (HASH_TYPE == HASH_PREFIX_ONE) {
            insert_word_to_server(md5_hash(1, word), word);
        } else if (HASH_TYPE == HASH_PREFIX_TWO) {
            insert_word_to_server(md5_hash(3, word), word);
        } else if (HASH_TYPE == HASH_PREFIX_THREE) {
            insert_word_to_server(md5_hash(2, word), word);
        } else if (HASH_TYPE == HASH_DART_INIT){
            insert_word_to_vnode(clt, word);
        }
    }


    double sqrt_sum = 0;
    double sum = 0;
    int avg_word_count = word_count/dart->num_server;
    int s = 0;
    for (s = 0; s < dart->num_server; s++) {
        dart_server server = dart->all_servers[s];
        printf("Server %d has %d keys\n", server.id, server.word_count);
        sum += server.word_count;
        sqrt_sum += (double)((double)server.word_count * (double)server.word_count);
        //sqrt_sum=sqrt_sum+ (server.word_count - avg_word_count)*(server.word_count - avg_word_count);
    }
    double mean = sum/dart->num_server;
    double variance = sqrt_sum/dart->num_server - mean * mean;
    double stddev = sqrt(variance);
    double duration = get_current_time()-start;
    printf("%d keys inserted spent %0.9f seconds\n", word_count, duration);
    printf("Throughput = %.9f Insertions/sec\n", (double)word_count/duration);
    printf("sum of keywords = %f\n", sum);
    printf("sqrt sum of keywords = %f\n", sqrt_sum);
    printf("mean of keywords = %f\n", mean);
    printf("variance of keywords = %f\n", variance);
    printf("Standard Deviation of key distribution on %d servers = %.9f\n", dart->num_server, stddev);

    
    return 0;
}