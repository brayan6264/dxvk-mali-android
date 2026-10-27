# Building

This is what I did on Windows 11. Linux should be the same with different paths, but I didn't try it.

## You need

- Android NDK. I used 27.0.12077973 (clang 18). Set `ANDROID_NDK_HOME` to its folder.
- meson (I had 1.12.1), ninja, git, python 3.
- `libSDL3.so` for arm64. I took it from the app's apk (`lib/arm64-v8a/libSDL3.so`). It only has to
  exist at link time, the app loads its own copy.
- SDL3 headers. I used the `release-3.2.0` tag of libsdl-org/SDL.

## Steps

1. Get dxvk-native at the tag the patch is made for:

   ```
   git clone --branch native-1.9.2b --depth 1 https://github.com/Joshua-Ashton/dxvk-native.git
   cd dxvk-native
   git apply --ignore-whitespace ../dxvk-mali-android/patches/dxvk-native-1.9.2b-mali-android.patch
   ```

   I checked that the patch applies clean on a fresh copy of that tag.

2. Get the SDL3 headers somewhere, for example:

   ```
   git clone --branch release-3.2.0 --depth 1 https://github.com/libsdl-org/SDL.git
   ```

   The headers are then in `SDL/include`.

3. Copy `build/android-arm64.ini.example` to `android-arm64.ini` and replace `NDK_BIN` and
   `SDL3_INCLUDE` as the comments say.

4. DXVK calls `glslangValidator` while it builds. I didn't install it, I use `build/glslang_shim.py`
   which does the same job with the `glslc` that comes with the NDK. It only understands the exact
   command line DXVK uses.

   - Windows: put `build/windows` in your `PATH`. It has `glslangValidator.cmd` and an `echo.cmd`
     (meson looks for a program called `echo` in this fork).
   - Linux: make a script called `glslangValidator` in your `PATH` that runs
     `python3 /path/to/build/glslang_shim.py "$@"`.

5. Configure and build:

   ```
   meson setup out --cross-file android-arm64.ini --buildtype=release \
       -Ddxvk_native_force=true -Ddxvk_native_wsi=sdl3 \
       -Dsdl3_libdir=/folder/that/has/libSDL3.so \
       -Denable_d3d9=false -Denable_d3d10=false -Denable_tests=false
   ninja -C out
   ```

   The libraries come out in `out/src/d3d11/libdxvk_d3d11.so` and `out/src/dxgi/libdxvk_dxgi.so`.

6. Quick check. The d3d11 library should export 3 functions and dxgi 10 (the extra ones are SHA1
   helpers). With the NDK's `llvm-nm`:

   ```
   llvm-nm -D --defined-only libdxvk_d3d11.so | grep " T "
   ```

   If you export hundreds of symbols the hidden visibility flag didn't get applied. That breaks
   exception handling between the two libraries and the game crashes on the first error.

## Notes

- The release zip is from a build on my machine. I haven't checked if a rebuild somewhere else gives
  the same bytes.
- `meson.build` runs `git describe`, so build from a git checkout, not a zip of the source.
- The BC decoder has a small test: `g++ -std=c++17 tests/bc/test_bc.cpp -o test_bc && ./test_bc`.
  It checks BC1 to BC5 against blocks I worked out by hand.
