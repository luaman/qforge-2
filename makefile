MAKEDEPS=$(CC) -MM $(CPPFLAGS) $< | sed -e 's!$*\.o:*!$*\.o $@:!g' > $@

%.d: %.c
	$(MAKEDEPS)

CPPFLAGS=-DELF

sources=\
	client/cl_cin.c		\
	client/cl_ents.c	\
	client/cl_fx.c		\
	client/cl_input.c	\
	client/cl_inv.c		\
	client/cl_main.c	\
	client/cl_newfx.c	\
	client/cl_parse.c	\
	client/cl_pred.c	\
	client/cl_scrn.c	\
	client/cl_tent.c	\
	client/cl_view.c	\
	client/console.c	\
	client/keys.c		\
	client/menu.c		\
	client/qmenu.c		\
	client/snd_dma.c	\
	client/snd_mem.c	\
	client/snd_mix.c	\
	client/x86.c		\
	game/m_flash.c		\
	game/q_shared.c		\
	linux/cd_linux.c	\
	linux/glob.c		\
	linux/net_udp.c		\
	linux/q_shlinux.c	\
	linux/snd_linux.c	\
	linux/snd_mixa.S	\
	linux/sys_linux.c	\
	linux/vid_so.c		\
	linux/vid_menu.c	\
	qcommon/cmd.c		\
	qcommon/cmodel.c	\
	qcommon/common.c	\
	qcommon/crc.c		\
	qcommon/cvar.c		\
	qcommon/files.c		\
	qcommon/md4.c		\
	qcommon/net_chan.c	\
	qcommon/pmove.c		\
	server/sv_ccmds.c	\
	server/sv_ents.c	\
	server/sv_game.c	\
	server/sv_init.c	\
	server/sv_main.c	\
	server/sv_send.c	\
	server/sv_user.c	\
	server/sv_world.c

objects=$(patsubst %.S,%.o,\
		$(patsubst %.c,%.o,\
		$(patsubst %.cc,%.o,\
		  $(sources))))

quake2: $(objects)
	gcc -o $@ $^ -lm -ldl

-include $(objects:.o=.d)
