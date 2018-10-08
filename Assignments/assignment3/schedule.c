#include<context.h>
#include<memory.h>
#include<schedule.h>
#include<lib.h>
#include<idt.h>
#include<apic.h>

static u64 numticks;

static void save_current_context()
{
}

static void schedule_context(struct exec_context *next)
{
    /*Your code goes in here. get_current_ctx() still returns the old context*/
    struct exec_context *current = get_current_ctx();
    printf("schedluing: old pid = %d  new pid  = %d\n", current->pid, next->pid); /*XXX: Don't remove*/
    /*These two lines must be executed*/
    set_tss_stack_ptr(next);
    set_current_ctx(next);
    /*Your code for scheduling context*/
    next->state = RUNNING;
    asm volatile ("pushq %0" :: "r"(next->regs.entry_rip));
    asm volatile ("pushq %0" :: "r"(next->regs.entry_cs));
    asm volatile ("pushq %0" :: "r"(next->regs.entry_rflags));
    asm volatile ("pushq %0" :: "r"(next->regs.entry_rsp));
    asm volatile ("pushq %0" :: "r"(next->regs.entry_ss));
    asm volatile ("iretq");
    return;
}

static struct exec_context *pick_next_context(struct exec_context *list)
{
    struct exec_context *current = get_current_ctx();
    return NULL;
}
static void schedule()
{
    /*
    struct exec_context *next;
    struct exec_context *current = get_current_ctx();
    struct exec_context *list = get_ctx_list();
    next = pick_next_context(list);
    schedule_context(next);
    */
}

static void do_sleep_and_alarm_account()
{
    /*All processes in sleep() must decrement their sleep count*/
}

/*The five functions above are just a template. You may change the signatures as you wish*/
void handle_timer_tick()
{
    /*
    This is the timer interrupt handler.
    You should account timer ticks for alarm and sleep
    and invoke schedule
    */
    asm volatile("cli;"
                    :::"memory");
    printf("Got a tick. #ticks = %u\n", numticks++);   /*XXX Do not modify this line*/

    // For sleeping processes
    do_sleep_and_alarm_account();

    struct exec_context *current = get_current_ctx();
    printf("\nCurrent ticks_to_alarm is %d\n", current->ticks_to_alarm);
    if (current->ticks_to_alarm > 0)
        current->ticks_to_alarm--;
    else if (current->alarm_config_time && (current->pending_signal_bitmap && (1 << SIGALRM)))
        {
            printf("\nEncountered Alarm.\n");
            u64 *rbp;
            asm volatile ("movq %%rbp, %0" : "=r"(rbp));
            printf("RIP: %x, RSP: %x\n", rbp[1], rbp[4]);
            invoke_sync_signal(SIGALRM, &rbp[4], &rbp[1]);
            // Clear pending bit
            current->pending_signal_bitmap ^= (1 << SIGALRM);
        }

    ack_irq();  /*acknowledge the interrupt, before calling iretq */

    asm volatile("mov %%rbp, %%rsp;"
                "pop %%rbp;"
                "iretq;"
                :::"memory");
}

void do_exit()
{
    /*You may need to invoke the scheduler from here if there are
    other processes except swapper in the system. Make sure you make
    the status of the current process to UNUSED before scheduling
    the next process. If the only process alive in system is swapper,
    invoke do_cleanup() to shutdown gem5 (by crashing it, huh!)
    */
    do_cleanup();  /*Call this conditionally, see comments above*/
}

/*system call handler for sleep*/
long do_sleep(u32 ticks)
{
    struct exec_context *current = get_current_ctx();
    current->ticks_to_sleep = ticks;
    current->state = WAITING;
    struct exec_context *swapper_context = get_ctx_by_pid(0);
    schedule_context(swapper_context);
    return 0;
}

/*
  system call handler for clone, create thread like
  execution contexts
*/
long do_clone(void *th_func, void *user_stack)
{
}

long invoke_sync_signal(int signo, u64 *ustackp, u64 *urip)
{
    /*If signal handler is registered, manipulate user stack and RIP to execute signal handler*/
    /*ustackp and urip are pointers to user RSP and user RIP in the exception/interrupt stack*/
    printf("Called signal with ustackp=%x urip=%x\n", *ustackp, *urip);
    /*Default behavior is exit( ) if sighandler is not registered for SIGFPE or SIGSEGV.
      Ignore for SIGALRM*/
    struct exec_context *current = get_current_ctx();
    if (signo == SIGALRM)
    {
        if (current->sighandlers[SIGALRM] == 0)
            return 0;
        *ustackp = *urip;
        *urip = (u64)(current->sighandlers[SIGALRM]);
        return 0;
    }
    printf("DEBUG: %x\n", current->sighandlers[signo]);
    if (current->sighandlers[signo] != 0)
        *(urip) = (u64)(current->sighandlers[signo]);
    else
        do_exit();
    /*
    struct exec_context *ctx = get_current_ctx();
    if (signo != SIGALRM)
        {
            if (signo == SIGSEGV)
                {
                    if (ctx->sighandlers[SIGSEGV] != 0)
                        *(urip) = (u64)(ctx->sighandlers[SIGSEGV]);
                    else
                        do_exit();
                }
            else if (signo == SIGFPE)
                {
                    if (ctx->sighandlers[SIGFPE] != 0)
                        *(urip) = (u64)(ctx->sighandlers[SIGFPE]);
                    else
                        do_exit();
                }
        }
    else
        do_exit();
    */
    return 0;
}

/*system call handler for signal, to register a handler*/
long do_signal(int signo, unsigned long handler)
{
    struct exec_context *current = get_current_ctx();
    unsigned long* handlers = (unsigned long*)current->sighandlers;
    printf("\nCalled do_signal with %d signal number\n", signo);
    current->pending_signal_bitmap |= 1 << signo;
    handlers[signo] = handler;
    //printf("DEBUG: bitmap: %d -> handler: %x\n", current->pending_signal_bitmap, current->sighandlers[signo]);
    return 0;
}

/*system call handler for alarm*/
long do_alarm(u32 ticks)
{
    struct exec_context *current = get_current_ctx();
    current->ticks_to_alarm = ticks;
    current->alarm_config_time = ticks;
    printf("\nCalled do_alarm with %d ticks.\n", ticks);
    return 0;
}
