# Rebuilding and relinking projectM

TrimPod(RUS) links projectM statically. You may rebuild or modify projectM and
link your replacement into the application.

## Rebuild the libraries

From WSL/Linux with Docker available:

```sh
bash tools/build_projectm.sh
```

This cross-compiles the checked-in projectM 4.1.6 source for the NextUI
`tg5040` target and replaces:

```text
lib/projectm/lib/libprojectM-4.a
lib/projectm/lib/libprojectM_eval.a
```

The source already contains the TrimPod framebuffer patch. To start from the
unmodified upstream file, reverse it from the source directory with:

```sh
git apply -R ../../patches/0001-preserve-caller-framebuffer.patch
```

You may then edit the library and rerun the build script.

## Relink TrimPod

The normal source build relinks the executable against the two archives:

```sh
./build.sh
```

For a published release, download its repository source archive and matching
`TrimPod(RUS)-relink-kit.tar.gz`, extract both so that `build-trimpod/` is at the
repository root, replace the archives as described above, and run `./build.sh`.
The relink kit contains the machine-readable Rockbox object files, generated
headers and make metadata from that release, so replacing an LGPL library does
not require reverse-engineering the application binary.

The resulting executable is `build-trimpod/trimpod`. Packaging it for the
device is a separate step performed by `./package.sh`.
