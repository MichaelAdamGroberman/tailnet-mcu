#include <unity.h>
#include "TailnetPeer.h"
#include "backends/wg_backend.h"

static const char* FULL =
  "[Interface]\nPrivateKey=k\nAddress=10.20.30.5/24\n"
  "[Peer]\nPublicKey=p\nEndpoint=gw.example.com:51820\n";

class FakeBackend : public WgBackend {
public:
  bool begin_ret = true, up = false, begun = false;
  WgConfig last{};
  bool begin(const WgConfig& c) override { begun = true; last = c; return begin_ret; }
  void end() override { begun = false; }
  bool isUp() override { return up; }
};

void setUp(){} void tearDown(){}

void test_begin_parses_and_starts() {
  FakeBackend fb; TailnetPeer p(&fb);
  TEST_ASSERT_TRUE(p.begin(FULL));
  TEST_ASSERT_EQUAL(TailnetPeer::STARTING, p.state());
  TEST_ASSERT_TRUE(fb.begun);
  TEST_ASSERT_EQUAL_STRING("10.20.30.5", p.tunnelIP());
  TEST_ASSERT_EQUAL_STRING("gw.example.com:51820", p.peerEndpoint());
}
void test_tick_promotes_to_up() {
  FakeBackend fb; TailnetPeer p(&fb);
  p.begin(FULL);
  p.tick(); TEST_ASSERT_EQUAL(TailnetPeer::STARTING, p.state());
  fb.up = true; p.tick();
  TEST_ASSERT_EQUAL(TailnetPeer::UP, p.state());
}
void test_bad_config_fails() {
  FakeBackend fb; TailnetPeer p(&fb);
  TEST_ASSERT_FALSE(p.begin("garbage"));
  TEST_ASSERT_EQUAL(TailnetPeer::FAILED, p.state());
}
void test_backend_begin_failure() {
  FakeBackend fb; fb.begin_ret = false; TailnetPeer p(&fb);
  TEST_ASSERT_FALSE(p.begin(FULL));
  TEST_ASSERT_EQUAL(TailnetPeer::FAILED, p.state());
}
void test_stop_returns_to_off() {
  FakeBackend fb; TailnetPeer p(&fb);
  p.begin(FULL); p.stop();
  TEST_ASSERT_EQUAL(TailnetPeer::OFF, p.state());
  TEST_ASSERT_FALSE(fb.begun);
}
int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_begin_parses_and_starts);
  RUN_TEST(test_tick_promotes_to_up);
  RUN_TEST(test_bad_config_fails);
  RUN_TEST(test_backend_begin_failure);
  RUN_TEST(test_stop_returns_to_off);
  return UNITY_END();
}
