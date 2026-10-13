/*
 * widescreen.c - NFSU1 wide-display support: NOT PORTED YET.
 *
 * The NFSU2 module patches SPEED2.EXE at sites found from ThirteenAG's
 * NFSUnderground2.WidescreenFix; none of those addresses apply to Speed.exe.
 * Until the NFSUnderground.WidescreenFix sites are located in Speed.exe by
 * their byte patterns, this keeps the same interface with no effect, and a
 * RECOMP_HOOK reaching the host stops loudly (config/hooks.txt is empty).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include "nfs_widescreen.h"
#include "nfs_runtime.h"

void nfs_widescreen_init(const char *game_root) { (void)game_root; }
void nfs_widescreen_set_resolution(unsigned width, unsigned height) { (void)width; (void)height; }
int nfs_widescreen_hud_entries(void) { return 0; }
void nfs_widescreen_set_minimap_offset(float dy) { (void)dy; }
void nfs_widescreen_set_frame_rate(unsigned fps) { (void)fps; }
void nfs_widescreen_apply_frame_rate(void) {}

void recomp_hook(uint32_t va) {
    char why[96];
    snprintf(why, sizeof why, "RECOMP_HOOK at 0x%08X but no NFSU1 hook is implemented", va);
    nfs_report(why, 0);
    ExitProcess(3);
}
