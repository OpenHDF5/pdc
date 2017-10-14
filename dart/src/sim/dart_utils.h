#ifndef DART_UTILS_H
#define DART_UTILS_H

#include "dart_commons.h"


#define INPUT_RANDOM_STRING 0;
#define INPUT_UUID          1;
#define INPUT_DICTIONARY    2;
#define INPUT_WIKI_KEYWORD  3;

// calculated as second.
double global_clock = 0.0;
double net_comm_time_base = 0.00005000;

float get_comm_time();

void global_tick();

char **gen_uuids(int count);

char **gen_random_strings(int count, int maxlen);

char **read_words_from_text(char* argv[], int *word_count);

#endif //DART_UTILS_H