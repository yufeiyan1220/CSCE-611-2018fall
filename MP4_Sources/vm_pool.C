/*
 File: vm_pool.C
 
 Author:
 Date  :
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "vm_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "simple_keyboard.H"
#include "page_table.H"
/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long  _base_address,
               unsigned long  _size,
               ContFramePool *_frame_pool,
               PageTable     *_page_table) {
	base_address = (_base_address & 0xFFFFF000);
	size = _size/(PageTable::PAGE_SIZE);
	frame_pool = _frame_pool;
	page_table = _page_table;
	region_count = 0;
	//Use 1 page to store regions, so maximum is 512.
	maximum_region_count = (PageTable::PAGE_SIZE)/sizeof(region_node);
	nodes_list = (region_node*)(_base_address);
	//register this vm pool for page table
	page_table->register_pool(this);
    Console::puts("Constructed VMPool object.\n");
}

unsigned long VMPool::allocate(unsigned long _size) {
    int required_pages = _size/(PageTable::PAGE_SIZE);
	if(_size % (PageTable::PAGE_SIZE) != 0)
		required_pages++;

	int region_index = -1;  //allocate the region with this regions_index
	int isNeedNewRegion = 0;
	
	if((region_count + 1) > maximum_region_count){
        Console::puts("Can't add more regions info in the first page of one pool\n");
    }
	else{
		isNeedNewRegion = 1;
	}
	
	//search for regions with size 0.
	if(region_count!=0){
		for(int i =0; i < region_count; i++){
			int res_pages  = 0;
			if( nodes_list[i].size == 0 ){  //means it is not in used in the middle of nodes(not the end)
				res_pages  = (nodes_list[i+1].base_addr - nodes_list[i].base_addr)/(PageTable::PAGE_SIZE);
			}
			if(res_pages  >= required_pages){
				region_index = i;
				break;
			}
		}
	}
	unsigned long start_addr = 0;        
	//if need to allocate with a new node
	if((region_index == -1) && (isNeedNewRegion)){    
	//need to allocate new region nodes
		if(region_count == 0) 
			// first page in pool using for record list
			start_addr = base_address + (PageTable::PAGE_SIZE);  
		else {
			//follow the previous node
			start_addr = nodes_list[region_count-1].base_addr + (nodes_list[region_count-1].size)*(PageTable::PAGE_SIZE);
		}
			
		
		//check for total size of usage
		unsigned long end_addr = (start_addr>>12) + required_pages;  
		//num of pages for the total
		unsigned long end_pool_addr = (base_address>>12) + size; 
		if(end_addr > end_pool_addr){
			Console::puts("Can't allocate new space in this pool\n");
			return 0;
		}
		nodes_list[region_count].base_addr = start_addr;  //add one new node
		nodes_list[region_count].size = required_pages;	
		region_count++;	
	}
	else if((region_index != -1)){  
		//there is avaiable space from size 0 nodes.
		start_addr = nodes_list[region_index].base_addr;
		nodes_list[region_index].size = required_pages;
		
		if(isNeedNewRegion){

			unsigned long tmp_size = nodes_list[region_index+1].base_addr - (start_addr + required_pages*(PageTable::PAGE_SIZE));
			if(tmp_size >= (PageTable::PAGE_SIZE)){   
				//there is still avaiable space in the middle nodes, merge or add a new node with size 0.
				if(nodes_list[region_index + 1].size != 0){
					for(int i = region_count; i > (region_index + 1); i--){
						nodes_list[i].base_addr = nodes_list[i-1].base_addr;
						nodes_list[i].size = nodes_list[i-1].size;
					}
					nodes_list[region_index + 1].base_addr =  start_addr + required_pages*(PageTable::PAGE_SIZE);
					nodes_list[region_index + 1].size = 0;
					region_count++;
				}
				else {
					//merge 
					nodes_list[region_index + 1].base_addr = start_addr + (required_pages + 1)*(PageTable::PAGE_SIZE) ;
				}
			}
		}
	}
	else{   
		//No space to locate new nodes.
		Console::puts("Can't allocate new space in this pool\n");
		return 0;
	}
	return start_addr;
    //Console::puts("Allocated region of memory.\n");
}


void VMPool::release(unsigned long _start_address) {
    //release which region
	int index = -1;  
	//region with this index in list will be removed
	for(int i = 0; i < region_count; i++){
		if(nodes_list[i].base_addr == _start_address){
			index=i;
			break;
		}
	}
	//no match region found
	if(index < 0){
		Console::puts("release illegal address\n");
		return;
	}

	//release pages in this region	
	for (int j = 0; j < nodes_list[index].size; j++)
	{
		(*page_table).free_page(_start_address);
		_start_address = _start_address + PageTable::PAGE_SIZE;
		(*page_table).load(); // flush the TLB after release one page
	}
	nodes_list[index].size = 0; //release this node.
	
	//reconstruct new nodes_list. Merge size = 0's nodes
	int isNextNode0 = 0;
	int isPreNode0 = 0;
	//pre node:
	if((index - 1) >= 0 && nodes_list[index - 1].size == 0){
		isPreNode0 = 1;	
	}
	//next node:
	if((index + 1) < region_count && nodes_list[index + 1].size == 0){
		isNextNode0 = 1;	
	}
	//merge
	if(isPreNode0){
		for(int i = index + 1; i < region_count; i++) {
			nodes_list[i - 1].size = nodes_list[i].size;
			nodes_list[i].base_addr = nodes_list[i].base_addr;
		}
		//make the last one zero;
		nodes_list[region_count - 1].base_addr = 0;
		nodes_list[region_count - 1].size = 0;
		region_count--;
		//now index has become index - 1;
		index--;
	}
	if(isNextNode0){
		
		for(int i = index + 2; i < region_count; i++) {
			nodes_list[i - 1].size = nodes_list[i].size;
			nodes_list[i].base_addr = nodes_list[i].base_addr;
		}
		//make the last one zero;
		nodes_list[region_count - 1].base_addr = 0;
		nodes_list[region_count - 1].size = 0;
		region_count--;
		//now index has become index - 1;
	}
    Console::puts("Released region of memory.\n");
}


bool VMPool::is_legitimate(unsigned long _address) {
	if((base_address <= _address) && (_address <= (base_address + size)))
	{
		return true;
	}
    return false;
    Console::puts("Checked whether address is part of an allocated region.\n");
}

ContFramePool* VMPool::get_frame_pool()
{
	return frame_pool;
}