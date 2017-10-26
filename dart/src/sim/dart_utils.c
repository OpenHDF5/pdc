#include "dart_utils.h"


// calculated as second.
double global_clock = 0.0;
double net_comm_time_base = 0.00005000;

double get_current_time(){
    return global_clock;
}



// Constants are the integer part of the sines of integers (in radians) * 2^32.
const uint32_t k[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee ,
        0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501 ,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be ,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821 ,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa ,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8 ,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed ,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a ,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c ,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70 ,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05 ,
        0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665 ,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039 ,
        0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1 ,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1 ,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };

// r specifies the per-round shift amounts
const uint32_t r[] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                      5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
                      4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                      6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

// leftrotate function definition
#define LEFTROTATE(x, c) (((x) << (c)) | ((x) >> (32 - (c))))

void to_bytes(uint32_t val, uint8_t *bytes)
{
    bytes[0] = (uint8_t) val;
    bytes[1] = (uint8_t) (val >> 8);
    bytes[2] = (uint8_t) (val >> 16);
    bytes[3] = (uint8_t) (val >> 24);
}

uint32_t to_int32(const uint8_t *bytes)
{
    return (uint32_t) bytes[0]
           | ((uint32_t) bytes[1] << 8)
           | ((uint32_t) bytes[2] << 16)
           | ((uint32_t) bytes[3] << 24);
}

void md5(const uint8_t *initial_msg, size_t initial_len, uint8_t *digest) {

    // These vars will contain the hash
    uint32_t h0, h1, h2, h3;

    // Message (to prepare)
    uint8_t *msg = NULL;

    size_t new_len, offset;
    uint32_t w[16];
    uint32_t a, b, c, d, i, f, g, temp;

    // Initialize variables - simple count in nibbles:
    h0 = 0x67452301;
    h1 = 0xefcdab89;
    h2 = 0x98badcfe;
    h3 = 0x10325476;

    //Pre-processing:
    //append "1" bit to message
    //append "0" bits until message length in bits ≡ 448 (mod 512)
    //append length mod (2^64) to message

    for (new_len = initial_len + 1; new_len % (512/8) != 448/8; new_len++)
        ;

    msg = (uint8_t*)malloc(new_len + 8);
    memcpy(msg, initial_msg, initial_len);
    msg[initial_len] = 0x80; // append the "1" bit; most significant bit is "first"
    for (offset = initial_len + 1; offset < new_len; offset++)
        msg[offset] = 0; // append "0" bits

    // append the len in bits at the end of the buffer.
    to_bytes(initial_len*8, msg + new_len);
    // initial_len>>29 == initial_len*8>>32, but avoids overflow.
    to_bytes(initial_len>>29, msg + new_len + 4);

    // Process the message in successive 512-bit chunks:
    //for each 512-bit chunk of message:
    for(offset=0; offset<new_len; offset += (512/8)) {

        // break chunk into sixteen 32-bit words w[j], 0 ≤ j ≤ 15
        for (i = 0; i < 16; i++)
            w[i] = to_int32(msg + offset + i*4);

        // Initialize hash value for this chunk:
        a = h0;
        b = h1;
        c = h2;
        d = h3;

        // Main loop:
        for(i = 0; i<64; i++) {

            if (i < 16) {
                f = (b & c) | ((~b) & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | ((~d) & c);
                g = (5*i + 1) % 16;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3*i + 5) % 16;
            } else {
                f = c ^ (b | (~d));
                g = (7*i) % 16;
            }

            temp = d;
            d = c;
            c = b;
            b = b + LEFTROTATE((a + f + k[i] + w[g]), r[i]);
            a = temp;

        }

        // Add this chunk's hash to result so far:
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;

    }

    // cleanup
    free(msg);

    //var char digest[16] := h0 append h1 append h2 append h3 //(Output is in little-endian)
    to_bytes(h0, digest);
    to_bytes(h1, digest + 4);
    to_bytes(h2, digest + 8);
    to_bytes(h3, digest + 12);
}



unsigned long
hash(char *str, int len)
{
    unsigned long hash = 5381;
    int i = 0;
    int c;

    for (i = 0; i < len ; i++){
        if (str[i] == '\0') {
            c = 0;
        } else {
            c = (int)str[i];
        }
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

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
        result[c] = (char *)calloc(37, sizeof(char));
        uuid_unparse_lower(out, result[c]);
        //printf("generated %s\n", result[c]);

    }
    return result;
}

char **gen_random_strings(int count, int maxlen, int alphabet_size){
    
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
            char c = (char)((randnum%alphabet_size)+65);
            str[i] = c;
        }
        str[len-1] = '\0';
        //printf("generated %s\n", str);
        result[c] = str;
    }
    return result;
}

char **read_words_from_text(const char *fileName, int *word_count){
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