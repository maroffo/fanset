// ABOUTME: Unit tests for fan_apply (src/fan.c) against a fake in-memory SMC: success, all-or-nothing rollback, refusals.
// ABOUTME: Each test programs which reads or writes fail, then checks the resulting mode and target of every fan.
#include "../src/fan.h"
#include <math.h>
#include <string.h>

static int fails;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      fails++;                                                            \
    }                                                                     \
  } while (0)

typedef struct {
  char key[5], type[5];
  unsigned char bytes[4];
  UInt32 size;
  int fail_read;      // every read of this key fails
  int writes;         // writes attempted so far
  int fail_on_write;  // the nth write (1-based) fails; 0 = never
} fake_key_t;

static fake_key_t keys[64];
static int nkeys;
static FILE *devnull;

static fake_key_t *find(const char *key) {
  for (int i = 0; i < nkeys; i++)
    if (!strcmp(keys[i].key, key)) return &keys[i];
  return NULL;
}

static void add_flt(const char *key, float v) {
  fake_key_t *k = &keys[nkeys++];
  memset(k, 0, sizeof(*k));
  strcpy(k->key, key);
  strcpy(k->type, "flt ");
  k->size = 4;
  memcpy(k->bytes, &v, 4);
}

static void add_ui8(const char *key, unsigned char v) {
  fake_key_t *k = &keys[nkeys++];
  memset(k, 0, sizeof(*k));
  strcpy(k->key, key);
  strcpy(k->type, "ui8 ");
  k->size = 1;
  k->bytes[0] = v;
}

// fake_reset builds an SMC like the M5 MacBook Pro: min 1350, max 5349 / 5777, automatic mode.
static void fake_reset(int nfans) {
  nkeys = 0;
  char k[5];
  for (int i = 0; i < nfans; i++) {
    snprintf(k, sizeof(k), "F%dMn", i); add_flt(k, 1350);
    snprintf(k, sizeof(k), "F%dMx", i); add_flt(k, i ? 5777 : 5349);
    snprintf(k, sizeof(k), "F%dTg", i); add_flt(k, 1350);
    snprintf(k, sizeof(k), "F%dmd", i); add_ui8(k, 0);
  }
}

static int fake_read(const char *key, const char *type, void *out, UInt32 size) {
  fake_key_t *k = find(key);
  if (k && k->fail_read && k->size == size) {
    memcpy(out, k->bytes, size);  // plausible stale bytes: callers must check the result, not the value
    return -1;
  }
  if (!k || k->fail_read || strcmp(k->type, type) || k->size != size) return -1;
  memcpy(out, k->bytes, size);
  return 0;
}

static int fake_write(const char *key, const char *type, const void *val, UInt32 size) {
  fake_key_t *k = find(key);
  if (!k) return -1;
  k->writes++;
  if (k->fail_on_write == k->writes || strcmp(k->type, type) || k->size != size) return -1;
  memcpy(k->bytes, val, size);
  return 0;
}

static const smc_io_t fake = {fake_read, fake_write};

static int mode(int i) {
  char k[5];
  snprintf(k, sizeof(k), "F%dmd", i);
  return find(k)->bytes[0];
}

static float target(int i) {
  char k[5];
  float f;
  snprintf(k, sizeof(k), "F%dTg", i);
  memcpy(&f, find(k)->bytes, 4);
  return f;
}

static fake_key_t *key(const char *k) { return find(k); }

static void set_mode(int i, unsigned char md) {
  char k[5];
  snprintf(k, sizeof(k), "F%dmd", i);
  find(k)->bytes[0] = md;
}

static void test_sets_every_fan_manual_at_pct_of_max(void) {
  fake_reset(2);
  CHECK(fan_apply(&fake, 2, 80, devnull) == FAN_OK);
  CHECK(mode(0) == 1 && mode(1) == 1);
  CHECK(fabsf(target(0) - 4279.2f) < 0.5f);
  CHECK(fabsf(target(1) - 4621.6f) < 0.5f);
}

static void test_auto_returns_every_fan_to_auto(void) {
  fake_reset(2);
  fan_apply(&fake, 2, 80, devnull);
  CHECK(fan_apply(&fake, 2, 0, devnull) == FAN_OK);
  CHECK(mode(0) == 0 && mode(1) == 0);
}

static void test_no_fans_is_an_error(void) {
  fake_reset(0);
  CHECK(fan_apply(&fake, 0, 80, devnull) == FAN_FAILED);
  CHECK(fan_apply(&fake, 0, 0, devnull) == FAN_FAILED);
}

static void test_implausible_fan_count_writes_nothing(void) {
  fake_reset(2);
  CHECK(fan_apply(&fake, 10, 80, devnull) == FAN_FAILED);
  CHECK(key("F0md")->writes == 0);
}

static void test_mode_failure_on_second_fan_rolls_back_the_first(void) {
  fake_reset(2);
  key("F1md")->fail_on_write = 1;
  CHECK(fan_apply(&fake, 2, 80, devnull) == FAN_FAILED);
  CHECK(mode(0) == 0 && mode(1) == 0);
}

static void test_target_failure_returns_every_fan_to_auto(void) {
  fake_reset(2);
  key("F0Tg")->fail_on_write = 1;
  CHECK(fan_apply(&fake, 2, 80, devnull) == FAN_FAILED);
  CHECK(mode(0) == 0 && mode(1) == 0);
}

static void test_unreadable_min_leaves_fans_untouched(void) {
  fake_reset(2);
  key("F0Mn")->fail_read = 1;
  CHECK(fan_apply(&fake, 2, 80, devnull) == FAN_FAILED);
  CHECK(mode(0) == 0 && mode(1) == 0);
  CHECK(target(0) == 1350);
}

static void test_zero_min_is_refused(void) {
  fake_reset(1);
  float zero = 0;
  memcpy(key("F0Mn")->bytes, &zero, 4);
  CHECK(fan_apply(&fake, 1, 1, devnull) == FAN_FAILED);
  CHECK(mode(0) == 0);
}

static void test_wrong_key_type_is_refused(void) {
  fake_reset(1);
  strcpy(key("F0Mx")->type, "ui32");
  CHECK(fan_apply(&fake, 1, 80, devnull) == FAN_FAILED);
  CHECK(mode(0) == 0);
}

static void test_failed_rollback_reports_stuck_manual(void) {
  fake_reset(2);
  key("F1Tg")->fail_on_write = 1;  // fan 1 target fails...
  key("F1md")->fail_on_write = 2;  // ...and so does its return to auto
  CHECK(fan_apply(&fake, 2, 80, devnull) == (FAN_FAILED | FAN_STUCK_MANUAL));
  CHECK(mode(0) == 0);
  CHECK(mode(1) == 1);
}

static void test_failed_auto_reports_stuck_manual(void) {
  fake_reset(1);
  fan_apply(&fake, 1, 80, devnull);
  key("F0md")->fail_on_write = key("F0md")->writes + 1;
  CHECK(fan_apply(&fake, 1, 0, devnull) == (FAN_FAILED | FAN_STUCK_MANUAL));
}

static void test_auto_continues_past_a_refused_fan(void) {
  fake_reset(2);
  fan_apply(&fake, 2, 80, devnull);
  key("F0md")->fail_on_write = key("F0md")->writes + 1;
  CHECK(fan_apply(&fake, 2, 0, devnull) == (FAN_FAILED | FAN_STUCK_MANUAL));
  CHECK(mode(1) == 0);
}

static void test_rollback_continues_past_a_refused_fan(void) {
  fake_reset(2);
  key("F1Tg")->fail_on_write = 1;  // triggers the rollback on fan 1...
  key("F0md")->fail_on_write = 2;  // ...whose first step (fan 0 back to auto) is refused
  CHECK(fan_apply(&fake, 2, 80, devnull) == (FAN_FAILED | FAN_STUCK_MANUAL));
  CHECK(mode(1) == 0);
}

static void test_rollback_covers_fans_after_the_failing_one(void) {
  fake_reset(2);
  set_mode(1, 1);  // left manual by an earlier run
  key("F0Tg")->fail_on_write = 1;
  CHECK(fan_apply(&fake, 2, 80, devnull) == FAN_FAILED);
  CHECK(mode(0) == 0 && mode(1) == 0);
}

static void test_verify_accepts_the_intended_state(void) {
  fake_reset(2);
  CHECK(fan_verify(&fake, 2, 80, fan_apply(&fake, 2, 80, devnull), devnull, devnull) == FAN_OK);
  CHECK(fan_verify(&fake, 2, 0, fan_apply(&fake, 2, 0, devnull), devnull, devnull) == FAN_OK);
}

static void test_verify_detects_a_fan_macos_reverted(void) {
  fake_reset(2);
  int applied = fan_apply(&fake, 2, 80, devnull);
  set_mode(0, 0);
  // fan 1 is still manual as requested: it must not be reported as stuck
  CHECK(fan_verify(&fake, 2, 80, applied, devnull, devnull) == FAN_FAILED);
}

static void test_verify_fails_closed_on_an_unreadable_mode(void) {
  fake_reset(2);
  int applied = fan_apply(&fake, 2, 0, devnull);
  key("F0md")->fail_read = 1;
  CHECK(fan_verify(&fake, 2, 0, applied, devnull, devnull) == (FAN_FAILED | FAN_STUCK_MANUAL));
  fake_reset(2);
  applied = fan_apply(&fake, 2, 80, devnull);
  key("F0md")->fail_read = 1;
  CHECK(fan_verify(&fake, 2, 80, applied, devnull, devnull) == FAN_FAILED);
}

static void test_verify_detects_a_fan_still_manual_after_auto(void) {
  fake_reset(1);
  int applied = fan_apply(&fake, 1, 0, devnull);
  set_mode(0, 1);  // the write was accepted but the fan stayed manual
  CHECK(fan_verify(&fake, 1, 0, applied, devnull, devnull) == (FAN_FAILED | FAN_STUCK_MANUAL));
}

static void test_verify_detects_a_fan_still_manual_after_rollback(void) {
  fake_reset(2);
  key("F0Tg")->fail_on_write = 1;
  int applied = fan_apply(&fake, 2, 80, devnull);
  set_mode(1, 1);  // accepted the return to auto but stayed manual
  CHECK(fan_verify(&fake, 2, 80, applied, devnull, devnull) == (FAN_FAILED | FAN_STUCK_MANUAL));
}

static void test_verify_keeps_the_failure_of_a_clean_rollback(void) {
  fake_reset(2);
  key("F1Tg")->fail_on_write = 1;
  int applied = fan_apply(&fake, 2, 80, devnull);
  CHECK(mode(0) == 0 && mode(1) == 0);
  CHECK(fan_verify(&fake, 2, 80, applied, devnull, devnull) == FAN_FAILED);
}

int main(void) {
  devnull = fopen("/dev/null", "w");
  test_sets_every_fan_manual_at_pct_of_max();
  test_auto_returns_every_fan_to_auto();
  test_no_fans_is_an_error();
  test_implausible_fan_count_writes_nothing();
  test_mode_failure_on_second_fan_rolls_back_the_first();
  test_target_failure_returns_every_fan_to_auto();
  test_unreadable_min_leaves_fans_untouched();
  test_zero_min_is_refused();
  test_wrong_key_type_is_refused();
  test_failed_rollback_reports_stuck_manual();
  test_failed_auto_reports_stuck_manual();
  test_auto_continues_past_a_refused_fan();
  test_rollback_continues_past_a_refused_fan();
  test_rollback_covers_fans_after_the_failing_one();
  test_verify_accepts_the_intended_state();
  test_verify_detects_a_fan_macos_reverted();
  test_verify_fails_closed_on_an_unreadable_mode();
  test_verify_detects_a_fan_still_manual_after_auto();
  test_verify_detects_a_fan_still_manual_after_rollback();
  test_verify_keeps_the_failure_of_a_clean_rollback();
  if (fails) {
    fprintf(stderr, "%d check(s) failed\n", fails);
    return 1;
  }
  puts("fan tests: ok");
  return 0;
}
