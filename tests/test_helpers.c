// ABOUTME: Unit tests for the hardware-free helpers in smc.c: percent parsing, target speed, number decoding.
// ABOUTME: Plain asserts, no framework; prints every failure and exits non-zero if any.
#include "../src/smc.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      fails++;                                                            \
    }                                                                     \
  } while (0)

static void test_parse_percent(void) {
  CHECK(parse_percent("80") == 80);
  CHECK(parse_percent("1") == 1);
  CHECK(parse_percent("100") == 100);
  CHECK(parse_percent("0") == -1);
  CHECK(parse_percent("101") == -1);
  CHECK(parse_percent("-5") == -1);
  CHECK(parse_percent("+80") == -1);
  CHECK(parse_percent(" 80") == -1);
  CHECK(parse_percent("80abc") == -1);
  CHECK(parse_percent("abc") == -1);
  CHECK(parse_percent("") == -1);
  CHECK(parse_percent(NULL) == -1);
  CHECK(parse_percent("99999999999999999999") == -1);
}

static void test_fan_target(void) {
  CHECK(fabsf(fan_target(80, 1350, 5349) - 4279.2f) < 0.5f);
  CHECK(fan_target(100, 1350, 5349) == 5349);
  CHECK(fan_target(10, 1350, 5349) == 1350);  // 535 rpm would be below min
  CHECK(fan_target(80, 0, 0) < 0);            // max unreadable or zero
  CHECK(fan_target(1, 0, 5349) < 0);          // zero min would allow a near-stopped fan
  CHECK(fan_target(80, 6000, 5000) < 0);      // min above max
  CHECK(fan_target(80, -1, 5000) < 0);
  CHECK(fan_target(80, NAN, 5000) < 0);
  CHECK(fan_target(80, 1350, NAN) < 0);
}

static void test_smc_number(void) {
  smc_val_t v = {0};
  float f = 1856.0f;
  v.size = 4; v.type = smc_key("flt "); memcpy(v.bytes, &f, 4);
  CHECK(smc_number(&v) == 1856.0);
  v.size = 1; v.type = smc_key("ui8 "); v.bytes[0] = 2;
  CHECK(smc_number(&v) == 2.0);
  v.size = 2; v.type = smc_key("fpe2"); v.bytes[0] = 0x1d; v.bytes[1] = 0x00;
  CHECK(smc_number(&v) == 1856.0);  // 0x1d00 / 4
  v.size = 4; v.type = smc_key("hex_");
  CHECK(isnan(smc_number(&v)));
}

static void test_key_roundtrip(void) {
  char s[5];
  CHECK(smc_key("F0md") == 0x46306d64);
  smc_key_str(smc_key("FNum"), s);
  CHECK(strcmp(s, "FNum") == 0);
}

int main(void) {
  test_parse_percent();
  test_fan_target();
  test_smc_number();
  test_key_roundtrip();
  if (fails) {
    fprintf(stderr, "%d check(s) failed\n", fails);
    return 1;
  }
  puts("unit tests: ok");
  return 0;
}
