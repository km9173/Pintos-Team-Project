#include "page.h"
#include "kernel/hash.h"
#include "filesys/file.h"
#include "debug.h"

static unsigned vm_hash_func (const struct hash_elem *e, void *aux UNUSED);
static bool vm_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
static void vm_destroy_func (struct hash_elem *e, void *aux);

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
  hash_delete (vm, &vme->elem);
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
vm_hash_func (const struct hash_elem *e, void *aux UNUSED)
{

}

static bool
vm_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct vm_entry *va = hash_entry(a, struct vm_entry, elem);
  struct vm_entry *vb = hash_entry(b, struct vm_entry, elem);

  return va->vaddr < vb->vaddr;
}

static void
vm_destroy_func (struct hash_elem *e, void *aux)
{	// aux used as hash
  hash_delete (aux, e);
}

// V. Demand paging
bool load_file (void* kaddr, struct vm_entry *vme)
{
  off_t size = file_read_at (vme->file, kaddr, vme->read_bytes, vme->offset);
  bool success = false;
  size_t i;

  if (size > 0)
  {
    for (i = 0; i < vme->zero_bytes; i++)
      *(char *)(kaddr + size + i) = 0;
    success = true;
  }
  return success;
}