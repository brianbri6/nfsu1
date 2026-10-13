/*
 * nfs_runtime.c - runtime pieces the lifted Speed.exe links against that are
 * specific to this project: SSE state, the loud-failure hooks, and the crash
 * report. The machine itself (registers, dispatch, bridge) is pcrecomp's
 * native32.
 */
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "recomp_types.h"
#include "recomp_trace.h"
#include "native32.h"
#include "nfs_runtime.h"
#include "nfs_abk.h"
#include "nfs_log.h"

/* The SSE register file (g_xmm, g_mxcsr) is defined by native32 with the
 * rest of the register file. */

/* ---- names ---- */
const char *nfs_native_name(uint32_t va) { return native32_name(va); }

/* ---- the report ---- */
static volatile LONG g_reporting;

static void out(FILE *f, const char *fmt, ...) {
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    nfs_log(NFS_CRASH, "%s", line);
    if (f) { fputs(line, f); fputc('\n', f); }
}

static int readable(uint32_t va, uint32_t n) {
    MEMORY_BASIC_INFORMATION mbi;
    if (va < 0x10000) return 0;
    if (!VirtualQuery((void *)(uintptr_t)va, &mbi, sizeof mbi)) return 0;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return 0;
    return va + n <= (uint32_t)(uintptr_t)mbi.BaseAddress + (uint32_t)mbi.RegionSize;
}

void nfs_report(const char *why, const void *host_ctx) {
    if (InterlockedExchange(&g_reporting, 1)) return;   /* one report per process */
    char path[700];
    snprintf(path, sizeof path, "%s/crash_%s.txt", nfs_log_dir(), nfs_log_stamp());
    FILE *f = fopen(path, "w");
    out(f, "==== NFSU recomp crash report ====");
    out(f, "reason      : %s", why);
    out(f, "thread      : %lu", GetCurrentThreadId());
    out(f, "guest func  : sub_%08X (the lifted function running)", g_cur_func);
    out(f, "last import : %s", g_cur_import ? g_cur_import : "(none)");
    if (host_ctx) {
        const CONTEXT *c = (const CONTEXT *)host_ctx;
        HMODULE m = NULL;
        char mod[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)(uintptr_t)c->Eip, &m))
            GetModuleFileNameA(m, mod, sizeof mod);
        out(f, "host EIP    : %08X in %s (+0x%X)", c->Eip, mod,
            m ? c->Eip - (DWORD)(uintptr_t)m : 0);
        /* After a jump into data the caller is only visible on the host stack. */
        char line[160]; int n = 0;
        for (int i = 0; i < 8 && readable(c->Esp + 4u * i, 4); i++)
            n += snprintf(line + n, sizeof line - n, " %08X", *(uint32_t *)(uintptr_t)(c->Esp + 4u * i));
        out(f, "host stack  :%s", n ? line : " <unreadable>");
        n = 0;
        for (int i = 0; i < 32 && readable(c->Eip + i, 1); i++)
            n += snprintf(line + n, sizeof line - n, " %02X", *(uint8_t *)(uintptr_t)(c->Eip + i));
        out(f, "bytes @EIP  :%s", n ? line : " <unreadable>");
        /* Speed.exe timer-callback list (head 0x713C70; walked by 0x652960:
         * node = { next, ?, fn, arg }): a heap fn here is the known crash. */
        uint32_t node = readable(0x713C70u, 4) ? *(uint32_t *)0x713C70u : 0;
        for (int i = 0; i < 16 && node && readable(node, 16); i++) {
            const uint32_t *w = (const uint32_t *)(uintptr_t)node;
            out(f, "  timer node %08X: next %08X  %08X  fn %08X  arg %08X", node, w[0], w[1], w[2], w[3]);
            node = w[0];
        }
        for (int i = 0; i < 64 && readable(c->Esp + 4u * i, 4); i++) {
            uint32_t v = *(uint32_t *)(uintptr_t)(c->Esp + 4u * i);
            HMODULE vm = NULL; char vn[MAX_PATH];
            if (v < 0x10000u || !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)(uintptr_t)v, &vm)) continue;
            GetModuleFileNameA(vm, vn, sizeof vn);
            const char *b = strrchr(vn, '\\');
            out(f, "  host [esp+%02X] %08X = %s+0x%X", i * 4, v, b ? b + 1 : vn, v - (uint32_t)(uintptr_t)vm);
        }
    }
    out(f, "guest regs  : EAX=%08X EBX=%08X ECX=%08X EDX=%08X", g_eax, g_ebx, g_ecx, g_edx);
    out(f, "              ESI=%08X EDI=%08X EBP=%08X ESP=%08X", g_esi, g_edi, g_ebp, g_esp);
    out(f, "guest flags : kind=%u a=%08X b=%08X cf=%u   FS base=%08X", g_flag_k, g_flag_a,
        g_flag_b, g_flag_cf, g_fs_base);
    out(f, "x87         : top=%d cw=%04X st0=%g st1=%g", g_fp_top, g_fpu_cw, g_st[0], g_st[1]);

    out(f, "guest stack (ESP upward; values inside .text marked '<- code'):");
    for (int i = 0; i < 48; i++) {
        uint32_t a = g_esp + 4u * i;
        if (!readable(a, 4)) { out(f, "  %08X  <unreadable>", a); break; }
        uint32_t v = *(uint32_t *)(uintptr_t)a;
        int code = v >= 0x00401000u && v < 0x00697000u;   /* Speed.exe .text */
        out(f, "  %08X  %08X%s", a, v, code ? "  <- code" : "");
    }

    out(f, "last indirect calls (newest first; %u total):", g_icall_count);
    for (uint32_t i = 1; i <= 32 && i <= g_icall_trace_idx; i++) {
        uint32_t k = (g_icall_trace_idx - i) & (ICALL_TRACE_SIZE - 1);
        const char *n = native32_name(g_icall_trace[k]);
        out(f, "  -> %08X from sub_%08X %s", g_icall_trace[k], g_icall_from[k], n ? n : "");
    }

    out(f, "last native calls (newest first):");
    for (uint32_t i = 1; i <= 32 && i <= g_native_ring_idx; i++) {
        native32_call_rec_t *r = &g_native_ring[(g_native_ring_idx - i) & (NATIVE32_RING - 1)];
        const char *n = native32_name(r->target);
        out(f, "  [t%lu] %-36s from sub_%08X (%08X %08X %08X) -> %s%08X", (unsigned long)r->tid,
            n ? n : "?", r->from, r->a0, r->a1, r->a2, r->done ? "" : "(running) ", r->ret);
    }
    recomp_dump_trace(why);
    out(f, "==== end (written to %s) ====", f ? path : "(no file)");
    if (f) fclose(f);
    nfs_log_flush();
}

/* ---- loud failures from generated code ---- */
void recomp_unimpl(uint32_t va, const char *what) {
    char why[300];
    snprintf(why, sizeof why, "RECOMP_UNIMPL at guest 0x%08X: %s", va, what);
    nfs_report(why, NULL);
    TerminateProcess(GetCurrentProcess(), 3);
}

void recomp_unresolved(const char *kind, uint32_t va, uint32_t from) {
    char why[300];
    const char *n = native32_name(va);
    snprintf(why, sizeof why, "%s to unlifted guest address 0x%08X from sub_%08X %s",
             kind, va, from, n ? n : "");
    nfs_report(why, NULL);
    TerminateProcess(GetCurrentProcess(), 4);
}

/* ---- manual overrides, then the lifted routines of loaded sound banks ----
 * This runs first on EVERY indirect call: Speed.exe targets leave at once, and
 * each DLL/shim target is checked against the banks only on its first call. */
int nfs_guest_readable(uint32_t va, uint32_t n) { return readable(va, n); }

#define NOT_BANK 1024
static volatile uint32_t g_not_bank[NOT_BANK];
recomp_func_t recomp_lookup_manual(uint32_t va) {
    recomp_func_t f = (recomp_func_t)nfs_override_lookup(va);
    if (f || (va >= 0x00400000u && va < 0x007A0000u)) return f;
    uint32_t h = (va * 2654435761u) >> 22;
    if (g_not_bank[h] == va) return NULL;
    f = nfs_abk_lookup(va);
    if (!f) g_not_bank[h] = va;
    return f;
}

/* ---- host exceptions ----
 *
 * Only faults raised in OUR code (the host exe: lifted bodies and runtime) or
 * at a guest address are reported. Native libraries and drivers raise and
 * handle their own exceptions all the time, and C++ throws pass through here
 * too; reporting those would bury the real crash. */
static uint32_t g_host_lo, g_host_hi;

static LONG CALLBACK crash_veh(EXCEPTION_POINTERS *ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    uint32_t pc = (uint32_t)(uintptr_t)ep->ExceptionRecord->ExceptionAddress;
    int fatal = code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
                code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_STACK_OVERFLOW ||
                code == EXCEPTION_PRIV_INSTRUCTION || code == STATUS_HEAP_CORRUPTION;
    /* A jump into data (DEP execute fault) is always ours to report: no
     * library executes its heap, but a bad guest function pointer does. */
    int dep = code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->ExceptionInformation[0] == 8;
    int ours = (pc >= g_host_lo && pc < g_host_hi) || native32_in_guest(pc) || dep;
    if (fatal && ours) {
        char why[256];
        if (code == EXCEPTION_ACCESS_VIOLATION)
            snprintf(why, sizeof why, "host exception %08lX (%s of %08lX)", code,
                     ep->ExceptionRecord->ExceptionInformation[0] == 0 ? "read" :
                     ep->ExceptionRecord->ExceptionInformation[0] == 1 ? "write" : "execute",
                     (unsigned long)ep->ExceptionRecord->ExceptionInformation[1]);
        else
            snprintf(why, sizeof why, "host exception %08lX", code);
        nfs_report(why, ep->ContextRecord);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void nfs_install_crash_handler(void) {
    HMODULE self = GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((BYTE *)self + ((IMAGE_DOS_HEADER *)self)->e_lfanew);
    g_host_lo = (uint32_t)(uintptr_t)self;
    g_host_hi = g_host_lo + nt->OptionalHeader.SizeOfImage;
    AddVectoredExceptionHandler(0, crash_veh);
}
