#ifndef _IDT_H_
#define _IDT_H_
#include <sys/defs.h>



struct IDTDescr {
   uint16_t id_offset_1 ; // offset bits 0..15
   uint16_t id_selector;// = 8; // a code segment selector in GDT or LDT
   uint8_t id_ist ;       // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
   uint8_t id_type_attr;// = 14; // type and attributes
   uint16_t id_offset_2 ; // offset bits 16..31
   uint32_t id_offset_3; // offset bits 32..63
   uint32_t id_zero ;// = 0;     // reserved
}__attribute__((packed));
extern struct IDTDescr idt[256];

struct IDT_table_ptr {
  uint16_t limit;
  uint64_t base;
}__attribute__((packed));
extern struct IDT_table_ptr idt_ptr; 

void init_idt();
void set_idt(struct IDTDescr *idt_entry, uint16_t offset_1, uint16_t selector, uint8_t ist, uint16_t type_attr, uint16_t offset_2, uint32_t offset_3);
#endif
