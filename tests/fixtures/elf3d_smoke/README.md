# Elf3D Embedded Smoke Model

`elf3d_smoke.gltf` is the distributable model for the default quick importer
and rendering path. Its buffer and 2-by-2 PNG image are data URIs, so loading
the model requires no adjacent binary or image resource.

## Provenance

The geometry, buffer packing, materials, and image were authored for Elf3D.
This package was prepared on 2026-07-02 from the earlier project-owned
`tests/fixtures/textured_pbr.gltf` validation data. It contains no
third-party model, texture, or generated asset input.

## License

Copyright (c) 2026 Serge Zavelski. The complete package is available under the
MIT License reproduced in `LICENSE.txt` (SPDX identifier: `MIT`).

## Expected Facts

- glTF 2.0 JSON container with one embedded 280-byte buffer and one embedded
  2-by-2 RGB PNG;
- one default scene, one model node, and one glTF mesh whose two indexed
  triangle-list primitives become two imported mesh assets, eight vertices,
  and four triangles;
- object-space bounds from `[-2, -1, 0]` to `[2, 1, 0]`;
- two material assets, two sampler descriptions, two texture assets, and one
  image asset;
- both materials use base-color textures, and one also uses a
  metallic-roughness texture;
- successful Elf3D import with no compatibility diagnostics.

These facts are the fixture contract. The always-on importer and hidden-context
render tests check the model without an external model path.
