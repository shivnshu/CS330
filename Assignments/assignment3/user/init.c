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

static void sleep(int ticks)
{
    _syscall1(SYSCALL_SLEEP, ticks);
}

static int sigfpe_signal_handler()
{
    write("Hello from the SIGFPE Handler\n", 30);
    /* exit(0); */
}

static int sigsegv_signal_handler()
{
    write("Hello from the SIGSEG Handler\n", 30);
    /* exit(0); */
}

static int alarm_custom_handler()
{
    write("I am alarmed.\n", 14);
}

static int main()
{
    sleep(10);
    write("in the main\n", 12);
    return 0;
}
