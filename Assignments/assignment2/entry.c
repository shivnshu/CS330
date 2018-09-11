#include<init.h> 
#include<lib.h>
#include<context.h>
#include<memory.h>

u64 *extract_addr_from_page_entry(u64 *entry_ptr)
{
    u64 physical_addr;
    physical_addr = (*entry_ptr) >> 12;
    return (u64 *)osmap(physical_addr);
}

int is_valid_virtual_addr(long long addr, u64 pgd)
{
    u64 *l4_virtual_addr, *l3_virtual_addr, *l2_virtual_addr, *l1_virtual_addr;
    u64 *next_entry_ptr;

    l4_virtual_addr = (u64 *)osmap(pgd);
    next_entry_ptr = &l4_virtual_addr[(addr & 0xff8000000000) >> 39];
    if ((*next_entry_ptr) & 0x1 == 0)
        return 0;

    l3_virtual_addr = extract_addr_from_page_entry(next_entry_ptr);
    next_entry_ptr = &l3_virtual_addr[(addr & 0x007fc0000000) >> 30];
    if ((*next_entry_ptr) & 0x1 == 0)
        return 0;

    l2_virtual_addr = extract_addr_from_page_entry(next_entry_ptr);
    next_entry_ptr = &l2_virtual_addr[(addr & 0x00003fe00000) >> 21];
    if ((*next_entry_ptr) & 0x1 == 0)
        return 0;

    l1_virtual_addr = extract_addr_from_page_entry(next_entry_ptr);
    next_entry_ptr = &l1_virtual_addr[(addr & 0x0000001ff000) >> 12];
    if ((*next_entry_ptr) & 0x1 == 0)
        return 0;
    return 1;
}

void free_data_page(long long addr, u64 pgd)
{
    u64 *l4_virtual_addr, *l3_virtual_addr, *l2_virtual_addr, *l1_virtual_addr;
    u64 *next_entry_ptr;
    u64 pfn;

    l4_virtual_addr = (u64 *)osmap(pgd);
    next_entry_ptr = &l4_virtual_addr[(addr & 0xff8000000000) >> 39];

    l3_virtual_addr = extract_addr_from_page_entry(next_entry_ptr);
    next_entry_ptr = &l3_virtual_addr[(addr & 0x007fc0000000) >> 30];

    l2_virtual_addr = extract_addr_from_page_entry(next_entry_ptr);
    next_entry_ptr = &l2_virtual_addr[(addr & 0x00003fe00000) >> 21];

    l1_virtual_addr = extract_addr_from_page_entry(next_entry_ptr);
    next_entry_ptr = &l1_virtual_addr[(addr & 0x0000001ff000) >> 12];

    pfn = (u64)extract_addr_from_page_entry(next_entry_ptr);
    os_pfn_free(USER_REG, pfn);
    *next_entry_ptr = 0;
}

long do_syscall(int syscall, u64 param1, u64 param2, u64 param3, u64 param4)
{
    struct exec_context *current = get_current_ctx();
    printf("[GemOS] System call invoked. syscall no  = %d\n", syscall);
    switch(syscall)
    {
        case SYSCALL_EXIT:
            printf("[GemOS] exit code = %d\n", (int) param1);
            do_exit();
            break;

        case SYSCALL_GETPID:
            printf("[GemOS] getpid called for process %s, with pid = %d\n", current->name, current->id);
            return current->id;      

        case SYSCALL_WRITE:
            {  
                char *buf = (char *)param1;
                long length = (int)param2;
                if (!is_valid_virtual_addr((long long)buf, current->pgd) || !is_valid_virtual_addr((long long)(buf+length-1), current->pgd))
                    return -1;
                int i;
                for (i=0; i<length; ++i) {
                    printf("%c", buf[i]);
                }
                return length;
            }

        case SYSCALL_EXPAND:
            {
                u32 size = param1;
                int flags = param2;
                struct mm_segment *memory_segment = NULL;
                if (flags == MAP_RD)
                    memory_segment = &current->mms[MM_SEG_RODATA];
                if (flags == MAP_WR)
                    memory_segment = &current->mms[MM_SEG_DATA];
                long long res = memory_segment->next_free;
                memory_segment->next_free += (size << 12);
                if (memory_segment->next_free > memory_segment->end) {
                    memory_segment->next_free -= (size << 12);
                    return 0;
                }
                return res;
            }

        case SYSCALL_SHRINK:
            {  
                u32 size = param1;
                int flags = param2;
                struct mm_segment *memory_segment = NULL;
                if (flags == MAP_RD)
                    memory_segment = &current->mms[MM_SEG_RODATA];
                if (flags == MAP_WR)
                    memory_segment = &current->mms[MM_SEG_DATA];

                if ((memory_segment->next_free - (size << 12)) < memory_segment->start)
                    return 0;

                while (size > 0) {
                    free_data_page(memory_segment->next_free - (1 << 12), current->pgd);
                    memory_segment->next_free -= (1 << 12);
                    size--;
                }
                return (memory_segment->next_free);
            }
                             
        default:
            return -1;
    }
    return 0;
}

extern int handle_div_by_zero(void)
{
    u64 *rbp;
    u64 rip = rbp[1];
    printf("Div-by-zero detected at [%x]\n", rip);
    return 0;
}

void initialize_page(u64 *page_virtual_addr)
{
    int i;
    for (i = 0;i < 512; ++i) {
        page_virtual_addr[i] = 0; 
    }
}

u64 *assign_page_table_page(u64 *next_entry_ptr, u32 flags)
{
    u64 entry, frame_number;
    u64 *page_virtual_addr;
    if ((*next_entry_ptr) & 0x1) {    // Page already present
        frame_number = (*next_entry_ptr) >> 12;
        return (u64 *)osmap(frame_number);
    }
    frame_number = (u64)os_pfn_alloc(OS_PT_REG);
    entry = frame_number;
    entry = entry << 9;
    entry = (entry << 1) + 1;
    entry = (entry << 1) + ((flags & MM_WR) >> 1);
    entry = (entry << 1) + 1;
    *next_entry_ptr = entry;
    page_virtual_addr = (u64 *)osmap(frame_number);

    initialize_page(page_virtual_addr);

    return page_virtual_addr;
}

void assign_data_page(u64 *next_entry_ptr, u32 flags, u32 pfn)
{
    u64 entry, frame_number;
    if (*(next_entry_ptr) & 0x1) {
        return;
    }
    if (pfn == 0)
        frame_number = (u64)os_pfn_alloc(USER_REG);
    else
        frame_number = (u64)pfn;
    entry = frame_number;
    entry = entry << 9;
    entry = (entry << 1) + 1;
    entry = (entry << 1) + ((flags & MM_WR) >> 1);
    entry = (entry << 1) + 1;
    *next_entry_ptr = entry;
}

u64 *assign_page_table_pages(u64 *l4_virtual_addr, u64 start_addr, u32 flags)
{
    u64 *next_entry_ptr, *l3_virtual_addr, *l2_virtual_addr, *l1_virtual_addr;

    next_entry_ptr = &l4_virtual_addr[(start_addr & 0xff8000000000) >> 39];
    l3_virtual_addr = assign_page_table_page(next_entry_ptr, flags);

    next_entry_ptr = &l3_virtual_addr[(start_addr & 0x007fc0000000) >> 30];
    l2_virtual_addr = assign_page_table_page(next_entry_ptr, flags);

    next_entry_ptr = &l2_virtual_addr[(start_addr & 0x00003fe00000) >> 21];
    l1_virtual_addr = assign_page_table_page(next_entry_ptr, flags);

    next_entry_ptr = &l1_virtual_addr[(start_addr & 0x0000001ff000) >> 12];
    return next_entry_ptr;
}

void handle_mm_seg_data_page_fault (struct mm_segment *segment, u64 addr, u64 pgd)
{
    u64 *l4_virtual_addr = (u64 *)osmap(pgd);
    u64 *seg_data_page_entry_ptr = assign_page_table_pages(l4_virtual_addr, addr, segment->access_flags);
    assign_data_page(seg_data_page_entry_ptr, segment->access_flags, 0);
}

void handle_mm_seg_rodata_page_fault (struct mm_segment *segment, u64 addr, u64 pgd)
{
    u64 *l4_virtual_addr = (u64 *)osmap(pgd);
    u64 *seg_data_page_entry_ptr = assign_page_table_pages(l4_virtual_addr, addr, segment->access_flags);
    assign_data_page(seg_data_page_entry_ptr, segment->access_flags, 0);
}

void handle_mm_seg_stack_page_fault (struct mm_segment *segment, u64 addr, u64 pgd)
{
    u64 *l4_virtual_addr = (u64 *)osmap(pgd);
    u64 *seg_data_page_entry_ptr = assign_page_table_pages(l4_virtual_addr, addr, segment->access_flags);
    assign_data_page(seg_data_page_entry_ptr, segment->access_flags, 0);
  
}

extern int handle_page_fault(void)
{
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
                  "pushq %r15");

    u64 *rbp, cr2, rip, error_code, rsp;
    asm volatile ("movq %%rbp, %0" : "=r"(rbp));
    asm volatile ("movq %%cr2, %0" : "=r"(cr2));
    asm volatile ("movq %%rsp, %0" : "=r"(rsp));

    struct exec_context *current = get_current_ctx();
    rip = rbp[2];
    error_code = rbp[1];

    struct mm_segment *data_mm = &current->mms[MM_SEG_DATA];
    struct mm_segment *ROdata_mm = &current->mms[MM_SEG_RODATA];
    struct mm_segment *stack_mm = &current->mms[MM_SEG_STACK];

    /*printf("RIP: %x, Error: %d, CR2: %x\n", rip, error_code, cr2);*/
    /*printf("[DATA] start: %x, next_free: %x, end: %x\n", data_mm->start, data_mm->next_free, data_mm->end);*/
    /*printf("[RODATA] start: %x, next_free: %x, end: %x\n", ROdata_mm->start, ROdata_mm->next_free, ROdata_mm->end);*/
    /*printf("[STACK] start: %x, next_free: %x, end: %x\n", stack_mm->start, stack_mm->next_free, stack_mm->end);*/


    if (cr2 >= data_mm->start && cr2 < data_mm->next_free)
        handle_mm_seg_data_page_fault(data_mm, cr2, current->pgd);
    else if (cr2 >= ROdata_mm->start && cr2 < ROdata_mm->next_free)
        handle_mm_seg_rodata_page_fault(ROdata_mm, cr2, current->pgd);
    else if (cr2 >= stack_mm->start && cr2 < stack_mm->end)
        handle_mm_seg_stack_page_fault(stack_mm, cr2, current->pgd);
    else {
        printf("Page Fault at RIP: %x, VA: %x, Error Code: %d\n", rip, cr2, error_code);
        do_exit();
    }

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

    asm volatile ("movq %rbp, %rsp;"
                  "popq %rbp;"
                  "addq $8, %rsp;"
                  "iretq;");
    return 0;
}
