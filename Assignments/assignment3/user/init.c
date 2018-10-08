#include<init.h>
#include<lib.h>
static void exit(int);
static int main(void);

void init_start()
{
  int retval = main();
  exit(0);
}

/*Invoke system call with no additional arguments*/
static long _syscall0(int syscall_num)
{
  asm volatile ( "int $0x80;"
                 "leaveq;"
                 "retq;"
                :::"memory");
  return 0;   /*gcc shutup!*/
}

/*Invoke system call with one argument*/
static long _syscall1(int syscall_num, int exit_code)
{
  asm volatile ( "int $0x80;"
                 "leaveq;"
                 "retq;"
                :::"memory");
  return 0;   /*gcc shutup!*/
}

/*Invoke system call with two arguments*/
static long _syscall2(int syscall_num, u64 arg1, u64 arg2)
{
  asm volatile ( "int $0x80;"
                 "leaveq;"
                 "retq;"
                :::"memory");
  return 0;   /*gcc shutup!*/
}


static void exit(int code)
{
  _syscall1(SYSCALL_EXIT, code);
}

static long getpid()
{
  return(_syscall0(SYSCALL_GETPID));
}

static long write(char *ptr, int size)
{
   return(_syscall2(SYSCALL_WRITE, (u64)ptr, size));
}

static long signal(int signo, unsigned long handler)
{
    return(_syscall2(SYSCALL_SIGNAL, signo, handler));
}

static long alarm(int ticks)
{
    return(_syscall1(SYSCALL_ALARM, ticks));
}

int signal_handler()
{
    write("hello\n", 6);
    exit(0);
}

static int main()
{
    /*
    //signal(1, (unsigned long)(0x123456));
    signal(0, (unsigned long)(&signal_handler));
    int i;
    int *addr = (int *)0x4445555;
    //*addr = 3;
    i = 10 / (10 - 10);
    */
    signal(2, (unsigned long)(&signal_handler));
    alarm(10);
    while (1);
    return 0;
}
