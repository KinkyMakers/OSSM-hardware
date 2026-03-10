#include "test_helpers.h"

#include <string>

#include "Logos.h"
#include "Strings.h"

// ============================================================
// Logo rendering tests — mirrors the boot sequence in hello.cpp
// ============================================================

void test_logo_rd(void) {
    ui::LogoData rdLogo{ui::strings::researchAndDesire, ui::logos::RDLogo,
                        57, 50, 35, 14};
    ui::drawLogo(&u8g2, rdLogo);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "logos", "rd_logo"));
}

void test_logo_km(void) {
    ui::LogoData kmLogo{ui::strings::kinkyMakers, ui::logos::KMLogo, 50,
                        50, 40, 14};
    ui::drawLogo(&u8g2, kmLogo);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "logos", "km_logo"));
}

void test_logo_measuring_stroke(void) {
    std::string measuringTitle =
        std::string(ui::strings::measuringStroke) + "         ";
    ui::LogoData measuring{measuringTitle.c_str(), ui::logos::KMLogo, 50,
                           50, 40, 14};
    ui::drawLogo(&u8g2, measuring);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "logos", "measuring_stroke"));
}

// ============================================================
// Registration
// ============================================================

void register_logo_tests() {
    RUN_TEST(test_logo_rd);
    RUN_TEST(test_logo_km);
    RUN_TEST(test_logo_measuring_stroke);
}
