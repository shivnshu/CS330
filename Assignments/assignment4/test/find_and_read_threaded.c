#include "kvstore.h"
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

/* FILE *f; */

void * c1(void* arg)
{
    int size=1024*24;
    for(int ctr=0; ctr<50; ++ctr){
        char* ptr=malloc(size);
        char key[32];
        sprintf(key, "CS330###%d", ctr);

        if(get_key(key, ptr) < 0)
            printf("Read c1 error\n");
        printf("For key = %s value=%d\n", key, strlen(ptr));
        free(ptr);
    }
    pthread_exit(0);
}

void * c2(void* arg)
{
    int size=1024*24;
    for(int ctr=50; ctr<100; ++ctr){
        char* ptr=malloc(size);
        char key[32];
        sprintf(key, "CS330###%d", ctr);
        if(get_key(key, ptr) < 0)
            printf("Read c2 error\n");
        printf("For key = %s value=%d\n", key, strlen(ptr));
        free(ptr);
    }
    pthread_exit(0);
}

void * c3(void* arg)
{
	int size=1024*24;
    for(int ctr=100; ctr<150; ++ctr){
        char* ptr=malloc(size);
        char key[32];
        sprintf(key, "CS330###%d", ctr);
        if(get_key(key, ptr) < 0)
             printf("Read c3 error\n");
        printf("For key = %s value=%d\n", key, strlen(ptr));
        free(ptr);
    }
    pthread_exit(0);
}

int main()
{
    pthread_t threads[3];
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
    for(int ctr=0; ctr<3 ; ++ctr)
        pthread_join(threads[ctr], NULL);
    return 0;
}
