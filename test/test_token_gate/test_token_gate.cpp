#include <unity.h>
#include "transport/token_gate.h"
void setUp(){} void tearDown(){}
void test_equal_true(){ TEST_ASSERT_TRUE(tokenEquals("s3cret-token","s3cret-token")); }
void test_diff_false(){ TEST_ASSERT_FALSE(tokenEquals("s3cret-token","wrong-token!!")); }
void test_prefix_false(){ TEST_ASSERT_FALSE(tokenEquals("s3cret","s3cret-token")); }
void test_longer_false(){ TEST_ASSERT_FALSE(tokenEquals("s3cret-token-extra","s3cret-token")); }
void test_empty_inputs(){ TEST_ASSERT_FALSE(tokenEquals("", "x")); TEST_ASSERT_FALSE(tokenEquals(0,"x")); TEST_ASSERT_FALSE(tokenEquals("x",0)); }
int main(int,char**){ UNITY_BEGIN();
  RUN_TEST(test_equal_true); RUN_TEST(test_diff_false);
  RUN_TEST(test_prefix_false); RUN_TEST(test_longer_false);
  RUN_TEST(test_empty_inputs); return UNITY_END(); }
