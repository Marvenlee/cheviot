/* This file manages the super block structure.
 *
 * The entry points into this file are
 *   get_super:       search the 'superblock' table for a device
 *   read_super:      read a superblock
 *
 * Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 *
 * Updated (CheviotOS Filesystem Handler based)
 *   December 2023 (Marven Gilhespie) 
 */


/* @brief   Superblock related functions.
 *
 * This is partially based on the Minix Ext2 FS superblock.c sources
 */
 
#define LOG_LEVEL_INFO

#include "ext2.h"
#include "globals.h"


/* @brief   Read the superblock and Group Descriptor Table from disk into memory
 *
 * @return  0 on success, negative errno on failure
 */
int read_superblock(void)
{
  int sz;

	log_info("read_superblock()");
	
  // Read 1024 bytes from disk into the ondisk_superblock
  lseek64(block_fd, SUPERBLOCK_OFFSET, SEEK_SET);
  sz = read(block_fd, &ondisk_superblock, SUPERBLOCK_SIZE);

  if (sz != SUPERBLOCK_SIZE) {
    log_info("superblock read failed, sz:%d", sz);
	  return -EINVAL;
  }

  super_copy(&superblock, &ondisk_superblock);

  if (superblock.s_magic != SUPER_MAGIC) {
  	log_error("superblock magic != SUPER_MAGIC");
  	return -EINVAL;
  }
  
  sb_block_size = EXT2_MIN_BLOCK_SIZE * (1<<superblock.s_log_block_size);

  if ((sb_block_size % 512) != 0) {
    log_error("block size is not a multiple of 512");
  	return -EINVAL;
  }
  
  if (SUPERBLOCK_SIZE > sb_block_size) {
  	log_error("superblock size is larger than block size");
  	return -EINVAL;
  }

  sb_sectors_in_block = sb_block_size / 512;

  if (superblock.s_rev_level == EXT2_DYNAMIC_REV) {
    sb_inode_size = superblock.s_inode_size;
    sb_first_ino  = superblock.s_first_ino;
  } else {
    sb_inode_size = EXT2_GOOD_OLD_INODE_SIZE;
    sb_first_ino  = EXT2_GOOD_OLD_FIRST_INO;
  }

  if ((sb_inode_size & (sb_inode_size - 1)) != 0 || sb_inode_size > sb_block_size) {
	  log_error("inode size is incorrect");
	  return -EINVAL;
  }

  sb_blocksize_bits = superblock.s_log_block_size + EXT2_MIN_BLOCK_LOG_SIZE;
  sb_max_size = MAX_FILE_POS;
  sb_inodes_per_block = sb_block_size / sb_inode_size;

  if (sb_inodes_per_block == 0 || superblock.s_inodes_per_group == 0) {
	  log_error("either inodes_per_block or inodes_per_group count is 0");
	  return -EINVAL;
  }

  sb_inode_table_blocks_per_group = superblock.s_inodes_per_group / sb_inodes_per_block;
  sb_desc_per_block = sb_block_size / sizeof(struct group_desc);
  sb_groups_count = ((superblock.s_blocks_count - superblock.s_first_data_block - 1) / superblock.s_blocks_per_group) + 1;
  sb_group_desc_block_count = (sb_groups_count + sb_desc_per_block - 1) / sb_desc_per_block;
  sb_gdt_position = (superblock.s_first_data_block + 1) * sb_block_size;

  log_info("sb_inodes_per_group = %u", superblock.s_inodes_per_group);
  log_info("sb_inode_table_blocks_per_group = %u", sb_inode_table_blocks_per_group);    
  log_info("sb_desc_per_block = %u", sb_desc_per_block);    
  log_info("sb_groups_count   = %u", sb_groups_count);  
  log_info("sb_group_desc_block_count      = %u", sb_group_desc_block_count);  
  log_info("sb_gdt_position   = %u (lower 32 bits)", (uint32_t)sb_gdt_position);  
  log_info("sb_block_size  = %u", (uint32_t)sb_block_size);  
  
  if(!(group_descs = virtualalloc(NULL, sb_groups_count * sizeof(struct group_desc), PROT_READWRITE))) {
	  panic("can't allocate group desc array");
  }
  
  ondisk_group_descs = virtualalloc(NULL, sb_groups_count * sizeof(struct group_desc), PROT_READWRITE);

  if (ondisk_group_descs == NULL) {
	  panic("can't allocate group desc array");
  }
  
  /* s_first_data_block (block number, where superblock is stored)
   * is 1 for 1Kb blocks and 0 for larger blocks.
   * For fs with 1024-byte blocks first 1024 bytes (block0) used by MBR,
   * and block1 stores superblock. When block size is larger, block0 stores
   * both MBR and superblock, but gdt lives in next block anyway.
   *
   * If sb=N was specified, then gdt is stored in N+1 block, the block number
   * here uses 1k units.
   */
  lseek64(block_fd, sb_gdt_position, SEEK_SET);
  sz = read(block_fd, (char *)ondisk_group_descs, sb_groups_count * sizeof(struct group_desc));

  if (sz != sb_groups_count * sizeof(struct group_desc)) {
	  log_error("can not read group descriptors");
	  return -EINVAL;    
  }
  
  copy_group_descriptors(group_descs, ondisk_group_descs, sb_groups_count);

  /* Make a few basic checks to see if super block looks reasonable. */
  if (superblock.s_inodes_count < 1 || superblock.s_blocks_count < 1) {
	  log_error("not enough inodes or data blocks");
	  return -EINVAL;
  }

  sb_dirs_counter = ext2_count_dirs(&superblock);

	/* Precalculate some variables that help with searching for a block in the
   * inode's double indirect and triple indirect tables
	 */
	sb_addr_in_block = sb_block_size / BLOCK_ADDRESS_BYTES;
	sb_addr_in_block2 = sb_addr_in_block * sb_addr_in_block;
	sb_doub_ind_s = EXT2_NDIR_BLOCKS + sb_addr_in_block;
	sb_triple_ind_s = sb_doub_ind_s + sb_addr_in_block2;

#if 1
	sb_out_range_s = 0xFFFF0000;		// FIXME: sb_out_range_s initialization
#else
	sb_out_range_s = sb_triple_ind_s + sb_addr_in_block2 * sb_addr_in_block;
#endif

	log_info("***sb_out_range_s: %08x", (uint32_t)sb_out_range_s);

  return 0;
}


/* @brief   Write the superblock and Group Descriptor Table from memory onto disk
 *
 */
void write_superblock(void)
{
  int sz;
  
  log_info("write_superblock()");
  
  super_copy(&ondisk_superblock, &superblock);

  // Write 1024 bytes from the ondisk_superblock structure to disk
  lseek64(block_fd, SUPERBLOCK_OFFSET, SEEK_SET);
  sz = write(block_fd, &ondisk_superblock, SUPERBLOCK_SIZE);

  if (sz != SUPERBLOCK_SIZE) {
  	panic("ext2: failed to write complete superblock, sz:%d", sz);
  }
  
  if (sb_group_descriptors_dirty) {
    copy_group_descriptors(ondisk_group_descs, group_descs, sb_groups_count);

		log_info("write group descriptors");

    lseek64(block_fd, sb_gdt_position, SEEK_SET);
    sz = write(block_fd, (char *)ondisk_group_descs, sb_groups_count * sizeof(struct group_desc));

    if (sz != sb_groups_count * sizeof(struct group_desc)) {
	    panic("can not read group descriptors");
    }
	  
	  sb_group_descriptors_dirty = false;
  }
}


/* @brief   Copy the ondisk superblock structure and byte-swap fields if needed
 *
 * @param   dest, pointer to superblock to copy the byte-swapped fields to
 * @param   source, pointer to superblock to copy fields from
 */
void super_copy(struct superblock *dest, struct superblock *source)
{
  dest->s_inodes_count      = bswap4(be_cpu, source->s_inodes_count);
  dest->s_blocks_count      = bswap4(be_cpu, source->s_blocks_count);
  dest->s_r_blocks_count    = bswap4(be_cpu, source->s_r_blocks_count);
  dest->s_free_blocks_count = bswap4(be_cpu, source->s_free_blocks_count);
  dest->s_free_inodes_count = bswap4(be_cpu, source->s_free_inodes_count);
  dest->s_first_data_block  = bswap4(be_cpu, source->s_first_data_block);
  dest->s_log_block_size    = bswap4(be_cpu, source->s_log_block_size);
  dest->s_log_frag_size     = bswap4(be_cpu, source->s_log_frag_size);
  dest->s_blocks_per_group  = bswap4(be_cpu, source->s_blocks_per_group);
  dest->s_frags_per_group   = bswap4(be_cpu, source->s_frags_per_group);
  dest->s_inodes_per_group  = bswap4(be_cpu, source->s_inodes_per_group);
  dest->s_mtime             = bswap4(be_cpu, source->s_mtime);
  dest->s_wtime             = bswap4(be_cpu, source->s_wtime);
  dest->s_mnt_count         = bswap2(be_cpu, source->s_mnt_count);
  dest->s_max_mnt_count     = bswap2(be_cpu, source->s_max_mnt_count);
  dest->s_magic             = bswap2(be_cpu, source->s_magic);
  dest->s_state             = bswap2(be_cpu, source->s_state);
  dest->s_errors            = bswap2(be_cpu, source->s_errors);
  dest->s_minor_rev_level   = bswap2(be_cpu, source->s_minor_rev_level);
  dest->s_lastcheck         = bswap4(be_cpu, source->s_lastcheck);
  dest->s_checkinterval     = bswap4(be_cpu, source->s_checkinterval);
  dest->s_creator_os        = bswap4(be_cpu, source->s_creator_os);
  dest->s_rev_level         = bswap4(be_cpu, source->s_rev_level);
  dest->s_def_resuid        = bswap2(be_cpu, source->s_def_resuid);
  dest->s_def_resgid        = bswap2(be_cpu, source->s_def_resgid);
  dest->s_first_ino         = bswap4(be_cpu, source->s_first_ino);
  dest->s_inode_size        = bswap2(be_cpu, source->s_inode_size);
  dest->s_block_group_nr    = bswap2(be_cpu, source->s_block_group_nr);
  dest->s_feature_compat    = bswap4(be_cpu, source->s_feature_compat);
  dest->s_feature_incompat  = bswap4(be_cpu, source->s_feature_incompat);
  dest->s_feature_ro_compat = bswap4(be_cpu, source->s_feature_ro_compat);
  dest->s_algorithm_usage_bitmap  = bswap4(be_cpu, source->s_algorithm_usage_bitmap);
  dest->s_padding1                = bswap2(be_cpu, source->s_padding1);
  
  memcpy(dest->s_uuid, source->s_uuid, sizeof(dest->s_uuid));
  memcpy(dest->s_volume_name, source->s_volume_name, sizeof(dest->s_volume_name));
  memcpy(dest->s_last_mounted, source->s_last_mounted, sizeof(dest->s_last_mounted));

  dest->s_prealloc_blocks         = source->s_prealloc_blocks;
  dest->s_prealloc_dir_blocks     = source->s_prealloc_dir_blocks;
}


