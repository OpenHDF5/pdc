#include "dart_utils.h"


float get_comm_time() {
    int vary = rand()%1000;
    float rst = net_comm_time_base + ((float)vary)/100000000.0;
    return rst;
}

void global_tick(){
    global_clock += get_comm_time();
}

char **gen_uuids(int count){
    uuid_t out;
    int c = 0;
    char **result = (char **)calloc(count, sizeof(char*));
    for (c = 0; c < count ; c++) {
        uuid_generate_random(out);
        char uuid_str[37];
        uuid_unparse_lower(out, (char *)uuid_str);
        //printf("generated %s\n", uuid_str);
        result[c] = uuid_str;
    }
    return result;
}

char **gen_random_strings(int count, int maxlen){
    
    int c = 0;
    int i = 0;
    char **result = (char **)calloc(count, sizeof(char*));
    for (c = 0; c < count ; c++) {
        //int len = maxlen;//rand()%maxlen;
        int len = rand()%maxlen;
        char *str = (char *)calloc(len, sizeof(len));
        for (i = 0; i < len-1; i++) {
            int randnum = rand();
            if (randnum < 0) randnum *= -1;
            char c = (char)(randnum%DART_CHAR_SET_SIZE);
            str[i] = c;
        }
        str[len-1] = '\0';
        //printf("generated %s\n", str);
        result[c] = str;
    }
    return result;
}

char **read_words_from_text(char* argv[], int *word_count){
    char const* const fileName = argv[1]; /* should check that argc > 1 */
    FILE* file = fopen(fileName, "r"); /* should check the result */
    int lines_allocated =128;
    int max_line_len = 256;
    char **words = (char **)malloc(sizeof(char*)*lines_allocated
            );
    if (words == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    int i;
    for (i = 0; 1; i++) {

        int j;

        if (i >= lines_allocated) {
            int new_size;
            new_size = lines_allocated * 2;
            words = (char **)realloc(words, sizeof(char*)*new_size);
            if (words == NULL) {
                fprintf(stderr, "Out of memory\n");
                exit(3);
            }
            lines_allocated = new_size;
        }
        words[i] = (char *)malloc(sizeof(char)*max_line_len);
        if (words[i]==NULL) {
            fprintf(stderr, "out of memory\n");
            exit(4);
        }
        if (fgets(words[i], max_line_len-1, file)==NULL) {
            break;
        }
        /* Get rid of CR or LF at end of line */
        for (j=strlen(words[i])-1;j>=0 && (words[i][j]=='\n' || words[i][j]=='\r');j--)
            ;
        words[i][j+1]='\0';

    }
    *word_count = i;

    fclose(file);
    return words;
}