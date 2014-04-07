/*
 *  $Id: utime.h,v 1.1 2002/11/07 19:27:36 jjohnstn Exp $
 */

#ifndef __DIRENT_h__
#define __DIRENT_h__

#ifdef __cplusplus
extern "C" {
#endif

/*
 *
 */

#include <sys/types.h>
#include <limits.h>


typedef struct _dir
{
	int	fd;
} DIR;




int closedir(DIR *);
DIR *opendir(const char *);
struct dirent *readdir(DIR *);



struct dirent
{
    long d_reclen;  			/* Length of this dirent */
	long d_ino;
	char d_name[NAME_MAX + 1];
};


#define DIRENT_ALIGN		32



#ifdef __cplusplus
};
#endif

#endif /* _SYS_DIRENT_H */
