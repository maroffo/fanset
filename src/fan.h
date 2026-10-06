// ABOUTME: Fan control logic behind fanset: set every fan to a percentage of its max, or return them to auto.
// ABOUTME: SMC access goes through an injectable smc_io_t, so every failure path is unit-tested with a fake SMC.
#ifndef FAN_H
#define FAN_H

#include <IOKit/IOKitLib.h>
#include <stdio.h>

typedef struct {
  int (*read)(const char *key, const char *type, void *out, UInt32 size);
  int (*write)(const char *key, const char *type, const void *val, UInt32 size);
} smc_io_t;

enum { FAN_OK = 0, FAN_FAILED = 1, FAN_STUCK_MANUAL = 2 };

// fan_apply: pct 1-100 puts every fan in manual mode at pct of its max (never below min);
// pct 0 returns every fan to automatic mode. All or nothing: if any fan fails in manual
// mode, every fan is returned to automatic. Returns FAN_OK or FAN_FAILED, plus
// FAN_STUCK_MANUAL when a fan could not be returned to automatic mode.
int fan_apply(const smc_io_t *io, int nfans, int pct, FILE *err);

#endif
