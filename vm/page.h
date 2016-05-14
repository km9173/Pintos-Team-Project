#include <hash.h>

#define	VM_BIN	  0
#define	VM_FILE	  1			// Memory mapped file
#define	VM_ANON	  2			// Swapping

struct vm_entry {
  uint8_t type;				// Type of VM_BIN, FILE, ANON
  void *vaddr;				// Virtual address
  bool writable;			// flag Writable to address

  bool is_loaded;			// flag Physical memory
  struct file* file;		// Mapped file with virtual address

  /* Memory Mapped File */
  struct list_elem mmap_elem;

  size_t offset;			// File offset to read
  size_t read_bytes;		// Written data size of page
  size_t zero_bytes;		// Remain byte to fill

  /* Swapping */
  size_t swap_slot;			// Swap slot

  struct hash_elem elem;	// Hash table element
};

// II. Data Structure for vm_entry
void vm_init (struct hash *vm);
bool insert_vme (struct hash *vm, struct vm_entry *vme);
bool delete_vme (struct hash *vm, struct vm_entry *vme);
struct vm_entry *find_vme (void *vaddr);
void vm_destroy (struct hash *vm);

// V. Demand paging
bool load_file (void* kaddr, struct vm_entry *vme);
