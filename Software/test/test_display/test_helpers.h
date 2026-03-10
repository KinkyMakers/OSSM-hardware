#ifndef TEST_DISPLAY_HELPERS_H
#define TEST_DISPLAY_HELPERS_H

#include <unity.h>
#include <ui.h>
#include "pbm.h"

extern u8g2_t u8g2;
extern const char* OUTPUT_DIR;
extern const char* PNG_DIR;

static inline bool savePBMGrouped(u8g2_t* u8g2, const char* group,
                                  const char* name) {
    char dir[256], path[256];
    snprintf(dir, sizeof(dir), "%s/%s", OUTPUT_DIR, group);
    ensureDirRecursive(dir);
    snprintf(path, sizeof(path), "%s/%s.pbm", dir, name);
    bool ok = savePBM(u8g2, path);
    if (ok) printf("  -> Saved %s\n", path);
    return ok;
}

#endif  // TEST_DISPLAY_HELPERS_H
