# AVX2 variant (suffix `avx2`) — temp fork notes

Fork of `eritpchy/videosubfinder-cli` (VideoSubFinder **6.10.5** core) used only to build and
A/B a `/arch:AVX2` variant against the same commit built with MSVC's default (SSE2) codegen.

## What this fork changes vs upstream

1. `CMakeLists.txt` — one new option plus three lines:

```cmake
option(VSF_ARCH_AVX2 "Compile vsf_core with /arch:AVX2 (variant suffix: avx2)" OFF)
...
if(VSF_ARCH_AVX2)
    target_compile_options(vsf_core PUBLIC /arch:AVX2)
endif()
```

   With `VSF_ARCH_AVX2=OFF` the build is byte-for-byte the upstream configuration.

2. `.github/workflows/avx2.yml` — builds both arms from the same commit, verifies YMM
   arithmetic in the disassembly, and A/Bs them end-to-end on a generated 720p clip.

No algorithm source was modified. Detection thresholds, worker pool and I/O are identical
between the two arms, so any output difference would be a real AVX2 codegen bug, and any
speed difference is attributable to codegen alone.

## CLI compatibility with the official VSF command line

`VideoSubFinderCli` accepts the same flags as the official `VideoSubFinderWXW_intel.exe`:

| flag | meaning |
| --- | --- |
| `-ovffmpeg` | OpenCV FFmpeg backend |
| `-c` | clear generated images |
| `-r` | run search |
| `-i` / `-o` | input video / output dir |
| `-s` / `-e` | start / end time |
| `-te` `-be` `-le` `-re` | top/bottom/left/right crop fractions (`top`/`bottom`/`0..1`) |
| `-uc` | use CUDA — **errors out** on a non-CUDA build |
| `-ccti` | create cleared text images |

Two real differences from an installed VSF 5.60:

- **Engine version.** This fork is 6.10.5; `L:\VideoSubFinder_5.60_x64\v6.1` is 5.60. Output
  images/timings may legitimately differ from the 5.60 manifest.
- **Front-end.** No wxWidgets; `-uc` is compiled out unless `USE_CUDA=ON`.

Therefore the AVX2 gate is evaluated **between the two arms built here** (`_sse2` vs `_avx2`),
not against a local 5.60 manifest.
