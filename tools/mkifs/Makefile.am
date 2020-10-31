noinst_PROGRAMS = mkifs


mkifs_SOURCES = \
  main.c

# TODO: How to force GCC (not arm-none-eabi)

#CC = gcc
#COMPILE = gcc -O2 -std=c99 -g0
#LINK = gcc

CC_FOR_BUILD = gcc
CXXFLAGS_FOR_BUILD = -std=c99
CFLAGS_FOR_BUILD = -std=c99

LINK_FOR_BUILD = $(CC_FOR_BUILD) $(CXXFLAGS_FOR_BUILD) $(LDFLAGS_FOR_BUILD) $(TARGET_ARCH_FOR_BUILD)

mkifs$(BUILD_EXEEXT) : $(mkifs_OBJECTS)
	$(LINK_FOR_BUILD) $^ -o $@

$(mkifs_OBJECTS) : CC=$(CC_FOR_BUILD)
$(mkifs_OBJECTS) : CXXFLAGS=$(CXXFLAGS_FOR_BUILD)
$(mkifs_OBJECTS) : CFLAGS=$(CFLAGS_FOR_BUILD)

