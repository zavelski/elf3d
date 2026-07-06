# glTF Compatibility Reference

Elf3D loads static glTF 2.0 scenes from `.gltf` and `.glb` files.

## Files and Resources

| Content | Supported input |
| --- | --- |
| Containers | JSON `.gltf` and binary `.glb` |
| Buffers | Local files, data URIs, and GLB buffer data |
| Images | PNG and JPEG from local files, data URIs, and buffer views |
| Scene data | Default scene, hierarchy, names, TRS, matrices, and perspective cameras |
| Geometry | Indexed or non-indexed triangles, triangle strips, and triangle fans |
| Attributes | Positions, normals, generated normals, UV0, UV1, and vertex color |
| Sparse data | Sparse vertex attributes |

Keep external buffers and images at the paths referenced by the `.gltf` file.
For portable sharing, use a self-contained `.glb` or copy the complete asset
directory.

## Materials

The viewer displays:

- base color factors, textures, and vertex color;
- metallic and roughness factors and textures;
- opaque, alpha-mask, and alpha-blend materials;
- emissive and occlusion values and textures;
- double-sided and unlit materials;
- texture-coordinate selection and `KHR_texture_transform`;
- emissive strength, IOR, specular factors, and supported quantized attributes.

## Diagnostics

A scene can open with compatibility diagnostics. Open **Model Information** to
review messages associated with nodes, geometry, materials, textures, or
external resources. A hard load error leaves the currently displayed scene
unchanged.

When reporting a model problem, prefer a small self-contained reproduction that
you are allowed to share.
