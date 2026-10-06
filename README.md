# ABOUTME: fanset, fanread, fankeys: set Apple Silicon Mac fans to a fixed speed from the terminal, and inspect them
# ABOUTME: Small C tools over the AppleSMC IOKit service; fanset writes only the per-fan mode and target keys

# fanset

Set the fans of an Apple Silicon Mac to a fixed speed from the terminal, and give control back to macOS when you are done.

```
$ sudo fanset 80
fan 0: manual target=4279 actual=3950 rpm (74% of max)
fan 1: manual target=4622 actual=4310 rpm (75% of max)

$ sudo fanset auto
fan 0: auto target=1350 actual=1500 rpm (28% of max)
fan 1: auto target=1350 actual=1480 rpm (26% of max)
```

(Illustrative output: the write path has not been verified on hardware yet, see [Tested on](#tested-on).)

Three small tools, no dependencies beyond the macOS SDK:

| Tool | What it does | Needs root |
|------|--------------|------------|
| `fanset <1-100>` | Fixes every fan at that percentage of its max speed (never below its min) | yes |
| `fanset auto` | Returns every fan to automatic control | yes |
| `fanread` | Shows actual, target, min, max speed and mode for every fan | no |
| `fankeys` | Lists every SMC key starting with `F` (the fan keys and a few others) with type, size, attributes and value | no |

## Why

macOS has no built-in command to control the fans. The classic tools (smcFanControl and friends) switch a fan to manual through the SMC key `F0Md`. On the M5 MacBook Pro that key does not exist: the mode key is `F0md`, lowercase. `fankeys` is how that was found, and it is the first thing to run on a Mac where `fanset` does not work.

## Requirements

- An Apple Silicon Mac with fans (MacBook Air has none)
- macOS 12 or later
- Xcode Command Line Tools (`xcode-select --install`)

## Build and install

```
make all                        # builds bin/fanset, bin/fanread, bin/fankeys
sudo make install               # installs to /usr/local/bin
make install PREFIX=~/.local    # or somewhere you own
```

## How it works

The tools talk to the `AppleSMC` kernel service through IOKit. `fanset` writes exactly two keys per fan, and nothing else:

| Key | Type | Meaning |
|-----|------|---------|
| `FnMn`, `FnMx` | `flt` | Min and max speed in rpm (read only) |
| `Fnmd` | `ui8` | Mode: `0` automatic, `1` manual (written) |
| `FnTg` | `flt` | Target speed in rpm (written) |
| `FnAc` | `flt` | Actual speed in rpm (read only) |

## Safety

- `fanset auto` or a reboot always returns the fans to automatic control.
- `fanset` refuses to run without root, and accepts only `auto` or a plain integer from 1 to 100.
- It reads min and max before changing anything; if they are missing or implausible (a min of 0 included), nothing is switched to manual.
- All or nothing: if any fan fails along the way, every fan is returned to automatic. If even that fails, it prints a warning telling you to retry `sudo fanset auto` or reboot.
- Every read and write checks the key's type and size first. On a Mac whose keys have a different layout (Intel Macs use `fpe2` instead of `flt`) the write is refused instead of corrupting a value.
- After writing, it waits 4 seconds and reads everything back. If macOS has reverted a fan to automatic, it says so and exits with an error.
- The rollback paths are unit-tested against a fake SMC (`tests/test_fan.c`), since they cannot be triggered on demand on real hardware.

Forcing a fan to a fixed speed overrides macOS thermal management for the fans (the CPU still throttles on its own when it is hot). A high fixed speed is the safe direction; prefer `auto` for everyday use.

This is an unofficial tool that writes to hardware controller registers. Use it at your own risk.

## Tested on

| Mac | macOS | Read tools | Write (`fanset`) |
|-----|-------|------------|------------------|
| MacBook Pro M5 Pro (Mac17,8) | 27.0 | verified | not yet verified, run `make test-hw` |

Results on other Macs are welcome: open an issue with your Mac model, macOS version, and the output of `fankeys` and `make test-hw`.

## Tests

```
make check     # warnings-as-errors build, unit tests (incl. fan logic on a fake SMC), CLI tests
make test-e2e  # fanread and fankeys against the real SMC (skips without fans)
make test-hw   # end-to-end write test on real fans: asks for sudo, sets 60%,
               # checks manual mode and spin-up, then restores automatic mode
```

CI runs `make check` and `make test-e2e`; on its runners the e2e tests skip because there are no fans, and `make test-hw` never runs there.

## License

Apache License 2.0, see [LICENSE](LICENSE).
