#include <sys/idt.h>
#include <sys/defs.h>
#include <sys/kprintf.h>

struct IDTDescr idt[256];
struct IDT_table_ptr idt_ptr;

extern uint64_t kyb_isr;
extern uint64_t timer_isr;
extern uint64_t generic_isr_noerr;
extern uint64_t generic_isr_err8;
extern uint64_t generic_isr_err10;
extern uint64_t generic_isr_err11;
extern uint64_t generic_isr_err12;
extern uint64_t generic_isr_err13;
extern uint64_t generic_isr_err14;
extern uint64_t syscall;

void init_idt() {
	int i = 0;
	uint16_t offset1=(uint16_t)((uint64_t)&generic_isr_noerr & 0x000000000000FFFF);	
	uint16_t offset2=(uint16_t)(((uint64_t)&generic_isr_noerr >> 16) & 0x000000000000FFFF);
	uint32_t offset3=(uint32_t)(((uint64_t)&generic_isr_noerr >> 32) & 0xFFFFFFFF);
	for(i = 0; i < 32; i++) {
		//kprintf("%x, %x, %x, %x %d \n",  offset1, offset2, ~offset3, (uint64_t)&timer_isr, i);
		set_idt(&idt[i], offset1, 8, 0, 0x8f, offset2, offset3);
	}
	for(i = 32; i < 256; i++) {
		set_idt(&idt[i], offset1, 8, 0, 0x8e, offset2, offset3);	
	}
	offset1=(uint16_t)((uint64_t)&generic_isr_err8 & 0x000000000000FFFF);
        offset2=(uint16_t)(((uint64_t)&generic_isr_err8 >> 16) & 0x000000000000FFFF);
        offset3=(uint32_t)(((uint64_t)&generic_isr_err8 >> 32) & 0xFFFFFFFF);
	set_idt(&idt[8], offset1, 8, 0, 0x8f, offset2, offset3);
	offset1=(uint16_t)((uint64_t)&generic_isr_err10 & 0x000000000000FFFF);
        offset2=(uint16_t)(((uint64_t)&generic_isr_err10 >> 16) & 0x000000000000FFFF);
        offset3=(uint32_t)(((uint64_t)&generic_isr_err10 >> 32) & 0xFFFFFFFF);
	set_idt(&idt[10], offset1, 8, 0, 0x8f, offset2, offset3);
	offset1=(uint16_t)((uint64_t)&generic_isr_err11 & 0x000000000000FFFF);
        offset2=(uint16_t)(((uint64_t)&generic_isr_err11 >> 16) & 0x000000000000FFFF);
        offset3=(uint32_t)(((uint64_t)&generic_isr_err11 >> 32) & 0xFFFFFFFF);
	set_idt(&idt[11], offset1, 8, 0, 0x8f, offset2, offset3);
	offset1=(uint16_t)((uint64_t)&generic_isr_err12 & 0x000000000000FFFF);
        offset2=(uint16_t)(((uint64_t)&generic_isr_err12 >> 16) & 0x000000000000FFFF);
        offset3=(uint32_t)(((uint64_t)&generic_isr_err12 >> 32) & 0xFFFFFFFF);
	set_idt(&idt[12], offset1, 8, 0, 0x8f, offset2, offset3);
	offset1=(uint16_t)((uint64_t)&generic_isr_err13 & 0x000000000000FFFF);
        offset2=(uint16_t)(((uint64_t)&generic_isr_err13 >> 16) & 0x000000000000FFFF);
        offset3=(uint32_t)(((uint64_t)&generic_isr_err13 >> 32) & 0xFFFFFFFF);
	set_idt(&idt[13], offset1, 8, 0, 0x8f, offset2, offset3);
	offset1=(uint16_t)((uint64_t)&generic_isr_err14 & 0x000000000000FFFF);
        offset2=(uint16_t)(((uint64_t)&generic_isr_err14 >> 16) & 0x000000000000FFFF);
        offset3=(uint32_t)(((uint64_t)&generic_isr_err14 >> 32) & 0xFFFFFFFF);
	set_idt(&idt[14], offset1, 8, 0, 0x8f, offset2, offset3);
		
	offset1=(uint16_t)((uint64_t)&timer_isr & 0x000000000000FFFF);	
	offset2=(uint16_t)(((uint64_t)&timer_isr >> 16) & 0x000000000000FFFF);
	offset3=(uint32_t)(((uint64_t)&timer_isr >> 32) & 0xFFFFFFFF);
	set_idt(&idt[32], offset1, 8, 0, 0x8e, offset2, offset3);

	offset1=(uint16_t)((uint64_t)&kyb_isr & 0x000000000000FFFF);  
        offset2=(uint16_t)(((uint64_t)&kyb_isr >> 16) & 0x000000000000FFFF);
        offset3=(uint32_t)(((uint64_t)&kyb_isr >> 32) & 0xFFFFFFFF);
        set_idt(&idt[33], offset1, 8, 0, 0x8e, offset2, offset3);
	

	offset1=(uint16_t)((uint64_t)&syscall & 0x000000000000FFFF);
        offset2=(uint16_t)(((uint64_t)&syscall >> 16) & 0x000000000000FFFF);
        offset3=(uint32_t)(((uint64_t)&syscall >> 32) & 0xFFFFFFFF);
        set_idt(&idt[128], offset1, 8, 0, 0xee, offset2, offset3);

	idt_ptr.limit = 4095;
	idt_ptr.base = (uint64_t)idt;
	//uint64_t temp = (uint64_t)&idt_ptr;
		__asm__ __volatile("lidt (%0)"
			   :
            		   :"a"((uint64_t)&idt_ptr)

             		);
}
void set_idt(struct IDTDescr *idt_entry, uint16_t offset_1, uint16_t selector, uint8_t ist, uint16_t type_attr, uint16_t offset_2, uint32_t offset_3){
	idt_entry->id_offset_1 	= offset_1;
	idt_entry->id_selector 	= selector;
	idt_entry->id_ist 	= ist;
	idt_entry->id_type_attr = type_attr;
	idt_entry->id_offset_2 	= offset_2;
	idt_entry->id_offset_3 	= offset_3;
	idt_entry->id_zero 	= 0;
}

