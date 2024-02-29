
sbin_PROGRAMS = login

login_SOURCES = \
  common.c \
  copyrightstr.c \
	login.c
  
login_LDADD = -lcrypt -lbsdcompat -ldb

AM_CFLAGS = -O0 -std=c99 -g2
AM_CPPFLAGS = -I$(srcdir)
