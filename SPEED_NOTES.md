# Speed experiments — final note (Nov 2026, closed)

Three routes were evaluated against a 10% end-to-end gate with bit-identical-output
verification on synthetic, dense, wide, two-line, and real-content 720p clips
(≥7 interleaved runs per dataset). All three failed the gate. No route is
advertised as a speedup.

| Route | Measured gain | Gate |
|---|---|---|
| `/arch:AVX2` (vsf_core only) | +3,42% (real 720p), +8,57% (synthetic 60 s) | FAILED — route stopped |
| CUDA `-uc` (kmeans offload, GTX 1070) | +1,09% (720p), +1,60% (4K) | FAILED — `-uc` kept as optional compatibility flag only |
| `Intersect*` family optimization (`VSF_INTERSECT_OPT=ON`) | +2,68% (real 720p), −0,7%/−2,4%/−7,2% on sparse sets (noise) | FAILED — opt-in experiment only |

Why they failed: profiling showed the hot loops live in the VSF code itself
(AnalyseImage is only 5–7% of instrumented time; the `Intersect*` family is
~50% of instrumented busy, but instrumented busy is only ~52% of wall clock —
the rest is decode, copies, and image I/O). Branchless loop rewrites and a
fused double-pass made the family 19,6% faster in instrumented terms, which
maps to ~3% wall clock.

Outputs were **bit-identical** in every test (manifest hashes matched across
builds on all datasets). No detection threshold or algorithm was changed in
any route; the `VSF_INTERSECT_OPT=OFF` build is byte-for-byte the upstream
loop behavior, and the CUDA build without `-uc` matches the CPU build.

## Final state

- `-uc` / CUDA: optional compatibility flag (needs CUDA toolkit + GPU at
  runtime). No speedup claim. The package ships only `cudart64_12.dll`
  (NPP is dead-stripped since `FFMPEGVideo.cpp` is not in `vsf_core`).
- `VSF_INTERSECT_OPT`: opt-in CMake option (`-DVSF_INTERSECT_OPT=ON`), off
  by default. No speedup claim.
- AVX2: option `VSF_ARCH_AVX2` shipped in the fork but the route is stopped.
- All four CI workflows (`avx2`, `cuda`, `profile`, `intersect`) are green.

## Known issue — GPU decode

Video decode through a GPU path (NVDEC / OpenCV CUDA videoio) can
intermittently produce **corrupt video** (artefacted or truncated frames)
on some files. When that happens, fall back to **CPU decode** for those
files: use the OpenCV FFmpeg backend (`-ovffmpeg`, CPU libavcodec) or the
non-CUDA build, and compare the output manifest hash against the CPU
reference before trusting a GPU-decoded result.
