#include<context.h>
#include<memory.h>
#include<schedule.h>
#include<lib.h>
#include<idt.h>
#include<apic.h>

static u64 numticks;

static void save_current_context(u64 *rbp)
{
    struct exec_context *current = get_current_ctx();
    current->regs.r15 = rbp[2];
    current->regs.r14 = rbp[3];
    current->regs.r13 = rbp[4];
    current->regs.r12 = rbp[5];
    current->regs.r11 = rbp[6];
    current->regs.r10 = rbp[7];
    current->regs.r9 = rbp[8];
    current->regs.r8 = rbp[9];
    current->regs.rbp = rbp[10];
    current->regs.rdi = rbp[11];
    current->regs.rsi = rbp[12];
    current->regs.rdx = rbp[13];
    current->regs.rcx = rbp[14];
    current->regs.rbx = rbp[15];
    current->regs.entry_rip = rbp[16];
    current->regs.entry_cs = rbp[17];
    current->regs.entry_rflags = rbp[18];
    current->regs.entry_rsp = rbp[19];
    current->regs.entry_ss = rbp[20];
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

    if (next != get_ctx_by_pid(0)) {
        asm volatile ("movq %0, %%r15" :: "r"(next->regs.r15));
        asm volatile ("movq %0, %%r14" :: "r"(next->regs.r14));
        asm volatile ("movq %0, %%r13" :: "r"(next->regs.r13));
        asm volatile ("movq %0, %%r12" :: "r"(next->regs.r12));
        asm volatile ("movq %0, %%r11" :: "r"(next->regs.r11));
        asm volatile ("movq %0, %%r10" :: "r"(next->regs.r10));
        asm volatile ("movq %0, %%r9" :: "r"(next->regs.r9));
        asm volatile ("movq %0, %%r8" :: "r"(next->regs.r8));
        asm volatile ("movq %0, %%rbp" :: "r"(next->regs.rbp));
        asm volatile ("movq %0, %%rdi" :: "r"(next->regs.rdi));
        asm volatile ("movq %0, %%rsi" :: "r"(next->regs.rsi));
        asm volatile ("movq %0, %%rdx" :: "r"(next->regs.rdx));
        asm volatile ("movq %0, %%rcx" :: "r"(next->regs.rcx));
        asm volatile ("movq %0, %%rbx" :: "r"(next->regs.rbx));
        asm volatile ("movq %0, %%rax" :: "r"(next->regs.rax));
    }

    asm volatile ("pushq %0" :: "r"(next->regs.entry_ss));
    asm volatile ("pushq %0" :: "r"(next->regs.entry_rsp));
    asm volatile ("pushq %0" :: "r"(next->regs.entry_rflags));
    asm volatile ("pushq %0" :: "r"(next->regs.entry_cs));
    asm volatile ("pushq %0" :: "r"(next->regs.entry_rip));

    ack_irq();  /*acknowledge the interrupt, before calling iretq */

    asm volatile("iretq;"
                 :::"memory");
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

static void do_sleep_and_alarm_account_using_ctx(struct exec_context *ctx)
{
    struct exec_context *current = get_current_ctx();
    if (ctx->ticks_to_alarm > 0) {
        ctx->ticks_to_alarm -= 1;
    }
    if (ctx->ticks_to_sleep > 0) {
        ctx->ticks_to_sleep -= 1;
        if (ctx->ticks_to_sleep == 0) {
            // Current context
            /* save_current_context(); */
            current->state = READY;
            schedule_context(ctx);
        }
    }
}

static void do_sleep_and_alarm_account()
{
    /*All processes in sleep() must decrement their sleep count*/
    struct exec_context *ctx;
    int i;
    for (i=0;i<MAX_PROCESSES;++i) {
        ctx = get_ctx_by_pid(i);
        if (ctx == NULL || ctx->state != WAITING)
            continue;
        do_sleep_and_alarm_account_using_ctx(ctx);
    }
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
    u64 *rsp;
    asm volatile ("pushq %rax;"
                  "pushq %rbx;"
                  "pushq %rcx;"
                  "pushq %rdx;"
                  "pushq %rsi;"
                  "pushq %rdi;"
                  "pushq %r8;"
                  "pushq %r9;"
                  "pushq %r10;"
                  "pushq %r11;"
                  "pushq %r12;"
                  "pushq %r13;"
                  "pushq %r14;"
                  "pushq %r15;");
    asm volatile ("movq %%rsp, %0" : "=r"(rsp));

    printf("Got a tick. #ticks = %u\n", numticks++);   /*XXX Do not modify this line*/

    // For sleeping processes
    do_sleep_and_alarm_account();

    struct exec_context *current = get_current_ctx();
    printf("\nCurrent ticks_to_alarm is %d\n", current->ticks_to_alarm);
    if (current->ticks_to_alarm > 0)
        current->ticks_to_alarm--;
    else if (current->alarm_config_time)
        {
            printf("\nEncountered Alarm.\n");
            u64 *rbp;
            asm volatile ("movq %%rbp, %0" : "=r"(rbp));
            printf("RIP: %x, RSP: %x\n", rbp[1], rbp[4]);
            invoke_sync_signal(SIGALRM, &rbp[4], &rbp[1]);
            current->ticks_to_alarm = current->alarm_config_time;
            // Clear pending bit
            /* current->pending_signal_bitmap ^= (1 << SIGALRM); */
        }

    ack_irq();  /*acknowledge the interrupt, before calling iretq */

    asm volatile ("movq %0, %%rsp" :: "r"(rsp));
    asm volatile ("popq %r15;"
                  "popq %r14;"
                  "popq %r13;"
                  "popq %r12;"
                  "popq %r11;"
                  "popq %r10;"
                  "popq %r9;"
                  "popq %r8;"
                  "popq %rdi;"
                  "popq %rsi;"
                  "popq %rdx;"
                  "popq %rcx;"
                  "popq %rbx;"
                  "popq %rax;");

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
    u64 *curr_rbp, *syscall_handler_rbp;
    asm volatile ("movq %%rbp, %0" : "=r"(curr_rbp));
    syscall_handler_rbp = (u64 *)(*curr_rbp);
    save_current_context(syscall_handler_rbp);
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
        if (current->sighandlers[signo] == 0)
            return 0;
        u64 prev_urip = *urip;
        *urip = (u64)current->sighandlers[signo];
        *ustackp -= 8;
        *(u64 *)(*ustackp) = prev_urip;
        /* printf("DDDDDD: %x\n", prev_urip); */
    } else
    {
        if (current->sighandlers[signo] != 0) {
            u64 prev_urip = *urip;
            *urip = (u64)current->sighandlers[signo];
            *ustackp -= 8;
            *(u64 *)(*ustackp) = prev_urip;
            /* *urip = (u64)(current->sighandlers[signo]); */
        }
        else
            do_exit();
    }
}

/*system call handler for signal, to register a handler*/
long do_signal(int signo, unsigned long handler)
{
    struct exec_context *current = get_current_ctx();
    unsigned long* handlers = (unsigned long*)current->sighandlers;
    printf("\nCalled do_signal with %d signal number\n", signo);
    // current->pending_signal_bitmap |= 1 << signo;
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
