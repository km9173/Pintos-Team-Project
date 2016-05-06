#include <hash.h>
#include "page.h"


static unsigned vm_hash_func (const struct hash_elem *e, void *aux);
static bool vm_less_func (const struct hash_elem *a, const struct hash_elem *b);

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
vm_destroy (struct hash *vm)
{
	hash_destroy (vm, hash_delete); // I'm not sure 
}

// V. Demand paging
bool load_file (void* kaddr, struct vm_entry *vme)
{
	
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

	if (va->vaddr < vb->vaddr)
		return true;
	else
		return false;
}