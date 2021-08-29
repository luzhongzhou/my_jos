// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>
#include <inc/memlayout.h>
// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

extern volatile pte_t uvpt[];     // VA of "virtual page table"
extern volatile pde_t uvpd[];     // VA of current page directory

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	addr = ROUNDDOWN(addr, PGSIZE);
	uint32_t err = utf->utf_err;
	int r;
	pte_t pte;

	pte = uvpt[PGNUM(addr)];

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).
	
	// LAB 4: Your code here.
	if (!(err & FEC_WR)){
		panic("err %x\n", err);
	}
	if (!(pte & PTE_COW )) {
		panic("addr %x 's pte is %x, not PTE_COW\n", addr, pte);
	}
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	//panic("pgfault not implemented");
	//just copy from dumbfork.c
	if ((r = sys_page_alloc(0, UTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_alloc: %e", r);
	
	memmove(UTEMP, addr, PGSIZE);

	if ((r = sys_page_map(0, UTEMP, 0, addr, PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_map: %e", r);
	
	if ((r = sys_page_unmap(0, UTEMP)) < 0)
		panic("sys_page_unmap: %e", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	pte_t pte;

	pte = uvpt[PGNUM((uintptr_t)pn*PGSIZE)];

	// LAB 4: Your code here.
	//panic("duppage not implemented");

	if(pte & PTE_W || pte & PTE_COW){
		r = sys_page_map(0, (void *)(pn*PGSIZE), envid, (void *)(pn*PGSIZE),  PTE_P | PTE_U | PTE_COW);
		if (r < 0) {
			panic("sys_page_map failed: %e\n", r);
		}
		r = sys_page_map(0, (void *)(pn*PGSIZE), 0, (void *)(pn*PGSIZE),  PTE_P | PTE_U | PTE_COW);
		if (r < 0) {
			panic("sys_page_map failed: %e\n", r);
		}
	} else {
		r = sys_page_map(0, (void *)(pn*PGSIZE), envid, (void *)(pn*PGSIZE),  PTE_P | PTE_U);
		if (r < 0) {
			panic("sys_page_map failed: %e\n", r);
		}
	}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
extern void _pgfault_upcall(void);

envid_t
fork(void)
{
	// LAB 4: Your code here.
	//panic("fork not implemented");
	envid_t envid;
	uintptr_t addr;

	extern unsigned char end[];
	int r;

	set_pgfault_handler(pgfault);

	envid = sys_exofork();

	if (envid > 0) { //father process

		for(addr = 0; addr < UTOP; addr += PGSIZE){
			if(addr == (UXSTACKTOP - PGSIZE))
				continue;
			
			if(!(uvpd[PDX(addr)] & PTE_P))
				continue;
			
			if(!(uvpt[PGNUM(addr)] & PTE_P))
				continue;

			if(!(uvpt[PGNUM(addr)] & PTE_U))
				continue;

			if(duppage(envid, PGNUM(addr)) < 0)
				panic("Duppage Failed!");
		}
		sys_page_alloc(envid, (void *)(UXSTACKTOP-PGSIZE), PTE_U | PTE_P | PTE_W);
		sys_env_set_pgfault_upcall(envid, _pgfault_upcall);

		sys_env_set_status(envid, ENV_RUNNABLE);
		
	} else if(envid == 0){ //child process
		thisenv = &envs[ENVX(sys_getenvid())];
	}

	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
