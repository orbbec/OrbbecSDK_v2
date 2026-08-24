# Test Scripts

These scripts run the OpenOrbbecSDK test suites (unit / hw / scenario / perf / destructive).

## Requirements

- cmake, ctest
- Git Bash (Windows) or bash (Linux/macOS)
- git, clang-format
- lsusb (device_health_check.sh, Linux), Get-PnpDevice (device_health_check.ps1, Windows)

## Scripts

| Script | Purpose |
|--------|---------|
| run_tests.sh | Main entry: health check (optional), configure/build, run ctest, save the raw junit log |
| device_health_check.sh / .ps1 | Check device infrastructure; exits 0 if ready, 1 if not |

## run_tests.sh

```
bash tests/scripts/run_tests.sh [options]
  --config=<TYPE>    Build type: Release|Debug|RelWithDebInfo (default: Release)
  --build-dir=<DIR>  CMake build directory (default: <root>/build)
  --jobs=<N>         Parallel build jobs
  --no-build         Skip configure & build, only run tests
  --format          Run clang-format check on changed C++ files
  --install         Run cmake install and verify
  --health          Run device health check before tests
  -h, --help        Show help
```

Exit codes:

- 0: all enabled suites passed
- 1: one or more ctest suites failed
- 2: environment or configuration error

Which suites run is controlled by `tests/test_config.json` (`suites` block).

## Output

- JUnit XML: `<build-dir>/test-results/junit-all.xml`
- Full log: `<build-dir>/test-results/run-all.log`
- SDK log: `<build-dir>/test-results/OrbbecSDK.log.txt`
