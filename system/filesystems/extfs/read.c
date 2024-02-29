/* This code handles the reading of files.
 *
 * Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 *
 * Updated (CheviotOS Filesystem Handler based)
 *   December 2023 (Marven Gilhespie) 
 */

#define LOG_LEVEL_INFO

#include "ext2.h"
#include "globals.h"


/* @brief   Read data from a file
 *
 * @param   ino_nr, inode number of file to read
 * @param   nrbytes, number of bytes in file to read
 * @param   position, offset in file to start reading from.
 * @return  Number of bytes read or negative errno on failure
 */
ssize_t read_file(ino_t ino_nr, size_t nrbytes, off64_t position)
{
  off64_t file_size;    // file size
  off64_t bytes_left;   // bytes left to end of file
  size_t off;           // offset in block
  size_t total_xfered;  // total bytes read
  size_t chunk_size;    // size of partial block of data we are currently reading
  struct inode *inode;  // inode of file to read from
  int res;              // result


  if ((inode = find_inode(ino_nr)) == NULL) {
    log_error("extfs: read_file, inode not found");
  	return -EINVAL;
  }
  
  file_size = inode->i_size;
  
  if (file_size < 0) {
    file_size = MAX_FILE_POS;
  }

  res = 0;  
  total_xfered = 0;
  
  while (nrbytes != 0) {
	  off = (unsigned int) (position % sb_block_size);
	  chunk_size = sb_block_size - off;
	  if (chunk_size > nrbytes) {
		  chunk_size = nrbytes;
    }
    
	  if (position >= file_size) {
	    break;
	  }

	  bytes_left = file_size - position;
	  
	  if (chunk_size > bytes_left) {
	    chunk_size = (int) bytes_left;
	  }

	  res = read_chunk(inode, position, off, chunk_size, total_xfered);

	  if (res != 0) {
	    break;
    }
    
	  nrbytes -= chunk_size;
	  total_xfered += chunk_size;
	  position += chunk_size;
  }

  if (res != 0) {
  	return res;
  }
  
  inode->i_update |= ATIME;  
  inode_markdirty(inode);  
  return total_xfered;
}


/* @brief   Read all or a partial chunk of a block
 *
 * @param   rip, pointer to inode for file to be rd/wr
 * @param   position, position within file to read or write
 * @param   off, offset within the current block
 * @param   chunk, number of bytes to read or write
 * @param   data, structure for (remote) user buffer
 * @param   msg_off, offset in message buffer
 * @param   block_size, block size of FS operating on
 * @return  0 on success, negative errno on failure
 */ 
int read_chunk(struct inode *inode, off64_t position, size_t off, size_t chunk_size, size_t msg_off)
{
  struct buf *buf = NULL;
  block_t block;
  ino_t ino = NO_INODE;
  uint64_t ino_off = rounddown(position, sb_block_size);
  int sc = 0;

  ino = inode->i_ino;
  block = read_map_entry(inode, position);
  
  if (block == NO_BLOCK) {
	  return read_nonexistent_block(msg_off, chunk_size);
  }

  buf = get_block(cache, block, BLK_READ);  
  assert(buf != NULL);
  
  sc = writemsg(portid, msgid, (uint8_t *)buf->data+off, chunk_size, msg_off + sizeof(struct fsreply));
  put_block(cache, buf);

  if (sc != chunk_size) {
    return -EIO;
  }
  
  return 0;
}


/* @brief   Write zeroes back to the kernel's VFS when reading a nonexistent block
 *
 * @param   off, offset within message buffer to write the zeroed bytes
 * @param   len, number of bytes to write
 * @return  0 on success or negative errno on failure
 */
int read_nonexistent_block(size_t msg_off, size_t len)
{
  size_t remaining = len;
  size_t nbytes_to_xfer;
  int sc;
  
  while (remaining > 0) {
    nbytes_to_xfer = (remaining < sizeof zero_block_data) ? remaining : sizeof zero_block_data;

  	sc = writemsg(portid, msgid, (void *)zero_block_data, nbytes_to_xfer, msg_off + sizeof (struct fsreply));

    if (sc != nbytes_to_xfer) {
      return -EIO;
    }
    
    msg_off += nbytes_to_xfer;
    remaining -= nbytes_to_xfer; 
  }
  
  return 0;
}


