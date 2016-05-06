#include "page.h"

// II. Data Structure for vm_entry
void
vm_init (struct hash *vm) {

}

static unsigned
vm_hash_func (const struct hash_elem *e, void *aux) {

}

static bool
vm_less_func (const struct hash_elem *a, const struct hash_elem *b) {

}

bool
insert_vme (struct hash *vm, struct vm_entry *vme) {

}

bool
delete_vme (struct hash *vm, struct vm_entry *vme) {

}

struct vm_entry
*find_vme (void *vaddr) {

}

void
vm_destroy (struct hash *vm) {

}

// V. Demand paging
bool load_file (void* kaddr, struct vm_entry *vme) {
	
}