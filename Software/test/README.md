# Testing

1. Create a new directory in the test directory with the name of your test
2. Name the directory 'test_<name>' and add a file called 'test_<name>/main.cpp'
3. Add the following code to the main.cpp file:

```cpp
void test_example() {
    TEST_ASSERT_EQUAL(1, 1);
}

int runUnityTests() {
    UNITY_BEGIN();
    RUN_TEST(test_example);
    return UNITY_END();
}

int main(void) { return runUnityTests(); }
```
4. Inside of the directory 'test_<name>', you can create files which support your tests.
5. You may also import files from '../../src' to use in your tests.