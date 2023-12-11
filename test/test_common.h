#ifndef TEST_COMMON_H
#define TEST_COMMON_H

typedef enum {
    TEST_SUCCESS,
    TEST_FAIL_SETUP,
    TEST_FAIL_INTEGRITY,
}test_result;

#define test_start() printf("######## starting test: %s ########\n", __func__)
#define test_end() printf("######## test over: %s ########\n", __func__)

#endif