#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable * PageTable::current_page_table = NULL;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = NULL;
ContFramePool * PageTable::process_mem_pool = NULL;
unsigned long PageTable::shared_size = 0;



void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
	//assert(false);
	kernel_mem_pool = _kernel_mem_pool;
	process_mem_pool = _process_mem_pool;
	shared_size = _shared_size;
	Console::puts("Initialized Paging System\n");
   
}
/*
PageTable::PageTable()
{
	
	//get 1 frame(4kb) to store page directory
	unsigned long page_directory_frame = (*kernel_mem_pool).get_frames(1);
	(*this).page_directory = (unsigned long*) (page_directory_frame * PAGE_SIZE);
	
	
	//get another 1 frame next, 4kb for page table page
	unsigned long getframe_address = kernel_mem_pool->get_frames(1) *PAGE_SIZE;
	//The first directory marked as su, read&write, present.
	((*this).page_directory)[0] = (unsigned long)(getframe_address | 3);
	//page_table = first page table page 
	unsigned long *page_table = (unsigned long*) (page_directory[0] & 0xFFFFF000);
	
	//set up a page table
	// map the first 4MB of memory
	unsigned long address=0; 
	for(int i = 0; i < 1024; i++)
	{
		page_table[i] = address | 3; 
		address = address + 4096; 
	};
	
	//(*this).page_directory[0] |= 0x3;
	for(int i = 1; i < 1024; i++)
	{
		page_directory[i] = 0 | 2; 
	// attribute set to: supervisor level, read/write, not present(010 in binary)
	};
	
	Console::puts("Constructed Page Table object\n");
}

*/
PageTable::PageTable()
{
	//get 1 frame(4kb) to store page directory
	unsigned long page_directory_frame = (*process_mem_pool).get_frames(1);
	(*this).page_directory = (unsigned long*) (page_directory_frame * PAGE_SIZE);
	
	
	//get another 1 frame next, 4kb for page table page
	unsigned long getframe_address = process_mem_pool->get_frames(1) *PAGE_SIZE;
	//The first directory marked as su, read&write, present.
	((*this).page_directory)[0] = (unsigned long)(getframe_address | 3);
	//page_table = first page table page 
	unsigned long *page_table = (unsigned long*) (page_directory[0] & 0xFFFFF000);
	
	//set up a page table
	// map the first 4MB of memory
	unsigned long address=0; 
	for(int i = 0; i < 1024; i++)
	{
		page_table[i] = address | 3; 
		address = address + 4096; 
	};
	
	//(*this).page_directory[0] |= 0x3;
	for(int i = 1; i < 1024; i++)
	{
		// attribute set to: supervisor level, read/write, not present(010 in binary)
		page_directory[i] = 0 | 2; 
	
	}
	
	//mark the last page_directory recursively
	page_directory[1023] = (unsigned long)((*this).page_directory) | 3;
	
	//initial vm_pool_array
	for (int i=0; i < VM_ARRAY_SIZE ; i++){
		vm_pool_array[i] = NULL;
	}
        
	Console::puts("Constructed Page Table object\n");
}
void PageTable::load()
{
	current_page_table = this;
	write_cr3((unsigned long)(*current_page_table).page_directory);
	
	Console::puts("Loaded page table\n");
}

void PageTable::enable_paging()
{
	//assert(false);
	paging_enabled = 1;
	write_cr0(read_cr0() | 0x80000000);
	Console::puts("Enabled paging\n");
}

void PageTable::handle_fault(REGS * _r)
{
	unsigned int error_no = _r-> int_no;
	//assert(false);
	//page fault signal 14
	if(error_no == 14){
		ContFramePool* current_mem_pool;
		VMPool** vm_array = current_page_table->vm_pool_array;
		unsigned long address = read_cr2();
		int vm_index = -1;
		for(int i = 0;i < VM_ARRAY_SIZE; i++)
            if(vm_array[i]!= NULL){
                if (vm_array[i]->is_legitimate(address)){
                    vm_index = i;
					current_mem_pool = current_page_table->vm_pool_array[i]->get_frame_pool();
                    break;
                }
            }
		if (vm_index < 0)
		{
           Console::puts("[Can't Access this Page Fault] INVALID ADDRESS in VM_POOL\n");
		   return;
		}

		
		unsigned long *current_page_directory = (unsigned long *) 0xFFFFF000;
		
		
		//get the first 10 bits for directory
		unsigned long dir_index = address >> 22;
		//get the PTE 
		unsigned long *page_table = (unsigned long *) (0xFFC00000 | (dir_index << 12));
		//if the requested page table is not valid
		if((current_page_directory[dir_index] & 1) != 1){	
			//get a frame for the requested dir
			current_page_directory[dir_index] = (unsigned long)(kernel_mem_pool->get_frames(1)*PAGE_SIZE);
			current_page_directory[dir_index] |= 3;
			//initialize, PTE not assigned yet.
			for(int i = 0; i < 1024; i++){
				page_table[i] = 0 | 2;
			}
		}
		//get the next 10 bit for page table page, 
		unsigned long table_index = ((address>>12) & 0x03FF);
		//page_table = (unsigned long*)(current_page_directory[dir_index] & 0xFFFFF000);
		//assign PTE as the address
		page_table[table_index] = (unsigned long)(process_mem_pool->get_frames(1)*PAGE_SIZE) | 3;
		
	}
	Console::puts("handled page fault\n");
}

void PageTable::register_pool(VMPool * _vm_pool)
{
    int index = -1;
	for (int i = 0; i < VM_ARRAY_SIZE; i++) {
		//find available position for vmpool
		if (vm_pool_array[i]==NULL){
			index = i;
			break;
		}
	}
	if (index >= 0){
		//register pool
		vm_pool_array[index]= _vm_pool;   
		Console::puts("register pool\n");
	}
	else{
		//Array is full
		Console::puts("register VMPool failed\n"); 
	}
}

void PageTable::free_page(unsigned long _page_no) {
	//PDE = |1023|X|Y|00
	//the first 10 bit is the index of PDE
	
    unsigned long dir_index = _page_no >> 22;
	//the next 10 bit is the index of PTE
	unsigned long table_index = (_page_no >> 12) & 0x03FF; 
	//PDE entry from |1023|X|0
	unsigned long* page_table = (unsigned long*)((0xFFC00 | dir_index) << 12);
	if((page_table[table_index]&0x1) == 0x0){
		//valid bit = 0 means this page allocate Virtual memory pool, but not allocate real physical memory pool.
		return; 
	}
    unsigned long frame_number= (page_table[table_index]&0xFFFFF000)>>12 ;
	ContFramePool::release_frames(frame_number);
	//mark as invalid
	page_table[table_index] &=(0xFFFFFFFE);  
    Console::puts("freed page\n");
}

