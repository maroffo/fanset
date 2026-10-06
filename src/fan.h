// ABOUTME: Fan control logic behind fanset: set every fan to a percentage of its max or back to auto, then verify.
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

// fan_verify: reads every fan back after fan_apply returned `applied`, prints one status
// line per fan to `out`, and adds FAN_FAILED when the state is not the intended one: a fan
// macOS reverted to auto, a mode that cannot be read, or a fan still manual after auto or a
// rollback (that last case also adds FAN_STUCK_MANUAL). Returns `applied` otherwise.
int fan_verify(const smc_io_t *io, int nfans, int pct, int applied, FILE *out, FILE *err);

#endif
