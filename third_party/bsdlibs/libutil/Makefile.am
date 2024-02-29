lib_LIBRARIES = libutil.a

libutil_a_SOURCES = \
	efun.c \
	login.c loginx.c   logout.c  \
	logwtmp.c  \
	passwd.c pw_scan.c pidfile.c pidlock.c  \
	raise_default_signal.c \
	secure_path.c  \
	strpct.c ttyaction.c \
	getusershell.c
	
#ttymsg.c
#stat_flags.c 
#pty.c
# login_tty.c
# login_cap.c
# logoutx.c
# logwtmpx.c
#	getfsspecname.c getmntopts.c getmaxpartitions.c \
#	if_media.c \
# opendisk.c \
#sockaddr_snprintf.c
#   


subdirheadersdir = $(includedir)/
subdirheaders_HEADERS = \
	include/util.h

AM_CFLAGS = -O2 -std=c99 -g0 -I.
AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(srcdir)/include \
	-I$(top_srcdir)/libutil/include
	
