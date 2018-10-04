#include<init.h>
#include<lib.h>
#include<context.h>
#include<memory.h> //For page table access

// You are required to access the stack pointer by accessing RSP register. Require C in-line assembly.

/* Function to check if a VA is valid or not */
int checkPresenceVA(u64 VAddr, u64 pgd){
  u64 value;
  u64* Level;
  Level = osmap(pgd); // To know the Virtual address of location pointed by CR3
  value = *(Level + 8*(VAddr & 0xFF8000000000));
  if ((value & 0x1)== 0x1){
    Level = osmap(value>>12);  // Extracting VA, since PA is stored in PTE
    value = *(Level + 8*(VAddr & 0x7FC0000000));
    if ((value & 0x1)== 0x1){
      Level = osmap(value>>12);
      value = *(Level + 8*(VAddr & 0x3FE00000));
      if ((value & 0x1)== 0x1){
        Level = osmap(value>>12);
        value = *(Level + 8*(VAddr & 0x1FF000));
        if ((value & 0x1)== 0x1){
          return 1;
        }
      }
    }
  }
  return 0;
}

/* Function to remove PP mapped to a VP */
void rmPage(u64 VAddr, u64 pgd){
  u64 value;
  u64 *Level;
  Level = osmap(pgd); // To know the Virtual address of location pointed to by CR3
  value = *(Level + 8*(VAddr & 0xFF8000000000));
  if (((value)&0x1) != 1){ // Checking if the VA was mapped already or not
    return ;
  }
  Level = osmap(value>>12);  // Extracting VA, since PA is stored in PTE
  value = *(Level + 8*(VAddr & 0x7FC0000000));
  if (((value)&0x1) != 1){ // Checking if the VA was mapped already or not
    return ;
  }
  Level = osmap(value>>12);
  value = *(Level + 8*(VAddr & 0x3FE00000));
  if (((value)&0x1) != 1){ // Checking if the VA was mapped already or not
    return ;
  }
  Level = osmap(value>>12);
  value = *(Level + 8*(VAddr & 0x1FF000));
  if (((value)&0x1) != 1){ // Checking if the VA was mapped already or not
    return ;
  }
  os_pfn_free(USER_REG, value>>12);
  *(Level) = (*(Level))^0x1;  // Setting present bit=0 
  return;
}

/*System Call handler*/
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
                                     /*Your code goes here*/
                                
                                // Since the buffer is a contiguous array, just checking the presence of first and last buf element 
                                // would suffice and will reduce overhead of checking each bytes
                                if (checkPresenceVA(param1, current->pgd) && checkPresenceVA(param1+param2-1, current->pgd)){
                                  char *buf = (char *)param1; 
                                  for (int i=0; i<param2; i+=1){
                                    printf("%c",buf[i]);
                                  }
                                  return param2;
                                }
                                else
                                  return -1;
                                // invalid Length check!! What it mean??
                             }

          case SYSCALL_EXPAND:
                             {  
                                     /*Your code goes here*/
                                u64 retNextFree;
                                u64 size = param1;
                                if (param2 == MAP_RD){
                                  retNextFree = current->mms[MM_SEG_RODATA].next_free;
                                  if ((retNextFree+(size<<12)) > current->mms[MM_SEG_RODATA].end){
                                    return 0;
                                  }
                                  // Since we are given Page size and Size of a page is 4K
                                  current->mms[MM_SEG_RODATA].next_free += (param1<<12);  
                                }
                                else if (param2 == MAP_WR){
                                  retNextFree = current->mms[MM_SEG_DATA].next_free;
                                  if ((retNextFree+(size<<12)) > current->mms[MM_SEG_DATA].end){
                                    return 0;
                                  }
                                  current->mms[MM_SEG_DATA].next_free += (param1<<12);
                                }
                                else{
                                  return -1;
                                }
                                return retNextFree;
                             }
          case SYSCALL_SHRINK:
                             {  
                                     /*Your code goes here*/  // Replace!! Size with param1
                                u64 retNextFree;
                                u64 size = param1;
                                int i;
                                if (param2 == MAP_RD){
                                  retNextFree = current->mms[MM_SEG_RODATA].next_free;
                                  if ((retNextFree - (size<<12)) < current->mms[MM_SEG_RODATA].start){
                                    return 0;
                                  }
                                  // Since we are given Page size and Size of a page is 4K
                                  for (i=1; i<=size; i+=1){
                                    current->mms[MM_SEG_RODATA].next_free -= 4096;
                                    rmPage(current->mms[MM_SEG_RODATA].next_free, current->pgd);
                                  }
                                  return current->mms[MM_SEG_RODATA].next_free;
                                }
                                else if (param2 == MAP_WR){
                                  retNextFree = current->mms[MM_SEG_DATA].next_free;
                                  if ((retNextFree - (size<<12)) > current->mms[MM_SEG_DATA].start){
                                    return 0;
                                  }
                                  // Since we are given Page size and Size of a page is 4K
                                  for (i=1; i<=size; i+=1){
                                    current->mms[MM_SEG_DATA].next_free -= 4096;
                                    rmPage(current->mms[MM_SEG_DATA].next_free, current->pgd);
                                  }
                                  return current->mms[MM_SEG_DATA].next_free;
                                }
                                else{
                                  return 0;
                                }
                             }
                             
          default:
                              return -1;
                                
    }
    return 0;   /*GCC shut up!*/
}

extern int handle_div_by_zero(void)
{
    /*Your code goes in here*/
  //Extract RIP from somewhere (what is the return register for this handler? That address - instruction size of x86)

    u64 retBasePointer, rip;
    asm volatile("mov %%rbp, %0;": "=r" (retBasePointer):);
    rip = *((u64 *)retBasePointer + 1);
    printf("Div-by-zero detected at [%x]\n", rip);
    do_exit();
    return 0;
}

extern int handle_page_fault(void)
{
    /*Your code goes in here*/
    printf("page fault handler: unimplemented!\n");
    return 0;
}
