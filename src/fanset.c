// ABOUTME: Sets every Mac fan to a fixed percentage of its max speed, or returns them to automatic control.
// ABOUTME: Usage: sudo fanset <1-100> | sudo fanset auto. Writes only the SMC keys Fnmd (mode) and FnTg (target).
#include "smc.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int usage(void) {
  fprintf(stderr, "usage: sudo fanset <1-100> | sudo fanset auto\n");
  return 2;
}

static int set_auto(int i) {
  char k[5];
  unsigned char md = 0;
  snprintf(k, sizeof(k), "F%dmd", i);
  if (smc_write(k, &md, 1)) {
    fprintf(stderr, "fan %d: mode write refused\n", i);
    return 1;
  }
  return 0;
}

// set_manual reads min/max before touching anything, and restores automatic
// mode if the target write fails, so a fan is never left manual at a stale speed.
static int set_manual(int i, int pct) {
  char k[5];
  float min, max;
  snprintf(k, sizeof(k), "F%dMn", i);
  int bad = smc_read(k, &min, 4);
  snprintf(k, sizeof(k), "F%dMx", i);
  bad |= smc_read(k, &max, 4);
  float tg = bad ? -1 : fan_target(pct, min, max);
  if (tg < 0) {
    fprintf(stderr, "fan %d: cannot read a plausible min/max, left untouched\n", i);
    return 1;
  }
  unsigned char md = 1;
  snprintf(k, sizeof(k), "F%dmd", i);
  if (smc_write(k, &md, 1)) {
    fprintf(stderr, "fan %d: mode write refused\n", i);
    return 1;
  }
  snprintf(k, sizeof(k), "F%dTg", i);
  if (smc_write(k, &tg, 4)) {
    fprintf(stderr, "fan %d: target write refused, restoring automatic mode\n", i);
    set_auto(i);
    return 1;
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc != 2) return usage();
  int automode = !strcmp(argv[1], "auto");
  int pct = automode ? 0 : parse_percent(argv[1]);
  if (!automode && pct < 0) return usage();
  if (geteuid() != 0) {
    fprintf(stderr, "fanset: needs root, run it with sudo\n");
    return 1;
  }
  if (smc_open()) {
    fprintf(stderr, "fanset: cannot open SMC\n");
    return 1;
  }
  unsigned char n = 0;
  if (smc_read("FNum", &n, 1) || n > 9) {
    fprintf(stderr, "fanset: cannot read fan count\n");
    smc_close();
    return 1;
  }

  int rc = 0;
  for (int i = 0; i < n; i++) rc |= automode ? set_auto(i) : set_manual(i, pct);

  sleep(4);
  for (int i = 0; i < n; i++) {
    char k[5];
    unsigned char md = 0;
    float ac = 0, tg = 0, max = 0;
    snprintf(k, sizeof(k), "F%dmd", i); smc_read(k, &md, 1);
    snprintf(k, sizeof(k), "F%dAc", i); smc_read(k, &ac, 4);
    snprintf(k, sizeof(k), "F%dTg", i); smc_read(k, &tg, 4);
    snprintf(k, sizeof(k), "F%dMx", i); smc_read(k, &max, 4);
    printf("fan %d: %s target=%.0f actual=%.0f rpm (%.0f%% of max)\n", i, md ? "manual" : "auto", tg, ac,
           max > 0 ? 100 * ac / max : 0);
    if (!automode && !md) {
      fprintf(stderr, "fan %d: macOS put it back to auto, the setting did not stick\n", i);
      rc = 1;
    }
    if (automode && md) {
      fprintf(stderr, "fan %d: still in manual mode\n", i);
      rc = 1;
    }
  }
  smc_close();
  return rc;
}
