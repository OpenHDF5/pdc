#ifndef DART_UTILS_H
#define DART_UTILS_H

#include "dart_commons.h"



void to_bytes(uint32_t val, uint8_t *bytes);

uint32_t to_int32(const uint8_t *bytes);

void md5(const uint8_t *initial_msg, size_t initial_len, uint8_t *digest);

double get_current_time();

unsigned long
hash(char *str, int len);

float get_comm_time();

void global_tick();

char **gen_uuids(int count);

char **gen_random_strings(int count, int maxlen, int alphabet_size);

char **read_words_from_text(const char *fileName, int *word_count);

#endif //DART_UTILS_H