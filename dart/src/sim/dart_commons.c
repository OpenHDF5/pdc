#include "dart_commons.h"

string new_string(const char *arr){
    printf("begin string a\n");
    string a;

    printf("begin string a\n");
    a.start = arr;
    printf("assign value");
    a.length = strlen(arr);
    printf("about to return");
    return a;
}

string substring(const string original, int start, int end){
    string rst;
    rst.start = original.start+start;
    rst.length = end - start;
    return rst;
}

double log_with_base(double base, double x){
    return log(x)/log(base);
}