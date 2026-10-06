// ABOUTME: Sets every Mac fan to a fixed percentage of its max speed, or returns them to automatic control.
// ABOUTME: Usage: sudo fanset <1-100> | sudo fanset auto. Writes only the SMC keys Fnmd (mode) and FnTg (target).
#include "fan.h"
#include "smc.h"
#include <string.h>
#include <unistd.h>

static const smc_io_t smc_io = {smc_read, smc_write};

static int usage(void) {
  fprintf(stderr,
          "usage: sudo fanset <1-100> | sudo fanset auto\n"
          "  <1-100>  fixed speed, as a percentage of each fan's max (never below its min)\n"
          "  auto     return every fan to automatic control\n");
  return 2;
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
  if (smc_read("FNum", "ui8 ", &n, 1)) {
    fprintf(stderr, "fanset: cannot read fan count\n");
    smc_close();
    return 1;
  }

  int rc = fan_apply(&smc_io, n, pct, stderr);
  if (n < 1 || n > 9) {  // fan_apply already said why nothing changed
    smc_close();
    return 1;
  }
  sleep(4);
  rc = fan_verify(&smc_io, n, pct, rc, stdout, stderr);
  smc_close();
  if (rc & FAN_STUCK_MANUAL)
    fprintf(stderr, "fanset: WARNING a fan may still be in manual mode: retry 'sudo fanset auto', or reboot\n");
  else if (rc)
    fprintf(stderr, "fanset: run 'fankeys' to inspect this Mac's SMC keys; 'sudo fanset auto' returns every fan to automatic\n");
  return rc ? 1 : 0;
}
