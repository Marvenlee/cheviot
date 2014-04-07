#include <errno.h>
#include <sys/syscalls.h>
#include <stdlib.h>
#include <string.h>
#include <sys/lists.h>
#include <stdint.h>
#include <sys/debug.h>




// Memory management data structures
// Note we don't know what QBlock we belong to when freeing memory unless
// we are a large block of memory in which case we find it on the hash.
// So can't maintain statistics on a per-block basis.


struct QHeader
{
    size_t prev_size;               // 0 if beginning of block
    size_t size;                    // (1<<0) holds Q_INUSE flag (needs masking)
    LIST_ENTRY (QHeader) q_link;    // Can be part of data area to make header overhead 8-bytes
};


struct QBlock
{
    int type;
    void *addr;
    size_t size;
    LIST_ENTRY (QBlock) link;
};

// Memory management alignment macros

#define ALIGN_UP(val, alignment)                                \
    ((((val) + (alignment) - 1)/(alignment))*(alignment))
#define ALIGN_DOWN(val, alignment)                              \
            ((val) - ((val) % (alignment)))

// Memory management constants

#define SIZEOF_QHEADER      16
#define Q_INUSE             (1<<0)
#define QF_BLOCK_SZ         0x20000
#define QF_MAX_ALLOC_SZ     0x08000
#define QF_GRANULARITY      32
#define QF_SLOTS            64
#define QBLOCKS_TO_ALLOC    512
#define QBLOCK_HASH_SZ      512
#define HASH_SHIFT          12
#define MINIMUM_SPLIT_SZ    64
#define MEMTYPE_ALLOC       0
#define MEMTYPE_QFIT        1
#define SIZE_TO_Q(sz)       ((sz/QF_GRANULARITY) - 1)


// private variables

static struct QBlock *last_allocated_block = NULL;
static LIST (QHeader) qheader_list[QF_SLOTS];
static LIST (QHeader) oddment_list;
static LIST (QBlock) free_qblock_list;
static LIST (QBlock) qblock_hash[QBLOCK_HASH_SZ];

// private function prototypes

static void qcoalesce (void);
static void qcoalesce_block (struct QBlock *blk);
static void qsplit (struct QHeader *qh, size_t size);
static void *qallocblock (size_t sz, int type);
static void qfreeblock (void *mem);
static size_t qmemsize (void *ptr);





// malloc();

void *malloc (size_t size)
{
    int q;
    struct QHeader *qh, *qtail, *best;
    void *mem;
    int t;
    size_t best_size;
    
    size = ALIGN_UP (size, QF_GRANULARITY);

    
    // Perfect Fit then 2 * size + header overhead
    
    if (size <= QF_SLOTS * QF_GRANULARITY)
    {
        q = SIZE_TO_Q(size);
        
        if ((qh = LIST_HEAD (&qheader_list[q])) != NULL)
        {
            LIST_REM_HEAD (&qheader_list[q], q_link);
            qh->size |= Q_INUSE;
            
            mem = (uint8 *)qh + SIZEOF_QHEADER;
            return mem;
        }
        
        q = SIZE_TO_Q(size*2+SIZEOF_QHEADER);
        
        if (q < QF_SLOTS)
        {
            if ((qh = LIST_HEAD (&qheader_list[q])) != NULL)
            {
                LIST_REM_HEAD (&qheader_list[q], q_link);
                qsplit (qh, size);
                qh->size |= Q_INUSE;

                mem = (uint8 *)qh + SIZEOF_QHEADER;
                return mem;
            }
        }
    }
    

    // Best-Fit

    if (size < QF_MAX_ALLOC_SZ)
    {   
        for (t=0; t<2; t++)
        {
            if (t == 1)
                qcoalesce();
                    
            if (size <= QF_SLOTS * QF_GRANULARITY)
            {
                for (q = SIZE_TO_Q(size); q < QF_SLOTS; q++)
                {
                    if ((qh = LIST_HEAD (&qheader_list[q])) != NULL)
                    {
                        LIST_REM_HEAD (&qheader_list[q], q_link);
                        qh->size |= Q_INUSE;

                        mem = (uint8 *)qh + SIZEOF_QHEADER;
                        return mem;
                    }
                }
            }
            
            qh = LIST_HEAD (&oddment_list);
            best = NULL;
            best_size = 0;
                                                
            while (qh != NULL)
            {
                if (qh->size == size)
                {
                    LIST_REM_ENTRY (&oddment_list, qh, q_link);
                    qh->size |= Q_INUSE;
            
            
                    mem = (uint8 *)qh + SIZEOF_QHEADER;
                    return mem;
                }
                
                if ((best == NULL && qh->size >= size)
                    || (qh->size < best_size && qh->size >= size))
                {
                    best = qh;
                    best_size = qh->size;
                }
                
                qh = LIST_NEXT (qh, q_link);
            }
            
            if (best != NULL)
            {
                LIST_REM_ENTRY (&oddment_list, best, q_link);
                qsplit (best, size);
                best->size |= Q_INUSE;

                mem = (uint8 *)best + SIZEOF_QHEADER;
                return mem;

            }
        }
        
                        
        // Allocate a new block of memory
        
        if ((mem = qallocblock (QF_BLOCK_SZ, MEMTYPE_QFIT)) != NULL)
        {
            qh = mem;
            qh->prev_size = 0;
            qh->size = QF_BLOCK_SZ - (SIZEOF_QHEADER * 2);  // HEAD + TAIL (zero length).
                    
            qtail = mem + QF_BLOCK_SZ - SIZEOF_QHEADER;
            qtail->prev_size = QF_BLOCK_SZ - (SIZEOF_QHEADER * 2);
            qtail->size = 0;

            qsplit (qh, size);
            
            qh->size |= Q_INUSE;
            
            mem = (uint8 *)qh + SIZEOF_QHEADER;
            return mem;
        }
    }
    else
    {
        mem = qallocblock (size, MEMTYPE_ALLOC);
        return mem;
    }
    
    return NULL;
}




// free();
// frees a piece of memory allocated using malloc().

void free (void *mem)
{
    struct QHeader *qh;
    struct QBlock *blk;
    int key;
    int q;

    key = ((uintptr_t)mem>>HASH_SHIFT) % QBLOCK_HASH_SZ;

    blk = LIST_HEAD (&qblock_hash[key]);

    while (blk != NULL)
    {
        if (blk->addr == mem)
        {
            LIST_REM_ENTRY (&qblock_hash[key], blk, link);
            
            VirtualFree (mem);
            blk->addr = NULL;
            blk->type = -1;
            
            LIST_ADD_HEAD (&free_qblock_list, blk, link);   
            return;
        }
        
        blk = LIST_NEXT (blk, link);
    }


    qh = (struct QHeader *)((uint8 *)mem - SIZEOF_QHEADER);
    
    qh->size = qh->size & ~Q_INUSE;
    
    if (qh->size <= QF_SLOTS * QF_GRANULARITY)
    {
        q = SIZE_TO_Q (qh->size);
        LIST_ADD_HEAD (&qheader_list[q], qh, q_link);
    }
    else
    {
        LIST_ADD_HEAD (&oddment_list, qh, q_link);
    }
}   




// realloc();
// Reallocates a block of memory, either expanding or contracting
// it. Copies the origianl data to the newly allocated region
// and frees the old allocation.

void *realloc (void *ptr, size_t size)
{
    size_t old_sz;
    void *nptr;
    
    
    if (ptr == NULL)
        return malloc (size);
    else if (size == 0)
    {
        free (ptr);
        return NULL;
    }
    else
    {
        old_sz = qmemsize (ptr);
        
        nptr = malloc (size);
                
        if (nptr != NULL)
        {
            memcpy (nptr, ptr, old_sz);
            free(ptr);
            return nptr;
        }
    }

}




// calloc();
// Allocates and clears an array of nelements of size 'elsize'.
 
void *calloc (size_t nelem, size_t elsize)
{
    void *mem;

    if ((mem = malloc (nelem * elsize)) != NULL)
        memset (mem, 0, nelem * elsize);

    return mem;
}




// **************************************************************************
// Private functions called by malloc() and free()
// **************************************************************************

// qsplit();

static void qsplit (struct QHeader *qh, size_t size)
{
    struct QHeader *mid, *next;
    size_t total_size;
    int q;

    if (qh->size > size + MINIMUM_SPLIT_SZ)
    {
        total_size = qh->size;
        mid = (struct QHeader *)((uint8 *)qh + SIZEOF_QHEADER + size);
        
        mid->size = total_size - size - SIZEOF_QHEADER;
        mid->prev_size = size;
        
        if (mid->size <= QF_SLOTS * QF_GRANULARITY)
        {
            q = SIZE_TO_Q (mid->size);
            LIST_ADD_HEAD (&qheader_list[q], mid, q_link);
        }   
        else
        {
            LIST_ADD_HEAD (&oddment_list, mid, q_link);
        }
                        
        next = (struct QHeader *)((uint8 *)qh + SIZEOF_QHEADER + qh->size);
        next->prev_size = mid->size;
        
        qh->size = size;
        return;
    }
}




// qcoalesce();

static void qcoalesce (void)
{
    int t;
    struct QBlock *qblk, *next_qblk;
    
    
    for (t=0; t < QBLOCK_HASH_SZ; t++)
    {
        qblk = LIST_HEAD (&qblock_hash[t]);
        
        while (qblk != NULL)
        {
            next_qblk = LIST_NEXT (qblk, link);
            
            if (qblk->type == MEMTYPE_QFIT)
            {
                qcoalesce_block (qblk);
            }
            
            qblk = next_qblk;
        }
    }
}




// qcoalesce_block();

static void qcoalesce_block (struct QBlock *blk)
{
    struct QHeader *head, *qh;
    size_t new_size;
    int q;


    head = blk->addr;
    
    while (head->size != 0)
    {   
        if ((head->size & Q_INUSE) == 0)
        {
            qh = (struct QHeader *)((uint8*)head + head->size + SIZEOF_QHEADER);
            
            new_size = head->size;
            
            while (qh->size != 0 && (qh->size & Q_INUSE) == 0)
            {
                new_size += qh->size;
            
                qh = (struct QHeader *)((uint8 *)qh + (qh->size & ~ Q_INUSE) + SIZEOF_QHEADER);
                
                if (qh->size <= QF_SLOTS * QF_GRANULARITY)
                {
                    q = SIZE_TO_Q (qh->size);
                    LIST_REM_ENTRY (&qheader_list[q], qh, q_link);
                }
                else
                {
                    LIST_REM_ENTRY (&oddment_list, qh, q_link);
                }   
            }
            
            
            if (new_size != head->size)
            {
                if (head->size <= QF_SLOTS * QF_GRANULARITY)
                {
                    q = SIZE_TO_Q (head->size);
                    LIST_REM_ENTRY (&qheader_list[q], head, q_link);
                }
                else
                {
                    LIST_REM_ENTRY (&oddment_list, head, q_link);
                }                   
            
                head->size = new_size & ~Q_INUSE;
                
                if (head->size <= QF_SLOTS * QF_GRANULARITY)
                {
                    q = SIZE_TO_Q (head->size);
                    LIST_ADD_HEAD (&qheader_list[q], head, q_link);
                }
                else
                {
                    LIST_ADD_HEAD (&oddment_list, head, q_link);
                }
            }
        }
        else
        {
            head = (struct QHeader *)((uint8*)head + (head->size & ~Q_INUSE) + SIZEOF_QHEADER);
        }
    }
    
        
    head = blk->addr;
    
    if (head->size == QF_BLOCK_SZ - 2 * SIZEOF_QHEADER)
    {
        LIST_REM_ENTRY (&oddment_list, head, q_link);
        qfreeblock (blk->addr);
    }
}
    
    


// qmemsize();
// Returns the size of the memory allocation pointed to by 'mem'.
// First checks the hash table of blocks to see if it is a large
// allocation. If it is then the size can be obtained from the
// QBlock structure.
// Otherwise the size is obtained from the QHeader structure
// that precedes 'mem'.

static size_t qmemsize (void *mem)
{
    size_t memsize;
    struct QHeader *qh;
    struct QBlock *blk;
    int key;
    int q;


    key = ((uintptr_t)mem>>HASH_SHIFT) % QBLOCK_HASH_SZ;

    blk = LIST_HEAD (&qblock_hash[key]);

    while (blk != NULL)
    {
        if (blk->addr == mem)
        {
            return blk->size & ~Q_INUSE;
        }
        
        blk = LIST_NEXT (blk, link);
    }

    qh = (struct QHeader *)((uint8 *)mem - SIZEOF_QHEADER);
    return qh->size & ~Q_INUSE;
}




// qallocblock();
// Allocates a large block of memory from the operating system,
// allocates an associated QBlock structure and adds it to
// a hash table for quick lookup.

static void *qallocblock (size_t sz, int type)
{
    void *mem;
    struct QBlock *blk, *new_qblocks;
    int key;
    int t;
    

    // Find some way of allocating a smallish table on startup to avoid virtualalloc
    
    
    if (LIST_HEAD (&free_qblock_list) == NULL)
    {
        if ((new_qblocks = VirtualAlloc (NULL, QBLOCKS_TO_ALLOC * sizeof (struct QBlock), PROT_READWRITE)) == NULL)
        {
            return NULL;
        }
        
        for (t=0; t < QBLOCKS_TO_ALLOC; t++)
        {
            LIST_ADD_TAIL(&free_qblock_list, &new_qblocks[t], link);
        }
    }
    
    blk = LIST_HEAD (&free_qblock_list);
    
    if ((mem = VirtualAlloc (NULL, sz, PROT_READWRITE)) == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
        
    LIST_REM_HEAD (&free_qblock_list, link);
    key = (int)mem % QBLOCK_HASH_SZ;
    LIST_ADD_HEAD (&qblock_hash[key], blk, link);
    blk->addr = mem;
    blk->type = type;
    last_allocated_block = blk;
    
    return blk->addr;
}




// qfreeblock();
// Frees a block of memory obtained from the operating system.
// Removes it from the qblock hash table.

static void qfreeblock (void *mem)
{
    struct QBlock *blk;
    int key;
    
    key = ((int)mem>>HASH_SHIFT) % QBLOCK_HASH_SZ;

    blk = LIST_HEAD (&qblock_hash[key]);

    while (blk != NULL)
    {
        if (blk->addr == mem)
        {
            LIST_REM_ENTRY (&qblock_hash[key], blk, link);
            
            VirtualFree(mem);
            blk->addr = NULL;
            blk->type = -1;
            
            if (blk == last_allocated_block)
                last_allocated_block = NULL;
            
            LIST_ADD_HEAD (&free_qblock_list, blk, link);   
            return;
        }
        
        blk = LIST_NEXT (blk, link);
    }
}








/*
 * Reentrant memory management functions needed by Newlib.
 */

void *_malloc_r (struct _reent *r, size_t size)
{
    return malloc (size);
}


void _free_r (struct _reent *r, void *ptr)
{
    free (ptr);
}


void *_calloc_r (struct _reent *r, size_t nelem, size_t elsize)
{
    return calloc (nelem, elsize);
}


void *_realloc_r (struct _reent *r, void *ptr, size_t size)
{
    return realloc (ptr, size);
}


