#include <unity.h>
#include "WgConfig.h"

static const char* FULL =
  "[Interface]\n"
  "PrivateKey = aGVsbG9wcml2YXRla2V5MDAwMDAwMDAwMDAwMDA9\n"
  "Address = 10.20.30.5/24\n"
  "[Peer]\n"
  "PublicKey = cGVlcnB1YmxpY2tleTAwMDAwMDAwMDAwMDAwMDA9\n"
  "Endpoint = gw.example.com:51820\n"
  "AllowedIPs = 0.0.0.0/0\n";

void setUp(){} void tearDown(){}

void test_full_config_parses() {
  WgConfig c;
  TEST_ASSERT_TRUE(wgConfigParse(FULL, &c));
  TEST_ASSERT_TRUE(c.valid);
  TEST_ASSERT_EQUAL_STRING("10.20.30.5", c.if_addr);
  TEST_ASSERT_EQUAL_STRING("gw.example.com", c.peer_host);
  TEST_ASSERT_EQUAL_UINT16(51820, c.peer_port);
}
void test_default_port_when_no_colon() {
  WgConfig c;
  const char* t = "PrivateKey=k\nAddress=10.0.0.2\nPublicKey=p\nEndpoint=1.2.3.4\n";
  TEST_ASSERT_TRUE(wgConfigParse(t, &c));
  TEST_ASSERT_EQUAL_UINT16(51820, c.peer_port);
  TEST_ASSERT_EQUAL_STRING("1.2.3.4", c.peer_host);
}
void test_custom_port_parsed() {
  WgConfig c;
  const char* t = "PrivateKey=k\nAddress=10.0.0.2\nPublicKey=p\nEndpoint=1.2.3.4:12345\n";
  TEST_ASSERT_TRUE(wgConfigParse(t, &c));
  TEST_ASSERT_EQUAL_UINT16(12345, c.peer_port);
}
void test_missing_private_key_invalid() {
  WgConfig c;
  const char* t = "Address=10.0.0.2\nPublicKey=p\nEndpoint=1.2.3.4\n";
  TEST_ASSERT_FALSE(wgConfigParse(t, &c));
  TEST_ASSERT_FALSE(c.valid);
}
void test_comments_and_whitespace() {
  WgConfig c;
  const char* t = "# comment\n  PrivateKey   =   k  \nAddress=10.0.0.2/32\nPublicKey=p\nEndpoint=h:9\n";
  TEST_ASSERT_TRUE(wgConfigParse(t, &c));
  TEST_ASSERT_EQUAL_STRING("k", c.if_priv);
  TEST_ASSERT_EQUAL_STRING("10.0.0.2", c.if_addr);
  TEST_ASSERT_EQUAL_UINT16(9, c.peer_port);
}
int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_full_config_parses);
  RUN_TEST(test_default_port_when_no_colon);
  RUN_TEST(test_custom_port_parsed);
  RUN_TEST(test_missing_private_key_invalid);
  RUN_TEST(test_comments_and_whitespace);
  return UNITY_END();
}
