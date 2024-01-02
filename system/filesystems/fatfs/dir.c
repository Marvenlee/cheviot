/*
 * Copyright 2014  Marven Gilhespie
 *
 * Licensed under the Apache License, segment_id 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#include "fat.h"
#include "globals.h"
#include "sys/debug.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fsreq.h>
#include <sys/mount.h>
#include <sys/syscalls.h>
#include <unistd.h>

int fat_dir_read(struct FatNode *node, void *buf, off_t cookie,
                 uint32_t *r_sector, uint32_t *r_sector_offset) {
  uint32_t cluster;
  uint32_t sector;
  uint32_t cluster_offset;
  uint32_t sector_offset;

//  KLog ("fat: fat_dir_read, buf = %08x", buf);

  if (node->is_root == true &&
      (fsb.fat_type == TYPE_FAT12 || fsb.fat_type == TYPE_FAT16)) {
    if (cookie < fsb.bpb.root_entries_cnt) {
      sector = fsb.bpb.reserved_sectors_cnt +
               (fsb.bpb.fat_cnt * fsb.sectors_per_fat) +
               ((cookie * FAT_DIRENTRY_SZ) / 512);
      sector_offset = (cookie * FAT_DIRENTRY_SZ) % 512;

      if (readSector(buf, sector, sector_offset, FAT_DIRENTRY_SZ) == 0) {
        if (r_sector != NULL)
          *r_sector = sector;
        if (r_sector_offset != NULL)
          *r_sector_offset = sector_offset;
        
        return 0;
      }
    }
  } else {
    if (FindCluster(node, cookie * FAT_DIRENTRY_SZ, &cluster) == 0) {
      cluster_offset =
          (cookie * FAT_DIRENTRY_SZ) % (fsb.bpb.sectors_per_cluster * 512);
      sector = (((cluster - 2) * fsb.bpb.sectors_per_cluster) +
                fsb.first_data_sector) +
               (cluster_offset / 512);
      sector_offset = (cookie * FAT_DIRENTRY_SZ) % 512;

      if (readSector(buf, sector, sector_offset, FAT_DIRENTRY_SZ) == 0) {
        if (r_sector != NULL)
          *r_sector = sector;
        if (r_sector_offset != NULL)
          *r_sector_offset = sector_offset;

        return 0;
      }
    }
  }
  
  KLog ("fat: fat_dir_read failed");

  return -1;
}

/*
 * Comparison can be simplified, shouldn't need len. (old?)
 */

int fat_cmp_dirent(struct FatDirEntry *dirent, char *comp) {
  char packed_dirent_name[13];

  if (!(dirent->name[0] != DIRENTRY_FREE &&
        (uint8_t)dirent->name[0] != DIRENTRY_DELETED &&
        (dirent->attributes & ATTR_LONG_FILENAME) != ATTR_LONG_FILENAME)) {
    return -1;
  }

  FatDirEntryToASCIIZ(packed_dirent_name, dirent);
  return strcmp(comp, packed_dirent_name);
}

/*
 * DirEntryToASCIIZ();
 *
 */

int FatDirEntryToASCIIZ(char *pathbuf, struct FatDirEntry *dirent) {
  char *p = pathbuf;
  int i;
  int len = 0;

  *pathbuf = 0;
  len = 0;

  for (i = 0; i < 8; i++) {
    if (dirent->name[i] != ' ') {
      *p = dirent->name[i];

      if (*p >= 'A' && *p <= 'Z')
        *p += ('a' - 'A');

      p++;
      len++;
    } else
      break;
  }

  if (dirent->extension[0] != ' ') {
    *p++ = '.';
    len++;
  }

  for (i = 0; i < 3; i++) {
    if (dirent->extension[i] != ' ') {
      *p = dirent->extension[i];

      if (*p >= 'A' && *p <= 'Z')
        *p += ('a' - 'A');
      p++;
      len++;
    } else
      break;
  }

  *p = 0;

  return len;
}

/*
 *
 */

int FatASCIIZToDirEntry(struct FatDirEntry *dirent, char *filename) {
  char *p = filename;
  int i;

  if (FatIsDosName(filename) == 0) {
    for (i = 0; i < 8; i++) {
      if (*p == '\0' || *p == '.')
        dirent->name[i] = ' ';
      else
        dirent->name[i] = *p++;
    }

    if (*p == '.') {
      p++;

      for (i = 0; i < 3; i++) {
        if (*p == '\0')
          dirent->extension[i] = ' ';
        else
          dirent->extension[i] = *p++;
      }
    } else {
      for (i = 0; i < 3; i++)
        dirent->extension[i] = ' ';
    }

    return 0;
  }

  return -ENOENT;
}

/*
 * FatIsDosName();
 *
 * Checks that component is a valid DOS filename
 *
 */

int FatIsDosName(char *s) {
  int32 name_cnt = 0;
  int32 extension_cnt = 0;
  int dot_cnt = 0;

  while (*s != '\0' && *s != '/') {
    /* Add support for special Japanese Kanji character 0x05 0xe5 ?? */

    if (*s == '.') {
      dot_cnt++;

      if (dot_cnt > 1)
        return -1;

      s++;
      continue;
    } else if (*s >= 'a' && *s <= 'z') {
      *s -= 'a' - 'A';
    } else if (!((*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') ||
                 (*s < 0))) {
      switch (*s) {
      case '$':
      case '%':
      case '\'':
      case '-':
      case '_':
      case '@':
      case '~':
      case '`':
      case '!':
      case '(':
      case ')':
      case '{':
      case '}':
      case '^':
      case '#':
      case '&':
      case ' ':
        break;
      default:
        return -1;
      }
    }

    if (dot_cnt == 0)
      name_cnt++;
    else if (dot_cnt == 1)
      extension_cnt++;
    else {
      return -1;
    }

    if (name_cnt == 0 && dot_cnt >= 1) {
      return -1;
    }

    if (name_cnt > 8 || dot_cnt > 1 || extension_cnt > 3) {
      return -1;
    }

    s++;
  }

  return 0;
}

/*
 *
 */

int FatCreateDirEntry(struct FatNode *parent, struct FatDirEntry *dirent,
                      uint32_t *r_sector, uint32_t *r_sector_offset) {
  uint32_t cluster;
  uint32_t sector;
  uint32_t cluster_offset;
  uint32_t sector_offset;
  uint32_t offset;
  struct FatDirEntry current_dirent;

  offset = 0;

  if (parent == &fsb.root_node &&
      (fsb.fat_type == TYPE_FAT12 || fsb.fat_type == TYPE_FAT16)) {
    while (offset < fsb.bpb.root_entries_cnt) {
      sector = fsb.bpb.reserved_sectors_cnt +
               (fsb.bpb.fat_cnt * fsb.sectors_per_fat) +
               ((offset * FAT_DIRENTRY_SZ) / 512);
      sector_offset = (offset * FAT_DIRENTRY_SZ) % 512;

      if (readSector(&current_dirent, sector, sector_offset, FAT_DIRENTRY_SZ) ==
          0) {
        if (current_dirent.name[0] == DIRENTRY_FREE ||
            current_dirent.name[0] == DIRENTRY_DELETED) {
          writeSector(dirent, sector, sector_offset, FAT_DIRENTRY_SZ);

          if (r_sector != NULL)
            *r_sector = sector;
          if (r_sector_offset != NULL)
            *r_sector_offset = sector_offset;

          return 1;
        }
      } else {
        return -1;
      }

      offset++;
    }

    return 0;
  } else {
    while (FindCluster(parent, offset * FAT_DIRENTRY_SZ, &cluster) == 0) {
      cluster_offset =
          (offset * FAT_DIRENTRY_SZ) % (fsb.bpb.sectors_per_cluster * 512);
      sector = ClusterToSector(cluster) + (cluster_offset / 512);
      sector_offset = (offset * FAT_DIRENTRY_SZ) % 512;

      if (readSector(&current_dirent, sector, sector_offset, FAT_DIRENTRY_SZ) ==
          0) {
        if (current_dirent.name[0] == DIRENTRY_FREE ||
            current_dirent.name[0] == DIRENTRY_DELETED) {
          writeSector(dirent, sector, sector_offset, FAT_DIRENTRY_SZ);

          if (r_sector != NULL)
            *r_sector = sector;
          if (r_sector_offset != NULL)
            *r_sector_offset = sector_offset;

          return 1;
        }

      } else {
        return -1;
      }

      offset++;
    }

    if (AppendCluster(parent, &cluster) == 0) {
      ClearCluster(cluster);

      sector = ClusterToSector(cluster);
      sector_offset = 0;

      writeSector(dirent, sector, sector_offset, FAT_DIRENTRY_SZ);

      if (r_sector != NULL)
        *r_sector = sector;
      if (r_sector_offset != NULL)
        *r_sector_offset = sector_offset;

      return 1;
    } else {
      return 0;
    }
  }

  return -1;
}

/*
 *
 */

void FatDeleteDirEntry(uint32_t sector, uint32_t sector_offset) {
  struct FatDirEntry dirent;

  if (readSector(&dirent, sector, sector_offset, FAT_DIRENTRY_SZ) == 0) {
    dirent.name[0] = DIRENTRY_DELETED;

    writeSector(&dirent, sector, sector_offset, FAT_DIRENTRY_SZ);
  }
}

/*
 *
 */

int IsDirEmpty(struct FatNode *node) {
  struct FatDirEntry dirent;
  char asciiz_name[16];
  int rc;
  off_t offset;

  offset = 0;

  while ((rc = fat_dir_read(node, &dirent, offset, NULL, NULL)) == 1) {
    offset++;

    if (dirent.name[0] == DIRENTRY_FREE)
      return 0;
    else if (dirent.name[0] == DIRENTRY_DELETED)
      continue;
    else if (dirent.attributes & ATTR_VOLUME_ID)
      continue;
    else {
      FatDirEntryToASCIIZ(asciiz_name, &dirent);

      if (!((strcmp("..", asciiz_name) == 0) ||
            (strcmp(".", asciiz_name) == 0)))
        return -1;
    }
  }

  return rc;
}

/*
 * FlushNode();
 */

int FlushDirent(struct FatNode *node) {
  if (node == &fsb.root_node)
    return 0;

  writeSector(&node->dirent, node->dirent_sector, node->dirent_offset,
              sizeof(struct FatDirEntry));

  return 0;
}
