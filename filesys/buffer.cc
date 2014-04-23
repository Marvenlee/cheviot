#include <sys/types.h>
#include <sys/syscalls.h>
#include <sys/lists.h>
#include <sys/debug.h>
#include <interfaces/block.h>
#include <interfaces/filesystem.h>
#include <string.h>
#include <stdlib.h>
#include "filesystem.h"




/*
 * Fixed sized block cache.  See Maurice Bach book on the Design of the Unix
 * Operating System for details of the algorithm.
 *
 * Needs to be converted to a file segment cache instead of a physical disk
 * block cache.  Use a fixed size file segment cache, perhaps each segment is
 * 64k, and cache a fixed number of them until we add memory management
 * notifications.  Later add different sizes for more efficiency.
 *
 * The layering of the file system will look something like..
 *
 * Client, Filesys, File System Translator and Block Device Driver
 *
 * The file segments will be passed as message between all 4 layers/processes.
 * 
 * Make use of segment-change counter 
 *
 *
 */







/*
 * GetBlk();
 *
 */

struct Buf *GetBlk (struct BlockDev *bdev, uint64 cluster_nr)
{
	struct Buf *buf;
	
	
	KAssert (bdev != NULL);
	
	while (1)
	{	
		if (bdev->flags & BDF_ABORT)
			return NULL;
		
		if ((buf= FindBlk (bdev, cluster_nr)) != NULL)
		{
			if (buf->flags & B_BUSY)
			{
				tasksleep (&buf->rendez);
				continue;
			}
			
			buf->flags |= B_BUSY;
			LIST_REM_ENTRY (&buf_avail_list, buf, free_link);

			return buf;	
		}		
		else
		{
			if ((buf = LIST_HEAD (&buf_avail_list)) == NULL)
			{
				tasksleep (&buf_list_rendez);
				continue;
			}
			
			LIST_REM_HEAD (&buf_avail_list, free_link);			
			buf->flags |= B_BUSY;
			
			
			if (buf->flags & B_DELWRI)
			{
				buf->write_offset = 0;
				buf->write_size = cluster_size;
				buf->flags = (buf->flags | B_WRITE | B_WRITECLUSTER | B_ASYNC) & ~(B_READ | B_DELWRI);
				Strategy (buf);
				continue;
			}
			
			if (buf->flags & B_VALID)
			{
				LIST_REM_ENTRY (&buf_hash[buf->cluster_nr % NR_HASH], buf, lookup_link);
			}
						
			buf->flags &= ~B_VALID;
			buf->blockdev = bdev;
			buf->cluster_nr = cluster_nr;
			LIST_ADD_HEAD (&buf_hash[buf->cluster_nr % NR_HASH], buf, lookup_link);
			return buf;
		}
	}
}




/*
 * FindBlk();
 */
 
struct Buf *FindBlk (struct BlockDev *bdev, uint64 cluster_nr)
{
	struct Buf *buf;
	
	buf= LIST_HEAD(&buf_hash[cluster_nr % NR_HASH]);
	
	while (buf!= NULL)
	{
		if (buf->blockdev == bdev && buf->cluster_nr == cluster_nr)
			return buf;
		
		buf= LIST_NEXT(buf, lookup_link);
	}
	
	return NULL;
}




/*
 * BRead();
 *
 * Searches for the cluster in the clusterfer cache, if not present allocates
 * a cluster and reads the data from disk.
 */
 
struct Buf *BRead (struct BlockDev *bdev, uint64 cluster_nr)
{
	struct Buf *buf;


	buf= GetBlk (bdev, cluster_nr);
	
	if (buf->flags & B_VALID)
	{
		buf->addr = (uint8 *)buf->cluster_buf;
		return buf;
	}
	
	buf->flags = (buf->flags | B_READ) & ~(B_WRITE | B_ASYNC);
	Strategy(buf);
	
	while (!(buf->flags & B_IODONE))
		tasksleep (&buf->rendez);
	
	buf->flags &= ~B_IODONE;
	
	if (buf->flags & B_ERROR)
	{
		BRelse(buf);
		return NULL;
	}
	
	buf->flags = (buf->flags | B_VALID) & ~B_READ;
	buf->addr = (uint8 *)buf->cluster_buf;
	buf->write_offset = 0;
	buf->write_size = cluster_size;
	return buf;
}




/*
 * BRelse();
 *
 * Releases a cluster, wakes up next coroutine waiting for the cluster
 */

void BRelse (struct Buf *buf)
{
	KAssert (buf!= NULL);
	KAssert (buf->flags & B_BUSY);

	if (buf->flags & B_ERROR)
	{
		LIST_REM_ENTRY (&buf_hash[buf->cluster_nr % NR_HASH], buf, lookup_link);
		buf->flags &= ~(B_VALID | B_ERROR);
		buf->cluster_nr = -1;
		buf->blockdev = NULL;
		
		if (buf->cluster_buf != NULL)
		{
			LIST_ADD_HEAD (&buf_avail_list, buf, free_link);		
		}
	}
	else if (buf->cluster_buf != NULL)
	{	
		LIST_ADD_TAIL (&buf_avail_list, buf, free_link);
	}
	
	buf->flags &= ~B_BUSY;
	taskwakeupall (&buf_list_rendez);
	taskwakeupall (&buf->rendez);
}




/*
 * BDWrite();
 */

int BDWrite (struct Buf *buf)
{
	KAssert (buf->flags & B_BUSY);

	if (buf->flags & B_DELWRI && buf->expire_time.seconds < current_time.seconds)
	{
		BRelse (buf);
		return 0;
	}
	
	buf->flags = (buf->flags | B_WRITE | B_DELWRI) & ~(B_READ | B_ASYNC);
	buf->expire_time.seconds = current_time.seconds + DELWRI_SECONDS;
	buf->expire_time.microseconds = 0;
	BRelse (buf);
	return 0;

}






/*
 * BWrite();
 *
 * Writes a block to disk and releases the cluster.  Waits for IO to complete before
 * returning to the caller.
 */

int BWrite (struct Buf *buf)
{
	KAssert (buf->flags & B_BUSY);
	
	buf->flags = (buf->flags | B_WRITE) & ~(B_READ | B_ASYNC);
	Strategy (buf);
	
	while (!(buf->flags & B_IODONE))
		tasksleep (&buf->rendez);

	buf->flags &= ~B_IODONE;

	if (buf->flags & B_ERROR)
	{
		BRelse(buf);
		return -1;
	}
	
	buf->flags &= ~B_WRITE;
	BRelse (buf);
	return 0;
}




/*
 * BAWrite();
 *
 * Writes cluster immediately and don't wait for it to complete.  Releases cluster
 * asynchronously.  Used for when writing at end of cluster and cluster unlikely
 * to be written again soon.
 *
 */
 
int BAWrite (struct Buf *buf)
{
	KAssert (buf->flags & B_BUSY);

	buf->flags = (buf->flags | B_WRITE | B_ASYNC) & ~B_READ;
	Strategy (buf);
	return 0;
}




/*
 * Strategy();
 *
 * FIXME: Test for sb abort, return as if IOERROR IODONE occured.
 */

void Strategy (struct Buf *buf)
{
	struct BlockDev *bdev;
	
	KAssert (buf!= NULL);
	KAssert (buf->flags & B_BUSY);	
	
	bdev = buf->blockdev;
	LIST_ADD_TAIL (&bdev->strategy_list, buf, strategy_link);
	taskwakeup (&bdev->rendez);
}




/*
 * StrategyTask();
 *
 * FIXME: Do we need to release strategy list clusters on BDF_ABORT ???
 * Or is that done by AutoMount task?
 *
 * FIXME: What if block size < sector_size  (2k blocks and 4k sectors?)
 */
 
void StrategyTask (void *arg)
{
	struct BlockDev *bdev = (struct BlockDev *)arg;
	struct Buf *buf;
//	msginfo_t mi;
	struct MsgBlock *msg;
	size_t msg_sz;
	uint64 remaining;
	
	msg_sz = VirtualSizeOf (bdev->msg);

	try
	{
		while ((bdev->flags & BDF_ABORT) == 0)
		{
			while ((buf= LIST_HEAD(&bdev->strategy_list)) == NULL
										&& (bdev->flags & BDF_ABORT) == 0)
			{
				tasksleep (&bdev->rendez);
			}
			
			if (bdev->flags & BDF_ABORT)
				break;
						
			msg = bdev->msg;

			while ((buf= LIST_HEAD(&bdev->strategy_list)) != NULL)
			{
				msg->cmd = BLK_CMD_STRATEGY;
			
				if (buf->flags & B_WRITE)
					msg->s.strategy.direction = BLK_DIR_WRITE;			
				else
					msg->s.strategy.direction = BLK_DIR_READ;
				

				if (buf->flags & B_READ || buf->flags & B_WRITECLUSTER)
				{
					// Read a cluster or write a cluster for a delayed write
					
					msg->s.strategy.buf_offset = 0;
					msg->s.strategy.start_sector = buf->cluster_nr * cluster_size / bdev->gendisk.sector_size;
					remaining = bdev->gendisk.capacity - msg->s.strategy.start_sector * bdev->gendisk.sector_size;	
					
					if (remaining < cluster_size)
					{
						msg->s.strategy.nsectors = remaining / bdev->gendisk.sector_size;
						memset ((uint8*) buf->cluster_buf + remaining, 0, cluster_size - remaining);
					}
					else
					{
						msg->s.strategy.nsectors = cluster_size / bdev->gendisk.sector_size;
					}
				}
				else
				{
					// BWrite(), BWriteA(), write a single block

					msg->s.strategy.buf_offset = buf->write_offset;
					msg->s.strategy.start_sector = (buf->cluster_nr * cluster_size + buf->write_offset) / bdev->gendisk.sector_size;

					remaining = bdev->gendisk.capacity - msg->s.strategy.start_sector * bdev->gendisk.sector_size;	
					
					if (remaining < buf->write_size)
					{
						msg->s.strategy.nsectors = remaining / bdev->gendisk.sector_size;
						memset ((uint8*) buf->cluster_buf + remaining, 0, cluster_size - remaining);
					}
					else
					{
						msg->s.strategy.nsectors = buf->write_size / bdev->gendisk.sector_size;
					}
				}
				
				
				
				
//				PutMsg (bdev->handle, bdev->msg);
				bdev->msg = NULL;
			
//				PutMsg (bdev->handle, buf->cluster_buf);
				buf->addr = NULL;
						
//				while (GetMsg (bdev->handle, &mi) == -1 && (bdev->flags & BDF_ABORT) == 0)
//					tasksleep (&bdev->rendez);
			
				if (bdev->flags & BDF_ABORT) 
					break;

//				buf->cluster_buf = (uint8 *) mi.msg;

//				if (mi.size != cluster_size)
//					break;
						
//				while (GetMsg (bdev->handle, &mi) == -1 && (bdev->flags & BDF_ABORT) == 0)
//					tasksleep (&bdev->rendez);
			
				if (bdev->flags & BDF_ABORT) 
					break;
					
			
//				bdev->msg = (struct MsgBlock *)mi.msg;

//				if (mi.size != msg_sz)
//					break;
			
				if (bdev->msg->r.strategy != 0)
					break;

				LIST_REM_ENTRY (&bdev->strategy_list, buf, strategy_link);
				buf->flags |= B_IODONE;
				buf->flags &= ~B_WRITECLUSTER;
				
				if (buf->flags & B_ASYNC)
				{
					buf->flags = (buf->flags | B_VALID) & ~(B_READ | B_WRITE | B_ASYNC | B_IODONE);
					BRelse(buf);
				}
				
				taskwakeupall (&buf->rendez);
			}
		}		
	}
	catch (int err)
	{
		if (bdev->msg != NULL)
		{
			VirtualFree (bdev->msg);
			bdev->msg = NULL;
		}

			
		if  ((buf == NULL)
			|| (buf->cluster_buf != NULL && VirtualSizeOf (buf->cluster_buf) != cluster_size))
		{
			VirtualFree (buf->cluster_buf);
			buf->cluster_buf = NULL;
			
			buf->flags |= B_ERROR;
		}
	};
	
	
	KLog (" ***** STRATEGY CLEAN UP *****");
	
	while ((buf= LIST_HEAD (&bdev->strategy_list)) != NULL)
	{
		LIST_REM_HEAD (&bdev->strategy_list, free_link);
		buf->flags |= B_IODONE | B_ERROR;
		taskwakeup (&buf->rendez);
		
		if (buf->flags & B_ASYNC)
		{
			buf->flags = buf->flags & ~(B_READ | B_WRITE | B_ASYNC | B_IODONE | B_ERROR);
			BRelse(buf);
		}
	}
	
	KLog ("Strategy EXIT");
	
//	H_CLR (bdev->handle, &event_set);
	bdev->task_id = -1;
	taskwakeup (&bdev->rendez);
}




