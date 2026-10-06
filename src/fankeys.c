// ABOUTME: Read-only SMC key enumerator: lists every key starting with 'F' with its type, size, attributes and value.
// ABOUTME: Used to discover which fan-control keys a given Mac exposes; performs no writes.
#include "smc.h"
#include <stdio.h>

int main(int argc, char **argv) {
  (void)argv;
  if (argc > 1) {
    fprintf(stderr, "usage: fankeys\n");
    return 2;
  }
  if (smc_open()) {
    fprintf(stderr, "fankeys: cannot open SMC\n");
    return 1;
  }
  UInt32 n;
  if (smc_key_count(&n)) {
    fprintf(stderr, "fankeys: cannot read key count\n");
    smc_close();
    return 1;
  }
  printf("total keys: %u\n", n);
  for (UInt32 i = 0; i < n; i++) {
    UInt32 key;
    char k[5], t[5];
    if (smc_key_at(i, &key)) continue;
    smc_key_str(key, k);
    if (k[0] != 'F') continue;
    smc_val_t v;
    int err = smc_get(key, &v);
    if (err == -1) {
      printf("%s (info err)\n", k);
      continue;
    }
    smc_key_str(v.type, t);
    printf("%s type=%s size=%u attr=0x%02x val=", k, t, v.size, v.attr);
    if (err) {
      printf("(read err)");
    } else if (v.type == smc_key("flt ")) {
      printf("%.1f", smc_number(&v));
    } else {
      for (UInt32 j = 0; j < v.size && j < 8; j++) printf("%02x", v.bytes[j]);
    }
    putchar('\n');
  }
  smc_close();
  return 0;
}
