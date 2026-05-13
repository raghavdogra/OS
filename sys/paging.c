#include <sys/paging.h>
#include <sys/kprintf.h>
pg_desc_t *free_list_head;
pg_desc_t *free_list;
uint64_t *PML4_kern;
uint64_t *PDTP;
uint64_t *PDE;
uint64_t *PTE1;
uint64_t *tss_kstack;
extern char kernmem, physbase;

uint64_t get_physical_free_page () {
  //kprintf("Freeing Page\n");
  if(free_list_head == NULL) {
    kprintf("ERROR: free list head NULL %x\n",(uint64_t) (free_list_head->index * 4096));
    return 0;
  }

  if(free_list_head->is_avail==0) {
    kprintf("ERROR: trying to allocate a non avaialable page %x\n",(uint64_t) (free_list_head->index * 4096));
    while(1);
  }
  uint64_t addr = (uint64_t) (free_list_head->index * 4096);
  pg_desc_t * temp = free_list_head;
  free_list_head = free_list_head->next;
  temp->next = NULL;
  temp->prev = NULL;
  temp->is_avail = 0;
  temp->count = 0;
  temp->ref_count = 1;
  return addr;
}
void reload_cr3() {
  uint64_t cr3;
  __asm__ __volatile__("movq %%cr3, %0\n\t"
                             :"=a"(cr3));
  __asm __volatile__("movq %0, %%cr3\n\t"
                    :
                    :"a"(cr3));
}
void free_physical_page( pg_desc_t *page){
 // pg_desc_t* page = (pg_desc_t *) ((uint64_t)0xffffffff80000000 + (uint64_t) pg);
  page->ref_count--;
  if(page->ref_count!=0)
	return;
  page->next=free_list_head;
  page->prev = NULL;
  page->is_avail = 1;
  free_list_head = page;
  reload_cr3();
}

uint64_t setup_memory( void *physbase, void *physfree, smap_copy_t *smap_copy, int index) {
    int num_pages = 0;
    num_pages = (smap_copy[index-1].last_addr - smap_copy[0].starting_addr)/4096;
    uint64_t free_list_begin;
    if (((uint64_t)physfree & 0x0000000000000fff) == 0)
        free_list_begin = (uint64_t)(physfree);
    else
        free_list_begin = (((uint64_t)(physfree+4096)>>12)<<12);

    free_list = (pg_desc_t *)free_list_begin;

    uint64_t free_list_end;
    if (((free_list_begin + (num_pages * sizeof(pg_desc_t))) & 0x0000000000000fff) == 0)
        free_list_end = (free_list_begin + (num_pages * sizeof(pg_desc_t)));
    else
        free_list_end = (((free_list_begin + (num_pages * sizeof(pg_desc_t)))+4096)>>12)<<12;
    
    free_list_end += 8192; // since stack grows downward
    tss_kstack = (uint64_t *)(free_list_end  + 0xffffffff80000000);
  // mark area between (kernmem+physbase) and (kernmem+physfree+space occupied by free_list) as occupied
  free_list[0].is_avail = 1;
  free_list[0].prev = NULL;
  free_list[0].next =(struct pg_desc *) ((uint64_t)0xffffffff80000000 + (uint64_t)(&free_list[1]));
  free_list[0].index = 0;
  free_list[0].count = 0;
  free_list[0].ref_count = 0;
  int i=0;
  for (i=1; i < (num_pages-1) ; i++) {
        free_list[i].is_avail = 1;
        free_list[i].prev =(struct pg_desc *) ((uint64_t)0xffffffff80000000 + (uint64_t)(&free_list[i-1]));
        free_list[i].next =(struct pg_desc *) ((uint64_t)0xffffffff80000000 + (uint64_t)(&free_list[i+1]));
        free_list[i].index = i;
        free_list[i].count = 0;
        free_list[i].ref_count = 0;
  }
  free_list[num_pages - 1].is_avail = 1;
  free_list[num_pages - 1].prev =(struct pg_desc *) ((uint64_t)0xffffffff80000000 + (uint64_t) &free_list[num_pages - 2]);
  free_list[num_pages - 1].next = NULL;
  free_list[num_pages - 1].index = num_pages - 1;
  free_list[num_pages-1].count = 0;
  free_list[num_pages-1].ref_count = 0;

  free_list_head = (&free_list[1]);

  uint64_t x;
  // note free_list_begin and free_list_end hold vaddresses while smap_copy holds phys addresses

  uint64_t begin;
  if (((uint64_t)physbase & 0x0000000000000fff) == 0) // already aligned
        begin = (uint64_t)physbase;
  else
        begin = ((uint64_t)physbase >> 12) << 12;

  uint64_t prev_valid = begin - 4096;
  
  // kernel + free list area
  for (x=begin ; x < free_list_end + (520 *4096); x+=4096) {
        free_list[x/4096].is_avail = 0; // it is not free
        if ((x/4096) == (num_pages-1)) {
                free_list[(x/4096)-1].next = NULL;
                free_list[x/4096].prev = NULL;
                free_list[x/4096].next = NULL;
        }
        else if ((x/4096) == 1) {
                free_list_head = &free_list[(x/4096)+1];
                free_list[(x/4096)+1].prev = NULL;
                free_list[x/4096].prev = NULL;
                free_list[x/4096].next = NULL;
        }
        else {
                free_list[(x/4096)-1].next =(struct pg_desc *) ((uint64_t)0xffffffff80000000 + (uint64_t) &free_list[(x/4096)+1]);
                free_list[(x/4096)+1].prev =(struct pg_desc *) ((uint64_t)0xffffffff80000000 + (uint64_t) &free_list[(x/4096)-1]);
                free_list[x/4096].prev = NULL;
                free_list[x/4096].next = NULL;
        }
        //kprintf("%d ",x/4096);
  }
  free_list[(prev_valid/4096)].next = (struct pg_desc *) ((uint64_t)0xffffffff80000000 + (uint64_t) &free_list[(x/4096)+1]);
  free_list[(x/4096)+1].prev = (struct pg_desc *) ((uint64_t)0xffffffff80000000 + (uint64_t) &free_list[(prev_valid/4096)]);
  

    int j;
  // other areas where ram does not exist
  for (i=0; i<(index-1) ; i++) {
        int first_page, second_page;
	uint64_t last_addr = smap_copy[i].last_addr;
	last_addr = last_addr>>12;
	last_addr = last_addr<<12;
        first_page = (last_addr/4096 );
        if ((smap_copy[i+1].starting_addr & 0x0000000000000fff) == 0) // already aligned
                second_page = smap_copy[i+1].starting_addr / 4096;
        else
                second_page = (smap_copy[i+1].starting_addr / 4096) + 1;

        for (j = first_page; j < second_page; j++) {
                free_list[j].is_avail = 0; // it is not free
                if (j == (num_pages-1)) {
                        free_list[j-1].next = NULL;
                        free_list[j].prev = NULL;
                        free_list[j].next = NULL;
                }
                else if (j == 1) {
                        free_list_head = &free_list[j+1];
                        free_list[j+1].prev = NULL;
                        free_list[j].prev = NULL;
                        free_list[j].next = NULL;
                }
                else {
                        free_list[j].prev = NULL;
                        free_list[j].next = NULL;
                }
                //kprintf("%d ",j);
        }
	
        free_list[first_page-1].next =(struct pg_desc *) ((uint64_t)0xffffffff80000000 + (uint64_t)&free_list[second_page]);
        free_list[second_page].prev =(struct pg_desc *) ((uint64_t)0xffffffff80000000 + (uint64_t)&free_list[first_page-1]);
  }
  free_list[free_list_end / 4096].is_avail = 0;
  free_list[(free_list_end / 4096) + 1].is_avail = 0;
  free_list_head = (struct pg_desc *)((uint64_t)0xffffffff80000000 + (uint64_t)free_list_head);
    return free_list_end;
}

void init_self_referencing(uint64_t free_list_end, uint64_t index) {
  PML4_kern =(uint64_t *) ((uint64_t)free_list_end);
  *PML4_kern = 0;
  PDTP = (uint64_t *)((uint64_t)free_list_end+(uint64_t)4096);
  PDE = (uint64_t *)((uint64_t)free_list_end+(uint64_t)8192);
  uint64_t temp = (uint64_t)free_list_end+(uint64_t)12288;
  //PTE1 = (uint64_t *)((uint64_t)free_list_end+(uint64_t)12288);
  //uint64_t * PTE2 = (uint64_t *)((uint64_t)free_list_end+(uint64_t)16384);
  PML4_kern[511] = ((uint64_t)PDTP) |(uint64_t) SUPERVISOR_ONLY;
  PDTP[510] = ((uint64_t)PDE) | (uint64_t) SUPERVISOR_ONLY; 
  for(int i = 0; i < 510; i++) {
    PML4_kern[i] = 0;
  }
  //PDE[0] = (uint64_t)PTE1;
  //PDE[index] = (uint64_t)PTE2;
  for(int i = 0; i < 512; ++i) {
      PDE[i] = (uint64_t) temp + (uint64_t)(i * 4096);
  }
}

void map_memory_range(uint64_t start, uint64_t end, uint64_t map_index) {
  //uint64_t offset = (uint64_t) (map_index *512);
  uint64_t *PTE = (uint64_t *)PDE[map_index];
  //PML4[map_index] = (uint64_t)PTE;
  //PML4[map_index] = PML4[map_index] | 3;
  uint64_t temp_addr = (uint64_t)(0xffffffff80000000 + start);
  temp_addr = temp_addr >> 12;
  uint64_t temp = 0x1ff;
  uint64_t ind = temp & temp_addr;
  for(int i = 0; i < ind; ++i) {
    PTE[i] = 0;
  }
  for(uint64_t x = (uint64_t)start; x < (uint64_t) end; x += 4096) {
    PTE[ind] = x | (uint64_t) SUPERVISOR_ONLY;
    ind++;
  }
  for(int i = ind; i < 512; i++) {
    PTE[i] = 0;
  }
}
uint64_t get_free_page(uint64_t flags, uint64_t cr3) {
  uint64_t phy_addr = get_physical_free_page();
  uint64_t virt_addr;
  uint64_t x;
   if(flags == 7)
	virt_addr= 4096 + phy_addr;
   else
	virt_addr = 0xffffffff80000000 + phy_addr;

  uint64_t PMLframe = (virt_addr >> 39) & (uint64_t) 0x1ff;
  uint64_t PDPTEindex = (virt_addr >> 30) & (uint64_t) 0x1ff;
  uint64_t PDEindex = (virt_addr >> 21) & (uint64_t) 0x1ff;
  uint64_t PTEindex = (virt_addr >> 12) & (uint64_t) 0x1ff;
  uint64_t *PML4 =  (uint64_t *)((uint64_t)cr3 +(uint64_t) 0xffffffff80000000);
//PML4
  uint64_t *PDPTE;
  if(PML4[PMLframe] == 0) {
    PDPTE = (uint64_t *)get_physical_free_page();
    PML4[PMLframe] = (uint64_t) PDPTE | (uint64_t) flags;
    x = (uint64_t)0xffffffff80000000 + (uint64_t) PML4[PMLframe];
    x = x & 0xfffffffffffff000;
    PDPTE = (uint64_t *) x;
    for(int i = 0; i < 512; i++) {
      PDPTE[i] = 0;
    }
  }
  else { 
    x = (uint64_t)0xffffffff80000000 + (uint64_t) PML4[PMLframe];
    x = x & 0xfffffffffffff000; 
    PDPTE = (uint64_t *) x;
  }


///PDTP
  uint64_t *PDE;
  if(PDPTE[PDPTEindex] == 0) {
    PDE = (uint64_t *)get_physical_free_page();
    PDPTE[PDPTEindex] = (uint64_t) PDE | (uint64_t) flags;
    x = (uint64_t)0xffffffff80000000 + (uint64_t) PDPTE[PDPTEindex];
    x = x & 0xfffffffffffff000; 
    PDE = (uint64_t *) x;
    for(int i = 0; i < 512; i++) {
      PDE[i] = 0;
    }
    int freeInd = ((PML4[PMLframe] >> 12) << 12) / 4096;
    pg_desc_t page = free_list[freeInd];
    page.count = page.count + 1;
  }
  else {
    x = (uint64_t)0xffffffff80000000 + (uint64_t) PDPTE[PDPTEindex];
    x = x & 0xfffffffffffff000; 
    PDE = (uint64_t *) x;
  }

//PDE
  uint64_t *PTE;
  if(PDE[PDEindex] == 0) {
    PTE = (uint64_t *)get_physical_free_page();
    PDE[PDEindex] = (uint64_t) PTE | (uint64_t) flags;
    x = (uint64_t)0xffffffff80000000 + (uint64_t) PDE[PDEindex];
    x = x & 0xfffffffffffff000; 
    PTE = (uint64_t *) x;
    for(int i = 0; i < 512; i++) {
      PTE[i] = 0;
    }
    pg_desc_t page = free_list[((PDPTE[PDPTEindex] >> 12) << 12) / 4096];
    page.count++;
  }
  
  else {
    x = (uint64_t)0xffffffff80000000 + (uint64_t) PDE[PDEindex];
    x = x & 0xfffffffffffff000; 
    PTE = (uint64_t *) x;
  }

//PTE
  
  PTE[PTEindex] = (uint64_t) phy_addr | (uint64_t) flags;
  pg_desc_t page = free_list[((PDE[PDEindex] >> 12) << 12) / 4096];
  page.count++; 
  //free_list[((PDE[PDEindex] >> 12) << 12) / 4096].count++;
 
  return virt_addr;
}

void free_page(void *addr, uint64_t cr3) {
  uint64_t virt_addr = (uint64_t) (addr); 
  virt_addr = (virt_addr >> 12 ) << 12;
  uint64_t PMLframe = (virt_addr >> 39) & (uint64_t) 0x1ff;
  uint64_t PDPTEindex = (virt_addr >> 30) & (uint64_t) 0x1ff;
  uint64_t PDEindex = (virt_addr >> 21) & (uint64_t) 0x1ff;
  uint64_t PTEindex = (virt_addr >> 12) & (uint64_t) 0x1ff;
  uint64_t *PDPTE, *PDE, *PTE, *PML4;
  PML4 = (uint64_t *)((uint64_t)cr3 +(uint64_t) 0xffffffff80000000);
  uint64_t x;
  x = (uint64_t)0xffffffff80000000 + (uint64_t) PML4[PMLframe];
  x = x & 0xfffffffffffff000;
  PDPTE = (uint64_t *) x;
  
  x = (uint64_t)0xffffffff80000000 + (uint64_t) PDPTE[PDPTEindex];
  x = x & 0xfffffffffffff000;
  PDE = (uint64_t *) x;
  
  x = (uint64_t)0xffffffff80000000 + (uint64_t) PDE[PDEindex];
  x = x & 0xfffffffffffff000;
  PTE = (uint64_t *) x;
 
  uint64_t phys_addr = (uint64_t ) ((PTE[PTEindex] >> 12) <<12);
//  kprintf("virt %x phy %x\n",virt_addr,phys_addr); 
  free_physical_page((pg_desc_t*)(&free_list[phys_addr/4096] )); 
 // PTE[PTEindex] = 0;
   
 /* pg_desc_t page = free_list[((PDE[PDEindex] >> 12) << 12) / 4096];
  page.count--;

  if(page.count == 0) {
    //free_physical_page(&page);
    PDE[PDEindex] = 0;
    page = free_list[((PDPTE[PDPTEindex] >> 12) << 12) / 4096];
    page.count--;
    if(page.count == 0) {
       PDPTE[PDPTEindex] = 0;
      //free_physical_page(&page);
       page = free_list[((PML4[PMLframe] >> 12) << 12) / 4096];
       page.count--;
       if(page.count == 0) {
         PML4[PMLframe] = 0;
         //free_physical_page(&page);
       }
    }
  }*/
}
uint64_t put_page_mapping(uint64_t flags, uint64_t virt_addr, uint64_t cr3val) {
  uint64_t phy_addr = get_physical_free_page();
  //uint64_t virt_addr;
  uint64_t x;
   /*if(flags == 7)
	virt_addr= 4096 + phy_addr;
   else
	virt_addr = 0xffffffff80000000 + phy_addr;
*/
  uint64_t PMLframe = (virt_addr >> 39) & (uint64_t) 0x1ff;
  uint64_t PDPTEindex = (virt_addr >> 30) & (uint64_t) 0x1ff;
  uint64_t PDEindex = (virt_addr >> 21) & (uint64_t) 0x1ff;
  uint64_t PTEindex = (virt_addr >> 12) & (uint64_t) 0x1ff;

  uint64_t *PML4 = (uint64_t *)((uint64_t)cr3val +(uint64_t) 0xffffffff80000000);
//PML4
  uint64_t *PDPTE;
  if(PML4[PMLframe] == 0) {
    PDPTE = (uint64_t *)get_physical_free_page();
    PML4[PMLframe] = (uint64_t) PDPTE | (uint64_t) flags;
    x = (uint64_t)0xffffffff80000000 + (uint64_t) PML4[PMLframe];
    x = x & 0xfffffffffffff000;
    PDPTE = (uint64_t *) x;
    for(int i = 0; i < 512; i++) {
      PDPTE[i] = 0;
    }
  }
  else { 
    x = (uint64_t)0xffffffff80000000 + (uint64_t) PML4[PMLframe];
    x = x & 0xfffffffffffff000; 
    PDPTE = (uint64_t *) x;
  }


///PDTP
  uint64_t *PDE;
  if(PDPTE[PDPTEindex] == 0) {
    PDE = (uint64_t *)get_physical_free_page();
    PDPTE[PDPTEindex] = (uint64_t) PDE | (uint64_t) 7;
    x = (uint64_t)0xffffffff80000000 + (uint64_t) PDPTE[PDPTEindex];
    x = x & 0xfffffffffffff000; 
    PDE = (uint64_t *) x;
    for(int i = 0; i < 512; i++) {
      PDE[i] = 0;
    }
    int freeInd = ((PML4[PMLframe] >> 12) << 12) / 4096;
    pg_desc_t page = free_list[freeInd];
    page.count = page.count + 1;
  }
  else {
    x = (uint64_t)0xffffffff80000000 + (uint64_t) PDPTE[PDPTEindex];
    x = x & 0xfffffffffffff000; 
    PDE = (uint64_t *) x;
  }

//PDE
  uint64_t *PTE;
  if(PDE[PDEindex] == 0) {
    PTE = (uint64_t *)get_physical_free_page();
    PDE[PDEindex] = (uint64_t) PTE | (uint64_t) flags;
    x = (uint64_t)0xffffffff80000000 + (uint64_t) PDE[PDEindex];
    x = x & 0xfffffffffffff000; 
    PTE = (uint64_t *) x;
    for(int i = 0; i < 512; i++) {
      PTE[i] = 0;
    }
    pg_desc_t page = free_list[((PDPTE[PDPTEindex] >> 12) << 12) / 4096];
    page.count++;
  }
  
  else {
    x = (uint64_t)0xffffffff80000000 + (uint64_t) PDE[PDEindex];
    x = x & 0xfffffffffffff000; 
    PTE = (uint64_t *) x;
  }

  PTE[PTEindex] = (uint64_t) phy_addr | (uint64_t) flags;
  //kprintf("\n Printing: %d, %d, %d, %d, %x, %x",PMLframe,PDPTEindex,PDEindex, PTEindex, phy_addr, PTE[PTEindex]); 
  pg_desc_t page = free_list[((PDE[PDEindex] >> 12) << 12) / 4096];
  page.count++; 
  //free_list[((PDE[PDEindex] >> 12) << 12) / 4096].count++;
 
  return virt_addr;
}

uint64_t walk_pml4_get_address(uint64_t virt_addr, uint64_t cr3val) {

  uint64_t *PML4 = (uint64_t*)((uint64_t)0xffffffff80000000 + cr3val);
  uint64_t PMLframe = (virt_addr >> 39) & (uint64_t) 0x1ff;
  uint64_t PDPTEindex = (virt_addr >> 30) & (uint64_t) 0x1ff;
  uint64_t PDEindex = (virt_addr >> 21) & (uint64_t) 0x1ff;
  uint64_t PTEindex = (virt_addr >> 12) & (uint64_t) 0x1ff;
  uint64_t x;
  uint64_t *PDPTE, *PDE, *PTE;

  x = (uint64_t)0xffffffff80000000 + (uint64_t) PML4[PMLframe];
  x = x & 0xfffffffffffff000;
  PDPTE = (uint64_t *) x;   

  x = (uint64_t)0xffffffff80000000 + (uint64_t) PDPTE[PDPTEindex];
  x = x & 0xfffffffffffff000;
  PDE = (uint64_t *) x;   

  x = (uint64_t)0xffffffff80000000 + (uint64_t) PDE[PDEindex];
  x = x & 0xfffffffffffff000;
  PTE = (uint64_t *) x;   
  reload_cr3();
  return PTE[PTEindex];
}

void walk_pml4_unmark_cow(uint64_t virt_addr, uint64_t cr3val, uint64_t flags) {

  uint64_t *PML4 = (uint64_t*)((uint64_t)0xffffffff80000000 + cr3val);
  uint64_t PMLframe = (virt_addr >> 39) & (uint64_t) 0x1ff;
  uint64_t PDPTEindex = (virt_addr >> 30) & (uint64_t) 0x1ff;
  uint64_t PDEindex = (virt_addr >> 21) & (uint64_t) 0x1ff;
  uint64_t PTEindex = (virt_addr >> 12) & (uint64_t) 0x1ff;
  uint64_t x;
  uint64_t *PDPTE, *PDE, *PTE;

  x = (uint64_t)0xffffffff80000000 + (uint64_t) PML4[PMLframe];
  x = x & 0xfffffffffffff000;
  PDPTE = (uint64_t *) x;   

  x = (uint64_t)0xffffffff80000000 + (uint64_t) PDPTE[PDPTEindex];
  x = x & 0xfffffffffffff000;
  PDE = (uint64_t *) x;   

  x = (uint64_t)0xffffffff80000000 + (uint64_t) PDE[PDEindex];
  x = x & 0xfffffffffffff000;
  PTE = (uint64_t *) x;   
 

// TODO: Should we preserve old flags? 
  PTE[PTEindex] = (PTE[PTEindex] >> 12) << 12;
  PTE[PTEindex] = PTE[PTEindex] | flags;
}

void free_old_page_tables(uint64_t addr) {
}
