#ifndef _KERN_THD
#define _KERN_THD
#include <sys/defs.h>
#include <sys/initfs.h>
// TODO: change this random value assigned for now
#define MAX_PROC 1000
#define FNAMELEN 100
#define MAX_FDS 10
#define USER_STACK 0xc0000000
#define USER_STACK_SIZE 0x10000000

extern uint64_t last_assn_pid;
typedef struct {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rsp, rbp, rip;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rflags, cr3;
    uint64_t cs,ds,es,fs,gs,ss;
} Registers;

struct mm_struct {
    //uint64_t code_begin, code_end, data_begin, data_end, stack_begin, brk_start;
    uint64_t stack_begin, brk_begin;
    struct vma *vm_begin;
    uint64_t pg_pml4;
    uint64_t e_entry; 	
};

struct vma {
    enum { NORMAL, HEAP, STACK } vm_type;
    struct mm_struct *vma_mm;
    uint64_t *vma_start;
    uint64_t *vma_end;
    uint64_t *vma_file_ptr;
    uint64_t vma_file_offset;
    uint64_t vma_mem_size;
    uint64_t vma_flags;
    uint64_t vma_size;
    struct vma *vma_prev;
    struct vma *vma_next;
};

struct FILE_OBJ {
    char file_name[FNAMELEN];
    uint64_t file_begin;
    uint64_t file_end;
    uint64_t file_offset; //file will read from this offset
    uint64_t file_ref_count;
};

struct DIRK {
  char d_name[100];
  int d_current;
  dentry* d_entry;
  int dir_ref_count;
};

typedef struct DIRK DIRK;

typedef struct TASK {
   int pid;
   int ppid;
   uint64_t *kstack;
   enum { READY, WAITING, ZOMBIE, SLEEP } state;
   Registers regs;
   uint64_t exit_value;
   struct TASK *prev;
   struct TASK *next;
   struct TASK *children;
   struct TASK *sibling;
   struct mm_struct *mm;
   struct FILE_OBJ *file_desc[MAX_FDS];  
   DIRK *dir_desc[MAX_FDS];
   char cwd[100];
} Task ;

extern Task *CURRENT_TASK;
extern void switchTask(Registers *oldregs, Registers *newregs); 
extern void switchTaskUser(Registers *oldregs, Registers *newregs); 
extern void saveState(Registers *oldregs); 
extern Task *FG_TASK;
extern Task *SLEEPING_TASK;
extern uint64_t TERMINAL_BUFFER;
extern uint64_t TERM_BUF_OFFSET;
#endif
