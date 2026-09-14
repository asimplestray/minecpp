# v1_8 — base legacy

Implementação completa da 1.8.x (protocolo 47). Todas as outras versões legacy
reutilizam este código via `version_api_t`, sem herança C++.

Chunk 1.8: 16x256x16, 16 sections 16³, `Blocks` + `Add` + `Data` + `BlockLight`
+ `SkyLight`, `HeightMap`, `Biomes`. NBT big-endian, region `.mca` zlib.
