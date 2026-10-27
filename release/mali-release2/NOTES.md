# mali-release2

The two DXVK libraries, built from `patches/dxvk-native-1.9.2b-mali-android.patch` in this repo, on
top of dxvk-native `native-1.9.2b`.

Files:

- `dxvk-1.9.2-mali-release2.zip` has `libdxvk_d3d11.so` and `libdxvk_dxgi.so` at the root of the zip.
- `import.json` is the small file the app keeps next to the libraries. Needed for the adb install in
  the README, not needed in the zip.
- `SHA256SUMS.txt` is for the zip.

SHA-256 of the files inside the zip:

```
90a8f98326923b9facf449183392d539f575ee44b9aac58a396689d1e99c4448  libdxvk_d3d11.so  (2736648 bytes)
3d47e60d75ee38bb84f8deaa9a9ffbf9ae639a630d4aa2bef94e014b9ed1c186  libdxvk_dxgi.so  (1303760 bytes)
```

What was tested with exactly these files (Retroid Pocket 4 Pro, T6 Requiem 2.0.0):

- Multiplayer menu: 42.3 fps, no crash.
- Zombies survival match, loaded and played for a few minutes, 41.7 fps average over the last 12
  samples, no crash.
- Before this release I also ran about 9 minutes of Multiplayer with bots at 960x540, 41.9 fps
  average. That was one build before this one. The only difference is the swap chain change.

Not tested with these files: the in-app zip import, any other device, any long session.

Everything that's wrong or missing is in the README, under Limitations.
