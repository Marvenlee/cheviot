
sbin_PROGRAMS = login

login_SOURCES = \
  common.c \
  copyrightstr.c \
	login.c
  
login_LDADD = -lc -lcrypt -ldb -lutil

AM_CFLAGS = -O0 -std=c99 -g2
AM_CPPFLAGS = -I$(srcdir)
