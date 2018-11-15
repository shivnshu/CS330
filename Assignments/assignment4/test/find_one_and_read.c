#include "kvstore.h"

int main(int argc, char **argv)
{
    int size = 1024*1024*6;
  if (argv[1]) {
    char *value;
    value = (char *)malloc(size);
      /*char value[size];*/
    if(get_key(argv[1], value) < 0) {
      printf("Get error for key = %s\n", argv[1]);
    } else {
      printf("For key = %s value=%s\n", argv[1], value);
    }
    free(value);
  } else {
    printf("Usage: %s <key>\n", argv[0]);
  }

}
