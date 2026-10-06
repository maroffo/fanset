// ABOUTME: Read-only fan report: count, actual/target/min/max rpm and mode (auto or manual) for every fan.
// ABOUTME: Needs no root; performs no writes.
#include "smc.h"
#include <math.h>
#include <stdio.h>

static double num(const char *key) {
  smc_val_t v;
  return smc_get(smc_key(key), &v) ? NAN : smc_number(&v);
}

int main(void) {
  if (smc_open()) {
    fprintf(stderr, "fanread: cannot open SMC\n");
    return 1;
  }
  double fnum = num("FNum");
  if (isnan(fnum) || fnum > 9) {
    fprintf(stderr, "fanread: cannot read fan count\n");
    smc_close();
    return 1;
  }
  int n = (int)fnum;
  printf("fans: %d\n", n);
  for (int i = 0; i < n; i++) {
    char k[5];
#define FAN(suffix) (snprintf(k, sizeof(k), "F%d" suffix, i), num(k))
    double ac = FAN("Ac"), tg = FAN("Tg"), min = FAN("Mn"), max = FAN("Mx"), md = FAN("md");
#undef FAN
    const char *mode = isnan(md) ? "unknown" : md ? "manual" : "auto";
    printf("fan %d: actual=%.0f rpm target=%.0f min=%.0f max=%.0f mode=%s (%.0f%% of max)\n", i, ac, tg, min, max,
           mode, max > 0 ? 100 * ac / max : 0);
  }
  smc_close();
  return 0;
}
