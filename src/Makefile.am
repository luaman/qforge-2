# $Id$

# Process this file with automake to create Makefile.in

bin_PROGRAMS = quake2
quake2_SOURCES = main.c q_sh.c vid_menu.c vid_so.c glob.c net_udp.c \
				\
				cl_cin.c cl_ents.c cl_fx.c cl_input.c cl_inv.c \
				cl_main.c cl_parse.c cl_pred.c cl_tent.c cl_scrn.c cl_view.c \
				cl_newfx.c console.c keys.c menu.c qmenu.c \
				\
				snd_dma.c snd_mix.c snd_mem.c \
				\
				checksum.c cmd.c cmodel.c common.c crc.c cvar.c files.c \
				mdfour.c net_chan.c \
				\
				sv_ccmds.c sv_ents.c sv_game.c sv_init.c sv_main.c \
				sv_send.c sv_user.c sv_world.c \
				\
				cd.c snd.c \
				\
				q_shared.c pmove.c \
				\
				m_flash.c \
				\
				#snd_mixa.S
				
AM_CFLAGS = -Wall -Werror -pipe
AM_CPPFLAGS = -DLINUX_VERSION="\"$(VERSION)\"" -Dstricmp=strcasecmp -I../game
