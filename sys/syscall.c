#include <sys/kprintf.h>
#include <sys/defs.h>
#include <sys/kmalloc.h>
#include <sys/paging.h>
#include <sys/kernel_threads.h>
#include <sys/copy_tables.h>
#include <sys/elf64.h>
#include <sys/scheduler.h>
#include <sys/initfs.h>
#include <sys/file_handling.h>
#include <sys/string.h>
#include <sys/kmemcpy.h>
#include <sys/kstring.h>
#include <sys/gdt.h>

uint64_t sleeping_time=0;
uint64_t wait_time = 0;
/*TODO populate syscall number
void * syscall_tbl[NUM_SYSCALLS] = 
{
	sys_fork,
	opendir,
}
*/

void save_state(Task *child, uint64_t rsp) {
	
	uint64_t *stack = (uint64_t*) rsp;
	child->kstack -= 20;
	for(int i = 0; i < 21; ++i) {
		child->kstack[i] = stack[i+1];
	}
//	child->kstack[14] = 0;
	child->regs.rax = 0;
	child->regs.rip = stack[0];
	child->regs.rsp = (uint64_t)child->kstack ;
}

void setup_stack_exec(Task *child, uint64_t rsp) {

        uint64_t *stack = (uint64_t*) rsp;
        child->kstack -= 20;
        for(int i = 0; i < 15; ++i) {
                child->kstack[i] = 0;
        }
        for(int i = 15; i < 21; ++i) {
                child->kstack[i] = stack[i+1];
        }
	child->kstack[15] = child->mm->e_entry;
	child->kstack[18] = child->mm->stack_begin;	
//      child->kstack[14] = 0;
        child->regs.rip = stack[0];
        child->regs.rsp = (uint64_t)child->kstack ;
}


uint64_t yyield() {
    Task *last = CURRENT_TASK;
    CURRENT_TASK = CURRENT_TASK->next;
    switchTaskUser(&last->regs, &CURRENT_TASK->regs);
    return CURRENT_TASK->pid;
}
void deep_copy_vma(struct vma *parent, struct vma *child) {

    child->vma_start = parent->vma_start;
    child->vma_end = parent->vma_end;
    child->vma_file_ptr = parent->vma_file_ptr;
    child->vma_file_offset = parent->vma_file_offset;
    child->vma_flags = parent->vma_flags;
    child->vma_size = parent->vma_size;
    child->vm_type = parent->vm_type;
    
}

void copy_vma_list(struct vma *parent,struct mm_struct *child) {

    struct vma * curr_par_vma = parent;
    
    if(parent) {
        child->vm_begin = (struct vma *) kmalloc(sizeof(struct vma ));
	deep_copy_vma(parent,child->vm_begin);
        child->vm_begin->vma_next =NULL;
    }
    curr_par_vma = parent->vma_next;
    struct vma * child_vma = child->vm_begin; 
    
    while(curr_par_vma) {
	struct vma * tempvma = (struct vma *)kmalloc(sizeof(struct vma ));
	deep_copy_vma(curr_par_vma,tempvma);
	child_vma->vma_next = tempvma;
	child_vma = tempvma;
	child_vma->vma_next = NULL;
	curr_par_vma = curr_par_vma->vma_next;
    }
}
void duplicate_fds(Task *parent, Task *child) {
    int i;
    for (i=0; i<MAX_FDS; i++) {
	child->file_desc[i] = parent->file_desc[i];
	if (child->file_desc[i] != NULL) {
		child->file_desc[i]->file_ref_count++;
	}
    }
    return;
}
int readFile(int fd, char *buf, uint64_t length) {
    if ((fd <=2) || (fd >= MAX_FDS)) {
        kprintf("PANIC: invalid fd\n");
        return -1;
    }
    if (CURRENT_TASK->file_desc[fd] == NULL) {
        kprintf("PANIC: fd does not point to any file obj\n");
        return -1;
    }
    // TODO: check if file is WRONLY
    if ((CURRENT_TASK->file_desc[fd]->file_begin+CURRENT_TASK->file_desc[fd]->file_offset)
        >=CURRENT_TASK->file_desc[fd]->file_end) {
//	kprintf("%p file begin + offset > file end %d %d %d\n", CURRENT_TASK->file_desc[fd],
//		CURRENT_TASK->file_desc[fd]->file_begin,
//		CURRENT_TASK->file_desc[fd]->file_offset,
//		CURRENT_TASK->file_desc[fd]->file_end);
        return -1;
    }
    if ((CURRENT_TASK->file_desc[fd]->file_begin+CURRENT_TASK->file_desc[fd]->file_offset+length)
        > CURRENT_TASK->file_desc[fd]->file_end) {
        length = CURRENT_TASK->file_desc[fd]->file_end
                 - (CURRENT_TASK->file_desc[fd]->file_begin+CURRENT_TASK->file_desc[fd]->file_offset);
    }
    kmemcpy(buf,(char *)(CURRENT_TASK->file_desc[fd]->file_begin+CURRENT_TASK->file_desc[fd]->file_offset),length);
    CURRENT_TASK->file_desc[fd]->file_offset += length;
    buf[length]='\0';
//    kprintf("after reading begin %x offset %x end %x\n",CURRENT_TASK->file_desc[fd]->file_begin,CURRENT_TASK->file_desc[fd]->file_offset,
//							CURRENT_TASK->file_desc[fd]->file_end);
    return length;
}
void copy_to_child(Task *parent_task, Task *child_task) {

    child_task->ppid = parent_task->pid;
    child_task->pid =  (last_assn_pid+1)%MAX_PROC;
    kstrcpy(child_task->cwd, parent_task->cwd);
    last_assn_pid++;
    child_task->state = READY;
    duplicate_fds(parent_task, child_task); 
    //copy mm_struct
    child_task->mm = (struct mm_struct *)kmalloc(sizeof(struct mm_struct));
    child_task->mm->stack_begin = parent_task->mm->stack_begin;
    child_task->mm->brk_begin = parent_task->mm->brk_begin;
    child_task->mm->e_entry = parent_task->mm->e_entry;
    child_task->mm->pg_pml4 = copy_on_write();
    child_task->kstack = (uint64_t *)( 4088+(uint64_t)get_free_page(SUPERVISOR_ONLY, child_task->mm->pg_pml4));
        
    child_task->regs = parent_task->regs; 
    child_task->regs.cr3 = child_task->mm->pg_pml4;
    copy_vma_list(parent_task->mm->vm_begin, child_task->mm);
    // TODO: need to change this circular mapping
    put_in_run_queue(child_task);

}

//#include <sys/ahci.h>
uint64_t fork_handler(Task * child_task) {
    Task * parent_task = CURRENT_TASK;

//    child_task->regs = *reg;
    copy_to_child(parent_task,child_task);
    return child_task->pid;
    //TODO To be continued ....

}    

ssize_t read_handler(int fd, char *buf, size_t count) {
    // TODO: not checking bounds on buf
    if (fd == 0) { // special treatment for stdin
	// if no chars are available just return 0 right away
	if (TERM_BUF_OFFSET == 0)
		return 0;
	int x, chars_read =0; // whats the use of file offset in this case??
	// if lesser num of bytes are available than count then read only available number of chars
	if (TERM_BUF_OFFSET < count)
		count = TERM_BUF_OFFSET; 
	//kprintf("count inside read_handler %d\n",count);	
	for (x=0; x < count;x++) {
		char c = *(char *)(TERMINAL_BUFFER+x);
		if (c == '\n') {
			chars_read++;
			*buf = c;
			return chars_read;
		}
		*buf = c;
		buf++;
		chars_read++;
	}
	return -1;
    }
    else { // read regular files
    }
    return -1;
}

int printLine(char *str) {
    kprintf("%s",str);
    return kstrlen(str);
}

int brk_handler(uint64_t addr) {
    struct vma *stack_vma;
    for(stack_vma = CURRENT_TASK->mm->vm_begin; stack_vma->vm_type != STACK; stack_vma = stack_vma->vma_next) {
	if (stack_vma == NULL) return -1;
    }
    //kprintf("1\n");
    //kprintf("addr %x stack vma start %x\n",addr,(uint64_t)stack_vma->vma_start);
    if (addr >= (uint64_t)stack_vma->vma_start) return -1;
    //kprintf("2\n");
    struct vma *heap_vma;
    for (heap_vma = CURRENT_TASK->mm->vm_begin; heap_vma->vm_type != HEAP; heap_vma = heap_vma->vma_next) {
	if (heap_vma == NULL) return -1;
    }
    //kprintf("3\n");
    heap_vma->vma_end = (uint64_t *)addr;
    CURRENT_TASK->mm->brk_begin = addr;
    return 0; 
}
//static uint64_t bk =0;
uint64_t syscall_handler(void)
{
    // don't put anything before this!!!
    uint64_t arg1,arg2,arg3,arg4;
    uint64_t ret = 0;
    __asm__ __volatile__("movq %%rdi, %0\n\t"
			 "movq %%rsi, %1\n\t"
			 "movq %%rdx, %2\n\t"
			 "movq %%rcx, %3\n\t"
			 :"=D"(arg1),"=S"(arg2),"=d"(arg3),"=c"(arg4));
    /*
    uint64_t *binaryname=NULL;
    __asm__ __volatile__("movq %%rdi, %0\n\t"
                         :"=D" (binaryname) :);
     */
    uint64_t rsp;
    __asm__ __volatile__("movq %%rbx, %0\n\t"
                        :"=b" (rsp)
                        :);
    rsp -= 8;
    uint64_t syscall_number=0;
    __asm__ __volatile__("movq %%rax, %0\n\t"
			:"=a" (syscall_number)
                        :);
    //kprintf("Syscallno %d from process %d\n",syscall_number,CURRENT_TASK->pid);
    switch(syscall_number) {
	case 0:; /* read syscall-arg1-file desc, arg2-buffer to copy to, arg3-number of chars to copy or till \n*/
		int fd = arg1;
                char *buffer = (char *)arg2;
                int count = arg3;
		if (fd == 0) {
			while (1) {
				if (FG_TASK != NULL) {
					schedule(); // TODO: is this the right call?
				}
				else
					break;
			}
			FG_TASK = CURRENT_TASK;
			ssize_t chars_read = read_handler(fd, buffer, count);
			if ((chars_read == -1) || (chars_read == 0)) {
				FG_TASK->state = WAITING;
				while(FG_TASK->state == WAITING) {
                                        schedule();
                                }
				chars_read = read_handler(fd, buffer, count);
			}
			//chars_read = read_handler(fd, buffer, count);
			if ((chars_read == 0) || (chars_read == -1)) return chars_read;	
			buffer[chars_read]='\0';
			int x; // TODO:look into this again
			for (x= chars_read; x <4096; x++) {
				*(char *)(TERMINAL_BUFFER + x - chars_read) = *(char *)(TERMINAL_BUFFER + x);
			}
			TERM_BUF_OFFSET -= chars_read;
			FG_TASK = NULL;
			return chars_read;
		}
		else return readFile(fd,buffer,count);
		break;
	case 1:; /* write syscall-arg1-file desc, arg2-buffer to copy from, arg3-number of chars to copy */	
		int fdw = arg1;
		if (fdw == 1 || fdw == 2) {
			int lengthw = arg3;
                	if (lengthw == 1) {
                        	char str[2]; // TODO: should i use kmalloc for this?
                        	str[0] = *(char *)arg2;
                        	str[1] = '\0';
				return printLine(str);
                	}
			else {
				char *bufferw = (char *)arg2;
				return  printLine(bufferw);
			}
		}
		else {
			kprintf("Unknown fd for write\n");
			return -1;
		}	
		break;
	case 3:;
		//kprintf("process read this %s\n",(char *)arg1);
		dentry *file_entry = dentry_lookup((char*)arg1, (uint64_t) arg2);
		if (file_entry == NULL) ret = -1;
		else ret =  (allocate_file_object(file_entry));
		break;
	case 5:;
		deallocate_file_object((uint64_t) arg1);
		break;
	case 10:
    	        kprintf("I'm in parent process %d\n",CURRENT_TASK->pid);
		break;
	case 9:
		kprintf("Hi from sbush\n");
		break;
	case 12:; /* brk system call- rdi-unsigned long addr */
	//	kprintf("brk called %d\n",bk++);
		//TODO: incomplete
		uint64_t addr = arg1;
		return brk_handler(addr);
		break;
	case 13:;
		return CURRENT_TASK->mm->brk_begin;
		break;
	case 11:
    	        kprintf("I'm in child process %d with arc %d\n",CURRENT_TASK->pid, arg1);
		//schedule();
		break;
	case 24:
		display_queue();
	        schedule();
		break;
	case 35:;
		wait_time = (uint64_t)arg1;
		SLEEPING_TASK = CURRENT_TASK;
		SLEEPING_TASK->state = SLEEP;
		while (SLEEPING_TASK->state == SLEEP) {
			schedule();
		}
		SLEEPING_TASK = NULL;
		return 0;
		break;
	case 39:
		return (CURRENT_TASK->pid);
	case 57:;
		//kprintf("rsp value %x\n",rsp);
    		Task * child_task = (Task *) kmalloc(sizeof(Task));
                //Registers *reg = (Registers *)&child_task->regs;
		__asm__ __volatile__("movq %%ds, %0\n\t"
                      		    :"=a" (child_task->regs.ds)
                        	    :);
                __asm__ __volatile__("movq %%es, %0\n\t"
                      		    :"=a" (child_task->regs.es)
                        	    :);
                __asm__ __volatile__("movq %%fs, %0\n\t"
                      		    :"=a" (child_task->regs.fs)
                        	    :);
                __asm__ __volatile__("movq %%gs, %0\n\t"
                      		    :"=a" (child_task->regs.gs)
                        	    :);
//		kprintf("rsp value %x\n",rsp);
		ret = fork_handler(child_task);
		save_state(child_task, rsp);
	        //saveState(reg);
		display_queue();
		break;
	case 59:; /* execve- rdi-binary name,rsi-argv,rdx-envp*/
		//kprintf("%d %d\n",arg2, arg3);	
		//kprintf("process read this %s\n",(char *)arg1);
		char **argv; char **envp;
		argv = (char **)arg2;
		envp = (char **)arg3;
		char exec_check_path[100];
		kstrcpy(exec_check_path, (char*)arg1);
		kstrrem(exec_check_path, '/');
		char *exec_full_path = dentry_lookup_get_path(exec_check_path);
		if(!exec_full_path) {
			CURRENT_TASK->state = ZOMBIE;
			return -1;
		}
		dentry *dent = dentry_lookup((char*)arg1, 0);
		if(!dent) {
			CURRENT_TASK->state = ZOMBIE;
			return -1;	
		} 
		kstrcpy((void*)arg1, (void*)(exec_full_path+8));
		kstrcat((void*)arg1,dent->d_name );
		//kprintf("\n EXECUTE PATH %s", arg1);
		kfree((uint64_t*)exec_full_path);
		Task *replacement_task = loadElf((char *)arg1, argv, envp);
		if(!replacement_task) {
			CURRENT_TASK->state = ZOMBIE;
			return -1;
		}
		#if 0
                if (argv != NULL) {
                        kprintf("after loadelf in exec handler argv0 %s\n",argv[0]);
                }
                #endif
		kstrcpy(replacement_task->cwd, CURRENT_TASK->cwd);
		replacement_task->regs.rax = 0;
    		replacement_task->regs.rbx = 0;
    		replacement_task->regs.rcx = 0;
    		replacement_task->regs.rdx = 0;
    		replacement_task->regs.rsi = 0;
    		replacement_task->regs.rdi = 0;
    		replacement_task->regs.rflags = CURRENT_TASK->regs.rflags;
    	//	replacement_task->regs.rip = replacement_task->mm->e_entry;
   		replacement_task->regs.r8 = 0;
    		replacement_task->regs.r9 = 0;
    		replacement_task->regs.r10 = 0;
    		replacement_task->regs.r11 = 0;
    		replacement_task->regs.r12 = 0;
    		replacement_task->regs.r13 = 0;
    		replacement_task->regs.r14 = 0;
    		replacement_task->regs.r15 = 0;
		replacement_task->kstack = (uint64_t *)( 4048+(uint64_t)get_free_page(SUPERVISOR_ONLY, replacement_task->mm->pg_pml4));
    	//	replacement_task->regs.rsp = replacement_task->mm->stack_begin;
	        __asm__ __volatile__("movq %%ds, %0\n\t"
       			            :"=a" (replacement_task->regs.ds)
                                    :);
                __asm__ __volatile__("movq %%es, %0\n\t"
                                    :"=a" (replacement_task->regs.es)
                                    :);
                __asm__ __volatile__("movq %%fs, %0\n\t"
                                    :"=a" (replacement_task->regs.fs)
                                    :);
                __asm__ __volatile__("movq %%gs, %0\n\t"
                                    :"=a" (replacement_task->regs.gs)
                                    :);
/*
    		replacement_task->regs.ds = CURRENT_TASK->regs.ds;
		replacement_task->regs.es = CURRENT_TASK->regs.es;
		replacement_task->regs.fs = CURRENT_TASK->regs.fs;
		replacement_task->regs.gs = CURRENT_TASK->regs.gs;
		replacement_task->regs.ss = CURRENT_TASK->regs.ss;
		replacement_task->regs.cs = CURRENT_TASK->regs.cs;*/
		// put in run queue and give it the same pid
		replacement_task->pid = CURRENT_TASK->pid;
		replacement_task->ppid = CURRENT_TASK->ppid;
		setup_stack_exec(replacement_task, rsp);
		put_in_run_queue(replacement_task);
//		replace_ptr_in_queue(CURRENT_TASK,replacement_task);
		CURRENT_TASK->pid = 0;
		CURRENT_TASK->ppid = 1;
		CURRENT_TASK->state = ZOMBIE;
		Task * oldtask = CURRENT_TASK;
		CURRENT_TASK = replacement_task;
		CURRENT_TASK->state = READY;
		set_tss_rsp((void *)((uint64_t)CURRENT_TASK->kstack));
		#if 0
                if (argv != NULL) {
                        kprintf("right before switch task in exec handler argv0 %s\n",argv[0]);
                }
                #endif
		switchTask(&oldtask->regs, &CURRENT_TASK->regs);
/*
		// TODO: remove this circular list
		replacement_task->next = CURRENT_TASK->next;
		replacement_task->prev = CURRENT_TASK->prev;
		replacement_task->next->next = replacement_task;
		replacement_task->next->prev = replacement_task;
		
		CURRENT_TASK->next = replacement_task;
		delete_page_tables(CURRENT_TASK->regs.cr3);
		yyield();*/	
		return -1; // if execve returns its an error*/
		break;
	case 60: /* exit- rdi-return value of main*/
//		if((uint64_t)arg1 == 0) {//main returned 0, normal exit
//			Task * deleteme = CURRENT_TASK;
//			
//		}
		//kprintf(" %d",CURRENT_TASK->pid);
		//remove_from_run_queue(CURRENT_TASK);
		reparent_orphans(CURRENT_TASK);
		CURRENT_TASK->state = ZOMBIE;
		CURRENT_TASK->exit_value = arg1;
		display_queue();
		schedule();
		break;
	case 61: /* wait- rdi-ptr to status of the exiting child process*/
		
		if(has_child(CURRENT_TASK)==0) 
			return -1; //this process doesn't have any child
		while(1) {
			Task * child_task = zombie_child_exists(CURRENT_TASK);
			if(child_task) {
				uint64_t child_id = child_task->pid;
				uint64_t *wstatus = (uint64_t *) arg1;
				*wstatus = child_task->exit_value;
				reap_process(child_task);
				return child_id;
			}
			schedule();
		}
                break;
	case 62: /* kill- rdi-ptr to status of the exiting child process*/
		if((uint64_t)arg1 == 1) {	//trying to kill the pid 1, can't do it
			return -1;
		}
		if(!pid_exists((uint64_t)arg1))		//trying to kill someone who doesn't exist
			return -1;
		if((uint64_t)arg1 == CURRENT_TASK->pid) {
			kprintf("I'm killing myself, bye from %d\n",CURRENT_TASK->pid);
			CURRENT_TASK->state = ZOMBIE;
			schedule();
		}
		else {
			Task * killThis = get_task_from_pid((uint64_t)arg1);
			if(!killThis)
				return -1;
			killThis->state = ZOMBIE;
			kprintf("Killing process %d\n",killThis->pid);
			return 0;
		}
		break;
	case 76:
		deallocate_new_dir((uint64_t)arg1);
		break; 
	// Read dir
	case 77:;
		DIRK *read_dir = CURRENT_TASK->dir_desc[(uint64_t)arg1];
		ldirent *ret_dir = (ldirent*)arg2;
		int current = read_dir->d_current + 1;
		int end = read_dir->d_entry->d_end;
		if(current < end) {
			read_dir->d_current = current;
			ret_dir->d_off = current;
			dentry *dir_entry = read_dir->d_entry->d_children[read_dir->d_current];
			memset(read_dir->d_name, 0, 100);
			memset(ret_dir->d_name, 0, 100);
			kstrcpy(read_dir->d_name, dir_entry->d_name);
			kstrcpy(ret_dir->d_name, dir_entry->d_name);
			ret = (uint64_t)ret_dir;
		}
		else {
			kstrcpy(ret_dir->d_name, "");
			ret = (uint64_t)ret_dir;
		}
		break;
	// open dir
	case 78:;	
		//kprintf("process read this %s\n",(char *)arg1);
		dentry *dir_entry = dentry_lookup((char*)arg1, 0);
		if(dir_entry) {
			ret = allocate_new_dir(dir_entry);
		}
		else ret = -1;
		break;
	// getcwd
	case 79:;
		kstrcpy((char*)arg1, CURRENT_TASK->cwd);
		ret = (uint64_t) arg1;
		break;
	// chdir
	case 80:;
		char *path = (char *) arg1;
		char check_path[100] = {0};
		if(path[0] == '/') {
			kstrcpy(check_path, (char*) arg1);
		} else {
			kstrcat(check_path, CURRENT_TASK->cwd);
			kstrcat(check_path, (char*) arg1);
		}
		char *full_path = dentry_lookup_get_path(check_path);
		if(full_path){
			kstrcpy(CURRENT_TASK->cwd, full_path);
			kfree((uint64_t*)full_path);
			ret = 0;
		} else {
			ret = -1;
		}
		break;
	case 110:
		return (CURRENT_TASK->ppid);
	case 247:; /* kill- rdi-ptr to status of the exiting child process*/
		uint64_t pid = (uint64_t)arg1;
		Task * child = get_task_from_pid(pid);
		if(child->ppid != CURRENT_TASK->pid) {
			kprintf("Can't wait on %d, not the child of %d\n",child->pid,CURRENT_TASK->pid);
			return -1;
		}
                while(1) {
                        if(child->state==ZOMBIE) {//TODO: check if change state is to be supported: do it if possible
                                uint64_t child_id = child->pid;
                                uint64_t *wstatus = (uint64_t *) arg2;
                                *wstatus = child->exit_value;
                                reap_process(child);
                                return child_id;
                        }
                        schedule();
                }
		break;
	case 1000:
		display_pid();
		break;
	default:
		kprintf("Syscall not found %d\n",syscall_number);
    }
	
	return ret;
}
