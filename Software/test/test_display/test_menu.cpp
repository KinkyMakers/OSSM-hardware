#include "test_helpers.h"

#include <cstdio>
#include <cctype>
#include <cstring>
#include "MenuItems.h"

static void toSnakeCase(const char* src, char* dst, int maxLen) {
    int j = 0;
    for (int i = 0; src[i] && j < maxLen - 1; i++) {
        char c = src[i];
        if (c == ' ' || c == '-') {
            dst[j++] = '_';
        } else if (c == '(' || c == ')') {
            continue;
        } else {
            dst[j++] = tolower(c);
        }
    }
    dst[j] = '\0';
}

// ---------------------------------------------------------------------------
// Real menu: render every option position
// ---------------------------------------------------------------------------

void test_drawMenu_allOptions(void) {
    for (int i = 0; i < Menu::NUM_OPTIONS; i++) {
        initTestDisplay(&u8g2);

        ui::MenuData data{};
        data.items = menuStrings;
        data.numItems = Menu::NUM_OPTIONS;
        data.selectedIndex = i;

        ui::drawMenu(&u8g2, data);
        u8g2_SetMaxClipWindow(&u8g2);
        TEST_ASSERT_TRUE(bufferHasContent(&u8g2));

        char snake[64];
        toSnakeCase(menuStrings[i], snake, sizeof(snake));
        char name[80];
        snprintf(name, sizeof(name), "%d_%s", i, snake);
        TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "menu", name));
    }
}

// ---------------------------------------------------------------------------
// Edge case: empty menu (0 items)
// ---------------------------------------------------------------------------

void test_drawMenu_emptyArray(void) {
    ui::MenuData data{};
    data.items = nullptr;
    data.numItems = 0;
    data.selectedIndex = 0;

    ui::drawMenu(&u8g2, data);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "menu", "edge_empty"));
}

// ---------------------------------------------------------------------------
// Edge case: single item
// ---------------------------------------------------------------------------

void test_drawMenu_singleItem(void) {
    static const char* single[] = {"Only Option"};

    ui::MenuData data{};
    data.items = single;
    data.numItems = 1;
    data.selectedIndex = 0;

    ui::drawMenu(&u8g2, data);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "menu", "edge_single_item"));
}

// ---------------------------------------------------------------------------
// Edge case: two items, both positions
// ---------------------------------------------------------------------------

void test_drawMenu_twoItems(void) {
    static const char* two[] = {"Alpha", "Bravo"};

    for (int i = 0; i < 2; i++) {
        initTestDisplay(&u8g2);

        ui::MenuData data{};
        data.items = two;
        data.numItems = 2;
        data.selectedIndex = i;

        ui::drawMenu(&u8g2, data);
        u8g2_SetMaxClipWindow(&u8g2);
        TEST_ASSERT_TRUE(bufferHasContent(&u8g2));

        char name[64];
        snprintf(name, sizeof(name), "edge_two_items_%d", i);
        TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "menu", name));
    }
}

// ---------------------------------------------------------------------------
// Stress: 100 items, render first / middle / last
// ---------------------------------------------------------------------------

void test_drawMenu_manyItems(void) {
    static char labels[100][16];
    static const char* ptrs[100];
    for (int i = 0; i < 100; i++) {
        snprintf(labels[i], sizeof(labels[i]), "Item %d", i);
        ptrs[i] = labels[i];
    }

    int positions[] = {0, 49, 99};
    for (int p = 0; p < 3; p++) {
        initTestDisplay(&u8g2);

        ui::MenuData data{};
        data.items = ptrs;
        data.numItems = 100;
        data.selectedIndex = positions[p];

        ui::drawMenu(&u8g2, data);
        u8g2_SetMaxClipWindow(&u8g2);
        TEST_ASSERT_TRUE(bufferHasContent(&u8g2));

        char name[64];
        snprintf(name, sizeof(name), "stress_100_items_pos_%d", positions[p]);
        TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "menu", name));
    }
}

// ---------------------------------------------------------------------------
// Stress: very long text values
// ---------------------------------------------------------------------------

void test_drawMenu_longText(void) {
    static const char* longItems[] = {
        "This is an extremely long menu option that exceeds the display width",
        "Another ridiculously long option name for stress testing purposes",
        "Short",
    };

    for (int i = 0; i < 3; i++) {
        initTestDisplay(&u8g2);

        ui::MenuData data{};
        data.items = longItems;
        data.numItems = 3;
        data.selectedIndex = i;

        ui::drawMenu(&u8g2, data);
        u8g2_SetMaxClipWindow(&u8g2);
        TEST_ASSERT_TRUE(bufferHasContent(&u8g2));

        char name[64];
        snprintf(name, sizeof(name), "stress_long_text_%d", i);
        TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "menu", name));
    }
}

// ---------------------------------------------------------------------------
// Wrapping: first item shows last as previous, last shows first as next
// ---------------------------------------------------------------------------

void test_drawMenu_firstAndLastWrap(void) {
    static const char* items[] = {"First", "Middle", "Last"};

    initTestDisplay(&u8g2);
    ui::MenuData dataFirst{};
    dataFirst.items = items;
    dataFirst.numItems = 3;
    dataFirst.selectedIndex = 0;
    ui::drawMenu(&u8g2, dataFirst);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "menu", "edge_wrap_first"));

    initTestDisplay(&u8g2);
    ui::MenuData dataLast{};
    dataLast.items = items;
    dataLast.numItems = 3;
    dataLast.selectedIndex = 2;
    ui::drawMenu(&u8g2, dataLast);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "menu", "edge_wrap_last"));
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_menu_tests() {
    RUN_TEST(test_drawMenu_allOptions);
    RUN_TEST(test_drawMenu_emptyArray);
    RUN_TEST(test_drawMenu_singleItem);
    RUN_TEST(test_drawMenu_twoItems);
    RUN_TEST(test_drawMenu_manyItems);
    RUN_TEST(test_drawMenu_longText);
    RUN_TEST(test_drawMenu_firstAndLastWrap);
}
