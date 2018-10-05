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

void signal_handler()
{
    /*asm volatile ("pushq %rax;"*/
                  /*"pushq %rbx;"*/
                  /*"pushq %rcx;"*/
                  /*"pushq %rdx;"*/
                  /*"pushq %rsi;"*/
                  /*"pushq %rdi;"*/
                  /*"pushq %r8;"*/
                  /*"pushq %r9;"*/
                  /*"pushq %r10;"*/
                  /*"pushq %r11;"*/
                  /*"pushq %r12;"*/
                  /*"pushq %r13;"*/
                  /*"pushq %r14;"*/
                  /*"pushq %r15;");*/
    write("In custom signal handler\n", 25);
    printf("In custom signal handler by printf\n");
    /*asm volatile ("popq %r15;"*/
                  /*"popq %r14;"*/
                  /*"popq %r13;"*/
                  /*"popq %r12;"*/
                  /*"popq %r11;"*/
                  /*"popq %r10;"*/
                  /*"popq %r9;"*/
                  /*"popq %r8;"*/
                  /*"popq %rdi;"*/
                  /*"popq %rsi;"*/
                  /*"popq %rdx;"*/
                  /*"popq %rcx;"*/
                  /*"popq %rbx;"*/
                  /*"popq %rax;");*/
    exit(0);
}

static int main()
{
    signal(1, (unsigned long)signal_handler);
    int i = 10 / (10 - 10);
    return 0;
}
