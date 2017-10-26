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

const int INPUT_RANDOM_STRING =0;
const int INPUT_UUID          =1;
const int INPUT_DICTIONARY    =2;
const int INPUT_WIKI_KEYWORD  =3;

const int HASH_PREFIX_ONE = 0;
const int HASH_PREFIX_TWO = 1;
const int HASH_PREFIX_THREE = 2;
const int HASH_DART_INIT = 3;

int INPUT_TYPE = INPUT_RANDOM_STRING;
int HASH_TYPE = HASH_DART_INIT;


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


void dart_server_init(DART *dart, int serverId){
    dart_server *server = &dart->all_servers[serverId];
    server->id = serverId;
    server->word_count=0;
    server->request_per_sec=0;
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

    // Initialize all servers
    for (i = 0; i < num_server; i++) {
        dart_server_init(dart, i);
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

uint32_t dart_init_hash(int tree_height, char *str){
//    char *substr = (char *)calloc(tree_height+1, sizeof(char));
//    memcpy(substr, str, tree_height);
    int n = 0;
    uint32_t c;
    uint32_t rst = 0;
    uint32_t i_t_n;
    for (n = 0; n < tree_height; n++) {
        if (str[n] != '\0') {
            i_t_n = ((uint32_t)str[n])%dart->alphabet_size;
        }
        c= (i_t_n-1) * ((uint32_t)pow(dart->alphabet_size, tree_height - n));
        rst += c;
    }
    return rst;
}

int int_abs(int a){
    if (a < 0) {
        return 0-a;
    }
    return a;
}

int dart_magic_hash(int vnode_index, int tree_height, char *str) {
    int result;
    int str_len = strlen(str);
    int out_tree_char;
    int num_vnodes = (int)pow((double)dart->alphabet_size, (double)tree_height);
    if (tree_height > str_len) {
        result = vnode_index % dart->num_server;
    } else {
        out_tree_char = (int)str[tree_height];
        int otc_idx = out_tree_char % dart->alphabet_size;
        int i = 0;
        int pos = vnode_index;
        //pos = vnode_index % dart->num_server;
        for (i = 0; i < otc_idx; i++){
            if (i % 2 == 0) {
                pos = (pos + num_vnodes/2)%num_vnodes;
            } else {
                pos = (pos/2)%num_vnodes;
            }
        }
        result = pos % dart->num_server;
    }


    return result;

}

void insert_word_to_server(int serverId, char *word){
    dart_server *server = &dart->all_servers[serverId];
    server->word_count = server->word_count + 1;
    //printf("server word count %d\n", server->word_count);
}

void insert_word_to_vnode(int tree_height, char *word) {
    uint32_t vnode_idx = dart_init_hash(tree_height, word);
    //int server_idx = dart_magic_hash(vnode_idx, tree_height, word);
    printf("vnode_idx = %d\n", vnode_idx);
    insert_word_to_server((int)((vnode_idx+1)%dart->num_server), word);
}

int main(int argc, char **argv){

    init(argc, argv);

    char **input_word_list = NULL;
    int word_count = 1000000;
    
    switch(INPUT_TYPE) {
        case INPUT_DICTIONARY:
            input_word_list = read_words_from_text(argv[4], &word_count);
            break;
        case INPUT_RANDOM_STRING:
            input_word_list = gen_random_strings(word_count, 16, dart->alphabet_size);
            break;
        case INPUT_UUID:
            input_word_list = gen_uuids(word_count);
            break;
        case INPUT_WIKI_KEYWORD:
            input_word_list = read_words_from_text(argv[4], &word_count);
            break;
        default:
            input_word_list = gen_uuids(word_count);
            break;
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
        printf("Inserting term %s from client %d \n", word, clt->id);
        if (clt_idx == 0) {
            global_tick();
        }
        switch(HASH_TYPE) {
            case HASH_PREFIX_ONE:
                insert_word_to_server(md5_hash(1, word), word);
                break;
            case HASH_PREFIX_TWO:
                insert_word_to_server(md5_hash(3, word), word);
                break;
            case HASH_PREFIX_THREE:
                insert_word_to_server(md5_hash(2, word), word);
                break;
            case HASH_DART_INIT:
                insert_word_to_vnode(clt->dart_tree_height, word);
                break;
            default:
                break;
        }
    }



    int s = 0;
    for (s = 0; s < dart->num_server; s++) {
        dart_server server = dart->all_servers[s];
        printf("Server %d has %d keys\n", server.id, server.word_count);
    }

    printf("Insertion spent %0.9f seconds\n", get_current_time()-start);

    
    return 0;
}