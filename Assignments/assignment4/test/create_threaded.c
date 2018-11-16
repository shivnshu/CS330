#include "kvstore.h"
#include <pthread.h>

void *c1(void* arg)
{
	int size=1024*100;
	for(int ctr=0; ctr<50; ++ctr){
        char* ptr=malloc(size);
        for(long i=0;i<size;i++)
            ptr[i]='a' + (i%26);
        ptr[size-1]='\0';
        char key[32];
        sprintf(key, "CS330###%d", ctr);
        if(put_key(key, ptr, size) < 0)
             printf("Create error\n");
        free(ptr);
    }
    pthread_exit(0);
}

void * c2(void* arg)
{
	int size=1024*100;
    for(int ctr=50; ctr<100; ++ctr){
        char* ptr=malloc(size);
        for(long i=0;i<size;i++)
            ptr[i]='b' + (i%26);
        ptr[size-1]='\0';
        char key[32];
        sprintf(key, "CS330###%d", ctr);
        if(put_key(key, ptr, size) < 0)
            printf("Create error\n");
        free(ptr);
    }
    pthread_exit(0);
}

void *c3(void* arg)
{
    int size=1024*100;
    for(int ctr=100; ctr<150; ++ctr){
        char* ptr=malloc(size);
        for(long i=0;i<size;i++)
            ptr[i]='c' + (i%26);
        ptr[size-1]='\0';
        char key[32];
        sprintf(key, "CS330###%d", ctr);
        if(put_key(key, ptr, size) < 0)
             printf("Create error\n");
    }
    pthread_exit(0);
}

void *c4(void* arg)
{
    int size=1024*100;
    for(int ctr=150; ctr<200; ++ctr){
        char* ptr=malloc(size);
        for(long i=0;i<size;i++)
            ptr[i]='c' + (i%26);
        ptr[size-1]='\0';
        char key[32];
        sprintf(key, "CS330###%d", ctr);
        if(put_key(key, ptr, size) < 0)
            printf("Create error\n");
    }
    pthread_exit(0);
}

int main()
{
    pthread_t threads[4];

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
