/**
 * Buddy Allocator
 *
 * For the list library usage, see http://www.mcs.anl.gov/~kazutomo/list/
 */

/**************************************************************************
 * Conditional Compilation Options
 **************************************************************************/
#define USE_DEBUG 0

/**************************************************************************
 * Included Files
 **************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "buddy.h"
#include "list.h"

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define MIN_ORDER 12//4kb min size(page size)
#define MAX_ORDER 20//1mg total size

#define PAGE_SIZE (1<<MIN_ORDER) // page size = 4kb
/* page index to address */
#define PAGE_TO_ADDR(page_idx) (void *)(((page_idx)*PAGE_SIZE) + g_memory)//returns a pointer to the memory from page index

/* address to page index */
#define ADDR_TO_PAGE(addr) ((unsigned long)(((void *)(addr)) - (void *)g_memory) / PAGE_SIZE)//returns a page index from a mem pointer

/* find buddy address */
#define BUDDY_ADDR(addr, o) (void *)((((unsigned long)(addr) - (unsigned long)g_memory) ^ (1<<(o))) + (unsigned long)g_memory)//\ //returns a pointer to the memory's buddy.(args = address,order)
									 



#if USE_DEBUG == 1
#  define PDEBUG(fmt, ...) \
	fprintf(stderr, "%s(), %s:%d: " fmt,			\
		__func__, __FILE__, __LINE__, ##__VA_ARGS__)
#  define IFDEBUG(x) x
#else
#  define PDEBUG(fmt, ...)
#  define IFDEBUG(x)
#endif


/**************************************************************************
 * Public Types
 **************************************************************************/
typedef struct {
	struct list_head list;//the node of the linked list for this page/ page's node in the linked list (usually in free_area).
	/* TODO: DECLARE NECESSARY MEMBER VARIABLES */
	//add pointer to loctaion in g_memory
	int index;
	char *mem;//pointer to location in g_memory
	//may want to put the current page's order...(or may not.)
	int order;//current 

} page_t;

/**************************************************************************
 * Global Variables
 **************************************************************************/
/* free lists*/
struct list_head free_area[MAX_ORDER+1];//array of linked lists of free area(one linked list for each order(0-11 not used due to MIN_ORDER = 12))//linked lists may not be very long.

/* memory area */
char g_memory[1<<MAX_ORDER];//actual/raw memory in bytes (to be returned as (void *))

/* page structures */
page_t g_pages[(1<<MAX_ORDER)/PAGE_SIZE];//array of pages (page_t) (in g_memory?)//page = mininum size unit(may be 4 bytes for example.)

/**************************************************************************
 * Public Function Prototypes
 **************************************************************************/

/**************************************************************************
 * Local Functions
 **************************************************************************/

/**
 * Initialize the buddy system
 */
void buddy_init()
{
	int i;
	int n_pages = (1<<MAX_ORDER) / PAGE_SIZE;//number of pages
	for (i = 0; i < n_pages; i++) {
		/* TODO: INITIALIZE PAGE STRUCTURES */
		//point pointers to g_memory
		g_pages[i].index = i;
		g_pages[i].mem = PAGE_TO_ADDR(i);
		g_pages[i].order = -1;//****************************************************
		
	}

	/* initialize freelist */
	for (i = MIN_ORDER; i <= MAX_ORDER; i++) {//from min to max order,[12 to 20]
		INIT_LIST_HEAD(&free_area[i]);//init next and prev pointers of free area.(init free area lists)
	}
	g_pages[0].order = MAX_ORDER;//

	/* add the entire memory as a freeblock */
	list_add(&g_pages[0].list, &free_area[MAX_ORDER]);//add first page (o) of g_pages to the free area of max order
}

//size to min order block size needed
int sizeToOrder(int size){
	int o;
	for (o = MIN_ORDER; o <= MAX_ORDER; o++) {
		if ((1<<o) >= size){
			return o;
		}
	}

	return -1;//order cannot be high enough.
}
/*order of things that need to happen:
 find the next largest free_area above order needed
 breakdown the next Largest free_area(found above) to two smaller ones, one size smaller(may or may not be the correct size yet)
 	find that page's (new) buddy(cause its one size smaller now and has a buddy)
 	add that page's buddy to the free area
 	see if that origional pages is now of the order needed, if so, return it, else break it again.



*/
void breakdown(page_t* page,int currentOrder,int orderNeeded){
if(currentOrder == orderNeeded){
	return;
}
	page_t* buddy = &g_pages[ADDR_TO_PAGE(BUDDY_ADDR(page->mem,currentOrder-1))];//put back to free mem.
	buddy->order = currentOrder-1;
	list_add(&(buddy->list),&free_area[currentOrder-1]);//adds buddy to free area
	breakdown(page,currentOrder-1,orderNeeded);
}
/**
 * Allocate a memory block.
 *
 * On a memory request, the allocator returns the head of a free-list of the
 * matching size (i.e., smallest block that satisfies the request). If the
 * free-list of the matching block size is empty, then a larger block size will
 * be selected. The selected (large) block is then splitted into two smaller
 * blocks. Among the two blocks, left block will be used for allocation or be
 * further splitted while the right block will be added to the appropriate
 * free-list.
 *
 * @param size size in bytes
 * @return memory block address
 */
void *buddy_alloc(int size)
{
	/* TODO: IMPLEMENT THIS FUNCTION */
	int orderNeeded = sizeToOrder(size);//get order size.
	//see if there is a open block of (exact) size orderNeeded.
	int i;
	for (i = orderNeeded; i<= MAX_ORDER; i++){ //i = current order we are looking at.
		if(!list_empty(&free_area[i])){//smallest page entry that is free.
			//found it
			page_t *pg = list_entry(free_area[i].next,page_t,list);//get page., need to remove from free_area before breakdown.
			
			list_del_init(&(pg->list));
			breakdown(pg, i, orderNeeded);//pg's of the right size. and its buddy's(if we broke any blocks down, are reallocated to free_area)
			pg->order = orderNeeded;
			return ((void*)(pg->mem));
		}

	}
	//else return null as there is not enough room
	return NULL;
}

/**
 * Free an allocated memory block.
 *
 * Whenever a block is freed, the allocator checks its buddy. If the buddy is
 * free as well, then the two buddies are combined to form a bigger block. This
 * process continues until one of the buddies is not free.
 *
 * @param addr memory block address to be freed
 */
void buddy_free(void *addr)//need to check to not free a already freed page/block.
{
	page_t * pg = &g_pages[ADDR_TO_PAGE(addr)];
	int i;
	int currentOrder=pg->order;
	if((currentOrder<MIN_ORDER) || (currentOrder>MAX_ORDER)){
		printf("Error: currentOrder out of bounds! currentOrder=%d",currentOrder);
		while(1);
	}   
	//if currentOrder == Maxorder, just add to free_area at top level.
	for(i = currentOrder; i<MAX_ORDER; i++){//for however far up we can go
		//get the buddy,(if there is one!(toplevel does not have a buddy, and just needs to go back to free.))
		page_t* buddy = &g_pages[ADDR_TO_PAGE(BUDDY_ADDR(pg->mem,i))];
		char freed = 0;
		struct list_head *pos;
		list_for_each(pos,&free_area[i]){

			if(list_entry(pos,page_t,list)==buddy){//if we find buddy in free area list,
			//if(pos == ){
				freed = 1;
			}
		}
		if (!freed){//if cant free buddy, break
			break;
		}

		list_del_init(&buddy->list);//remove from the free area list.
		//remove buudy from free list, 
		//go higher.
		if(buddy<pg){
			pg = buddy;
		}
	}
	pg->order = i;
	list_add(&pg->list,&free_area[i]);
	//now add page to free area at level i.

	/* TODO: IMPLEMENT THIS FUNCTION */
}

/**
 * Print the buddy system status---order oriented
 *
 * print number of free pages in each order.
 */
void buddy_dump()
{
	int o;
	for (o = MIN_ORDER; o <= MAX_ORDER; o++) {
		struct list_head *pos;
		int cnt = 0;
		list_for_each(pos, &free_area[o]) {
			cnt++;
		}
		printf("%d:%dK ", cnt, (1<<o)/1024);
	}
	printf("\n");
}
