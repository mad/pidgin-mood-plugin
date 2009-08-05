CFLAGS = -O2 -pipe -shared -fPIC -DPIC
PURPLE_CFLAGS = $(CFLAGS) -DPURPLE_PLUGINS
PURPLE_CFLAGS += $(shell pkg-config --cflags purple)
PURPLE_CFLAGS += $(shell pkg-config --cflags pidgin)

# for win
PIDGIN_DIR=/home/mad/git/pidgin-clone/pidgin/plugins/
GTK_TOP=/home/mad/workspace/project/pidgin-dev/win32-dev/gtk_2_0/

all:
	gcc -Wall ${PURPLE_CFLAGS} pidgin-mood.c -o mood.so
	strip mood.so
	cp mood.so ~/.purple/plugins/

win:
	cp pidgin-mood.c $(PIDGIN_DIR)
	$(MAKE) -f Makefile.mingw -C $(PIDGIN_DIR) pidgin-mood.dll GTK_TOP=$(GTK_TOP)
	cp $(PIDGIN_DIR)/pidgin-mood.dll mood.dll
	i586-mingw32msvc-strip mood.dll

test: test.c
	gcc test.c -o test ${PURPLE_CFLAGS} -lglib -lpurple
