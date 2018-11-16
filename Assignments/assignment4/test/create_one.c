#include "kvstore.h"
#include <stdlib.h>

void main()
{
    int size = 1024*1024*13;
    char *value;
    value = (char *)malloc(size);
    /*char value[size];*/
    char key[32];
    sprintf(key, "test");
    for (int i=0;i<size;++i) {
        value[i] = 'a' + (i%26);
    }
    if(put_key(key, value, size) < 0)
         printf("Create error\n");
    free(value);
}
