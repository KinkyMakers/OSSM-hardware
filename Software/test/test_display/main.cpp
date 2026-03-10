#include <unity.h>
#include "test_helpers.h"

u8g2_t u8g2;
const char* OUTPUT_DIR = "test/test_display/_output/pbm";
const char* PNG_DIR = "test/test_display/_output/png";

void setUp(void) {
    initTestDisplay(&u8g2);
}

void tearDown(void) {}

extern void register_hello_tests();
extern void post_hello_gif();
extern void register_logo_tests();
extern void register_pages_tests();
extern void register_play_controls_tests();
extern void register_menu_tests();
extern void register_header_icons_tests();
extern void register_textpage_tests();
extern void register_scroll_tests();

static void convertPbmToPng() {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "which magick >/dev/null 2>&1 && "
             "find %s -name '*.pbm' | while read f; do "
             "rel=\"${f#%s/}\"; "
             "dir=\"%s/$(dirname \"$rel\")\"; "
             "name=$(basename \"$f\" .pbm); "
             "mkdir -p \"$dir\"; "
             "magick \"$f\" -negate -scale 400%%%% \"$dir/${name}.png\"; "
             "done",
             OUTPUT_DIR, OUTPUT_DIR, PNG_DIR);
    int rc = system(cmd);
    if (rc == 0) {
        printf("\n  -> PNGs saved to %s/\n", PNG_DIR);
    }
}

static void cleanOutputDir() {
    system("rm -rf test/test_display/_output");
}

int runUnityTests() {
    cleanOutputDir();
    ensureDirRecursive(OUTPUT_DIR);

    UNITY_BEGIN();
    register_hello_tests();
    register_logo_tests();
    register_pages_tests();
    register_play_controls_tests();
    register_menu_tests();
    register_header_icons_tests();
    register_textpage_tests();
    register_scroll_tests();
    int result = UNITY_END();

    convertPbmToPng();
    post_hello_gif();
    return result;
}

int main(void) { return runUnityTests(); }
