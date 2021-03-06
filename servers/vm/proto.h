/* Function prototypes. */

struct vmproc;
struct stat;
struct mem_map;
struct memory;
struct vir_region;
struct phys_region;

#include <minix/ipc.h>
#include <minix/endpoint.h>
#include <minix/safecopies.h>
#include <minix/vm.h>
#include <timers.h>
#include <stdio.h>
#include <pagetable.h>

#include "vm.h"
#include "yielded.h"

/* alloc.c */
phys_clicks alloc_mem(phys_clicks clicks, u32_t flags);
struct memlist *alloc_mem_in_list(phys_bytes bytes, u32_t flags);
int do_adddma(message *msg);
int do_deldma(message *msg);
int do_getdma(message *msg);
void release_dma(struct vmproc *vmp);
void memstats(int *nodes, int *pages, int *largest);
void printmemstats(void);
void usedpages_reset(void);
int usedpages_add_f(phys_bytes phys, phys_bytes len, char *file, int
	line);
void free_mem(phys_clicks base, phys_clicks clicks);
void free_mem_list(struct memlist *list, int all);
void print_mem_list(struct memlist *ml);
#define usedpages_add(a, l) usedpages_add_f(a, l, __FILE__, __LINE__)

void mem_init(struct memory *chunks);

/* utility.c */
int get_mem_map(int proc_nr, struct mem_map *mem_map);
void get_mem_chunks(struct memory *mem_chunks);
void reserve_proc_mem(struct memory *mem_chunks, struct mem_map
	*map_ptr);
int vm_isokendpt(endpoint_t ep, int *proc);
int get_stack_ptr(int proc_nr, vir_bytes *sp);
int do_info(message *);
int swap_proc_slot(struct vmproc *src_vmp, struct vmproc *dst_vmp);
int swap_proc_dyn_data(struct vmproc *src_vmp, struct vmproc *dst_vmp);

/* exit.c */
void clear_proc(struct vmproc *vmp);
int do_exit(message *msg);
int do_willexit(message *msg);
void free_proc(struct vmproc *vmp);

/* fork.c */
int do_fork(message *msg);

/* exec.c */
int do_exec_newmem(message *msg);
int proc_new(struct vmproc *vmp, phys_bytes start, phys_bytes text_addr,
	phys_bytes text_bytes, phys_bytes data_addr, phys_bytes data_bytes,
	phys_bytes stack, phys_bytes gap, phys_bytes text_here, phys_bytes
	data_here, vir_bytes stacktop, int prealloc_stack, int is_elf, int full);
phys_bytes find_kernel_top(void);

/* break.c */
int do_brk(message *msg);
int adjust(struct vmproc *rmp, vir_clicks data_clicks, vir_bytes sp);
int real_brk(struct vmproc *vmp, vir_bytes v);

/* signal.c */
int do_push_sig(message *msg);

/* map_mem.c */
int map_memory(endpoint_t sour, endpoint_t dest, vir_bytes virt_s,
	vir_bytes virt_d, vir_bytes length, int flag);
int unmap_memory(endpoint_t sour, endpoint_t dest, vir_bytes virt_s,
	vir_bytes virt_d, vir_bytes length, int flag);

/* mmap.c */
int do_mmap(message *msg);
int do_munmap(message *msg);
int do_map_phys(message *msg);
int do_unmap_phys(message *msg);
int do_remap(message *m);
int do_get_phys(message *m);
int do_shared_unmap(message *m);
int do_get_refcount(message *m);

/* pagefaults.c */
void do_pagefaults(message *m);
void do_memory(void);
char *pf_errstr(u32_t err);
int handle_memory(struct vmproc *vmp, vir_bytes mem, vir_bytes len, int
	wrflag);

/* $(ARCH)/pagetable.c */
void pt_init(phys_bytes limit);
void pt_init_mem(void);
void pt_check(struct vmproc *vmp);
int pt_new(pt_t *pt);
void pt_free(pt_t *pt);
int pt_map_in_range(struct vmproc *src_vmp, struct vmproc *dst_vmp,
	vir_bytes start, vir_bytes end);
int pt_ptmap(struct vmproc *src_vmp, struct vmproc *dst_vmp);
int pt_ptalloc_in_range(pt_t *pt, vir_bytes start, vir_bytes end, u32_t
	flags, int verify);
void pt_clearmapcache(void);
int pt_writemap(struct vmproc * vmp, pt_t *pt, vir_bytes v, phys_bytes
	physaddr, size_t bytes, u32_t flags, u32_t writemapflags);
int pt_checkrange(pt_t *pt, vir_bytes v, size_t bytes, int write);
int pt_bind(pt_t *pt, struct vmproc *who);
void *vm_allocpage(phys_bytes *p, int cat);
void pt_cycle(void);
int pt_mapkernel(pt_t *pt);
void vm_pagelock(void *vir, int lockflag);
int vm_addrok(void *vir, int write);

#if SANITYCHECKS
void pt_sanitycheck(pt_t *pt, char *file, int line);
#endif

/* slaballoc.c */
void *slaballoc(int bytes);
void slabfree(void *mem, int bytes);
void slabstats(void);
void slab_sanitycheck(char *file, int line);
#define SLABALLOC(var) (var = slaballoc(sizeof(*var)))
#define SLABFREE(ptr) do { slabfree(ptr, sizeof(*(ptr))); (ptr) = NULL; } while(0)
#if SANITYCHECKS

void slabunlock(void *mem, int bytes);
void slablock(void *mem, int bytes);
int slabsane_f(char *file, int line, void *mem, int bytes);
#endif

/* region.c */
void map_region_init(void);
struct vir_region * map_page_region(struct vmproc *vmp, vir_bytes min,
	vir_bytes max, vir_bytes length, vir_bytes what, u32_t flags, int
	mapflags);
struct vir_region * map_proc_kernel(struct vmproc *dst);
int map_region_extend(struct vmproc *vmp, struct vir_region *vr,
	vir_bytes delta);
int map_region_extend_upto_v(struct vmproc *vmp, vir_bytes vir);
int map_region_shrink(struct vir_region *vr, vir_bytes delta);
int map_unmap_region(struct vmproc *vmp, struct vir_region *vr,
	vir_bytes len);
int map_free_proc(struct vmproc *vmp);
int map_proc_copy(struct vmproc *dst, struct vmproc *src);
int map_proc_copy_from(struct vmproc *dst, struct vmproc *src, struct
	vir_region *start_src_vr);
struct vir_region *map_lookup(struct vmproc *vmp, vir_bytes addr);
int map_pf(struct vmproc *vmp, struct vir_region *region, vir_bytes
	offset, int write);
int map_pin_memory(struct vmproc *vmp);
int map_handle_memory(struct vmproc *vmp, struct vir_region *region,
	vir_bytes offset, vir_bytes len, int write);
void map_printmap(struct vmproc *vmp);
int map_writept(struct vmproc *vmp);
void printregionstats(struct vmproc *vmp);
phys_bytes map_lookup_phys(struct vmproc *vmp, u32_t tag);
void map_setparent(struct vmproc *vmp);
int yielded_block_cmp(struct block_id *, struct block_id *);

struct vir_region * map_region_lookup_tag(struct vmproc *vmp, u32_t
	tag);
void map_region_set_tag(struct vir_region *vr, u32_t tag);
u32_t map_region_get_tag(struct vir_region *vr);
int map_remap(struct vmproc *dvmp, vir_bytes da, size_t size, struct
	vir_region *region, vir_bytes *r, int ro);
int map_get_phys(struct vmproc *vmp, vir_bytes addr, phys_bytes *r);
int map_get_ref(struct vmproc *vmp, vir_bytes addr, u8_t *cnt);

void pb_unreferenced(struct vir_region *region, struct phys_region *pr);
void get_stats_info(struct vm_stats_info *vsi);
void get_usage_info(struct vmproc *vmp, struct vm_usage_info *vui);
int get_region_info(struct vmproc *vmp, struct vm_region_info *vri, int
	count, vir_bytes *nextp);
int copy_abs2region(phys_bytes abs, struct vir_region *destregion,
	phys_bytes offset, phys_bytes len);
#if SANITYCHECKS
void map_sanitycheck(char *file, int line);
void blockstats(void);
#endif
int do_forgetblocks(message *m);
int do_forgetblock(message *m);
int do_yieldblockgetblock(message *m);
vir_bytes free_yielded(vir_bytes bytes);

/* $(ARCH)/vm.c */
vir_bytes arch_map2vir(struct vmproc *vmp, vir_bytes addr);
char *arch_map2str(struct vmproc *vmp, vir_bytes addr);
vir_bytes arch_map2info(struct vmproc *vmp, vir_bytes addr, int *space,
	int *prot);
vir_bytes arch_vir2map(struct vmproc *vmp, vir_bytes addr);
vir_bytes arch_vir2map_text(struct vmproc *vmp, vir_bytes addr);
vir_bytes arch_addrok(struct vmproc *vmp, vir_bytes addr);

/* rs.c */
int do_rs_set_priv(message *m);
int do_rs_update(message *m);
int do_rs_memctl(message *m);

/* queryexit.c */
int do_query_exit(message *m);
int do_watch_exit(message *m);
int do_notify_sig(message *m);
void init_query_exit(void);
