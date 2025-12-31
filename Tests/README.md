# Rush Test Harness

This folder hosts the automated test runner for librush. The intent is to cover
all public APIs by executing rendering or compute workloads and validating GPU
readback results (buffers or screenshots).

## Structure

- `TestFramework.h` / `TestFramework.cpp`: registry, runner state machine, and
  screenshot capture path.
- `TestMain.cpp`: application entry point.
- `Test*.cpp`: individual tests.

## Adding a test

1. Create a new `Test*.cpp` file.
2. Implement a `TestCase` subclass.
3. Provide a description string for the test.
4. Register it with `RUSH_REGISTER_TEST(MyTest, "category", "Description")`.

Tests can opt out of screenshots by returning `TestConfig{false}` from
`config()` and can set `requiresGraphics = false` for CPU-only tests. Future
tests may map buffers or use custom readback paths.

## Build instructions

Use CMake presets (recommended):

- `cmake --list-presets` to see available presets.
- `cmake --preset <preset>` then `cmake --build --preset <preset> --target Tests`

## Running tests

- `./Build/<preset>/Tests/Tests` to run tests built by that preset
- `--list` to list tests
- `--list-categories` to list categories
- `--test, -t PATTERN` to run tests matching a wildcard
- `--category, -c CAT` to run categories matching a wildcard
- `--no-gfx` or `--cpu-only` to run only non-graphics tests
