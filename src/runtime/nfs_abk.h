/*
 * nfs_abk.h - x86 routines carried by EA's AEMS sound banks (the .abk files under Sound).
 *
 * Speed.exe copies a bank to its heap, patches the bank's call operands to
 * point into its own AEMS function table and then calls routines of the
 * bank as callbacks. Those routines are lifted ahead of time from the local
 * banks (scripts/abk_module.py, run_lift.py --prefix abk_; output in
 * src/recomp/gen_abk/, never committed). At run time a call to a heap address
 * is matched against the original bytes of each block (call operands masked)
 * and runs the lifted routine at the same offset.
 */
#ifndef NFS_ABK_H
#define NFS_ABK_H
#include <stdint.h>
#include "recomp_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t va;              /* the block's address in the analysis module */
    const uint8_t *code;      /* original bytes, call operands unpatched */
    uint32_t len;
    const uint16_t *rel;      /* offsets of the 4-byte call operands the game patches */
    uint32_t nrel;
} nfs_abk_block_t;

extern const nfs_abk_block_t nfs_abk_blocks[];
extern const unsigned nfs_abk_block_count;
extern const recomp_dispatch_entry_t abk_recomp_dispatch_table[];
extern const uint32_t abk_recomp_dispatch_count;

/* Provided by each host: 1 if guest [va, va+n) can be read. */
int nfs_guest_readable(uint32_t va, uint32_t n);

/* The lifted routine for a guest address inside a loaded bank, or NULL. */
recomp_func_t nfs_abk_lookup(uint32_t va);

#ifdef __cplusplus
}
#endif
#endif
