/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2006 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org


    The YARI video implementation is based on SDL_dcvideo.c
*/
#include "SDL_config.h"

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

/* Hidden "this" pointer for the video functions */
#define _THIS	SDL_VideoDevice *this

/* Initialization/Query functions */
static int YARI_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **YARI_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *YARI_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int YARI_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors);
static void YARI_VideoQuit(_THIS);

/* Hardware surface functions */
static int YARI_AllocHWSurface(_THIS, SDL_Surface *surface);
static int YARI_LockHWSurface(_THIS, SDL_Surface *surface);
static void YARI_UnlockHWSurface(_THIS, SDL_Surface *surface);
static void YARI_FreeHWSurface(_THIS, SDL_Surface *surface);
static int YARI_FlipHWSurface(_THIS, SDL_Surface *surface);

/* etc. */
static void YARI_UpdateRects(_THIS, int numrects, SDL_Rect *rects);

/* YARI driver bootstrap functions */

void YARI_InitOSKeymap(_THIS)
{
}

static uint32_t keys;

#define RS232IN_DATA (*(volatile unsigned *) 0xFF000004)
#define RS232IN_TAG  (*(volatile unsigned *) 0xFF000008)
#define SER_OUTBUSY() (*(volatile unsigned *)0xFF000000 != 0)
#define SER_OUT(data) (*(volatile unsigned *)0xFF000000 = (data))
#define READKEYS      (*(volatile unsigned *) 0xFF000010)

void YARI_PumpEvents(_THIS)
{
    int posted;

    do {
        uint32_t new_keys = READKEYS;
        int i;
        SDL_keysym keysym;
        SDL_Event ev;

	posted = 0;

        keysym.mod = 0;
        keysym.scancode = 0xff;
        memset(&ev, 0, sizeof ev);

        for (i = 0; i < 10; ++i)
            if ((1 << i) & (keys ^ new_keys)) {
                // 0 = ESCAPE, 1 = LEFT, 2 = SPACE, 3 = RIGHT
                switch (i) {
                case 0: keysym.sym = SDLK_ESCAPE; break;
                case 1: keysym.sym = SDLK_LEFT; break;
                case 2: keysym.sym = SDLK_RIGHT; break;
                case 3: keysym.sym = SDLK_SPACE; break;
                default: keysym.sym = '0' + i; break;
                }
                keysym.unicode = keysym.sym;
                ev.type = (1 << i) & new_keys ? SDL_KEYDOWN : SDL_KEYUP;
                ev.key.state = 0;
                ev.key.keysym = keysym;

                SDL_PushEvent(&ev);

                keys ^= 1 << i;
                ++posted;
            }
    } while (posted);
}

static int YARI_Available(void)
{
	return 1;
}

static void YARI_DeleteDevice(SDL_VideoDevice *device)
{
	SDL_free(device);
}

static SDL_VideoDevice *YARI_CreateDevice(int devindex)
{
	SDL_VideoDevice *device;

	/* Initialize all variables that we clean on shutdown */
	device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
	if (device)
		SDL_memset(device, 0, sizeof *device);

	if (!device) {
		SDL_OutOfMemory();

		return 0;
	}

	/* Set the function pointers */
	device->VideoInit = YARI_VideoInit;
	device->ListModes = YARI_ListModes;
	device->SetVideoMode = YARI_SetVideoMode;
	device->CreateYUVOverlay = NULL;
	device->SetColors = YARI_SetColors;
	device->UpdateRects = YARI_UpdateRects;
	device->VideoQuit = YARI_VideoQuit;
	device->AllocHWSurface = YARI_AllocHWSurface;
	device->CheckHWBlit = NULL;
	device->FillHWRect = NULL;
	device->SetHWColorKey = NULL;
	device->SetHWAlpha = NULL;
	device->LockHWSurface = YARI_LockHWSurface;
	device->UnlockHWSurface = YARI_UnlockHWSurface;
	device->FlipHWSurface = YARI_FlipHWSurface;
	device->FreeHWSurface = YARI_FreeHWSurface;
	device->SetCaption = NULL;
	device->SetIcon = NULL;
	device->IconifyWindow = NULL;
	device->GrabInput = NULL;
	device->GetWMInfo = NULL;
	device->InitOSKeymap = YARI_InitOSKeymap;
	device->PumpEvents = YARI_PumpEvents;

	device->free = YARI_DeleteDevice;

	return device;
}

VideoBootStrap YARI_bootstrap = {
	"yarivideo", "YARI Video",
	YARI_Available, YARI_CreateDevice
};


int YARI_VideoInit(_THIS, SDL_PixelFormat *vformat)
{
    /* YARI's video interface only supports RGB332 (= 8-bit) */
    vformat->BitsPerPixel = 8;
    vformat->Rmask = 7 << 5;
    vformat->Gmask = 7 << 3;
    vformat->Bmask = 3;

    return 0;
}

/* Currently, YARI only supports 1024x768 */
static SDL_Rect RECT_1024x768 = {0,0,1024,768};
static SDL_Rect *vid_modes[] = {
    &RECT_1024x768,
    NULL
};

SDL_Rect **YARI_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
    if (format->BitsPerPixel != 8)
        return NULL;

    return vid_modes;
}

SDL_Surface *YARI_SetVideoMode(_THIS, SDL_Surface *current,
                               int width, int height, int bpp, Uint32 flags)
{
    int disp_mode,pixel_mode,pitch;
    Uint32 Rmask, Gmask, Bmask;

    if (width > 1024 || height > 768 || bpp != 8 || (flags & SDL_DOUBLEBUF)) {
        SDL_SetError("Couldn't find requested mode in list");
        return NULL;
    }

    Rmask = 7 << 5;
    Gmask = 7 << 3;
    Bmask = 3;

    /* Set up the new mode framebuffer */
    current->flags  = SDL_FULLSCREEN | SDL_HWSURFACE;
    current->w      = width;
    current->h      = height;
    current->pitch  = 1024;
    current->pixels = (void *) 0x40100000;

    if (!SDL_ReallocFormat(current, bpp, Rmask, Gmask, Bmask, 0)) {
        return NULL;
    }

    return current;
}

/* We don't actually allow hardware surfaces other than the main one */
static int YARI_AllocHWSurface(_THIS, SDL_Surface *surface)
{
	return -1;
}
static void YARI_FreeHWSurface(_THIS, SDL_Surface *surface)
{
}

/* We need to wait for vertical retrace on page flipped displays */
static int YARI_LockHWSurface(_THIS, SDL_Surface *surface)
{
    return 0;
}

static void YARI_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
}

static int YARI_FlipHWSurface(_THIS, SDL_Surface *surface)
{
	if (surface->flags & SDL_DOUBLEBUF)
            return -1;

	return 0;
}

static void YARI_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
{
	/* do nothing. */
}

static int YARI_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
	/* do nothing of note. */
	return 1;
}

/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
static void YARI_VideoQuit(_THIS)
{
}
