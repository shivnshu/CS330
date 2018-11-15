#include "kvstore.h"
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

void * c1(void* arg)
{
	    int size=1024*1024*1;
        int l=320;
        for(int ctr=1; ctr<=160; ++ctr){
        char* ptr=malloc(size);
        char key[32];
        sprintf(key, "CS330###%d", ctr);
       
        if(get_key(key, ptr) < 0)
             printf("Create error\n");
         printf("For key = %s value=%d\n", key, strlen(ptr));
     
 }

    
	
    pthread_exit(0);
}

void * c2(void* arg)
{
	int size=1048576;
        int l=320;
        for(int ctr=161; ctr<=320; ++ctr){
        char* ptr=malloc(size);
        char key[32];
        sprintf(key, "CS330###%d", ctr);
       
        if(get_key(key, ptr) < 0)
             printf("Create error\n");
         printf("For key = %s value=%d\n", key, strlen(ptr));
     
 }
    pthread_exit(0);
}

void * c3(void* arg)
{
	int size=1048576;
        int l=320;
        for(int ctr=321; ctr<=500; ++ctr){
        char* ptr=malloc(size);
        char key[32];
        sprintf(key, "CS330###%d", ctr);
       
        if(get_key(key, ptr) < 0)
             printf("Create error\n");
         printf("For key = %s value=%d\n", key, strlen(ptr));
     
 }
    pthread_exit(0);
}

int main()
{
    int total=500, ctr;
    pthread_t threads[3];
   

   /*sif(pthread_create(&threads[0], NULL, c1, NULL) != 0){
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
    for(ctr=0; ctr <3 ; ++ctr)
           pthread_join(threads[ctr], NULL);*/
           
		int size=1048576;
        int l=320;
        for(int ctr=500; ctr<=500; ++ctr){
        char* ptr=malloc(size);
	    char key[32];
        sprintf(key, "CS330###%d", ctr);
       
        if(get_key(key, ptr) < 0)
             printf("Create error\n");
         printf("For key = %s value=%d\n", key,strlen(ptr));
         free(ptr);
     }
 
    


     


     char key1[32];
     sprintf(key1, "CS330###3");
     char key2[32];
     sprintf(key2, "Yoyo");
     rename_key(key1,key2);
     /*

     char key3[32];
     sprintf(key3, "CS330###4");
     char key4[32];
     sprintf(key4, "Hello");
     rename_key(key3,key4);


     char key5[32];
     sprintf(key5, "CS330###5");
     char key6[32];
     sprintf(key6, "Rere");
     rename_key(key5,key6);*//*
     // sprintf(key, "CS330###10");
      //delete_key(key);
     	//int size=5120;
        for(int ctr=1; ctr<=3; ++ctr){
        char* ptr=malloc(size);
	    char key[32];
        sprintf(key, "CS330###%d", ctr);
       
        if(get_key(key, ptr) < 0)
             printf("Create error\n");
         printf("For key = %s value=%s\n", key, ptr);

    }

    char* ptr=malloc(32);
	    char key[32];
        sprintf(key, "Yoyo");
       
        if(get_key(key, ptr) < 0)
             printf("Create error\n");
         printf("For key = %s value=%s\n", key, ptr);*/

    return 0;


    
}
