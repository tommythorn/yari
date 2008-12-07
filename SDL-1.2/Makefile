CC = mips-elf-gcc -msoft-float -g
AR = mips-elf-ar

TARGET = libSDL.a
SDLMAIN_TARGET = libSDLmain.a

CFLAGS=$(KOS_CFLAGS) $(DEFS) -Iinclude -O3

SRCS = \
	src/audio/dummy/SDL_dummyaudio.c \
	src/audio/SDL_audio.c \
	src/audio/SDL_audiocvt.c \
	src/audio/SDL_audiodev.c \
	src/audio/SDL_mixer.c \
	src/audio/SDL_wave.c \
	src/cdrom/dummy/SDL_syscdrom.c \
	src/cdrom/SDL_cdrom.c \
	src/events/SDL_active.c \
	src/events/SDL_events.c \
	src/events/SDL_expose.c \
	src/events/SDL_keyboard.c \
	src/events/SDL_mouse.c \
	src/events/SDL_quit.c \
	src/events/SDL_resize.c \
	src/file/SDL_rwops.c \
	src/joystick/dummy/SDL_sysjoystick.c \
	src/joystick/SDL_joystick.c \
	src/loadso/dummy/SDL_sysloadso.c \
	src/SDL.c \
	src/SDL_error.c \
	src/SDL_fatal.c \
	src/stdlib/SDL_getenv.c \
	src/stdlib/SDL_iconv.c \
	src/stdlib/SDL_malloc.c \
	src/stdlib/SDL_qsort.c \
	src/stdlib/SDL_stdlib.c \
	src/stdlib/SDL_string.c \
	src/thread/generic/SDL_syscond.c \
	src/thread/generic/SDL_sysmutex.c \
	src/thread/generic/SDL_syssem.c \
	src/thread/generic/SDL_systhread.c \
	src/thread/SDL_thread.c \
	src/timer/yari/yari_hwr.c \
	src/timer/yari/SDL_systimer.c \
	src/timer/SDL_timer.c \
	src/video/yari/SDL_yarivideo.c \
	src/video/dummy/SDL_nullevents.c \
	src/video/dummy/SDL_nullmouse.c \
	src/video/dummy/SDL_nullvideo.c \
	src/video/SDL_blit.c \
	src/video/SDL_blit_0.c \
	src/video/SDL_blit_1.c \
	src/video/SDL_blit_A.c \
	src/video/SDL_blit_N.c \
	src/video/SDL_bmp.c \
	src/video/SDL_cursor.c \
	src/video/SDL_gamma.c \
	src/video/SDL_pixels.c \
	src/video/SDL_RLEaccel.c \
	src/video/SDL_stretch.c \
	src/video/SDL_surface.c \
	src/video/SDL_video.c \
	src/video/SDL_yuv.c \
	src/video/SDL_yuv_sw.c \
	src/cpuinfo/SDL_cpuinfo.c

OBJS = $(SRCS:.c=.o)

SDLMAIN_SRC = src/main/dummy/SDL_dummy_main.c
SDLMAIN_OBJ = $(SDLMAIN_SRC:.c=.o)


TEST = \
	test/checkkeys.c \
	test/graywin.c \
	test/loopwave.c \
	test/testalpha.c \
	test/testbitmap.c \
	test/testcdrom.c \
	test/testerror.c \
	test/testgamma.c \
	test/testgl.c \
	test/testhread.c \
	test/testjoystick.c \
	test/testkeys.c \
	test/testlock.c \
	test/testoverlay.c \
	test/testpalette.c \
	test/testsem.c \
	test/testsprite.c \
	test/testtimer.c \
	test/testtypes.c \
	test/testver.c \
	test/testvidinfo.c \
	test/testwin.c \
	test/testwm.c \
	test/threadwin.c \
	test/torturethread.c

all: $(TARGET) $(SDLMAIN_TARGET)

install: all
	install -d                          $(YARI_XTOOLS_INSTALL_PREFIX)/mips-elf/lib \
					    $(YARI_XTOOLS_INSTALL_PREFIX)/mips-elf/include
	install $(TARGET) $(SDLMAIN_TARGET) $(YARI_XTOOLS_INSTALL_PREFIX)/mips-elf/lib
	install include/*                   $(YARI_XTOOLS_INSTALL_PREFIX)/mips-elf/include

$(TARGET): copy_config $(OBJS) Makefile
	$(AR) rcs $(TARGET) $(OBJS)

$(SDLMAIN_TARGET): $(SDLMAIN_OBJ) Makefile
	$(AR) rcs $(SDLMAIN_TARGET) $(SDLMAIN_OBJ)

copy_config:
	@cp include/SDL_config.h.default include/SDL_config.h

clean:
	rm -f include/SDL_config.h $(OBJS)
