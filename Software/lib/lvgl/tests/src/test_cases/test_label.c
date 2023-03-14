#if LV_BUILD_TEST
#include "../lvgl.h"

#include "unity/unity.h"

static const char * long_text =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Cras malesuada ultrices magna in rutrum.";
static const char * long_text_multiline =
    "Lorem ipsum dolor sit amet,\nconsectetur adipiscing elit.\nCras malesuada ultrices magna in rutrum.\n";
static const char * empty_text = "";

static lv_obj_t * active_screen = NULL;
static lv_obj_t * label;
static lv_obj_t * long_label;
static lv_obj_t * long_label_multiline;
static lv_obj_t * empty_label;

void setUp(void)
{
    active_screen = lv_scr_act();
    label = lv_label_create(active_screen);
    long_label = lv_label_create(active_screen);
    long_label_multiline = lv_label_create(active_screen);
    empty_label = lv_label_create(active_screen);

    lv_label_set_text(long_label, long_text);
    lv_label_set_text(long_label_multiline, long_text_multiline);
    lv_label_set_text(empty_label, empty_text);
}

void tearDown(void)
{
    lv_obj_del(label);
    lv_obj_del(long_label);
    lv_obj_del(long_label_multiline);
    lv_obj_del(empty_label);
}

void test_label_creation(void)
{
    TEST_ASSERT_EQUAL_STRING(lv_label_get_text(label), LV_LABEL_DEFAULT_TEXT);
    TEST_ASSERT_EQUAL(lv_label_get_long_mode(label), LV_LABEL_LONG_WRAP);
}

void test_label_recolor(void)
{
    lv_label_set_recolor(label, true);
    TEST_ASSERT(lv_label_get_recolor(label));

    lv_label_set_recolor(label, false);
    TEST_ASSERT_FALSE(lv_label_get_recolor(label));
}

void test_label_set_text(void)
{
    const char * new_text = "Hello world";

    /* Refresh with the current text. */
    lv_label_set_text(label, NULL);
    TEST_ASSERT_EQUAL_STRING(lv_label_get_text(label), LV_LABEL_DEFAULT_TEXT);

    /* '\0' terminated character string to set new text. */
    lv_label_set_text(label, new_text);
    TEST_ASSERT_EQUAL_STRING(lv_label_get_text(label), new_text);
}

void test_label_get_letter_pos_align_left(void)
{
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, _LV_STYLE_STATE_CMP_SAME);
    lv_point_t first_letter_point;
    lv_point_t last_letter_point;
    const lv_point_t expected_first_letter_point = {
        .x = 0,
        .y = 0
    };
    const lv_point_t expected_last_letter_point = {
        .x = 29,
        .y = 0
    };

    const uint32_t first_letter_idx = 0;
    const uint32_t last_letter_idx = strlen(lv_label_get_text(long_label_multiline)) - 1; /* char index starts at 0 */

    lv_label_get_letter_pos(label, first_letter_idx, &first_letter_point);

    TEST_ASSERT_EQUAL(expected_first_letter_point.x, first_letter_point.x);
    TEST_ASSERT_EQUAL(expected_first_letter_point.y, first_letter_point.y);

    lv_label_get_letter_pos(label, last_letter_idx, &last_letter_point);

    TEST_ASSERT_EQUAL(expected_last_letter_point.x, last_letter_point.x);
    TEST_ASSERT_EQUAL(expected_last_letter_point.y, last_letter_point.y);
}

void test_label_get_letter_pos_align_left_on_empty_text(void)
{
    lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_LEFT, _LV_STYLE_STATE_CMP_SAME);
    lv_point_t first_letter_point;
    lv_point_t last_letter_point;
    const lv_point_t expected_first_letter_point = {
        .x = 0,
        .y = 0
    };
    const lv_point_t expected_last_letter_point = {
        .x = 0,
        .y = 0
    };

    const uint32_t first_letter_idx = 0;
    const uint32_t last_letter_idx = strlen(lv_label_get_text(empty_label)) - 1;

    lv_label_get_letter_pos(empty_label, first_letter_idx, &first_letter_point);

    TEST_ASSERT_EQUAL(expected_first_letter_point.x, first_letter_point.x);
    TEST_ASSERT_EQUAL(expected_first_letter_point.y, first_letter_point.y);

    lv_label_get_letter_pos(empty_label, last_letter_idx, &last_letter_point);

    TEST_ASSERT_EQUAL(expected_last_letter_point.x, last_letter_point.x);
    TEST_ASSERT_EQUAL(expected_last_letter_point.y, last_letter_point.y);
}

void test_label_long_text_multiline_get_letter_pos_align_left(void)
{
    lv_obj_set_style_text_align(long_label_multiline, LV_TEXT_ALIGN_LEFT, _LV_STYLE_STATE_CMP_SAME);
    lv_point_t first_letter_point;
    lv_point_t last_letter_point;
    lv_point_t last_letter_after_new_line_point;
    const lv_point_t expected_first_letter_point = {
        .x = 0,
        .y = 0
    };
    const lv_point_t expected_last_letter_point = {
        .x = 305,
        .y = 32
    };
    const lv_point_t expected_last_letter_after_new_line_point = {
        .x = 0,
        .y = 48
    };

    const uint32_t first_letter_idx = 0;
    const uint32_t last_letter_idx = strlen(lv_label_get_text(long_label_multiline)) - 1; /* char index starts at 0 */
    const uint32_t last_letter_after_new_line = strlen(lv_label_get_text(long_label_multiline));

    lv_label_get_letter_pos(long_label_multiline, first_letter_idx, &first_letter_point);

    TEST_ASSERT_EQUAL(expected_first_letter_point.x, first_letter_point.x);
    TEST_ASSERT_EQUAL(expected_first_letter_point.y, first_letter_point.y);

    lv_label_get_letter_pos(long_label_multiline, last_letter_idx, &last_letter_point);

    TEST_ASSERT_EQUAL(expected_last_letter_point.x, last_letter_point.x);
    TEST_ASSERT_EQUAL(expected_last_letter_point.y, last_letter_point.y);

    lv_label_get_letter_pos(long_label_multiline, last_letter_after_new_line, &last_letter_after_new_line_point);

    TEST_ASSERT_EQUAL(expected_last_letter_after_new_line_point.x, last_letter_after_new_line_point.x);
    TEST_ASSERT_EQUAL(expected_last_letter_after_new_line_point.y, last_letter_after_new_line_point.y);
}

void test_label_long_text_get_letter_pos_align_left(void)
{
    lv_label_set_long_mode(long_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(long_label, 150);
    lv_obj_set_height(long_label, 500);
    lv_obj_set_style_text_align(long_label, LV_TEXT_ALIGN_LEFT, _LV_STYLE_STATE_CMP_SAME);

    lv_point_t first_letter_point;
    lv_point_t last_letter_point;
    const lv_point_t expected_first_letter_point = {
        .x = 0,
        .y = 0
    };
    const lv_point_t expected_last_letter_point = {
        .x = 0,
        .y = 1536
    };

    const uint32_t first_letter_idx = 0;
    const uint32_t last_letter_idx = strlen(lv_label_get_text(long_label)) - 1; /* char index starts at 0 */

    lv_label_get_letter_pos(long_label, first_letter_idx, &first_letter_point);

    TEST_ASSERT_EQUAL(expected_first_letter_point.x, first_letter_point.x);
    TEST_ASSERT_EQUAL(expected_first_letter_point.y, first_letter_point.y);

    lv_label_get_letter_pos(long_label, last_letter_idx, &last_letter_point);

    TEST_ASSERT_EQUAL(expected_last_letter_point.x, last_letter_point.x);
    TEST_ASSERT_EQUAL(expected_last_letter_point.y, last_letter_point.y);
}

void test_label_get_letter_pos_align_right(void)
{
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, _LV_STYLE_STATE_CMP_SAME);
    lv_point_t first_letter_point;
    lv_point_t last_letter_point;
    const lv_point_t expected_first_letter_point = {
        .x = -29,
        .y = 0
    };
    const lv_point_t expected_last_letter_point = {
        .x = 0,
        .y = 0
    };

    const uint32_t first_letter_idx = 0;
    const uint32_t last_letter_idx = strlen(lv_label_get_text(long_label_multiline)) - 1; /* char index starts at 0 */

    lv_label_get_letter_pos(label, first_letter_idx, &first_letter_point);

    TEST_ASSERT_EQUAL(expected_first_letter_point.x, first_letter_point.x);
    TEST_ASSERT_EQUAL(expected_first_letter_point.y, first_letter_point.y);

    lv_label_get_letter_pos(label, last_letter_idx, &last_letter_point);

    TEST_ASSERT_EQUAL(expected_last_letter_point.x, last_letter_point.x);
    TEST_ASSERT_EQUAL(expected_last_letter_point.y, last_letter_point.y);
}

void test_label_get_letter_pos_align_right_on_empty_text(void)
{
    lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_RIGHT, _LV_STYLE_STATE_CMP_SAME);
    lv_point_t first_letter_point;
    lv_point_t last_letter_point;
    const lv_point_t expected_first_letter_point = {
        .x = 0,
        .y = 0
    };
    const lv_point_t expected_last_letter_point = {
        .x = 0,
        .y = 0
    };

    const uint32_t first_letter_idx = 0;
    const uint32_t last_letter_idx = strlen(lv_label_get_text(empty_label)) - 1;

    lv_label_get_letter_pos(empty_label, first_letter_idx, &first_letter_point);

    TEST_ASSERT_EQUAL(expected_first_letter_point.x, first_letter_point.x);
    TEST_ASSERT_EQUAL(expected_first_letter_point.y, first_letter_point.y);

    lv_label_get_letter_pos(empty_label, last_letter_idx, &last_letter_point);

    TEST_ASSERT_EQUAL(expected_last_letter_point.x, last_letter_point.x);
    TEST_ASSERT_EQUAL(expected_last_letter_point.y, last_letter_point.y);
}

void test_label_long_text_multiline_get_letter_pos_align_right(void)
{
    lv_obj_set_style_text_align(long_label_multiline, LV_TEXT_ALIGN_RIGHT, _LV_STYLE_STATE_CMP_SAME);
    lv_point_t first_letter_point;
    lv_point_t last_letter_point;
    lv_point_t last_letter_after_new_line_point;
    const lv_point_t expected_first_letter_point = {
        .x = -205,
        .y = 0
    };
    const lv_point_t expected_last_letter_point = {
        .x = 0,
        .y = 32
    };
    const lv_point_t expected_last_letter_after_new_line_point = {
        .x = 0,
        .y = 48
    };

    const uint32_t first_letter_idx = 0;
    const uint32_t last_letter_idx = strlen(lv_label_get_text(long_label_multiline)) - 1; /* char index starts at 0 */
    const uint32_t last_letter_after_new_line = strlen(lv_label_get_text(long_label_multiline));

    lv_label_get_letter_pos(long_label_multiline, first_letter_idx, &first_letter_point);

    TEST_ASSERT_EQUAL(expected_first_letter_point.x, first_letter_point.x);
    TEST_ASSERT_EQUAL(expected_first_letter_point.y, first_letter_point.y);

    lv_label_get_letter_pos(long_label_multiline, last_letter_idx, &last_letter_point);

    TEST_ASSERT_EQUAL(expected_last_letter_point.x, last_letter_point.x);
    TEST_ASSERT_EQUAL(expected_last_letter_point.y, last_letter_point.y);

    lv_label_get_letter_pos(long_label_multiline, last_letter_after_new_line, &last_letter_after_new_line_point);

    TEST_ASSERT_EQUAL(expected_last_letter_after_new_line_point.x, last_letter_after_new_line_point.x);
    TEST_ASSERT_EQUAL(expected_last_letter_after_new_line_point.y, last_letter_after_new_line_point.y);
}

void test_label_long_text_get_letter_pos_align_right(void)
{
    lv_label_set_long_mode(long_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(long_label, 150);
    lv_obj_set_height(long_label, 500);
    lv_obj_set_style_text_align(long_label, LV_TEXT_ALIGN_RIGHT, _LV_STYLE_STATE_CMP_SAME);

    lv_point_t first_letter_point;
    lv_point_t last_letter_point;
    const lv_point_t expected_first_letter_point = {
        .x = -8,
        .y = 0
    };
    const lv_point_t expected_last_letter_point = {
        .x = -3,
        .y = 1536
    };

    const uint32_t first_letter_idx = 0;
    const uint32_t last_letter_idx = strlen(lv_label_get_text(long_label)) - 1; /* char index starts at 0 */

    lv_label_get_letter_pos(long_label, first_letter_idx, &first_letter_point);

    TEST_ASSERT_EQUAL(expected_first_letter_point.x, first_letter_point.x);
    TEST_ASSERT_EQUAL(expected_first_letter_point.y, first_letter_point.y);

    lv_label_get_letter_pos(long_label, last_letter_idx, &last_letter_point);

    TEST_ASSERT_EQUAL(expected_last_letter_point.x, last_letter_point.x);
    TEST_ASSERT_EQUAL(expected_last_letter_point.y, last_letter_point.y);
}

void test_label_get_letter_pos_align_center(void)
{
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, _LV_STYLE_STATE_CMP_SAME);
    lv_point_t first_letter_point;
    lv_point_t last_letter_point;
    const lv_point_t expected_first_letter_point = {
        .x = -14,
        .y = 0
    };
    const lv_point_t expected_last_letter_point = {
        .x = 15,
        .y = 0
    };

    const uint32_t first_letter_idx = 0;
    const uint32_t last_letter_idx = strlen(lv_label_get_text(long_label_multiline)) - 1; /* char index starts at 0 */

    lv_label_get_letter_pos(label, first_letter_idx, &first_letter_point);

    TEST_ASSERT_EQUAL(expected_first_letter_point.x, first_letter_point.x);
    TEST_ASSERT_EQUAL(expected_first_letter_point.y, first_letter_point.y);

    lv_label_get_letter_pos(label, last_letter_idx, &last_letter_point);

    TEST_ASSERT_EQUAL(expected_last_letter_point.x, last_letter_point.x);
    TEST_ASSERT_EQUAL(expected_last_letter_point.y, last_letter_point.y);
}

void test_label_get_letter_pos_align_center_on_empty_text(void)
{
    lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_CENTER, _LV_STYLE_STATE_CMP_SAME);
    lv_point_t first_letter_point;
    lv_point_t last_letter_point;
    const lv_point_t expected_first_letter_point = {
        .x = 0,
        .y = 0
    };
    const lv_point_t expected_last_letter_point = {
        .x = 0,
        .y = 0
    };

    const uint32_t first_letter_idx = 0;
    const uint32_t last_letter_idx = strlen(lv_label_get_text(empty_label)) - 1;

    lv_label_get_letter_pos(empty_label, first_letter_idx, &first_letter_point);

    TEST_ASSERT_EQUAL(expected_first_letter_point.x, first_letter_point.x);
    TEST_ASSERT_EQUAL(expected_first_letter_point.y, first_letter_point.y);

    lv_label_get_letter_pos(empty_label, last_letter_idx, &last_letter_point);

    TEST_ASSERT_EQUAL(expected_last_letter_point.x, last_letter_point.x);
    TEST_ASSERT_EQUAL(expected_last_letter_point.y, last_letter_point.y);
}

void test_label_long_text_multiline_get_letter_pos_align_center(void)
{
    lv_obj_set_style_text_align(long_label_multiline, LV_TEXT_ALIGN_CENTER, _LV_STYLE_STATE_CMP_SAME);
    lv_point_t first_letter_point;
    lv_point_t last_letter_point;
    lv_point_t last_letter_after_new_line_point;
    const lv_point_t expected_first_letter_point = {
        .x = -102,
        .y = 0
    };
    const lv_point_t expected_last_letter_point = {
        .x = 153,
        .y = 32
    };
    const lv_point_t expected_last_letter_after_new_line_point = {
        .x = 0,
        .y = 48
    };

    const uint32_t first_letter_idx = 0;
    const uint32_t last_letter_idx = strlen(lv_label_get_text(long_label_multiline)) - 1; /* char index starts at 0 */
    const uint32_t last_letter_after_new_line = strlen(lv_label_get_text(long_label_multiline));

    lv_label_get_letter_pos(long_label_multiline, first_letter_idx, &first_letter_point);

    TEST_ASSERT_EQUAL(expected_first_letter_point.x, first_letter_point.x);
    TEST_ASSERT_EQUAL(expected_first_letter_point.y, first_letter_point.y);

    lv_label_get_letter_pos(long_label_multiline, last_letter_idx, &last_letter_point);

    TEST_ASSERT_EQUAL(expected_last_letter_point.x, last_letter_point.x);
    TEST_ASSERT_EQUAL(expected_last_letter_point.y, last_letter_point.y);

    lv_label_get_letter_pos(long_label_multiline, last_letter_after_new_line, &last_letter_after_new_line_point);

    TEST_ASSERT_EQUAL(expected_last_letter_after_new_line_point.x, last_letter_after_new_line_point.x);
    TEST_ASSERT_EQUAL(expected_last_letter_after_new_line_point.y, last_letter_after_new_line_point.y);
}

void test_label_long_text_get_letter_pos_align_center(void)
{
    lv_label_set_long_mode(long_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(long_label, 150);
    lv_obj_set_height(long_label, 500);
    lv_obj_set_style_text_align(long_label, LV_TEXT_ALIGN_CENTER, _LV_STYLE_STATE_CMP_SAME);

    lv_point_t first_letter_point;
    lv_point_t last_letter_point;
    const lv_point_t expected_first_letter_point = {
        .x = -4,
        .y = 0
    };
    const lv_point_t expected_last_letter_point = {
        .x = -1,
        .y = 1536
    };

    const uint32_t first_letter_idx = 0;
    const uint32_t last_letter_idx = strlen(lv_label_get_text(long_label)) - 1; /* char index starts at 0 */

    lv_label_get_letter_pos(long_label, first_letter_idx, &first_letter_point);

    TEST_ASSERT_EQUAL(expected_first_letter_point.x, first_letter_point.x);
    TEST_ASSERT_EQUAL(expected_first_letter_point.y, first_letter_point.y);

    lv_label_get_letter_pos(long_label, last_letter_idx, &last_letter_point);

    TEST_ASSERT_EQUAL(expected_last_letter_point.x, last_letter_point.x);
    TEST_ASSERT_EQUAL(expected_last_letter_point.y, last_letter_point.y);
}

void test_label_is_char_under_pos_align_left(void)
{
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, _LV_STYLE_STATE_CMP_SAME);
    bool expected_result = true;
    bool result = false;
    lv_point_t last_letter_point;

    /* Getting the last letter point */
    const uint32_t last_letter_idx = strlen(lv_label_get_text(label)) - 1;
    lv_label_get_letter_pos(label, last_letter_idx, &last_letter_point);

    /* We should get true, since we know the last letter is in this coordinate */
    result = lv_label_is_char_under_pos(label, &last_letter_point);
    TEST_ASSERT_EQUAL(expected_result, result);

    /* If we test a move away from the last letter then we should get false */
    expected_result = false;
    last_letter_point.x += 50;

    result = lv_label_is_char_under_pos(label, &last_letter_point);
    TEST_ASSERT_EQUAL(expected_result, result);
}

void test_label_is_char_under_pos_align_right(void)
{
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, _LV_STYLE_STATE_CMP_SAME);
    bool expected_result = true;
    bool result = false;
    lv_point_t last_letter_point;

    /* Getting the last letter point */
    const uint32_t last_letter_idx = strlen(lv_label_get_text(label)) - 1;
    lv_label_get_letter_pos(label, last_letter_idx, &last_letter_point);

    /* We should get true, since we know the last letter is in this coordinate */
    result = lv_label_is_char_under_pos(label, &last_letter_point);
    TEST_ASSERT_EQUAL(expected_result, result);

    /* If we test a move away from the last letter then we should get false */
    expected_result = false;
    last_letter_point.x -= 50;

    result = lv_label_is_char_under_pos(label, &last_letter_point);
    TEST_ASSERT_EQUAL(expected_result, result);
}

void test_label_is_char_under_pos_align_center(void)
{
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, _LV_STYLE_STATE_CMP_SAME);
    bool expected_result = true;
    bool result = false;
    lv_point_t last_letter_point;

    /* Getting the last letter point */
    const uint32_t last_letter_idx = strlen(lv_label_get_text(label)) - 1;
    lv_label_get_letter_pos(label, last_letter_idx, &last_letter_point);

    /* We should get true, since we know the last letter is in this coordinate */
    result = lv_label_is_char_under_pos(label, &last_letter_point);
    TEST_ASSERT_EQUAL(expected_result, result);

    /* If we test a move away from the last letter then we should get false */
    expected_result = false;
    last_letter_point.x -= 50;

    result = lv_label_is_char_under_pos(label, &last_letter_point);
    TEST_ASSERT_EQUAL(expected_result, result);
}

void test_label_cut_text(void)
{
    const char * expected_text = "Te";
    /* label has the default text which is "Text", let's remove "xt" from it */
    lv_label_cut_text(label, 2, 2);

    TEST_ASSERT_EQUAL_STRING(expected_text, lv_label_get_text(label));
}

void test_label_get_letter_on_left(void)
{
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, _LV_STYLE_STATE_CMP_SAME);
    lv_point_t last_letter_point;

    const uint32_t last_letter_idx = strlen(lv_label_get_text(label)) - 1;
    lv_label_get_letter_pos(label, last_letter_idx, &last_letter_point);

    uint32_t letter_idx_result = lv_label_get_letter_on(label, &last_letter_point);

    TEST_ASSERT_EQUAL(last_letter_idx, letter_idx_result);
}

void test_label_get_letter_on_center(void)
{
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, _LV_STYLE_STATE_CMP_SAME);
    lv_point_t last_letter_point;

    const uint32_t last_letter_idx = strlen(lv_label_get_text(label)) - 1;
    lv_label_get_letter_pos(label, last_letter_idx, &last_letter_point);

    uint32_t letter_idx_result = lv_label_get_letter_on(label, &last_letter_point);

    TEST_ASSERT_EQUAL(last_letter_idx, letter_idx_result);
}

void test_label_get_letter_on_right(void)
{
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, _LV_STYLE_STATE_CMP_SAME);
    lv_point_t last_letter_point;

    const uint32_t last_letter_idx = strlen(lv_label_get_text(label)) - 1;
    lv_label_get_letter_pos(label, last_letter_idx, &last_letter_point);

    uint32_t letter_idx_result = lv_label_get_letter_on(label, &last_letter_point);

    TEST_ASSERT_EQUAL(last_letter_idx, letter_idx_result);
}

void test_label_text_selection(void)
{
    const uint32_t selection_start = 0;
    const uint32_t selection_end = strlen(lv_label_get_text(label)) - 1;

    lv_label_set_text_selection_start(label, selection_start);
    lv_label_set_text_selection_end(label, selection_end);

    uint32_t start = lv_label_get_text_selection_start(label);
    uint32_t end = lv_label_get_text_selection_end(label);

    TEST_ASSERT_EQUAL(selection_start, start);
    TEST_ASSERT_EQUAL(selection_end, end);
}

#endif
