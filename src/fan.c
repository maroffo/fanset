// ABOUTME: Fan control logic behind fanset: set every fan to a percentage of its max, or return them to auto.
// ABOUTME: All or nothing: any failure in manual mode returns every fan to automatic control.
#include "fan.h"
#include "smc.h"

static int set_auto(const smc_io_t *io, int i, FILE *err) {
  char k[5];
  unsigned char md = 0;
  snprintf(k, sizeof(k), "F%dmd", i);
  if (io->write(k, "ui8 ", &md, 1)) {
    fprintf(err, "fan %d: mode write refused\n", i);
    return -1;
  }
  return 0;
}

// set_manual reads min/max before touching anything, so a fan with missing or
// implausible limits is never switched to manual.
static int set_manual(const smc_io_t *io, int i, int pct, FILE *err) {
  char k[5];
  float min, max;
  snprintf(k, sizeof(k), "F%dMn", i);
  int bad = io->read(k, "flt ", &min, 4);
  snprintf(k, sizeof(k), "F%dMx", i);
  bad |= io->read(k, "flt ", &max, 4);
  float tg = bad ? -1 : fan_target(pct, min, max);
  if (tg < 0) {
    fprintf(err, "fan %d: cannot read a plausible min/max\n", i);
    return -1;
  }
  unsigned char md = 1;
  snprintf(k, sizeof(k), "F%dmd", i);
  if (io->write(k, "ui8 ", &md, 1)) {
    fprintf(err, "fan %d: mode write refused\n", i);
    return -1;
  }
  snprintf(k, sizeof(k), "F%dTg", i);
  if (io->write(k, "flt ", &tg, 4)) {
    fprintf(err, "fan %d: target write refused\n", i);
    return -1;
  }
  return 0;
}

int fan_apply(const smc_io_t *io, int nfans, int pct, FILE *err) {
  if (nfans < 1 || nfans > 9) {
    fprintf(err, nfans ? "unexpected fan count %d, nothing changed\n" : "this Mac reports no fans\n", nfans);
    return FAN_FAILED;
  }
  if (pct == 0) {
    int rc = FAN_OK;
    for (int i = 0; i < nfans; i++)
      if (set_auto(io, i, err)) rc = FAN_FAILED | FAN_STUCK_MANUAL;
    return rc;
  }
  for (int i = 0; i < nfans; i++) {
    if (set_manual(io, i, pct, err) == 0) continue;
    fprintf(err, "returning every fan to automatic mode\n");
    int rc = FAN_FAILED;
    for (int j = 0; j < nfans; j++)
      if (set_auto(io, j, err)) rc |= FAN_STUCK_MANUAL;
    return rc;
  }
  return FAN_OK;
}
