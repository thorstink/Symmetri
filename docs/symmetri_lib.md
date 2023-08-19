# Symmetri library

## Build

### Vanilla CMake:

Symmetri can be build as stand-alone CMake-project:

```bash
git clone --recurse-submodules https://github.com/thorstink/Symmetri.git
mkdir build
cd build
cmake ..
make
```

### In a Colcon-workspace
It can also be build as part of a [colcon-workspace](https://colcon.readthedocs.io/en/released/user/what-is-a-workspace.html):

```bash
mkdir ws/src
cd ws/src
git clone --recurse-submodules https://github.com/thorstink/Symmetri.git
cd .. # back to ws-directory
colcon build
```

## Test

Symmetri has a set of tests. There are three build configurations:

- No sanitizers
- [ASAN](https://github.com/google/sanitizers/wiki/AddressSanitizer) + [UBSAN](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
- [TSAN](https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual)

Each sanitizer can be enabled by setting either the 'ASAN_BUILD'- or 'TSAN_BUILD'-flag to true. They can not both be true at the same time.

The sanitizers generally increase compiltation time quite significantly, hence they are off by default. In the CI-pipeline however they are turned on for testing.

```bash
# from the build directory...
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_EXAMPLES=ON -DBUILD_TESTING=ON -DASAN_BUILD=OFF -DTSAN_BUILD=OFF ..
make
make test
```

## Architecture
