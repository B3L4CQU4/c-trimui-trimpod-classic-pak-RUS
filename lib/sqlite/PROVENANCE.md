# Vendored SQLite amalgamation

`sqlite3.c` and `sqlite3.h` are the public-domain SQLite amalgamation, dropped in
verbatim from the official upstream release. Only these two files are needed for
a standard build — `shell.c` (CLI) and `sqlite3ext.h` (loadable extensions) are
not vendored, and every other in-tree `#include` inside `sqlite3.c` is guarded
off for our build (`_HAVE_SQLITE_CONFIG_H`, `SQLITE_OS_WIN`, `SQLITE_TCL`,
`SQLITE_ENABLE_RTREE`).

## Pinned release

| | |
|---|---|
| Version | **3.53.3** |
| Source | `https://www.sqlite.org/2026/sqlite-amalgamation-3530300.zip` |
| Zip size | 2945929 bytes |
| Zip SHA3-256 | `d45c688a8cb23f68611a894a756a12d7eb6ab6e9e2468ca70adbeab3808b5ab9` |
| `sqlite3.c` SHA-256 | `87497ab605bedd0dbee27a209c1eeff8c89b229b13f921a7efdbb81a13f779fd` |
| `sqlite3.h` SHA-256 | `4ff81af4849acabc76fc8349abb926814395072617ca18e08800abf734ab7612` |

The zip SHA3-256 above matches the value published by sqlite.org on
<https://www.sqlite.org/download.html> (both the human-readable note and the
machine-readable `PRODUCT,...` CSV line) at vendoring time.

## Re-verify the vendored files

    sha256sum lib/sqlite/sqlite3.c lib/sqlite/sqlite3.h   # must match the table above

## Updating

1. Download the new `sqlite-amalgamation-*.zip` from sqlite.org.
2. Confirm its SHA3-256 equals the value published on the download page:
   `openssl dgst -sha3-256 <zip>`.
3. Unzip, copy `sqlite3.c` + `sqlite3.h` here, and update this file's hashes.

## Build flags (lean, single-threaded use)

    -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION
    -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_OMIT_DEPRECATED
    -DSQLITE_DQS=0
