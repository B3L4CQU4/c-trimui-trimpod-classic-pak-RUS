# Trimpod font sources

These source fonts are vendored so the Rockbox `.fnt` files can be regenerated
without downloading anything during a release build.

| File | Upstream | Version | SHA-256 |
|---|---|---|---|
| `ChicagoFLF.ttf` | https://fontlibrary.org/en/font/chicagoflf | 2.0 | `b442111f37639e27572d9df0c5190e7480e6a7b01ec768aea47a154efab8d50d` |
| `Mulmaru.ttf` | https://github.com/mushsooni/mulmaru/releases/tag/v1.0 | 1.0 | `02545e10374c0797be32df8670e18663c6ab73eea6966bb98f4ffd0283138810` |

ChicagoFLF is public domain. Mulmaru is distributed under SIL OFL 1.1 and
declares `Mulmaru` as a Reserved Font Name. Generated derivative Rockbox fonts
therefore use the neutral name `TrimpodRus` (Russian UI using Mulmaru for all
available glyphs). The derivative does not claim the Reserved Font Name. The
unmodified upstream `Mulmaru.ttf` retains its original name. English mode loads
the original checked-in ChicagoFLF `.fnt` files directly.
