/* Minimal shim replacing Wine's internal debug-channel machinery.
 * We don't need real trace output, just something that compiles
 * without pulling in Wine-internal ntdll exports. */
#ifndef SHIM_WINE_DEBUG_H
#define SHIM_WINE_DEBUG_H

#include <stdio.h>
#include <string.h>

#define WINE_DEFAULT_DEBUG_CHANNEL(name) static const char __wine_dbch_name[] = #name
#define WINE_DECLARE_DEBUG_CHANNEL(name)
#define TRACE(...) do {} while (0)
#define TRACE_(ch) TRACE
#define FIXME(...) do { fprintf(stderr, "SHIM-FIXME: " __VA_ARGS__); fflush(stderr); } while (0)
#define WARN(...) do {} while (0)
#define ERR(...) do { fprintf(stderr, "SHIM-ERR: " __VA_ARGS__); fflush(stderr); } while (0)

static inline const char *debugstr_guid(const void *g)
{
    static char buf[64];
    const unsigned char *p = (const unsigned char *)g;
    unsigned int d1; unsigned short d2, d3;
    if (!g) return "(null)";
    memcpy(&d1, p, 4); memcpy(&d2, p + 4, 2); memcpy(&d3, p + 6, 2);
    snprintf(buf, sizeof(buf), "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        d1, d2, d3, p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
    return buf;
}
static inline const char *debugstr_w(const void *s) { (void)s; return "(wstr)"; }
static inline const char *debugstr_hstring(void *s) { (void)s; return "(hstring)"; }

#endif
