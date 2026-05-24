#include <unity.h>
#include "RadioManager.h"

// Fake radio hooks: track on/off state and assert WiFi & BT are NEVER both on.
struct FakeHooks : RadioHooks {
  bool wifi=false, bt=false; bool everBoth=false; bool failNext=false;
  bool startWifi() override { if(failNext){failNext=false;return false;} wifi=true; check(); return true; }
  void stopWifi()  override { wifi=false; }
  bool startBt()   override { if(failNext){failNext=false;return false;} bt=true; check(); return true; }
  void stopBt()    override { bt=false; }
  void check(){ if(wifi&&bt) everBoth=true; }
};
void setUp(){} void tearDown(){}

void test_starts_off(){ FakeHooks h; RadioManager r(&h); TEST_ASSERT_EQUAL(RadioManager::OFF, r.mode()); }
void test_off_to_wifi(){ FakeHooks h; RadioManager r(&h);
  TEST_ASSERT_TRUE(r.setMode(RadioManager::WIFI));
  TEST_ASSERT_EQUAL(RadioManager::WIFI, r.mode()); TEST_ASSERT_TRUE(h.wifi); TEST_ASSERT_FALSE(h.bt); }
void test_wifi_to_bt_is_exclusive(){ FakeHooks h; RadioManager r(&h);
  r.setMode(RadioManager::WIFI); TEST_ASSERT_TRUE(r.setMode(RadioManager::BT));
  TEST_ASSERT_EQUAL(RadioManager::BT, r.mode());
  TEST_ASSERT_FALSE(h.wifi); TEST_ASSERT_TRUE(h.bt);
  TEST_ASSERT_FALSE(h.everBoth); }      // never both on at once
void test_same_mode_noop(){ FakeHooks h; RadioManager r(&h);
  r.setMode(RadioManager::WIFI); TEST_ASSERT_TRUE(r.setMode(RadioManager::WIFI));
  TEST_ASSERT_EQUAL(RadioManager::WIFI, r.mode()); }
void test_start_failure_leaves_off(){ FakeHooks h; RadioManager r(&h);
  h.failNext=true; TEST_ASSERT_FALSE(r.setMode(RadioManager::WIFI));
  TEST_ASSERT_EQUAL(RadioManager::OFF, r.mode()); TEST_ASSERT_FALSE(h.wifi); }
int main(int,char**){ UNITY_BEGIN();
  RUN_TEST(test_starts_off); RUN_TEST(test_off_to_wifi);
  RUN_TEST(test_wifi_to_bt_is_exclusive); RUN_TEST(test_same_mode_noop);
  RUN_TEST(test_start_failure_leaves_off); return UNITY_END(); }
