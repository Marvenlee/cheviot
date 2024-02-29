lib_LIBRARIES = libdb.a

libdb_a_SOURCES = \
	btree/bt_conv.c \
	btree/bt_close.c \
	btree/bt_utils.c \
	btree/bt_split.c \
	btree/bt_seq.c \
	btree/bt_search.c \
	btree/bt_put.c \
	btree/bt_page.c \
	btree/bt_overflow.c \
	btree/bt_open.c \
	btree/bt_get.c \
	btree/bt_delete.c \
	btree/bt_debug.c \
	db/db.c \
	db/dbfile.c \
	mpool/mpool.c \
	recno/rec_seq.c \
	recno/rec_open.c \
	recno/rec_utils.c \
	recno/rec_search.c \
	recno/rec_put.c \
	recno/rec_get.c \
	recno/rec_delete.c \
	recno/rec_close.c
  
subdirheadersdir = $(includedir)/
subdirheaders_HEADERS = \
	include/btree.h \
	include/db.h \
	include/recno.h

AM_CFLAGS = -O2 -std=c99 -g0 -I.
AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(srcdir)/include \
	-I$(srcdir)/btree \
	-I$(srcdir)/hash \
	-I$(srcdir)/mpool \
	-I$(srcdir)/recno \
	-I$(top_srcdir)/libutil/include

