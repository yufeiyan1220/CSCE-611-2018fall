/*
 File: ContFramePool.C
 
 Author:
 Date  : 
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"


/*
	4 first elements in bitmap stand for the free/not free, the rest stand for header(if not free)
	eg. 01110111 = the first not free, the rest are free; and the first is the header.
*/
//used as a mark
Node *pool_list_head;

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no,
                             unsigned long _n_info_frames)
{
	//assert(_n_frames <= FRAME_SIZE * 4);
	nFreeFrames = _n_frames;   
    base_frame_no = _base_frame_no; 
    nframes = _n_frames;       
    info_frame_no = _info_frame_no; 
	n_info_frames = _n_info_frames;
	
	if(info_frame_no == 0) {
        bitmap = (unsigned char *) (base_frame_no * FRAME_SIZE);
    } else {
        bitmap = (unsigned char *) (info_frame_no * FRAME_SIZE);
    }
	
	// Number of frames must be "fill" the bitmap!
	//assert ((nframes % 4 ) == 0);
    // Everything ok. Proceed to mark all bits in the bitmap
    for(int i = 0; i * 4 < _n_frames; i++) {
        bitmap[i] = 0xFF;
    }
    
    // Mark the first '_n_info_frames' frame as being used if it is being used
    
	unsigned char mask = 0x80;
	//bitmap begin from info_frame_no
	/*
	for (int i = _info_frame_no; i < _info_frame_no + _n_info_frames; i++) {
		int index_info = (i - _info_frame_no) / 4;
		mask = mask >> (i - _info_frame_no) % 4;
		bitmap[index_info] ^= mask;
		
	}
    nFreeFrames -= _n_info_frames;
	*/
	
	if(info_frame_no == 0){
		bitmap[0] = 0x7F;
		nFreeFrames--;
	}
	
	Node* n_node;

	(*n_node).currentPool = this;	
	Node* headNext = (*pool_list_head).next;
	if(headNext == NULL ){
		(*pool_list_head).next =  n_node;
	}else{
		(*n_node).next = (*pool_list_head).next;
		(*pool_list_head).next = n_node;
	}		
    Console::puts("Frame Pool initialized\n");
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    // find the first consective _n_frames available to get 
	assert(_n_frames <= nFreeFrames);
	unsigned char mask_1;
	unsigned int index_bitmap;
	unsigned long header = base_frame_no;
	int count = 0;
	//find from whole scape
	for(int i = base_frame_no; i < nframes + base_frame_no; i++) {
		index_bitmap = (i - base_frame_no) / 4;
		mask_1 = 0x80 >> (i - base_frame_no) % 4;
		//find a free frame
		if(mask_1 & bitmap[index_bitmap] != 0) {
			// no consective before, take head from it
			if(count == 0) {header = i;}		
			count++;
			//if the consective free frame == _n_frames, take them
			if(count == _n_frames) {
				//from the header to this.
				for(int j = header; j < header + _n_frames; j++) {
					//mark the jth as not free
					unsigned int index_bitmap_2 = (j - base_frame_no) / 4;
					unsigned char mask_2 = 0x80 >> (j - base_frame_no) % 4;
					bitmap[index_bitmap_2] ^= mask_2;
					//for header, marked as header
					if(j == header){
						unsigned char mask_head = 0x08 >> (header - base_frame_no) % 4;
						bitmap[index_bitmap_2] ^= mask_head;
					}
				}
				nFreeFrames -= _n_frames;
				return header;
			}
		}
		// not free, counting restart from 0
		else {
			count = 0;
		}
	}
	return 0;
}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    for(int i = _base_frame_no; i < _base_frame_no + _n_frames; i++){
        mark_inaccessible(i);
    }
}

void ContFramePool::mark_inaccessible(unsigned long _frame_no)
{
    // Let's first do a range check.
    assert ((_frame_no >= base_frame_no) && (_frame_no < base_frame_no + nframes));
    
    unsigned int bitmap_index = (_frame_no - base_frame_no) / 4;
    unsigned char mask = 0x80 >> ((_frame_no - base_frame_no) % 4);
    
    // Is the frame being used already?
    assert((bitmap[bitmap_index] & mask) != 0);
    
    // Update bitmap
    bitmap[bitmap_index] ^= mask;
    nFreeFrames--;
}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    //find the pool first!
	Node *temp_node = (*pool_list_head).next;
	ContFramePool current_pool = *((*temp_node).currentPool);

	while(temp_node != NULL) {
		current_pool = *((*temp_node).currentPool);
		//_first_frame_no is between the boundaries of a frame_pool, assign that pool
		if((_first_frame_no >= current_pool.base_frame_no && _first_frame_no < current_pool.base_frame_no+ current_pool.nframes)) 
			break;
		 temp_node = (*temp_node).next;
	}
	//release it!
	//releae the head(if true)
	unsigned char mask_re_head = 0x08 >> (_first_frame_no - current_pool.base_frame_no) % 4 ;
	unsigned int index_bitmap_re_head = (_first_frame_no - current_pool.base_frame_no) / 4;
	current_pool.bitmap[index_bitmap_re_head] |= mask_re_head;
	//release unavailable for free
	unsigned char mask_re;
	unsigned int index_bitmap_re;
	for(int i = _first_frame_no; i < current_pool.base_frame_no + current_pool.nframes; i++) {
		unsigned int diff = i - current_pool.base_frame_no;
		mask_re = 0x80 >> diff % 4;
		index_bitmap_re = diff / 4;
		//when this frame is the header of the next group, break 
		if(current_pool.bitmap[index_bitmap_re] & (0x08 >> diff % 4) == 0) break;
		//when this frame is the free, break  0100 1100
		if(current_pool.bitmap[index_bitmap_re] & (0x80 >> diff % 4) != 0) break;
		
		current_pool.bitmap[index_bitmap_re] |= mask_re;
	}
    
}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    
    return (_n_frames / (4096 * 4) + (_n_frames % (4096 * 4) > 0 ? 1 : 0));
}
