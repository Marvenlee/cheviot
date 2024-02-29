lib_LIBRARIES = libcrypt.a

libcrypt_a_SOURCES = \
	bcrypt.c \
	blowfish.c \
  crypt.c \
  crypt-sha1.c \
  hmac_sha1.c \
  md5c.c \
  md5crypt.c \
  pw_gensalt.c \
  sha1.c \
  util.c

subdirheadersdir = $(includedir)/
subdirheaders_HEADERS = \
	include/crypt.h \
	include/sys/md5.h \
	include/sys/sha1.h

  
AM_CFLAGS = -O2 -std=c99 -g0 -I.
AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(srcdir)/include \
	-I$(top_srcdir)/libutil/include

