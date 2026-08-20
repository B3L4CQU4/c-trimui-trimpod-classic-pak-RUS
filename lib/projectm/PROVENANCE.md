# projectM provenance

TrimPod(RUS) statically links projectM and projectM-eval for its MilkDrop
visualizer. The complete corresponding source is kept in this repository so a
recipient can inspect, rebuild, replace and relink these LGPL components.

## Upstream revisions

- projectM 4.1.6: `projectM-visualizer/projectm` commit
  `3158ee615eaafd93a8912b5f6dd84a9c47b2e00a`.
- projectM-eval 1.0.5: `projectM-visualizer/projectm-eval` commit
  `811eea5594cc4092d0985fea9ccf0e52dec8a20a`.
- GLM 0.9.9.0, vendored by projectM: `g-truc/glm` commit
  `fe7c7b5ac15ba8d5c9c45984831dc2e830726b33`.

The source snapshot is in `source/projectm-4.1.6/`. The projectM-eval source is
populated in its upstream `vendor/projectm-eval/` location. The only
TrimPod-specific source change is recorded as
`patches/0001-preserve-caller-framebuffer.patch`; that patch is already applied
to the checked-in source tree.

## Prebuilt archives

The original TrimPod project supplied the AArch64 static archives which this
fork initially inherited. Their SHA-256 values are:

- `lib/libprojectM-4.a`:
  `F00BF05BFC67AE00DEF9E78D33472BDFBBC07A6C287DA7B80D52651662AA1604`
- `lib/libprojectM_eval.a`:
  `F53E7B0DE26A1702012A9BD2CC53AE328991CC603A7EBCDBD9F01C3D539E4516`

The exact historical command line used to create those two inherited archives
was not retained. The source revision, local change and current cross-build
recipe are now recorded here. Run `bash tools/build_projectm.sh` from the repository
root to replace the archives with a build from the checked-in source.

See `RELINKING.md` for replacement and relinking instructions. Binary releases
also include a `TrimPod(RUS)-relink-kit.tar.gz` asset containing the generated
Rockbox object files and this projectM source tree.

## Licenses

projectM is LGPL-2.1-or-later and projectM-eval is MIT licensed. License texts
and notices for projectM and its bundled dependencies are shipped in
`pak/licenses/` and remain in the source snapshot.
