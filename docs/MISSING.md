# MISSING — Pacotes e funções não implementadas (Protocolo 47)

> Gerado a partir de `docs/protocol-1.8.md`. Marcar `[x]` quando implementado + testado.

---

## Legenda
- **P0** = crítico para HUD/login limpo (cliente vanilla não mostra erro)
- **P1** = interação mundo (inventário, blocos, entidades visíveis)
- **P2** = paridade fina (scoreboard, títulos, mapas, partículas)
- **feito** = já implementado e testado (goldens OK)

---

## CLIENTBOUND (Server → Client)

### P0 — Críticos (HUD, login, chunks)

| ID | Pacote | Função C++ necessária | Struct proto | Status |
|----|--------|----------------------|--------------|--------|
| 0x06 | UpdateHealth | `Server::SendUpdateHealth(Player&, float hp, int food, float sat)` | `UpdateHealth` | ✅ |
| 0x09 | HeldItemChange | `Server::SendHeldItemChange(Player&, uint8_t slot)` | `HeldItemChange` | ✅ |
| 0x1F | SetExperience | `Server::SendSetExperience(Player&, float bar, int level, int total)` | `SetExperience` | ✅ |
| 0x26 | MapChunkBulk | `Server::SendMapChunkBulk(vector<Chunk*>, bool sky)` | `MapChunkBulk` | ✅ |
| 0x2F | SetSlot | **IMPLEMENTADO** (P1a) | `SetSlot` | ✅ |
| 0x30 | WindowItems | **IMPLEMENTADO** (P1a) | `WindowItems` | ✅ |
| 0x37 | Statistics | **IMPLEMENTADO** (P0 login) | `Statistics` | ✅ |
| 0x38 | PlayerListItem | **IMPLEMENTADO** (P0 login) | `PlayerListItem` | ✅ |
| 0x39 | PlayerAbilities | **IMPLEMENTADO** (P0 login) | `PlayerAbilities` | ✅ |
| 0x3F | PluginMessage | **IMPLEMENTADO** (P0 login MC\|Brand) | `PluginMessage` | ✅ |
| 0x41 | ServerDifficulty | **IMPLEMENTADO** (P0 login) | `ServerDifficulty` | ✅ |
| 0x44 | WorldBorder | **IMPLEMENTADO** (P0 login INIT) | `WorldBorder` | ✅ |

### P1 — Entidades, janelas, blocos

| ID | Pacote | Função C++ necessária | Struct proto | Status |
|----|--------|----------------------|--------------|--------|
| 0x04 | EntityEquipment | `Server::SendEntityEquipment(Entity&, int slot, Slot)` | `Equipment` | ✅ |
| 0x07 | Respawn | `Server::SendRespawn(Player&, int dim, uint8_t diff, uint8_t mode, string level)` | `Respawn` | ✅ |
| 0x0A | UseBed | `Server::SendUseBed(Entity&, int x, int y, int z)` | `UseBed` | ✅ |
| 0x0B | Animation | **IMPLEMENTADO** (P1c swing) | `Animation` | ✅ |
| 0x0C | SpawnPlayer | **IMPLEMENTADO** (P1c OnJoin) | `SpawnPlayer` | ✅ |
| 0x0D | CollectItem | **IMPLEMENTADO** (P1c pickup) | `CollectItem` | ✅ |
| 0x0E | SpawnObject | **IMPLEMENTADO** (P1c drops) | `SpawnObject` | ✅ |
| 0x0F | SpawnMob | **IMPLEMENTADO** (P1c passive spawn) | `SpawnMob` | ✅ |
| 0x10 | SpawnPainting | `Server::SendSpawnPainting(Entity&, string title, pos, uint8_t dir)` | `SpawnPainting` | ✅ |
| 0x11 | SpawnExpOrb | `Server::SendSpawnExpOrb(Entity&, int x,y,z, int count)` | `ExpOrb` | ✅ |
| 0x12 | EntityVelocity | **IMPLEMENTADO** (P1c knockback) | `Velocity` | ✅ |
| 0x13 | DestroyEntities | **IMPLEMENTADO** (P1c despawn) | `DestroyEntities` | ✅ |
| 0x14 | Entity (rel move 0) | `Server::SendEntityRelMove(Entity&, int8_t dx,dy,dz, bool ground)` | `EntityRelMove` | ✅ |
| 0x15 | EntityRelativeMove | **IMPLEMENTADO** (P1c BroadcastPlayerMove) | `EntityRelMove` | ✅ |
| 0x16 | EntityLook | **IMPLEMENTADO** (P1c BroadcastPlayerLook) | `EntityLook` | ✅ |
| 0x17 | EntityLookMove | **IMPLEMENTADO** (P1c BroadcastPlayerMoveLook) | `EntityRelMoveLook` | ✅ |
| 0x18 | EntityTeleport | **IMPLEMENTADO** (P1c BroadcastPlayerMove large delta) | `EntityTeleport` | ✅ |
| 0x19 | EntityHeadLook | **IMPLEMENTADO** (P1c SendPlayerHeadLook) | `HeadLook` | ✅ |
| 0x1A | EntityStatus | **IMPLEMENTADO** (P1c hurt status=2) | `EntityStatus` | ✅ |
| 0x1B | AttachEntity | `Server::SendAttachEntity(int vehicle, int rider, bool leash)` | `AttachEntity` | ✅ |
| 0x1C | EntityMetadata | **IMPLEMENTADO** (P1c SendEntityMetadata) | `Metadata` | ✅ |
| 0x1D | EntityEffect | `Server::SendEntityEffect(Entity&, uint8_t effect, uint8_t amp, int dur, bool hide)` | `EntityEffect` | ✅ |
| 0x1E | RemoveEntityEffect | `Server::SendRemoveEntityEffect(Entity&, uint8_t effect)` | `RemoveEntityEffect` | ✅ |
| 0x20 | EntityProperties | **IMPLEMENTADO** (P1c SendEntityTo props) | `Properties` | ✅ |
| 0x22 | MultiBlockChange | `Server::SendMultiBlockChange(int cx, int cz, vector<BlockChange>)` | `MultiBlockChange` | ✅ |
| 0x27 | Explosion | `Server::SendExplosion(float x,y,z, float str, vector<BlockOffset>)` | `Explosion` | ✅ |
| 0x2A | Particle | `Server::SendParticle(int id, bool longDist, float x,y,z, float ox,oy,oz, float speed, int count, data)` | `Particle` | ✅ |
| 0x2B | ChangeGameState | `Server::SendChangeGameState(uint8_t reason, float value)` | `ChangeGameState` | ✅ |
| 0x2C | SpawnGlobalEntity | `Server::SendSpawnGlobalEntity(int eid, uint8_t type, int x,y,z)` | `SpawnGlobalEntity` | ✅ |
| 0x2D | OpenWindow | `Server::SendOpenWindow(uint8_t id, string type, string title, uint8_t slots)` | `OpenWindow` | ✅ |
| 0x2E | CloseWindow | `Server::SendCloseWindow(uint8_t id)` | `CloseWindowPkt` | ✅ |
| 0x31 | WindowProperty | `Server::SendWindowProperty(uint8_t win, int16_t prop, int16_t val)` | `WindowProperty` | ✅ |
| 0x32 | ConfirmTransaction | **IMPLEMENTADO** (P1a) | `ConfirmTransaction` | ✅ |
| 0x33 | UpdateSign | `Server::SendUpdateSign(int x,y,z, array<string,4> lines)` | `UpdateSign` | ✅ |
| 0x35 | UpdateBlockEntity | `Server::SendUpdateBlockEntity(int x,y,z, uint8_t action, NBT)` | `UpdateBlockEntity` | ✅ |
| 0x36 | SignEditorOpen | `Server::SendSignEditorOpen(int x,y,z)` | `SignEditorOpen` | ✅ |
| 0x42 | CombatEvent | `Server::SendCombatEvent(int event, ...)` | `CombatEvent` | ✅ |

### P2 — Scoreboard, títulos, mapas, recursos — **TODOS IMPLEMENTADOS**

| ID | Pacote | Função C++ necessária | Struct proto | Status |
|----|--------|----------------------|--------------|--------|
| 0x34 | Maps | `Server::SendMapData(...)` | `MapData` | ✅ |
| 0x3A | TabComplete | `Server::SendTabComplete(vector<string>)` | `CbTabComplete` | ✅ |
| 0x3B | ScoreboardObjective | `Server::SendScoreboardObjective(...)` | `ScoreboardObjective` | ✅ |
| 0x3C | UpdateScore | `Server::SendUpdateScore(...)` | `UpdateScore` | ✅ |
| 0x3D | DisplayScoreboard | `Server::SendDisplayScoreboard(...)` | `DisplayScoreboard` | ✅ |
| 0x3E | Teams | `Server::SendTeams(...)` | `Teams` | ✅ |
| 0x43 | Camera | `Server::SendCamera(int eid)` | `Camera` | ✅ |
| 0x45 | Title | `Server::SendTitle(...)` | `Title` | ✅ |
| 0x47 | PlayerListHeaderFooter | `Server::SendPlayerListHeaderFooter(...)` | `PlayerListHeaderFooter` | ✅ |
| 0x48 | ResourcePackSend | `Server::SendResourcePackSend(...)` | `ResourcePackSend` | ✅ |

---

## SERVERBOUND (Client → Server)

### P1 — Movimento, inventário, entidades — **TODOS IMPLEMENTADOS**

| ID | Pacote | Handler C++ necessário | Struct proto | Status |
|----|--------|------------------------|--------------|--------|
| 0x02 | UseEntity | **IMPLEMENTADO** (P1c HandleUseEntity) | `UseEntity` | ✅ |
| 0x09 | HeldItemChange | `Server::HandleHeldItemChange(Player&, proto::SbHeldItem)` | `SbHeldItem` | ✅ |
| 0x0A | ArmAnimation | **IMPLEMENTADO** (P1a swing broadcast) | `Animation` | ✅ |
| 0x0B | EntityAction | `Server::HandleEntityAction(Player&, proto::EntityAction)` | `EntityAction` | ✅ |
| 0x0C | SteerVehicle | `Server::HandleSteerVehicle(Player&, proto::SteerVehicle)` | `SteerVehicle` | ✅ |
| 0x0D | CloseWindow | `Server::HandleCloseWindow(Player&, uint8_t window)` | `CloseWindowPkt` | ✅ |
| 0x0E | ClickWindow | **IMPLEMENTADO** (P1a ProcessClick) | `ClickWindow` | ✅ |
| 0x0F | ConfirmTransaction | **IMPLEMENTADO** (P1a eco) | `ConfirmTransaction` | ✅ |
| 0x10 | CreativeInventoryAction | `Server::HandleCreativeAction(Player&, proto::CreativeAction)` | `CreativeAction` | ✅ |
| 0x11 | EnchantItem | `Server::HandleEnchantItem(Player&, uint8_t win, uint8_t ench)` | `EnchantItem` | ✅ |
| 0x12 | UpdateSign | `Server::HandleUpdateSign(Player&, int x,y,z, array<string,4>)` | `SbUpdateSign` | ✅ |
| 0x13 | Abilities | `Server::HandleAbilities(Player&, proto::PlayerAbilities)` | `SbPlayerAbilities` | ✅ |
| 0x15 | ClientSettings | `Server::HandleClientSettings(Player&, proto::ClientSettings)` | `ClientSettings` | ✅ |
| 0x16 | ClientStatus | `Server::HandleClientStatus(Player&, proto::ClientStatus)` | `ClientStatus` | ✅ |

### P2 — Recursos — **TODOS IMPLEMENTADOS**

| ID | Pacote | Handler C++ necessário | Struct proto | Status |
|----|--------|------------------------|--------------|--------|
| 0x14 | TabComplete | `Server::HandleTabComplete(Player&, string txt, ...)` | `SbTabComplete` | ✅ |
| 0x19 | ResourcePackStatus | `Server::HandleResourcePackStatus(Player&, string hash, int result)` | `ResourcePackStatus` | ✅ |

---

## Estruturas proto — **TODAS IMPLEMENTADAS (P0+P1+P2)**

### Clientbound
- ✅ `Respawn` (encode/decode)
- ✅ `UseBed` (encode/decode)
- ✅ `SpawnPainting` (encode/decode)
- ✅ `AttachEntity` (encode/decode)
- ✅ `EntityEffect` (encode/decode)
- ✅ `RemoveEntityEffect` (encode/decode)
- ✅ `MultiBlockChange` (encode/decode)
- ✅ `Explosion` (encode/decode)
- ✅ `Particle` (encode/decode)
- ✅ `ChangeGameState` (encode/decode)
- ✅ `SpawnGlobalEntity` (encode/decode)
- ✅ `UpdateSign` (encode/decode)
- ✅ `UpdateBlockEntity` (encode/decode)
- ✅ `SignEditorOpen` (encode/decode)
- ✅ `CombatEvent` (encode/decode)
- ✅ `MapData` (encode/decode) — P2
- ✅ `CbTabComplete` (encode/decode) — P2
- ✅ `ScoreboardObjective` (encode/decode) — P2
- ✅ `UpdateScore` (encode/decode) — P2
- ✅ `DisplayScoreboard` (encode/decode) — P2
- ✅ `Teams` (encode/decode) — P2
- ✅ `Camera` (encode/decode) — P2
- ✅ `Title` (encode/decode) — P2
- ✅ `PlayerListHeaderFooter` (encode/decode) — P2
- ✅ `ResourcePackSend` (encode/decode) — P2

### Serverbound
- ✅ `EntityAction` (decode)
- ✅ `SteerVehicle` (decode)
- ✅ `EnchantItem` (decode)
- ✅ `SbUpdateSign` (decode)
- ✅ `SbPlayerAbilities` (decode)
- ✅ `ClientSettings` (decode)
- ✅ `ClientStatus` (decode)
- ✅ `SbTabComplete` (decode) — P2
- ✅ `ResourcePackStatus` (decode) — P2

---

## Funções Server.cpp — **TODAS IMPLEMENTADAS (P0+P1+P2)**

### Broadcast / Envio
- ✅ `SendUpdateHealth(Player&, float, int, float)`
- ✅ `SendHeldItemChange(Player&, uint8_t)`
- ✅ `SendSetExperience(Player&, float, int, int)`
- ✅ `SendRespawn(Player&, int, uint8_t, uint8_t, string)`
- ✅ `SendUseBed(Entity&, int, int, int)`
- ✅ `SendSpawnPainting(Entity&, string, pos, uint8_t)`
- ✅ `SendSpawnExpOrb(Entity&, int, int, int, int)`
- ✅ `SendEntityRelMove(Entity&, int8_t, int8_t, int8_t, bool)`
- ✅ `SendAttachEntity(int, int, bool)`
- ✅ `SendEntityEffect(Entity&, uint8_t, uint8_t, int, bool)`
- ✅ `SendRemoveEntityEffect(Entity&, uint8_t)`
- ✅ `SendMultiBlockChange(int, int, vector<BlockChange>)`
- ✅ `SendExplosion(float, float, float, float, vector<BlockOffset>)`
- ✅ `SendParticle(int, bool, float, float, float, float, float, float, float, int, data)`
- ✅ `SendChangeGameState(uint8_t, float)`
- ✅ `SendSpawnGlobalEntity(int, uint8_t, int, int, int)`
- ✅ `SendOpenWindow(uint8_t, string, string, uint8_t)`
- ✅ `SendCloseWindow(uint8_t)`
- ✅ `SendWindowProperty(uint8_t, int16_t, int16_t)`
- ✅ `SendUpdateSign(int, int, int, array<string,4>)`
- ✅ `SendUpdateBlockEntity(int, int, int, uint8_t, NBT)`
- ✅ `SendSignEditorOpen(int, int, int)`
- ✅ `SendCombatEvent(int, ...)`
- ✅ `SendMapData(...)` — P2
- ✅ `SendTabComplete(...)` — P2
- ✅ `SendScoreboardObjective(...)` — P2
- ✅ `SendUpdateScore(...)` — P2
- ✅ `SendDisplayScoreboard(...)` — P2
- ✅ `SendTeams(...)` — P2
- ✅ `SendCamera(...)` — P2
- ✅ `SendTitle(...)` — P2
- ✅ `SendPlayerListHeaderFooter(...)` — P2
- ✅ `SendResourcePackSend(...)` — P2

### Handlers SB
- ✅ `HandleHeldItemChange(Player&, SbHeldItem)` — troca slot hotbar
- ✅ `HandleEntityAction(Player&, EntityAction)` — sneak/sprint/sleep
- ✅ `HandleSteerVehicle(Player&, SteerVehicle)` — boat/minecart
- ✅ `HandleCloseWindow(Player&, uint8_t)` — fecha janela
- ✅ `HandleCreativeAction(Player&, CreativeAction)` — creative mode
- ✅ `HandleEnchantItem(Player&, uint8_t, uint8_t)` — mesa encantamento
- ✅ `HandleUpdateSign(Player&, int, int, int, array<string,4>)` — edita placa
- ✅ `HandleAbilities(Player&, PlayerAbilities)` — creative fly
- ✅ `HandleClientSettings(Player&, ClientSettings)` — locale, view dist, etc.
- ✅ `HandleClientStatus(Player&, ClientStatus)` — **respawn (action=0)**, stats
- ✅ `HandleTabComplete(Player&, SbTabComplete)` — P2
- ✅ `HandleResourcePackStatus(Player&, ResourcePackStatus)` — P2

---

## Fase 2: Worldgen — **OVERWORLD IMPLEMENTADO** ✅

### Java-compatible RNG
- ✅ `JavaRandom` class (java.util.Random compatible: nextLong, nextInt, nextDouble, nextFloat, nextBoolean)
- ✅ Seed mixing: `(seed ^ 0x5DEECE66DL) & 0xFFFFFFFFFFFFL`

### Biome Generation
- ✅ 40 biomes implemented (vanilla 1.8 IDs 0-39)
- ✅ Temperature/humidity noise-based biome selection
- ✅ Biome settings: temperature, humidity, height variation, top/filler blocks, colors

### Overworld Terrain
- ✅ Heightmap generation with multi-octave noise (continental, mountains, hills)
- ✅ Sea level at Y=63
- ✅ Bedrock at Y=0
- ✅ Stone/dirt/grass layers
- ✅ Biome-specific top blocks (sand, mycelium, etc.)
- ✅ Cave carving (noise-based)
- ✅ Ravine generation (rare)
- ✅ Ore veins: coal, iron, gold, diamond, redstone, lapis
- ✅ Water/lava lakes

### Structure Generation (simplified)
- ✅ Villages (well at chunk center)
- ✅ Temples (sandstone pyramid with chest)
- ✅ Mineshafts (3x3 corridors with wood supports)
- ✅ Strongholds (stone brick room with end portal frame)

### Chunk/Region Integration
- ✅ `WorldGenerator` class with `GenerateChunk()`, `GenerateTerrain()`, `GenerateBiomes()`, `GenerateStructures()`
- ✅ `worldgen::GenerateChunk(cx, cz, seed, chunk)` for chunk jobs
- ✅ `ChunkJobMain` falls back to worldgen when region file missing
- ✅ Uses existing `Chunk` structure (sections, heightmap, biomes)

### Nether / End
- 🔲 Nether generation (dimension -1)
- 🔲 End generation (dimension 1)

---

## Fase 1 (Protocolo) — **100% COMPLETO** ✅

P0 + P1 + P2 todos implementados com:
- [x] Struct em `protocol.h`
- [x] Encode/Decode em `protocol.cpp` (testado contra goldens se existirem)
- [x] Handler SB em `server.cpp` (se aplicável)
- [x] Função `SendX` / `HandleX` em `server.cpp` / `server.h`
- [x] Handler cases em `Server::HandlePlay`
- [x] Teste em `test_server18.cpp` (unitário, sem socket)
- [x] Build Release + ASan limpo
- [x] Cliente vanilla 1.8 conecta, vê HUD correto, joga survival básico

---

## Próximos passos

### Fase 2 completo: Nether + End
- [ ] Nether terrain (netherrack, soul sand, glowstone, lava seas, fortresses)
- [ ] End terrain (end stone, obsidian pillars, end cities, dragon)

### Fase 3: Física autoritativa
- [ ] AABB collision
- [ ] Gravidade, água/lava physics
- [ ] Anti-fly/anti-cheat
- [ ] Explosions (TNT, creepers)

### Fase 4: ~200 blocos
- [ ] Redstone, pistões, fornalha, portas, trilhos
- [ ] TNT, cultivos, camas, portais
- [ ] Tile entities (baús, fornalhas, hoppers, etc.)

### Fase 5: ~30 mobs + IA
- [ ] Pathfinding (A*)
- [ ] Hostile mobs (creeper, skeleton, zombie, spider, enderman, dragon)
- [ ] Passive mobs breeding
- [ ] Projectiles (arrows, fireballs, snowballs)
- [ ] Vehicles (boats, minecarts)

### Fase 6: Gameplay
- [ ] Fome, sprint, crouch
- [ ] Combate 1.8 (cooldown, knockback, sweep)
- [ ] Encantamentos, poções, beacons
- [ ] Sono, vilas, conquistas, gamerules

### Fase 7: Admin
- [ ] ~30 comandos (tp, give, gamemode, ban, op, etc.)
- [ ] RCON, playerdata NBT, persistência total