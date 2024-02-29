sbin_PROGRAMS = pwd_mkdb

pwd_mkdb_SOURCES = \
  pwd_mkdb.c
  
pwd_mkdb_LDADD = -lcrypt -lbsdcompat -ldb -lutil

AM_CFLAGS = -O0 -std=c99 -g2
AM_CPPFLAGS = -I$(srcdir)
