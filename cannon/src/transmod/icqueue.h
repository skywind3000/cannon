/**********************************************************************
 *
 * icqueue.h
 *
 * NOTE: This is a C version queue operation wrapper
 * By Linwei 5/1/2005
 *
 **********************************************************************/

#ifndef __I_CQUEUE_H__
#define __I_CQUEUE_H__

/**********************************************************************
 * Define a node in queue                                             *
 **********************************************************************/
struct IVQNODE
{
	struct IVQNODE *prev;
	struct IVQNODE *next;
	void *queue;
	void *udata;
};

/**********************************************************************
 * Define the hole queue                                              *
 **********************************************************************/
struct IVQUEUE
{
	struct IVQNODE *head;
	struct IVQNODE *tail;
	long nodecnt;
	long reserve;
};


#ifdef __cplusplus
extern "C" {
#endif


/**********************************************************************
 * Queue macro                                                        *
 **********************************************************************/

/* init the queue struct   */
#define IV_INITQUEUE(q) do { \
			(q)->head = 0; (q)->tail = 0; \
		}	while (0)

/* add n to the queue head */
#define IV_HEADADD(q, n) do { \
			(n)->prev = 0; (n)->next = (q)->head; \
			if ((q)->head) (q)->head->prev = (n); \
			(q)->head = (n); \
			if ((q)->tail == 0) (q)->tail = (n); \
		}	while (0)

/* add n to the queue tail */
#define IV_TAILADD(q, n) do { \
			(n)->next = 0; (n)->prev = (q)->tail; \
			if ((q)->tail) (q)->tail->next = (n); \
			(q)->tail = (n); \
			if ((q)->head == 0) (q)->head = (n); \
		}	while (0)

/* remove node from queue  */
#define IV_REMOVE(q, n) do { \
			if ((n)->prev) (n)->prev->next = (n)->next; \
			else (q)->head = (n)->next; \
			if ((n)->next) (n)->next->prev = (n)->prev; \
			else (q)->tail = (n)->prev; \
		}	while (0)

/* insert s after node d   */
#define IV_INSERTA(q, d, s) do { \
			(s)->prev = (d); \
			(s)->next = (d)->next; \
			if ((d)->next != 0) (d)->next->prev = (s); \
			else (q)->tail = (s); \
			(d)->next = s; \
		}	while (0)

/* insert s before node d  */
#define IV_INSERTB(q, d, s) do { \
			(s)->next = (d); \
			(s)->prev = (d)->prev; \
			if ((d)->prev != 0) (d)->prev->next = (s); \
			else (q)->head = (s); \
			(d)->prev = s; \
		}	while (0)



/**********************************************************************
 * Queue function                                                     *
 **********************************************************************/

/**
 * @brief: init and prepare a queue struct  
 * @param: queue the queue to be init
 */
void iv_queue(struct IVQUEUE *queue);


/** 
 * @brief: init and prepare a queue node 
 * @param: node the node to be init
 */
void iv_qnode(struct IVQNODE *node, const void *udata);


/**
 * @brief: add a node to the head 
 * @param: queue the queue to be added to 
 * @param: node the node to be added
 */
void iv_headadd(struct IVQUEUE *queue, struct IVQNODE *node);


/** 
 * @brief: add a node to the tail 
 * @param: queue the queue to be added to 
 * @param: node the node to be added
 */
void iv_tailadd(struct IVQUEUE *queue, struct IVQNODE *node);


/**
 * @brief: insert src node before(or after) dest node 
 * @param: dest the destination node
 * @param: src the source node
 * @param: isafter insert src before dest when 'isafter' is zero
 */
void iv_insert(struct IVQNODE *dest, struct IVQNODE *src, int isafter);


/**
 * @brief: remove a node from a queue 
 * @param: queue the queue containing the node
 */
void iv_remove(struct IVQUEUE *queue, struct IVQNODE *node);


/**
 * @brief: pop a node from the head of the queue 
 * @brief: queue the queue struct
 * @return: first node from queue or 0 for queue empty
 */
struct IVQNODE *iv_headpop(struct IVQUEUE *queue);


/**
 * @brief: pop a node from the tail of the queue 
 * @brief: queue the queue struct
 * @return: last node from queue or 0 for queue empty
 */
struct IVQNODE *iv_tailpop(struct IVQUEUE *queue);


#ifdef __cplusplus
}
#endif

#endif


