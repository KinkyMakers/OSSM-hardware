#include "unity.h"
#include "utils/stringMethods.h"

void test_isStringEmpty(void) {
    // Testing with different kinds of empty strings
    TEST_ASSERT_TRUE(isStringEmpty(""));
    TEST_ASSERT_TRUE(isStringEmpty(" "));
    TEST_ASSERT_TRUE(isStringEmpty("    "));    // Multiple spaces
    TEST_ASSERT_TRUE(isStringEmpty("\t"));      // Tab character
    TEST_ASSERT_TRUE(isStringEmpty("   \t "));  // Spaces and a tab
    TEST_ASSERT_TRUE(isStringEmpty("\n"));      // Newline character
    TEST_ASSERT_TRUE(isStringEmpty("   \n "));  // Spaces and a newline
    TEST_ASSERT_TRUE(isStringEmpty("\r\n"));    // Carriage return and newline
    TEST_ASSERT_TRUE(isStringEmpty("\r"));      // Carriage return
    TEST_ASSERT_TRUE(isStringEmpty("\f"));      // Form feed
    TEST_ASSERT_TRUE(isStringEmpty("\v"));      // Vertical tab

    // Testing with strings containing non-whitespace characters
    TEST_ASSERT_FALSE(isStringEmpty("a"));
    TEST_ASSERT_FALSE(isStringEmpty("abc"));
    TEST_ASSERT_FALSE(
        isStringEmpty("            a"));  // Spaces before a character
    TEST_ASSERT_FALSE(
        isStringEmpty("a            "));           // Spaces after a character
    TEST_ASSERT_FALSE(isStringEmpty("   a    "));  // Spaces around a character
    TEST_ASSERT_FALSE(
        isStringEmpty("   a b  "));  // Spaces around and inside a string
    TEST_ASSERT_FALSE(isStringEmpty("\nabc\n"));  // Newline around characters
    TEST_ASSERT_FALSE(isStringEmpty("\tabc\t"));  // Tab around characters
}

void test_wrapText() {
    // Basic checks
    TEST_ASSERT_EQUAL_STRING("", wrapText("").c_str());
    TEST_ASSERT_EQUAL_STRING("01234567891", wrapText("01234567891").c_str());
    TEST_ASSERT_EQUAL_STRING("012345678912\n",
                             wrapText("012345678912").c_str());
    TEST_ASSERT_EQUAL_STRING(
        "012345678912\n345678901234\n567890",
        wrapText("012345678912345678901234567890").c_str());

    // Case with space exactly at 12th character
    TEST_ASSERT_EQUAL_STRING(
        "Hello World!\nThis is a\ntest string.",
        wrapText("Hello World! This is a test string.").c_str());

    // Case with no spaces
    TEST_ASSERT_EQUAL_STRING("012345678901\n234567890123\n",
                             wrapText("012345678901234567890123").c_str());

    // Case with long word more than 12 characters
    TEST_ASSERT_EQUAL_STRING(
        "Supercalifra\ngilisticexpi\nalidocious",
        wrapText("Supercalifragilisticexpialidocious").c_str());

    // Case with multiple spaces and newline characters
    TEST_ASSERT_EQUAL_STRING(
        "This is a\nlong string\nwith multiple\nspaces and\nnewlines",
        wrapText("This is a long string with multiple spaces and newlines")
            .c_str());

    // Case where last word exactly fits the line
    TEST_ASSERT_EQUAL_STRING("123456789012\nabcdefghijk",
                             wrapText("123456789012 abcdefghijk").c_str());
}

int runUnityTests() {
    UNITY_BEGIN();
    RUN_TEST(test_isStringEmpty);
    RUN_TEST(test_wrapText);
    return UNITY_END();
}

// WARNING!!! PLEASE REMOVE UNNECESSARY MAIN IMPLEMENTATIONS //

/**
 * For native dev-platform or for some embedded frameworks
 */
int main(void) { return runUnityTests(); }
