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
*/

/* YARI's minimal timer implementation is based on that from NDS */
#include "SDL_config.h"

#include "SDL_thread.h"
#include "SDL_timer.h"
#include "SDL_error.h"
#include "../SDL_timer_c.h"

uint64_t yari_rdhwr_TSC(void);

static uint32_t scale, counter_overflow;
static uint64_t start_TSC, prev_TSC;

uint32_t mips32_rdhwr_counter(void);
uint32_t mips32_rdhwr_cycles_pr_count(void);


void SDL_StartTicks(void)
{
    uint32_t khz_frequency = 50*1000;
    uint32_t cycles_pr_count = mips32_rdhwr_cycles_pr_count();
    scale = (1ULL << 32) * cycles_pr_count / khz_frequency;
    counter_overflow = 0;
    start_TSC = mips32_rdhwr_counter();
}

/*
 * Get the number of milliseconds since the SDL library
 * initialization. Note that this value wraps if the program runs for
 * more than ~49 days
 */
Uint32 SDL_GetTicks(void)
{
    uint64_t t = mips32_rdhwr_counter();

    counter_overflow += t < prev_TSC; // Happens practically never
    prev_TSC = t;
    t += ((uint64_t) counter_overflow << 32) - start_TSC;

    return t * scale >> 32;
}

void SDL_Delay(Uint32 delay_ms)
{
    Uint32 t0 = SDL_GetTicks();

    while (SDL_GetTicks() < t0 + delay_ms)
        SDL_PumpEvents(); // Improve responsiveness at the expense of accuracy
}

/* This is only called if the event thread is not running */
int SDL_SYS_TimerInit(void)
{
        return 0;
}

void SDL_SYS_TimerQuit(void)
{
}

int SDL_SYS_StartTimer(void)
{
        SDL_SetError("Timers not implemented on YARI");
        return -1;
}

void SDL_SYS_StopTimer(void)
{
}
