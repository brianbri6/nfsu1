/*
 * widescreen.c - NFSU1 render size and 3D FOV for wide displays.
 *
 * A port of parts of ThirteenAG's NFSUnderground.WidescreenFix
 * (WidescreenFixesPack, MIT): Rendering.ixx (FixFOV) and the render-size side
 * of Resolution.ixx. That mod patches x86 bytes in memory; the game here is
 * lifted C, so each patch is a host hook (config/hooks.txt -> RECOMP_HOOK ->
 * recomp_hook) right after the instruction whose result it changes. The hook
 * VAs were located in the pinned Speed.exe 1.4 by the mod's byte patterns.
 *
 * Off until a host gives the display aspect (Android: the surface; the Windows
 * bring-up host keeps the original 4:3 game). Front-end and HUD placement
 * (the mod's Frontend.ixx) are not ported yet: 2D stays stretched.
 * Shared by the Windows host and the Android runtime.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp_types.h"
#include "nfs_widescreen.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "nfs_runtime.h"
#elif defined(__ANDROID__)
#include <android/log.h>
#endif

static float s_display_aspect;          /* 0: off, the game keeps 4:3 */
static float s_aspect = 4.0f / 3.0f;    /* the render target's, once widened */
/* FOV constants of the three reads after 0x40DDD2, chosen per view id. */
static double s_fov_scale[3] = {1.0, 0.5, 1.0};
static int s_fe_cursor;                 /* draw the front-end arrow in FE units */

/* The front end's arrow (sub_004F3030) takes the cursor as 640x480 front-end
 * units while the cursor and its hit tests are in render pixels; they only
 * agree at 640x480. Hosts without the Win32 cursor (Android) draw the arrow. */
static uint32_t fe_units(uint32_t pixels, uint32_t size_va, float units) {
    uint32_t size = MEM32(size_va);
    if (!size || size > 8192) return pixels;
    return (uint32_t)(int32_t)lroundf((float)(int32_t)pixels * units / (float)size);
}
void nfs_frontend_cursor_units(int on) { s_fe_cursor = on; }


void nfs_widescreen_init(const char *game_root) { (void)game_root; }
void nfs_widescreen_set_display_aspect(float aspect) {
    s_display_aspect = aspect > 1.34f ? aspect : 0.0f;
}
void nfs_widescreen_set_resolution(unsigned width, unsigned height) {
    if (width && height) s_aspect = (float)width / (float)height;
}
int nfs_widescreen_hud_entries(void) { return 0; }
void nfs_widescreen_set_minimap_offset(float dy) { (void)dy; }
void nfs_widescreen_set_frame_rate(unsigned fps) { (void)fps; }
void nfs_widescreen_apply_frame_rate(void) {}

void recomp_hook(uint32_t va) {
    /* Arrow draw calls: X/Y loaded, about to be pushed for sub_004F3030. */
    if (va == 0x00409EB8u || va == 0x00401832u) {
        if (!s_fe_cursor) return;
        if (va == 0x00409EB8u) { g_ecx = fe_units(g_ecx, 0x0071A96Cu, 640.0f); g_eax = fe_units(g_eax, 0x0071A970u, 480.0f); }
        else { g_eax = fe_units(g_eax, 0x0071A96Cu, 640.0f); g_edx = fe_units(g_edx, 0x0071A970u, 480.0f); }
        return;
    }
    if (s_display_aspect <= 0.0f) return;
    switch (va) {
    /* End of the resolution setter (0x4086B0, width pushed, height in esi):
     * it filled D3DPRESENT_PARAMETERS (0x71A96C) and the screen size globals
     * (0x701034/0x701038). Keep the chosen height, widen to the display. */
    case 0x0040875Au: {
        uint32_t height = MEM32(0x0071A970u);
        uint32_t width = (uint32_t)lroundf((float)height * s_display_aspect) & ~1u;
        if (height && width > MEM32(0x0071A96Cu)) {
            MEM32(0x0071A96Cu) = width;
            MEM32(0x00701034u) = width;
            s_aspect = (float)width / (float)height;
        }
        break;
    }
    /* FixFOV: ST0 was viewport height/width; the mod loads a per-view scale
     * (rear-view mirror 3: 0.45) and picks the constants of the three later
     * reads by view id ([ebp+4]; 1 and 4: headlights and reflections). */
    case 0x0040DDE0u: {
        uint32_t view = MEM32(g_ebp + 4);
        if (view == 1 || view == 4) {
            s_fov_scale[0] = 1.0 / (s_aspect / (4.0 / 3.0)); s_fov_scale[1] = 0.4525; s_fov_scale[2] = 1.28;
        } else {
            s_fov_scale[0] = 1.0; s_fov_scale[1] = 0.5; s_fov_scale[2] = 1.0;
        }
        g_st[0] = view == 3 ? 0.45 : 1.0;
        MEM32(g_esp + 0x20) = 0x3F800000u;   /* 1.0f, as the mod stores */
        break;
    }
    /* `fmul [0.5]` (0x40DE43), `fdivr [1.0]` (0x40DEBB, 0x40DEEC): the mod
     * repoints those operands; rescale the results by new/original. */
    case 0x0040DE49u: g_st[0] *= s_fov_scale[1] / 0.5; break;
    case 0x0040DEC1u: g_st[0] *= s_fov_scale[0]; break;
    case 0x0040DEF2u: g_st[0] *= s_fov_scale[2]; break;
    default: {
        char why[96];
        snprintf(why, sizeof why, "RECOMP_HOOK at 0x%08X has no NFSU1 handler", (unsigned)va);
#ifdef _WIN32
        nfs_report(why, 0);
        ExitProcess(3);
#else
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_FATAL, "NFSU1", "%s", why);
#endif
        fprintf(stderr, "%s\n", why);
        abort();
#endif
    }
    }
}
