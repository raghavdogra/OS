#include <sys/defs.h>
#include <sys/gdt.h>
#include <sys/kprintf.h>
#include <sys/tarfs.h>
#include <sys/ahci.h>
#include <sys/idt.h>
#include <sys/pic.h>
#include <sys/kernel_threads.h>
#include <sys/kmalloc.h>
#include <sys/elf64.h>
#include <sys/scheduler.h>
#include <sys/initfs.h>
/*
deleted pci.c and pci.h due to reduction in available memory
#include <sys/pci.h>
*/
#include <sys/paging.h>
#include <sys/kmemcpy.h>

uint64_t last_assn_pid;
Task *CURRENT_TASK;
Task *FG_TASK;
Task *SLEEPING_TASK;
uint64_t TERMINAL_BUFFER;
uint64_t TERM_BUF_OFFSET;
uint64_t *PML4_debug;

#define INITIAL_STACK_SIZE 4096
uint8_t initial_stack[INITIAL_STACK_SIZE]__attribute__((aligned(16)));
uint32_t* loader_stack;
extern char kernmem, physbase;
//pg_desc_t *free_list_head;

//Task *runningTask;
void first_kern_thd() {
  //loadElf("bin/sbush"); 
  //switch_user_mode(CURRENT_TASK->mm->e_entry);
  //yield();
}


void start(uint32_t *modulep, void *physbase, void *physfree)
{
  struct smap_t {
    uint64_t base, length;
    uint32_t type;
  }__attribute__((packed)) *smap;

//  int num_pages = 0;
  smap_copy_t smap_copy[10];
  int smap_copy_index = 0;

  while(modulep[0] != 0x9001) modulep += modulep[1]+2;
  for(smap = (struct smap_t*)(modulep+2); smap < (struct smap_t*)((char*)modulep+modulep[1]+2*4); ++smap) {
    if (smap->type == 1 /* memory */ && smap->length != 0) {
      smap_copy[smap_copy_index].starting_addr = smap->base;
      smap_copy[smap_copy_index].last_addr = smap->base + smap->length;
      //kprintf("Available Physical Memory [%p-%p]\n",  smap_copy[smap_copy_index].starting_addr, smap_copy[smap_copy_index].last_addr );
      smap_copy_index++;
    }
  }

  //kprintf("value of PML %x &PML %x and PML[511] %x\n",PML4,&PML4,PML4[511]);
  uint64_t free_list_end = setup_memory(physbase, physfree, smap_copy, smap_copy_index);
  /*remap the 640K-1M region with direct one to one mapping from virtual to physical*/
  uint64_t virt_addr = (uint64_t) 0xffffffff80000000 + (uint64_t)physbase;
  uint64_t PDEframe = (virt_addr >> 21) & (uint64_t) 0x1ff;  
  init_self_referencing(free_list_end, PDEframe);
  
  map_memory_range(0x00000, (uint64_t)physbase, 0); 
  map_memory_range((uint64_t)physbase, free_list_end + (512*511*4096), PDEframe);

  for(int i = 0; i < 512; i++) {
    PDE[i] = PDE[i] | 7;
  }

  uint64_t cr3val = free_list_end;
  __asm __volatile("movq %0, %%cr3\n\t"
                    :
                    :"a"(cr3val));
  uint64_t t1 = (uint64_t)PML4_kern;
  uint64_t temp = (uint64_t)t1 + (uint64_t)0xffffffff80000000;
  PML4_debug = (uint64_t *) temp;
  t1 = (uint64_t)free_list;
  temp = (uint64_t)t1 + (uint64_t)0xffffffff80000000;
  free_list = (pg_desc_t *) temp;
  //-------------k malloc init----------------------
  init_kmalloc(); 
  SLEEPING_TASK = NULL;
  FG_TASK = NULL;
  TERMINAL_BUFFER = get_free_page(SUPERVISOR_ONLY, cr3val);
  TERM_BUF_OFFSET = 0;
  // ------------------------------------------------
  // switch to user mode
  init_idt();
  program_pic();
  set_tss_rsp((void *)((uint64_t)tss_kstack));
  // ------------------------------------------------
//  run_queue = NULL;
  last_assn_pid = 0;
 
  initfs();
  /*dentry *de = dentry_lookup("rootfs/bin/init");
  if(de != NULL)
  kprintf("\nFound: %s", de->d_name);
  de = dentry_lookup("rootfs/bin/sbush");
  if(de != NULL)
  kprintf("\nFound: %s", de->d_name);
  else kprintf("Not Found");
  de = dentry_lookup("rootfs/bin/../bin");
  if(de != NULL)
  kprintf("\nFound: %s", de->d_name);*/
  Task *mainTask = (Task*) kmalloc(sizeof(Task));
  Task *schedulerTask = (Task *)kmalloc(sizeof(Task));

//populate the main task cr3 and rflags
        __asm__ __volatile__("movq %%cr3, %0\n\t"
                             :"=a"(mainTask->regs.cr3));
        __asm__ __volatile__("PUSHFQ \n\t"
                             "movq (%%rsp), %%rax\n\t"
                             "movq %%rax, %0\n\t"
                             "POPFQ\n\t"
                             :"=m"(mainTask->regs.rflags)::"%rax");
//set up the scheduler
  setupTask(schedulerTask,scheduler,mainTask);

//one kinda hacky way for accessing schedulerTask once you switch
  CURRENT_TASK = schedulerTask;

//switch to scheduler initialization and never come back here
  set_tss_rsp((void *)((uint64_t)schedulerTask->kstack));
  switchTask(&mainTask->regs, &schedulerTask->regs);
}

void boot(void)
{
  // note: function changes rsp, local stack variables can't be practically used
  register char /**temp1,*/ *temp2;

  for(temp2 = (char*)0xb8001; temp2 < (char*)0xb8000+160*25; temp2 += 2) *temp2 = 7 /* white */;
  __asm__ volatile (
    "cli;"
    "movq %%rsp, %0;"
    "movq %1, %%rsp;"
    :"=g"(loader_stack)
    :"r"(&initial_stack[INITIAL_STACK_SIZE])
  );
  init_gdt();
  start(
    (uint32_t*)((char*)(uint64_t)loader_stack[3] + (uint64_t)&kernmem - (uint64_t)&physbase),
    (uint64_t*)&physbase,
    (uint64_t*)(uint64_t)loader_stack[4]
  );
 /* for(
    temp1 = "!!!!! start() returned !!!!!", temp2 = (char*)0xb8000;
    *temp1;
    temp1 += 1, temp2 += 2
  ) *temp2 = *temp1;*/
  while(1) __asm__ volatile ("hlt");
}
