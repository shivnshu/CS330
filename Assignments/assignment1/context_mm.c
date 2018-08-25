#include<context.h>
#include<memory.h>
#include<lib.h>

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

void prepare_context_mm(struct exec_context *ctx)
{
    u32 frame_number;
    u64 *l4_virtual_addr;
    u64 *stack_data_page_entry_ptr, *code_data_page_entry_ptr, *data_data_page_entry_ptr;
    struct mm_segment *stack_segment, *code_segment, *data_segment;

    stack_segment = &ctx->mms[MM_SEG_STACK];
    code_segment = &ctx->mms[MM_SEG_CODE];
    data_segment = &ctx->mms[MM_SEG_DATA];

    frame_number = os_pfn_alloc(OS_PT_REG);
    ctx->pgd = frame_number;
    l4_virtual_addr = (u64 *)osmap(frame_number);

    initialize_page(l4_virtual_addr);

    stack_data_page_entry_ptr = assign_page_table_pages(l4_virtual_addr, (stack_segment->end) - 0x1000, stack_segment->access_flags);
    assign_data_page(stack_data_page_entry_ptr, stack_segment->access_flags, 0);

    code_data_page_entry_ptr = assign_page_table_pages(l4_virtual_addr, code_segment->start, code_segment->access_flags);
    assign_data_page(code_data_page_entry_ptr, code_segment->access_flags, 0);

    data_data_page_entry_ptr = assign_page_table_pages(l4_virtual_addr, data_segment->start, data_segment->access_flags);
    assign_data_page(data_data_page_entry_ptr, data_segment->access_flags, ctx->arg_pfn);

    return;
}

int exist_in_array(u64 *pfn_array, int index, u64 pfn)
{
    int i;
    for (i = 0; i<index; ++i)
        if (pfn_array[i] == pfn)
            return 1;
    return 0;
}

u64 add_pages_to_array(u64 *l4_physical_addr, u64 start_addr, u64 *pfn_array, int *index)
{
    u64 *next_entry_ptr, *l3_physical_addr, *l2_physical_addr, *l1_physical_addr;
    u64 l3_pfn, l2_pfn, l1_pfn, data_pfn;

    next_entry_ptr = &l4_physical_addr[(start_addr & 0xff8000000000) >> 39];
    l3_pfn = (*next_entry_ptr) >> 12;
    l3_physical_addr = (u64 *)(l3_pfn << 12);

    next_entry_ptr = &l3_physical_addr[(start_addr & 0x007fc0000000) >> 30];
    l2_pfn = (*next_entry_ptr) >> 12;
    l2_physical_addr = (u64 *)(l2_pfn << 12);

    next_entry_ptr = &l2_physical_addr[(start_addr & 0x00003fe00000) >> 21];
    l1_pfn = (*next_entry_ptr) >> 12;
    l1_physical_addr = (u64 *)(l1_pfn << 12);

    next_entry_ptr = &l1_physical_addr[(start_addr & 0x0000001ff000) >> 12];
    data_pfn = (*next_entry_ptr) >> 12;

    if (!exist_in_array(pfn_array, *index, l3_pfn)) {
        pfn_array[*index] = l3_pfn;
        *index += 1;
    }
    if (!exist_in_array(pfn_array, *index, l2_pfn)) {
        pfn_array[*index] = l2_pfn;
        *index += 1;
    }
    if (!exist_in_array(pfn_array, *index, l1_pfn)) {
        pfn_array[*index] = l1_pfn;
        *index += 1;
    }
    return data_pfn;
}

void cleanup_context_mm(struct exec_context *ctx)
{
    u64 pfn_array[16]; // Max 13 possible
    int i, index = 0;
    u64 frame_number;
    u64 stack_data_pfn, code_data_pfn, data_data_pfn;
    u64 *l4_physical_addr;
    u64 *stack_ptr, *code_ptr, *data_ptr;
    u64 *stack_data_ptr, *code_data_ptr, *data_data_ptr;
    struct mm_segment *stack_segment, *code_segment, *data_segment;

    stack_segment = &ctx->mms[MM_SEG_STACK];
    code_segment = &ctx->mms[MM_SEG_CODE];
    data_segment = &ctx->mms[MM_SEG_DATA];

    frame_number = (u64)ctx->pgd;
    l4_physical_addr = (u64 *)(frame_number << 12);

    pfn_array[index++] = frame_number;

    stack_data_pfn = add_pages_to_array(l4_physical_addr, (stack_segment->end) - 0x1000, pfn_array, &index);
    code_data_pfn = add_pages_to_array(l4_physical_addr, code_segment->start, pfn_array, &index);
    data_data_pfn = add_pages_to_array(l4_physical_addr, data_segment->start, pfn_array, &index);

    for (i = 0;i < index; ++i) {
        os_pfn_free(OS_PT_REG, pfn_array[i]);
    }

    os_pfn_free(USER_REG, stack_data_pfn);
    os_pfn_free(USER_REG, code_data_pfn);
    os_pfn_free(USER_REG, data_data_pfn);

    return;
}
