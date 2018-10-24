#include<context.h>
#include<memory.h>
#include<schedule.h>
#include<lib.h>
#include<idt.h>
#include<apic.h>

static u64 numticks;

static void save_current_context()
{
    struct exec_context *current = get_current_ctx();
    if (current->pid == 0 || current->state == UNUSED)
        return;

    u64 *addr = (u64 *)osmap(current->os_stack_pfn);
    printf("DEBUG: In save context PID: %d\n", current->pid);
    current->regs.entry_rip = addr[507];
    current->regs.entry_cs = addr[508];
    current->regs.entry_rflags = addr[509];
    current->regs.entry_rsp = addr[510];
    current->regs.entry_ss = addr[511];

    current->regs.r15 = addr[493];
    current->regs.r14 = addr[494];
    current->regs.r13 = addr[495];
    current->regs.r12 = addr[496];
    current->regs.r11 = addr[497];
    current->regs.r10 = addr[498];
    current->regs.r9 = addr[499];
    current->regs.r8 = addr[500];
    current->regs.rbp = addr[501];
    current->regs.rdi = addr[502];
    current->regs.rsi = addr[503];
    current->regs.rdx = addr[504];
    current->regs.rcx = addr[505];
    current->regs.rbx = addr[506];
}

static void schedule_context(struct exec_context *next)
{
    /*Your code goes in here. get_current_ctx() still returns the old context*/
    struct exec_context *current = get_current_ctx();
    printf("scheduling: old pid = %d  new pid  = %d\n", current->pid, next->pid); /*XXX: Don't remove*/

    save_current_context();

    next->state = RUNNING;

    if (next->pid == 0) {
        set_tss_stack_ptr(next);
        set_current_ctx(next);
        /* ack_irq(); */
        asm volatile ("pushq %0" :: "r"(next->regs.entry_ss));
        asm volatile ("pushq %0" :: "r"(next->regs.entry_rsp));
        asm volatile ("pushq %0" :: "r"(next->regs.entry_rflags));
        asm volatile ("pushq %0" :: "r"(next->regs.entry_cs));
        asm volatile ("pushq %0" :: "r"(next->regs.entry_rip));
        asm volatile ("iretq");
    }

    u64 *rbp;
    asm volatile ("movq %%rbp, %0" : "=r"(rbp));
    *(rbp)   = next->regs.rbp;
    *(rbp+1) = next->regs.entry_rip;
    *(rbp+2) = next->regs.entry_cs;
    *(rbp+3) = next->regs.entry_rflags;
    *(rbp+4) = next->regs.entry_rsp;
    *(rbp+5) = next->regs.entry_ss;

    set_tss_stack_ptr(next);
    set_current_ctx(next);

    /* ack_irq(); */

    asm volatile("movq %0, %%r15" :: "r"(next->regs.r15):"memory");
    asm volatile("movq %0, %%r14" :: "r"(next->regs.r14):"memory");
    asm volatile("movq %0, %%r13" :: "r"(next->regs.r13):"memory");
    asm volatile("movq %0, %%r12" :: "r"(next->regs.r12):"memory");
    asm volatile("movq %0, %%r11" :: "r"(next->regs.r11):"memory");
    asm volatile("movq %0, %%r10" :: "r"(next->regs.r10):"memory");
    asm volatile("movq %0, %%r9" :: "r"(next->regs.r9):"memory");
    asm volatile("movq %0, %%r8" :: "r"(next->regs.r8):"memory");
    asm volatile("movq %0, %%rsi" :: "r"(next->regs.rsi):"memory");
    asm volatile("movq %0, %%rdi" :: "r"(next->regs.rdi):"memory");
    asm volatile("movq %0, %%rdx" :: "r"(next->regs.rdx):"memory");
    asm volatile("movq %0, %%rcx" :: "r"(next->regs.rcx):"memory");
    asm volatile("movq %0, %%rbx" :: "r"(next->regs.rbx):"memory");
    asm volatile("movq %0, %%rax" :: "r"(next->regs.rax):"memory");
    asm volatile("movq %rbp, %rsp;");
    asm volatile ("popq %rbp;");
    asm volatile ("iretq;");

}

static struct exec_context *pick_next_context(struct exec_context *list)
{
    printf("In pick_next_context\n");
    struct exec_context *current = get_current_ctx();
    int i, pid;
    pid = current->pid;
    for (i=1;i<MAX_PROCESSES;++i) {
        if (list[i+pid].state == READY)
            break;
    }
    if (i < MAX_PROCESSES)
        return get_ctx_by_pid(i+pid);
    return NULL;
}

static void schedule()
{
    printf("DEBUG: In schedule\n");
    struct exec_context *next;
    struct exec_context *current = get_current_ctx();
    struct exec_context *list = get_ctx_list();
    next = pick_next_context(list);
    printf("DEBUG: after pick_next_context In schedule\n");
    if (next != NULL) {
        printf("Picked process pid: %d\n", next->pid);
        schedule_context(next);
    }
}

static void do_sleep_and_alarm_account()
{
    /*All processes in sleep() must decrement their sleep count*/
    struct exec_context *current = get_current_ctx();
    struct exec_context *ctx;
    int i;
    for (i=0;i<MAX_PROCESSES;++i) {
        ctx = get_ctx_by_pid(i);
        if (ctx == current)
            continue;
        if (ctx->ticks_to_sleep > 0)
            ctx->ticks_to_sleep--;
        if (ctx->ticks_to_alarm > 0)
            ctx->ticks_to_alarm;
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

    do_sleep_and_alarm_account();

    struct exec_context *current = get_current_ctx();
    printf("Current PID: %d\n", current->pid);
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
        }

    struct exec_context *next;
    int i;
    for (i=0;i<MAX_PROCESSES;++i) {
        next = get_ctx_by_pid(i);
        if (next->state == WAITING && next->ticks_to_sleep == 0)
            break;
    }

    if (i < MAX_PROCESSES) {
        printf("scheduling: old pid = %d  new pid  = %d\n", current->pid, next->pid); /*XXX: Don't remove*/

        save_current_context();

        current->state = READY;
        next->state = RUNNING;

        if (next->pid == 0) {
            set_tss_stack_ptr(next);
            set_current_ctx(next);
            ack_irq();
            asm volatile ("pushq %0" :: "r"(next->regs.entry_ss));
            asm volatile ("pushq %0" :: "r"(next->regs.entry_rsp));
            asm volatile ("pushq %0" :: "r"(next->regs.entry_rflags));
            asm volatile ("pushq %0" :: "r"(next->regs.entry_cs));
            asm volatile ("pushq %0" :: "r"(next->regs.entry_rip));
            asm volatile ("iretq");
        }

        u64 *rbp;
        asm volatile ("movq %%rbp, %0" : "=r"(rbp));
        *(rbp)   = next->regs.rbp;
        *(rbp+1) = next->regs.entry_rip;
        *(rbp+2) = next->regs.entry_cs;
        *(rbp+3) = next->regs.entry_rflags;
        *(rbp+4) = next->regs.entry_rsp;
        *(rbp+5) = next->regs.entry_ss;

        set_tss_stack_ptr(next);
        set_current_ctx(next);

        ack_irq();

        asm volatile("movq %0, %%r15" :: "r"(next->regs.r15):"memory");
        asm volatile("movq %0, %%r14" :: "r"(next->regs.r14):"memory");
        asm volatile("movq %0, %%r13" :: "r"(next->regs.r13):"memory");
        asm volatile("movq %0, %%r12" :: "r"(next->regs.r12):"memory");
        asm volatile("movq %0, %%r11" :: "r"(next->regs.r11):"memory");
        asm volatile("movq %0, %%r10" :: "r"(next->regs.r10):"memory");
        asm volatile("movq %0, %%r9" :: "r"(next->regs.r9):"memory");
        asm volatile("movq %0, %%r8" :: "r"(next->regs.r8):"memory");
        asm volatile("movq %0, %%rsi" :: "r"(next->regs.rsi):"memory");
        asm volatile("movq %0, %%rdi" :: "r"(next->regs.rdi):"memory");
        asm volatile("movq %0, %%rdx" :: "r"(next->regs.rdx):"memory");
        asm volatile("movq %0, %%rcx" :: "r"(next->regs.rcx):"memory");
        asm volatile("movq %0, %%rbx" :: "r"(next->regs.rbx):"memory");
        asm volatile("movq %0, %%rax" :: "r"(next->regs.rax):"memory");
        asm volatile("movq %rbp, %rsp;");
        asm volatile("popq %rbp;");
        asm volatile ("iretq;");
    }

    printf("DEBUG: Exiting time interrupt handler normally\n");
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
    struct exec_context *current = get_current_ctx();
    printf("DEBUG: In do_exit(). Exiting PID: %d\n", current->pid);
    current->state = UNUSED;
    os_pfn_free(OS_PT_REG, current->os_stack_pfn);
    int flag = 0, i;
    struct exec_context *list = get_ctx_list();
    for (i=1;i<MAX_PROCESSES;++i) {
        if (list[i].pid > 0 && list[i].state != UNUSED) {
            flag = 1;
            /* printf("AAAAA: %d\n", list[i].pid); */
            break;
        }
    }
    if (flag) {
        printf("DEBUG: In do_exit() decided to schedule\n");
        schedule();
    }
    else
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
    printf("DEBUG: In clone\n");
    printf("DEBUG: th_func: %x, user_stack: %x\n", th_func, user_stack);
    struct exec_context *current = get_current_ctx();
    struct exec_context *new_ctx = get_new_ctx();
    new_ctx->os_stack_pfn = os_pfn_alloc(OS_PT_REG);
    u32 pid = new_ctx->pid;
    memcpy(new_ctx->name, current->name, CNAME_MAX);
    int index=0;
    while (new_ctx->name[index] != '\0')
        index++;
    if (pid < 10) {
        new_ctx->name[index++] = (pid + '0');
        new_ctx->name[index] = '\0';
    } else {
        new_ctx->name[index++] = '1';
        new_ctx->name[index++] = pid%10 + '0';
        new_ctx->name[index] = '\0';
    }

    new_ctx->regs.entry_rip = (u64)th_func;
    new_ctx->regs.entry_cs = 0x23;
    new_ctx->regs.entry_rflags = current->regs.entry_rflags;
    new_ctx->regs.entry_rsp = (u64)user_stack;
    new_ctx->regs.entry_ss = 0x2b;

    new_ctx->regs.r15 = current->regs.r15;
    new_ctx->regs.r14 = current->regs.r14;
    new_ctx->regs.r13 = current->regs.r13;
    new_ctx->regs.r12 = current->regs.r12;
    new_ctx->regs.r11 = current->regs.r11;
    new_ctx->regs.r10 = current->regs.r10;
    new_ctx->regs.r9 = current->regs.r9;
    new_ctx->regs.r8 = current->regs.r8;
    new_ctx->regs.rbp = current->regs.rbp;
    new_ctx->regs.rdi = current->regs.rdi;
    new_ctx->regs.rsi = current->regs.rsi;
    new_ctx->regs.rdx = current->regs.rdx;
    new_ctx->regs.rcx = current->regs.rcx;
    new_ctx->regs.rbx = current->regs.rbx;
    new_ctx->regs.rax = current->regs.rax;

    new_ctx->type = current->type;
    new_ctx->used_mem = 0;
    new_ctx->pgd = current->pgd;
    new_ctx->os_rsp = current->os_rsp;

    new_ctx->mms[MM_SEG_CODE] = current->mms[MM_SEG_CODE];
    new_ctx->mms[MM_SEG_RODATA] = current->mms[MM_SEG_RODATA];
    new_ctx->mms[MM_SEG_DATA] = current->mms[MM_SEG_DATA];
    new_ctx->mms[MM_SEG_STACK] = current->mms[MM_SEG_STACK];

    new_ctx->pending_signal_bitmap = current->pending_signal_bitmap;

    new_ctx->ticks_to_sleep = 0;
    new_ctx->alarm_config_time = 0;
    new_ctx->ticks_to_alarm = 0;

    new_ctx->state = READY;
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
