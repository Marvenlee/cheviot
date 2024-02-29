# Setup the build/host directory structure, effectively laying the keel
# of the project.  Creates the directory tree starting from the root '/'
# directory pointed to by the --prefix option.
#
# This script will be run by pseudo, so will all directories will be owned
# by "root" whenever pseudo is run.  Pseudo is also used to install files
# in other recipes so these all gain root ownership and avoids host uid/gid
# pollution.
#
# TODO: Restrict rwx user/group settings for some directories.
#

install-data-local:
	install -d $(prefix)/bin
	install -d $(prefix)/boot
	install -d $(prefix)/dev
	install -d $(prefix)/etc
	install -d $(prefix)/etc/opt
	install -d $(prefix)/home
	install -d $(prefix)/lib
	install -d $(prefix)/media
	install -d $(prefix)/media/root
	install -d $(prefix)/mnt
	install -d $(prefix)/root -m 0700 
	install -d $(prefix)/sbin
	install -d $(prefix)/sys
	install -d $(prefix)/tmp
	install -d $(prefix)/usr
	install -d $(prefix)/usr/bin
	install -d $(prefix)/usr/lib
	install -d $(prefix)/usr/sbin
	install -d $(prefix)/var
  

