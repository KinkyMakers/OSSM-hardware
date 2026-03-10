#include "HelloAnimation.h"
#include "test_helpers.h"

void test_drawHelloFrames(void) {
    for (int i = 0; i < ui::HELLO_FRAME_COUNT; i++) {
        u8g2_ClearBuffer(&u8g2);
        ui::drawHelloFrame(&u8g2, ui::HELLO_FRAMES[i]);
        u8g2_SetMaxClipWindow(&u8g2);

        char name[32];
        snprintf(name, sizeof(name), "hello_%02d", i);
        TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "hello", name));
    }
}

static void combineHelloGif() {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "which magick >/dev/null 2>&1 && "
             "magick -delay 8 -loop 0 "
             "%s/hello/hello_*.png "
             "%s/hello/hello_animation.gif "
             "&& echo 'GIF saved'",
             PNG_DIR, PNG_DIR);
    int rc = system(cmd);
    if (rc == 0) {
        printf("  -> GIF saved to %s/hello/hello_animation.gif\n", PNG_DIR);
    }
}

void register_hello_tests() { RUN_TEST(test_drawHelloFrames); }

void post_hello_gif() { combineHelloGif(); }
