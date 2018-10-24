#include<init.h>
#include<lib.h>
#include<context.h>
#include<memory.h>
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

static void *expand (u32 size, int flags)
{
    return (void *)(_syscall2(SYSCALL_EXPAND, size, flags));
}

static int clone(unsigned long handler, unsigned long user_stack)
{
    return (_syscall2(SYSCALL_CLONE, handler, user_stack));
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

static void clone_handler()
{
    write("I am clone\n", 11);
    exit(0);
}

void initialize(u64 *ptr, int limit)
{
    int i;
    for (i=0;i<limit;++i)
        ptr[i] = 0;
}

static int main()
{
    /* sleep(10); */
    /* alarm(5); */
    /* while(1); */
    /* signal(1, (unsigned long)&sigfpe_signal_handler); */
    /* int i; */
    /* i = 1; */
    /* i /= 0; */
    /* signal(0, (unsigned long)&sigsegv_signal_handler); */
    /* int *ptr = (int *)0x1234; */
    /* *ptr = 10; */
    int num_pages = 1;
    u64 *ptr = (u64 *)expand(num_pages, MAP_WR);
    initialize(ptr, 512*num_pages);
    clone((unsigned long)&clone_handler, (unsigned long)&ptr[512*num_pages-1]);
    write("In the main\n", 12);
    sleep(5);
    write("In the main again\n", 18);
    return 0;
}
