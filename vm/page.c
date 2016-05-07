#include <hash.h>
#include "page.h"


static unsigned vm_hash_func (const struct hash_elem *e, void *aux);
static bool vm_less_func (const struct hash_elem *a, const struct hash_elem *b);
static bool vm_destroy_func (const struct hash_elem *e, void *aux);

// II. Data Structure for vm_entry
void
vm_init (struct hash *vm)
{
	hash_init (vm, vm_hash_func, vm_less_func, NULL);
}

bool
insert_vme (struct hash *vm, struct vm_entry *vme)
{

}

bool
delete_vme (struct hash *vm, struct vm_entry *vme)
{
	hash_delete (vm, vme);
}

struct vm_entry
*find_vme (void *vaddr)
{

}

void
vm_destroy (struct hash *vm) // I'm not sure about vm_destroy_func & aux
{
	vm->aux = vm;
	hash_destroy (vm, vm_destroy_func);
}

static unsigned
vm_hash_func (const struct hash_elem *e, void *aux)
{

}

static bool
vm_less_func (const struct hash_elem *a, const struct hash_elem *b)
{
	struct vm_entry *va = hash_entry(a, struct vm_entry, elem);
	struct vm_entry *vb = hash_entry(b, struct vm_entry, elem);

	return va->vaddr < vb->vaddr;
}

static bool
vm_destroy_func (const struct hash_elem *e, void *aux)
{	// aux used as hash
	if (hash_delete (aux, e))
		return true;
	else
		return false;
}

// V. Demand paging
bool load_file (void* kaddr, struct vm_entry *vme)
{
	
}