# $Id$

SUBDIRS = . baseq2 ctf xatrix rogue

bin_PROGRAMS = quake2 @VID_REFS@

EXTRA_PROGRAMS = ref_soft.so ref_softx.so ref_softsdl.so \
		ref_glx.so ref_sdlgl.so ref_fxgl.so

# Common source files
REF_GL_COMMON = q_sh.c q_shared.c glob.c qgl.c \
		gl_draw.c gl_image.c gl_light.c gl_mesh.c \
		gl_model.c gl_rmain.c gl_rmisc.c gl_rsurf.c \
		gl_warp.c

REF_SOFT_COMMON = r_aclip.c r_alias.c r_bsp.c r_draw.c r_edge.c \
		r_image.c r_light.c r_main.c r_misc.c r_model.c \
		r_part.c r_poly.c r_polyse.c r_rast.c r_scan.c \
		r_sprite.c r_surf.c \
		q_shared.c q_sh.c glob.c

if ASM_ARCH
REF_SOFT_ASM = r_aclipa.S r_draw16.S r_drawa.S r_edgea.S r_scana.S \
		r_spr8.S r_surf8.S math.S d_polysa.S r_varsa.S \
		sys_dosa.S
else
REF_SOFT_ASM =
endif

quake2_SOURCES = main.c q_sh.c vid_menu.c vid_so.c glob.c net_udp.c \
		\
		cl_cin.c cl_ents.c cl_fx.c cl_input.c cl_inv.c \
		cl_main.c cl_parse.c cl_pred.c cl_tent.c cl_scrn.c \
		cl_view.c cl_newfx.c console.c keys.c menu.c qmenu.c \
		\
		snd_dma.c snd_mix.c snd_mem.c \
		\
		checksum.c cmd.c cmodel.c common.c crc.c cvar.c \
		files.c mdfour.c net_chan.c \
		\
		sv_ccmds.c sv_ents.c sv_game.c sv_init.c sv_main.c \
		sv_send.c sv_user.c sv_world.c \
		\
		cd.c snd.c \
		\
		q_shared.c pmove.c \
		\
		m_flash.c
#		\
#		#snd_mixa.S

quake2_LDFLAGS = -lm -pthread @DL_LIBS@

#AM_CFLAGS = -Wall -Werror -pipe -I/usr/X11R6/include $(shell sdl-config --cflags) -DOPENGL  -fPIC
AM_CFLAGS = -Wall -Werror -pipe

# ref_glx
if BUILD_GLX
ref_glx_so_SOURCES = $(REF_GL_COMMON) gl_glx.c
ref_glx_so_CFLAGS = -fPIC @X_CFLAGS@ @OPENGL_CFLAGS@
ref_glx_so_LDFLAGS = -shared
#ref_glx_so_LDADD = -L/usr/X11R6/lib -lX11 -lXext -lXxf86dga -lXxf86vm -lGL
ref_glx_so_LDADD = @X_PRE_LIBS@ @X_LIBS@ @XTRA_LIBS@ @X_EXTRA_LIBS@ @OPENGL_LIBS@
endif

# ref_fxgl
if BUILD_FXGL
ref_fxgl_so_SOURCES = $(REF_GL_COMMON) rw_in_svgalib.c gl_fxmesa.c
ref_fxgl_so_CFLAGS = -fPIC @GLIDE_CFLAGS@ @X_CFLAGS@ @SVGALIB_CFLAGS@
#ref_fxgl_so_LDFLAGS = -shared -L/usr/local/glide/lib -L/usr/X11R6/lib -L/usr/local/lib -lX11 -lXext -lGL -lvga
#ref_fxgl_so_LDFADD = -L/usr/local/glide/lib -L/usr/X11R6/lib -L/usr/local/lib -lX11 -lXext -lGL -lvga
ref_fxgl_so_LDFLAGS = -shared
ref_fxgl_so_LDADD = @GLIDE_LIBS@ @X_LIBS@ @SVGALIB_LIBS@
endif

# ref_sdlgl
if BUILD_SDLGL
ref_sdlgl_so_SOURCES = $(REF_GL_COMMON) rw_sdl.c
ref_sdlgl_so_CFLAGS = -fPIC @SDL_CFLAGS@ -DOPENGL @OPENGL_CFLAGS@
ref_sdlgl_so_LDFLAGS = -shared
ref_sdlgl_so_LDADD = @SDL_LIBS@ @OPENGL_LIBS@
endif

# ref_soft
if BUILD_SOFT
if ASM_ARCH
SOFT_ASM=$(REF_SOFT_ASM) d_copy.S
rgreelse
SOFT_ASM=$(REF_SOFT_ASM)
endif
ref_soft_so_SOURCES = $(REF_SOFT_COMMON) $(SOFT_ASM) \
	rw_svgalib.c rw_in_svgalib.c
ref_soft_so_CFLAGS = -fPIC @SVGALIB_CFLAGS@
ref_soft_so_LDFLAGS = -shared
ref_soft_so_LDADD = @SVGALIB_LIBS@
endif

# ref_softx
if BUILD_SOFTX
ref_softx_so_SOURCES = $(REF_SOFT_COMMON) $(REF_SOFT_ASM) rw_x11.c
ref_softx_so_CFLAGS = -fPIC @X_CFLAGS@
ref_softx_so_LDFLAGS = -shared
#ref_softx_so_LDADD = -L/usr/X11R6/lib -lX11 -lXext -lXxf86dga -lXxf86vm
ref_softx_so_LDADD = @X_PRE_LIBS@ @X_LIBS@ @XTRA_LIBS@ @X_EXTRA_LIBS@
endif

# ref_softsdl
if BUILD_SOFTSDL
ref_softsdl_so_SOURCES = $(REF_SOFT_COMMON) $(REF_SOFT_ASM) rw_sdl.c
ref_softsdl_so_CFLAGS = -fPIC @SDL_CFLAGS@
ref_softsdl_so_LDFLAGS = -shared
ref_softsdl_so_LDADD = @SDL_LIBS@
endif
