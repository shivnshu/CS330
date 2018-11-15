#include "kvstore.h"
main()
{
    char value[1024*6];
    char key[32];
    sprintf(key, "test");
    for (int i=0;i<sizeof(value);++i) {
        value[i] = 'a' + (i%26);
    }
    if(put_key(key, value, sizeof(value)) < 0)
         printf("Create error\n");
}
