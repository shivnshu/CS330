#include "kvstore.h"
#include <pthread.h>

void * c1(void* arg)
{
	int size=1024*1024*1;
	for(int ctr=1; ctr<=500; ++ctr){
        char* ptr=malloc(size);
		    for(long i=0;i<size;i++)ptr[i]='a';
		    ptr[size-1]='\0';
        char key[32];
        sprintf(key, "CS330###%d", ctr);
       
        if(put_key(key, ptr, size) < 0)
             printf("Create error\n");
    }
    pthread_exit(0);
}

void * c3(void* arg)
{
	int size=1048576;
  for(int ctr=161; ctr<=320; ++ctr){
        char* ptr=malloc(size);
        for(long i=0;i<size;i++)ptr[i]='a';
        ptr[size-1]='\0';
        char key[32];
        sprintf(key, "CS330###%d", ctr);
       
        if(put_key(key, ptr, size) < 0)
             printf("Create error\n");
    }
    pthread_exit(0);
}

void * c2(void* arg)
{
	int size=1048576;
  for(int ctr=321; ctr<=500; ++ctr){
        char* ptr=malloc(size);
        for(long i=0;i<size;i++)ptr[i]='a';
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
    int total=500, ctr;
    pthread_t threads[3];
   

    if(pthread_create(&threads[0], NULL, c1, NULL) != 0){
              perror("pthread_create");
              exit(-1);
    }
    /*if(pthread_create(&threads[1], NULL, c2, NULL) != 0){
              perror("pthread_create");
              exit(-1);
    }
    if(pthread_create(&threads[2], NULL, c3, NULL) != 0){
              perror("pthread_create");
              exit(-1);
    }*/
       for(ctr=0; ctr <1 ; ++ctr)
           pthread_join(threads[ctr], NULL);

    return 0;


    
}
