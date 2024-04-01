/* This code handles the reading of directorys.
 *
 * This is partially based on the fs_getdents in Minix's
 * ext2/read.c file.
 *
 * Created (CheviotOS Filesystem Handler based)
 *   December 2023 (Marven Gilhespie) 
 */

#define LOG_LEVEL_WARN

#include "ext2.h"
#include "globals.h"


/* @brief   Read directory entries in a buffer
 *
 * @param   dir_inode, inode of directory to read
 * @param   cookie, position within the directory to read from
 * @param   data, buffer to write to
 * @param   size, size of buffer
 */
ssize_t get_dirents(struct inode *dir_inode, off64_t *cookie, char *data, ssize_t size)
{
  static struct dirent_buf dirent_buf;
  ssize_t sz;
  bool full;
  off_t pos;
  //, block_pos;
  struct buf *bp = NULL;
  struct dir_entry *d_desc = NULL;
  off_t block_base_pos;
  
  log_info("get_dirents(ino:%d, cookie:%08x)", dir_inode->i_ino, (uint32_t)*cookie);
  
  pos = (off_t) *cookie;
  
  if ((unsigned int) pos % DIR_ENTRY_ALIGN) {
  	log_debug("get_dirents -ENOENT");
  	return -ENOENT;
  }
    
  dirent_buf_init(&dirent_buf, data, size);

  full = false;

	log_debug("dir_inode->odi.i_size:%08x", dir_inode->odi.i_size);

  while(!full && pos < dir_inode->odi.i_size) {
  	log_debug("loop: pos:%08x", pos);
  	
//    block_pos = pos % sb_block_size;
    
    log_debug("call get_dir_block");
    
    bp = get_dir_block(dir_inode, pos);
    assert(bp != NULL);
    
    log_debug("seek_to_valid_dirent");
    
    d_desc = seek_to_valid_dirent(bp, pos);

    if (d_desc == NULL) {
    	log_debug("advancing to next block");
      pos = (pos/sb_block_size)*sb_block_size + sb_block_size;
      put_block(cache, bp);
      continue;
    }
    
    log_debug("fill dirent_buffer");
    
    full = fill_dirent_buf(bp, &d_desc, &dirent_buf);
	  
	  log_debug("full=%d", full);
	  
	  // d_desc is updated by full_dirent_buf
	  block_base_pos = (pos/sb_block_size)*sb_block_size;
	 	pos = block_base_pos + ((uint8_t *)d_desc - (uint8_t *)bp->data);
	  
	  log_debug("updated pos:%d", (uint32_t)pos);
	  
	  put_block(cache, bp);
  }

  if ((sz = dirent_buf_finish(&dirent_buf)) >= 0) {
  	log_debug("new cookie :%d", (uint32_t)pos);
	  *cookie = pos;
	  dir_inode->i_update |= ATIME;
	  inode_markdirty(dir_inode);
  }

	log_debug("dirent buf sz:%d", sz);

  return sz;
}


/* @brief   Get a block belonging to a directory from the file offset
 *
 * @param   dir_inode, inode of the directory
 * @param   position, file offset within the directory
 * @return  buffer if block exists or NULL if block does not exist
 */
struct buf *get_dir_block(struct inode *dir_inode, off64_t position)
{
	struct buf *bp;	
	block_t b;
	
	log_debug("get_dir_block(dir_inode:%08x: pos:%d)", (uint32_t)dir_inode, (uint32_t)position);
	
	b = read_map_entry(dir_inode, position);
	if(b == NO_BLOCK) {
		log_debug("get_dir_block NO_BLOCK");
		return NULL;
  }

	log_debug("block = %d", (uint32_t)b);

	if ((bp = get_block(cache, b, BLK_READ)) == NULL) {
		panic("extfs: error getting block %d", b);
  }

	return bp;
}


/* @brief   Seek to dirent at last cookie position within block.
 *
 * We do this as dirents could have been modified or deleted since last call to
 * readdir, making the cookie have an invalid offset.
 */
struct dir_entry *seek_to_valid_dirent(struct buf *bp, off_t pos)
{
  struct dir_entry *d_desc;
	off_t scan_pos;
	off_t base_pos;
	
	base_pos = (pos/sb_block_size)*sb_block_size;
	scan_pos = base_pos;
	  
  d_desc = (struct dir_entry *)bp->data;

  while (scan_pos + bswap2(be_cpu, d_desc->d_rec_len) <= pos
         && NEXT_DISC_DIR_POS(d_desc, bp->data) < sb_block_size) {

    if (bswap2(be_cpu, d_desc->d_rec_len) == 0) {
      panic("extfs: readdir dirent record length is 0");
    }

	  scan_pos += bswap2(be_cpu, d_desc->d_rec_len);
    d_desc = NEXT_DISC_DIR_DESC(d_desc);
  }
  
  if ((scan_pos - base_pos) >= sb_block_size) {
//  if ((uint8_t *)d_desc - (uint8_t *)bp->data >= sb_block_size) {
		log_debug("reached end of block, no valid dirents");
  	return NULL;
  }
  
  log_debug("scan_pos after seek valid dirent: %d", scan_pos);
  return d_desc;
}


/*
 * @return  true if dirent buf is now full, false otherwise
 */
bool fill_dirent_buf(struct buf *bp, struct dir_entry **d_desc, struct dirent_buf *db)
{
  ino_t child_nr;
  unsigned int len;
  
  while(CUR_DISC_DIR_POS(*d_desc, bp->data) < sb_block_size) {
	  if ((*d_desc)->d_ino != 0) {
	    len = (*d_desc)->d_name_len;
	    
	    assert(len <= EXT2_NAME_MAX);

	    child_nr = (ino_t) bswap4(be_cpu, (*d_desc)->d_ino);
	    
	    if (dirent_buf_add(db, child_nr, (*d_desc)->d_name, len) == 0) {
	      return true; 
	    }
	  }
	  
	  *d_desc = NEXT_DISC_DIR_DESC((*d_desc));
  }
  
  return false;
}


/* @brief   Initialize offset in the dirent buffer
 *
 */
void dirent_buf_init(struct dirent_buf *db, void *buf, size_t sz)
{
	db->data = buf;
	db->size = sz;
	db->position = 0;
}


/* @brief   Add a directory entry to the dirent buffer
 *
 * @return  size of record added or 0 if no space remaining
 */
int dirent_buf_add(struct dirent_buf *db, int ino_nr, char *name, int namelen)
{
	struct dirent *dirent;
	size_t reclen;	
	int r;
  
  dirent = (struct dirent *)(db->data + db->position);
  
  reclen = roundup(((uintptr_t)&dirent->d_name[namelen] + 1 - (uintptr_t)dirent), 8);

  if (db->position + reclen <= db->size) {
    memset(dirent, 0, reclen);    
    memcpy(&dirent->d_name[0], name, namelen);     
    dirent->d_name[namelen] = '\0';
    dirent->d_ino = ino_nr;
    dirent->d_cookie = 0;
    dirent->d_reclen = reclen;

		log_debug("dirent add: %s", dirent->d_name);

    db->position += reclen;
    return reclen;
  } else {
    return 0;
  }
}


/* @brief   Get the number of bytes written to the dirent buffer
 *
 */
size_t dirent_buf_finish(struct dirent_buf *db)
{
	log_debug("dirent_buf_finish");
  return db->position;
}


/*
 * @brief   Compare non null-terminated string to null-terminated string
 *
 * @param   s1_nz, non-null terminated string to compare
 * @param   s2, null terminated string to compare
 * @param   s1_len, length of the s1_nz string
 * @return  0 if strings are equal otherwise it returns -1
 */
int strcmp_nz(char *s1_nz, char *s2, size_t s1_len)
{
  size_t remaining = s1_len;
  
	if (s1_len > 0) {
		do {
			if (*s2 == '\0') {			
				return -1;
		  }
			
			if (*s1_nz++ != *s2++) {
				return -1;
		  }

		} while (--remaining > 0);

		if (*s2 != '\0') {
			return -1;
	  }
	}
	return 0;
}

