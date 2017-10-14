#ifndef DART_COMMONS_H
#define DART_COMMONS_H

#include <assert.h>
#include <complex.h>
#include <ctype.h>
#include <errno.h>
#include <fenv.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tgmath.h>
#include <time.h>
#include <uuid/uuid.h>



#define ADDR_MAX 128
#define SUCCEED 1
#define FAIL    0

int DART_CHAR_SET_SIZE = 26;

typedef struct {
    const char *start;
    int length;
} string;



string new_string(const char *arr);
string substring(const string original, int start, int end);
double log_with_base(double base, double x);


#endif //DART_COMMONS_H