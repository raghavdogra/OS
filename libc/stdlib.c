#include <sys/defs.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int execvpe(const char *file, char *const argv[], char *const envp[]) {
  
  char buf[100];
  memset((void*)buf,0 ,100);
  if(file[0] == '/') {
	strcpy((void*)buf, (void*)file);
  }
  else {
    getcwd(buf, 100);
    strcat(buf, file);
  }
  execve(buf, argv, envp);
  if(envp == NULL) return -1;
  if(envp[0] == NULL) return -1;
  int i = 0, j = 0, flag = 0, k = 0, l = 0;
  char envVar[256];
  char values[50][1024];
  while(envp[i] != NULL) {
    if(envp[i][0] == '\0') break;
    j = 0;
    while(envp[i][j] != '='  && envp[i][j] != '\0') {
      envVar[j] = envp[i][j];
      j++;
    }
    envVar[j] = '\0';
    j++;
    if(strcmp(envVar, "PATH") == 0) {
      flag = 1;
      break;
    }
   i++;
  }
  if(flag == 1) {
    l = 0;
    while(envp[i][j] != '\0') {
      while(envp[i][j] != ':' && envp[i][j] != '\0') {
          values[k][l] = envp[i][j];
          j++;l++;
      }
      values[k][l] = '/'; l++;
      values[k][l] = '\0';
      if(envp[i][j] != '\0')
       { k++;j++;l=0; }
      else
       { k++;l=0; }
    }
  }
  for(i = 0; i < k; i++) {
    strcat(values[i], file);
    execve(values[i], argv, envp);
    i++;
  }
  return -1;
}

void exit(int ret) {
	uint64_t syscall_code = 60;
//	uint64_t retval= ret;
	uint64_t result;
	__asm__ __volatile("int $0x80\n\t"
			   :"=a"(result)
			   :"a"(syscall_code), "D"(ret));
}

/*void print_list() {
	m_header *temp = start;
	while(temp != NULL) {
		puts("i");	
	}
}*/

void divide_block(m_header *block, size_t alloc_size) {
	size_t remaining_block_size = 0;
	if(block->size > alloc_size) {
		remaining_block_size = block->size - alloc_size;
		if(remaining_block_size > ((sizeof(m_header) + 15 ) & ~(15))) {
			//splitblock
			m_header *split_block = (m_header*) ((uint64_t) block + alloc_size);
			split_block->next = block->next;
			block->next = split_block;
			split_block->size = remaining_block_size;
			split_block->available = 1;
		}
	}
}

void *find_first_fit(m_header *start, size_t alloc_size) {
	if(start == NULL) return NULL;
	m_header *temp = start;
	while(temp != NULL) {
		if(temp->size >= alloc_size && temp->available == 1) 
			return temp;
		temp = temp->next;
	}
	return NULL;
}

void *malloc(size_t size) {
	static m_header *start = NULL;
	static m_header *end = NULL;
	size_t alloc_size = (size + sizeof(m_header) + 15 ) & ~(15);
	//alloc_size = alloc_size << 1;
	m_header *block = (m_header*)find_first_fit(start , alloc_size);
	if(block != NULL) {
		divide_block(block, alloc_size);
		block->size = alloc_size;
		block->available = 0;
		return (void *)((uint64_t) block + sizeof(m_header));
	}
	else if(block == NULL) {
		uint64_t temp_block = ret_brk();
		uint64_t end_block = (uint64_t)temp_block + alloc_size;
		int status = brk((void*) end_block);
		if(status == -1) {
		//	puts("Not Possible to allocate More memory");
		}
		else {
			m_header *new_header = (m_header*)temp_block;
			new_header->size = alloc_size;
			new_header->available = 0;
			new_header->next = NULL;
			if(start == NULL) {
				start = new_header;
				end = new_header;
			}
			else {
				end->next = new_header;
				end = end->next;
			}
			return (void*)(temp_block + sizeof(m_header));
		}
	}
	else {
		return NULL;
	}
	return NULL;
}
void free(void *addr) {
	m_header *free_block = (m_header*)((uint64_t)addr - sizeof(m_header));
	free_block->available = 1;
	memset((uint8_t*) ((uint64_t) addr), 0, free_block->size - sizeof(m_header));
}
