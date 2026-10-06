// ABOUTME: AppleSMC access shared by the fan tools (open, key info, read, write, enumerate) plus pure helpers.
// ABOUTME: The helpers (percent parsing, target speed, number decoding) need no hardware and are unit-tested.
#ifndef SMC_H
#define SMC_H

#include <IOKit/IOKitLib.h>

typedef struct {
  UInt32 size, type;
  unsigned char attr;
  unsigned char bytes[32];
} smc_val_t;

UInt32 smc_key(const char *s);
void smc_key_str(UInt32 key, char out[5]);

int smc_open(void);
void smc_close(void);

// smc_get: 0 on success, -1 if the key does not exist, -2 if it exists but cannot be read.
int smc_get(UInt32 key, smc_val_t *v);
// smc_read / smc_write: 0 on success, -1 on failure or when the key's type or size differs
// from `type` (four characters, e.g. "flt ") and `size`.
int smc_read(const char *key, const char *type, void *out, UInt32 size);
int smc_write(const char *key, const char *type, const void *val, UInt32 size);
int smc_key_count(UInt32 *n);
int smc_key_at(UInt32 index, UInt32 *key);

// smc_number: decodes flt, ui8 and fpe2 values; NAN for any other type.
double smc_number(const smc_val_t *v);
// parse_percent: 1-100 for a plain decimal string, -1 for anything else.
int parse_percent(const char *s);
// fan_target: pct of max in rpm, never below min; -1 when min/max are implausible (min must be > 0).
float fan_target(int pct, float min, float max);

#endif
