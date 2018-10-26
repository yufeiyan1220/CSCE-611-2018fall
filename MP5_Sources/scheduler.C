/*
 File: scheduler.C
 
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

#include "scheduler.H"
#include "thread.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "simple_keyboard.H"

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
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/
Scheduler::Scheduler() {
	head->next = NULL;
	head->prev = NULL;
	head->thread = NULL;
	tail->next = NULL;
	tail->prev = NULL;
	tail->thread = NULL;
	thread_count = 0;
	Console::puts("Constructed Scheduler.\n");
}

void Scheduler::yield() {
	//get the next running node and set the next as new head.
	if(thread_count == 0) return;
	ListNode * next_node = head->next;
	ListNode * running = head;
	Thread * next_thread = running->thread;
	head->next = NULL;
	next_node->prev = NULL;
	
	thread_count--;
	head = next_node;
	//switching thread 
	
	Thread::dispatch_to(next_thread);
}

void Scheduler::resume(Thread * _thread) {
	//enable interrupts
	Machine::enable_interrupts();
	add(_thread);
}

void Scheduler::add(Thread * _thread) {
	ListNode * temp = new ListNode();
	temp->thread = _thread;
	temp->prev = NULL;
	temp->next = NULL;
	if(thread_count == 0) {
		head = temp;
		tail = temp;
	}
	else {
		tail->next = temp;
		temp->prev = tail;
		tail = tail->next;
	}
	thread_count++;
}

void Scheduler::terminate(Thread * _thread) {
	if(thread_count == 0) return;
	//find the ListNode of _thread
	ListNode * temp = head;
	for(int i = 0; i < thread_count; i++) {
		if(temp->thread == _thread) break;
		temp = temp->next;
	}
	
	if(temp == NULL) return; 
	//we reach the end of queue, Remove it
	if(temp->next == NULL) {
		temp->prev->next = NULL;
		tail = temp->prev;
	}
	else if(temp == head) {
		temp = head->next;
		temp->prev = NULL;
		head->next = NULL;
		delete head;
		head = temp;
	}
	else {
	//thread is in the middle of queue.
		temp->prev->next = temp->next;
		temp->next->prev = temp->prev;
		delete temp;
	}
	thread_count--;
	/*
	Console::puts("[thread count is: "); Console::puti(thread_count); Console::puts("]\n");*/
}
ListNode * Scheduler::current_head() {
	return head;
}