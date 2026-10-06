// ABOUTME: AppleSMC access shared by the fan tools (open, key info, read, write, enumerate) plus pure helpers.
// ABOUTME: Talks to the AppleSMC user client through IOConnectCallStructMethod with the classic 80-byte message.
#include "smc.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char major, minor, build, reserved[1]; UInt16 release; } smc_vers_t;
typedef struct { UInt16 version, length; UInt32 cpu, gpu, mem; } smc_plimit_t;
typedef struct { UInt32 dataSize, dataType; char dataAttributes; } smc_keyinfo_t;
typedef struct {
  UInt32 key;
  smc_vers_t vers;
  smc_plimit_t plimit;
  smc_keyinfo_t info;
  char result, status, data8;
  UInt32 data32;
  unsigned char bytes[32];
} smc_msg_t;

enum { SMC_SELECTOR = 2, CMD_READ = 5, CMD_WRITE = 6, CMD_KEY_AT_INDEX = 8, CMD_KEY_INFO = 9 };

static io_connect_t conn;

static int call(smc_msg_t *in, smc_msg_t *out) {
  size_t out_size = sizeof(*out);
  memset(out, 0, sizeof(*out));
  if (IOConnectCallStructMethod(conn, SMC_SELECTOR, in, sizeof(*in), out, &out_size) != kIOReturnSuccess)
    return -1;
  return out->result ? -1 : 0;
}

UInt32 smc_key(const char *s) {
  return ((UInt32)(unsigned char)s[0] << 24) | ((UInt32)(unsigned char)s[1] << 16) |
         ((UInt32)(unsigned char)s[2] << 8) | (UInt32)(unsigned char)s[3];
}

void smc_key_str(UInt32 key, char out[5]) {
  out[0] = (char)(key >> 24);
  out[1] = (char)(key >> 16);
  out[2] = (char)(key >> 8);
  out[3] = (char)key;
  out[4] = 0;
}

int smc_open(void) {
  io_service_t svc = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleSMC"));
  if (!svc) return -1;
  kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
  IOObjectRelease(svc);
  return kr == KERN_SUCCESS ? 0 : -1;
}

void smc_close(void) { IOServiceClose(conn); }

int smc_get(UInt32 key, smc_val_t *v) {
  smc_msg_t in = {0}, out;
  in.key = key;
  in.data8 = CMD_KEY_INFO;
  if (call(&in, &out)) return -1;
  v->size = out.info.dataSize;
  v->type = out.info.dataType;
  v->attr = (unsigned char)out.info.dataAttributes;
  if (v->size > sizeof(v->bytes)) return -2;
  in.info.dataSize = v->size;
  in.data8 = CMD_READ;
  if (call(&in, &out)) return -2;
  memcpy(v->bytes, out.bytes, v->size);
  return 0;
}

int smc_read(const char *key, const char *type, void *out, UInt32 size) {
  smc_val_t v;
  if (smc_get(smc_key(key), &v) || v.type != smc_key(type) || v.size != size) return -1;
  memcpy(out, v.bytes, size);
  return 0;
}

int smc_write(const char *key, const char *type, const void *val, UInt32 size) {
  smc_msg_t in = {0}, out;
  in.key = smc_key(key);
  in.data8 = CMD_KEY_INFO;
  if (call(&in, &out) || out.info.dataType != smc_key(type) || out.info.dataSize != size || size > sizeof(in.bytes))
    return -1;
  in.info.dataSize = size;
  in.data8 = CMD_WRITE;
  memcpy(in.bytes, val, size);
  return call(&in, &out);
}

int smc_key_count(UInt32 *n) {
  smc_val_t v;
  if (smc_get(smc_key("#KEY"), &v) || v.size != 4) return -1;
  *n = ((UInt32)v.bytes[0] << 24) | ((UInt32)v.bytes[1] << 16) | ((UInt32)v.bytes[2] << 8) | v.bytes[3];
  return 0;
}

int smc_key_at(UInt32 index, UInt32 *key) {
  smc_msg_t in = {0}, out;
  in.data8 = CMD_KEY_AT_INDEX;
  in.data32 = index;
  if (call(&in, &out)) return -1;
  *key = out.key;
  return 0;
}

double smc_number(const smc_val_t *v) {
  if (v->type == smc_key("flt ") && v->size == 4) {
    float f;
    memcpy(&f, v->bytes, 4);
    return f;
  }
  if (v->type == smc_key("ui8 ") && v->size == 1) return v->bytes[0];
  if (v->type == smc_key("fpe2") && v->size == 2) return ((v->bytes[0] << 8) | v->bytes[1]) / 4.0;
  return NAN;
}

int parse_percent(const char *s) {
  if (!s || !isdigit((unsigned char)s[0])) return -1;
  char *end;
  errno = 0;
  long v = strtol(s, &end, 10);
  if (errno || *end || v < 1 || v > 100) return -1;
  return (int)v;
}

float fan_target(int pct, float min, float max) {
  if (!(max > 0) || !(min > 0) || min > max) return -1;
  float t = max * (float)pct / 100.0f;
  return t < min ? min : t;
}
