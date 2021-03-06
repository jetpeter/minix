
#ifndef _I386_TYPES_H
#define _I386_TYPES_H

#include <minix/sys_config.h>
#include <machine/stackframe.h>
#include <machine/fpu.h>
#include <sys/cdefs.h>

struct segdesc_s {		/* segment descriptor for protected mode */
  u16_t limit_low;
  u16_t base_low;
  u8_t base_middle;
  u8_t access;		/* |P|DL|1|X|E|R|A| */
  u8_t granularity;	/* |G|X|0|A|LIMT| */
  u8_t base_high;
};

#define LDT_SIZE 2	   /* CS and DS */

/* Fixed local descriptors. */
#define CS_LDT_INDEX         0  /* process CS */
#define DS_LDT_INDEX         1  /* process DS=ES=FS=GS=SS */

typedef struct segframe {
	reg_t p_ldt_sel;    /* selector in gdt with ldt base and limit */
	reg_t	p_cr3;		/* page table root */
	u32_t	*p_cr3_v;
	char	*fpu_state;
	struct segdesc_s p_ldt[LDT_SIZE]; /* CS, DS and remote */
} segframe_t;

#define INMEMORY(p) (!p->p_seg.p_cr3 || get_cpulocal_var(ptproc) == p)

typedef u32_t atomic_t;	/* access to an aligned 32bit value is atomic on i386 */

#endif /* #ifndef _I386_TYPES_H */

