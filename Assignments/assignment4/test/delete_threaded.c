#include "kvstore.h"
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

/* FILE *f; */

void * c1(void* arg)
{
    for(int ctr=0; ctr<50; ++ctr){
        char key[32];
        sprintf(key, "CS330###%d", ctr);

        if(delete_key(key) < 0)
            printf("Delete error\n");
        else
            printf("Deleted %s\n", key);
    }
    pthread_exit(0);
}

void * c2(void* arg)
{
    for(int ctr=50; ctr<100; ++ctr){
        char key[32];
        sprintf(key, "CS330###%d", ctr);

        if(delete_key(key) < 0)
            printf("Delete error\n");
        else
            printf("Deleted %s\n", key);
    }
    pthread_exit(0);
}

void * c3(void* arg)
{
    for(int ctr=100; ctr<150; ++ctr){
        char key[32];
        sprintf(key, "CS330###%d", ctr);

        if(delete_key(key) < 0)
            printf("Delete error\n");
        else
            printf("Deleted %s\n", key);
    }
    pthread_exit(0);
}

void * c4(void* arg)
{
    for(int ctr=150; ctr<200; ++ctr){
        char key[32];
        sprintf(key, "CS330###%d", ctr);

        if(delete_key(key) < 0)
            printf("Delete error\n");
        else
            printf("Deleted %s\n", key);
    }
    pthread_exit(0);
}

int main()
{
    pthread_t threads[4];
    /* f = fopen("tmp", "w"); */
    if(pthread_create(&threads[0], NULL, c1, NULL) != 0){
        perror("pthread_create");
        exit(-1);
    }
   if(pthread_create(&threads[1], NULL, c2, NULL) != 0){
        perror("pthread_create");
        exit(-1);
    }
    if(pthread_create(&threads[2], NULL, c3, NULL) != 0){
        perror("pthread_create");
        exit(-1);
    }
    if(pthread_create(&threads[3], NULL, c4, NULL) != 0){
        perror("pthread_create");
        exit(-1);
    }
    for(int ctr=0; ctr<4 ; ++ctr)
        pthread_join(threads[ctr], NULL);
    return 0;
}
