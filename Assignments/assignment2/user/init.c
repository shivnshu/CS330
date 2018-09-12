#include<init.h>
#include<memory.h>

static void exit(int);
static int main(void);


void init_start()
{
  int retval = main();
  exit(0);
}

static int _syscall0(int syscall_num)
{
  asm volatile ( "int $0x80;"
                 "leaveq;"
                 "retq;"
                :::"memory");
  return 0;
}

static int _syscall1(int syscall_num, int exit_code)
{
  asm volatile ( "int $0x80;"
                 "leaveq;"
                 "retq;"
                :::"memory");
  return 0;
}

static u64 _syscall2(int syscall_num, u64 param1, u64 param2)
{
    asm volatile ( "int $0x80;"
                 "leaveq;"
                 "retq;"
                :::"memory");
    return 0;
}

static void exit(int code)
{
  _syscall1(SYSCALL_EXIT, code); 
}

static int getpid()
{
  return(_syscall0(SYSCALL_GETPID));
}

static int write (char *buf, int length)
{
    return(_syscall2(SYSCALL_WRITE, (u64)buf, length));
}

static void *expand (u32 size, int flags)
{
    return (void *)(_syscall2(SYSCALL_EXPAND, size, flags));
}

static void *shrink (u32 size, int flags)
{
    return (void *)(_syscall2(SYSCALL_SHRINK, size, flags));
}

void test_code()
{
    void *ptr1;
    char *ptr = (char *) expand(8, MAP_WR);
  
    if (ptr == NULL)
        write("FAILED\n", 7);
  
    *(ptr + 8192) = 'A';
  
    ptr1 = (char *) shrink(7, MAP_WR);
    *ptr = 'A';

    /**(ptr + 4096) = 'A';*/

    int x = 1/(1-1);
}

void tricky_test_code()
{
    char* ptr = ( char* ) expand(8, MAP_WR);
	*(ptr + 40) = 'X';
	write(ptr+40,1);
    shrink(8, MAP_WR);
    /**(ptr + 40) = 'A';*/
}

static int main()
{
  unsigned long i;
#if 0
  unsigned long *ptr = (unsigned long *)0x100032;
  i = *ptr;
#endif
  test_code();
  /*tricky_test_code();*/
  i = getpid();
  exit(-5);
  return 0;
}
