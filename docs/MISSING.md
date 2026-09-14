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
| 0x06 | UpdateHealth | `Server::SendUpdateHealth(Player&, float hp, int food, float sat)` | `UpdateHealth` | 🔲 |
| 0x09 | HeldItemChange | `Server::SendHeldItemChange(Player&, uint8_t slot)` | `HeldItemChange` | 🔲 |
| 0x1F | SetExperience | `Server::SendSetExperience(Player&, float bar, int level, int total)` | `SetExperience` | 🔲 |
| 0x26 | MapChunkBulk | `Server::SendMapChunkBulk(vector<Chunk*>, bool sky)` | `MapChunkBulk` | 🔲 |
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
| 0x04 | EntityEquipment | `Server::SendEntityEquipment(Entity&, int slot, Slot)` | `Equipment` | 🔲 |
| 0x07 | Respawn | `Server::SendRespawn(Player&, int dim, uint8_t diff, uint8_t mode, string level)` | `Respawn` | 🔲 |
| 0x0A | UseBed | `Server::SendUseBed(Entity&, int x, int y, int z)` | `UseBed` | 🔲 |
| 0x0B | Animation | **IMPLEMENTADO** (P1c swing) | `Animation` | ✅ |
| 0x0C | SpawnPlayer | **IMPLEMENTADO** (P1c OnJoin) | `SpawnPlayer` | ✅ |
| 0x0D | CollectItem | **IMPLEMENTADO** (P1c pickup) | `CollectItem` | ✅ |
| 0x0E | SpawnObject | **IMPLEMENTADO** (P1c drops) | `SpawnObject` | ✅ |
| 0x0F | SpawnMob | **IMPLEMENTADO** (P1c passive spawn) | `SpawnMob` | ✅ |
| 0x10 | SpawnPainting | `Server::SendSpawnPainting(Entity&, string title, pos, uint8_t dir)` | `SpawnPainting` | 🔲 |
| 0x11 | SpawnExpOrb | `Server::SendSpawnExpOrb(Entity&, int x,y,z, int count)` | `ExpOrb` | 🔲 |
| 0x12 | EntityVelocity | **IMPLEMENTADO** (P1c knockback) | `Velocity` | ✅ |
| 0x13 | DestroyEntities | **IMPLEMENTADO** (P1c despawn) | `DestroyEntities` | ✅ |
| 0x14 | Entity (rel move 0) | `Server::SendEntityRelMove(Entity&, int8_t dx,dy,dz, bool ground)` | `EntityRelMove` | 🔲 |
| 0x15 | EntityRelativeMove | **IMPLEMENTADO** (P1c BroadcastPlayerMove) | `EntityRelMove` | ✅ |
| 0x16 | EntityLook | **IMPLEMENTADO** (P1c BroadcastPlayerLook) | `EntityLook` | ✅ |
| 0x17 | EntityLookMove | **IMPLEMENTADO** (P1c BroadcastPlayerMoveLook) | `EntityRelMoveLook` | ✅ |
| 0x18 | EntityTeleport | **IMPLEMENTADO** (P1c BroadcastPlayerMove large delta) | `EntityTeleport` | ✅ |
| 0x19 | EntityHeadLook | **IMPLEMENTADO** (P1c SendPlayerHeadLook) | `HeadLook` | ✅ |
| 0x1A | EntityStatus | **IMPLEMENTADO** (P1c hurt status=2) | `EntityStatus` | ✅ |
| 0x1B | AttachEntity | `Server::SendAttachEntity(int vehicle, int rider, bool leash)` | `AttachEntity` | 🔲 |
| 0x1C | EntityMetadata | **IMPLEMENTADO** (P1c SendEntityMetadata) | `Metadata` | ✅ |
| 0x1D | EntityEffect | `Server::SendEntityEffect(Entity&, uint8_t effect, uint8_t amp, int dur, bool hide)` | `EntityEffect` | 🔲 |
| 0x1E | RemoveEntityEffect | `Server::SendRemoveEntityEffect(Entity&, uint8_t effect)` | `RemoveEntityEffect` | 🔲 |
| 0x20 | EntityProperties | **IMPLEMENTADO** (P1c SendEntityTo props) | `Properties` | ✅ |
| 0x22 | MultiBlockChange | `Server::SendMultiBlockChange(int cx, int cz, vector<BlockChange>)` | `MultiBlockChange` | 🔲 |
| 0x27 | Explosion | `Server::SendExplosion(float x,y,z, float str, vector<BlockOffset>)` | `Explosion` | 🔲 |
| 0x2A | Particle | `Server::SendParticle(int id, bool longDist, float x,y,z, float ox,oy,oz, float speed, int count, data)` | `Particle` | 🔲 |
| 0x2B | ChangeGameState | `Server::SendChangeGameState(uint8_t reason, float value)` | `ChangeGameState` | 🔲 |
| 0x2C | SpawnGlobalEntity | `Server::SendSpawnGlobalEntity(int eid, uint8_t type, int x,y,z)` | `SpawnGlobalEntity` | 🔲 |
| 0x2D | OpenWindow | `Server::SendOpenWindow(uint8_t id, string type, string title, uint8_t slots)` | `OpenWindow` | 🔲 |
| 0x2E | CloseWindow | `Server::SendCloseWindow(uint8_t id)` | `CloseWindowPkt` | 🔲 |
| 0x31 | WindowProperty | `Server::SendWindowProperty(uint8_t win, int16_t prop, int16_t val)` | `WindowProperty` | 🔲 |
| 0x32 | ConfirmTransaction | **IMPLEMENTADO** (P1a) | `ConfirmTransaction` | ✅ |
| 0x33 | UpdateSign | `Server::SendUpdateSign(int x,y,z, array<string,4> lines)` | `UpdateSign` | 🔲 |
| 0x35 | UpdateBlockEntity | `Server::SendUpdateBlockEntity(int x,y,z, uint8_t action, NBT)` | `UpdateBlockEntity` | 🔲 |
| 0x36 | SignEditorOpen | `Server::SendSignEditorOpen(int x,y,z)` | `SignEditorOpen` | 🔲 |
| 0x42 | CombatEvent | `Server::SendCombatEvent(int event, ...)` | `CombatEvent` | 🔲 |

### P2 — Scoreboard, títulos, mapas, recursos

| ID | Pacote | Função C++ necessária | Struct proto | Status |
|----|--------|----------------------|--------------|--------|
| 0x34 | Maps | `Server::SendMapData(int mapid, byte scale, icons, cols, rows, x,z, data[])` | `Maps` | 🔲 |
| 0x3A | TabComplete | `Server::SendTabComplete(vector<string>)` | `TabComplete` | 🔲 |
| 0x3B | ScoreboardObjective | `Server::SendScoreboardObjective(string name, string val, uint8_t action)` | `ScoreboardObjective` | 🔲 |
| 0x3C | UpdateScore | `Server::SendUpdateScore(string name, uint8_t action, string obj, int val)` | `UpdateScore` | 🔲 |
| 0x3D | DisplayScoreboard | `Server::SendDisplayScoreboard(uint8_t pos, string name)` | `DisplayScoreboard` | 🔲 |
| 0x3E | Teams | `Server::SendTeams(string name, uint8_t mode, ...)` | `Teams` | 🔲 |
| 0x43 | Camera | `Server::SendCamera(int eid)` | `Camera` | 🔲 |
| 0x45 | Title | `Server::SendTitle(int action, ...)` | `Title` | 🔲 |
| 0x47 | PlayerListHeaderFooter | `Server::SendPlayerListHeaderFooter(string header, string footer)` | `PlayerListHeaderFooter` | 🔲 |
| 0x48 | ResourcePackSend | `Server::SendResourcePack(string url, string hash)` | `ResourcePackSend` | 🔲 |

---

## SERVERBOUND (Client → Server)

### P1 — Movimento, inventário, entidades

| ID | Pacote | Handler C++ necessário | Struct proto | Status |
|----|--------|------------------------|--------------|--------|
| 0x02 | UseEntity | **IMPLEMENTADO** (P1c HandleUseEntity) | `UseEntity` | ✅ |
| 0x09 | HeldItemChange | `Server::HandleHeldItemChange(Player&, proto::SbHeldItem)` | `SbHeldItem` | 🔲 |
| 0x0A | ArmAnimation | **IMPLEMENTADO** (P1a swing broadcast) | `Animation` | ✅ |
| 0x0B | EntityAction | `Server::HandleEntityAction(Player&, proto::EntityAction)` | `EntityAction` | 🔲 |
| 0x0C | SteerVehicle | `Server::HandleSteerVehicle(Player&, proto::SteerVehicle)` | `SteerVehicle` | 🔲 |
| 0x0D | CloseWindow | `Server::HandleCloseWindow(Player&, uint8_t window)` | `CloseWindowPkt` | 🔲 |
| 0x0E | ClickWindow | **IMPLEMENTADO** (P1a ProcessClick) | `ClickWindow` | ✅ |
| 0x0F | ConfirmTransaction | **IMPLEMENTADO** (P1a eco) | `ConfirmTransaction` | ✅ |
| 0x10 | CreativeInventoryAction | `Server::HandleCreativeAction(Player&, proto::CreativeAction)` | `CreativeAction` | 🔲 |
| 0x11 | EnchantItem | `Server::HandleEnchantItem(Player&, uint8_t win, uint8_t ench)` | `EnchantItem` | 🔲 |
| 0x12 | UpdateSign | `Server::HandleUpdateSign(Player&, int x,y,z, array<string,4>)` | `UpdateSign` | 🔲 |
| 0x13 | Abilities | `Server::HandleAbilities(Player&, proto::PlayerAbilities)` | `PlayerAbilities` | 🔲 |
| 0x15 | ClientSettings | `Server::HandleClientSettings(Player&, proto::ClientSettings)` | `ClientSettings` | 🔲 |
| 0x16 | ClientStatus | `Server::HandleClientStatus(Player&, proto::ClientStatus)` | `ClientStatus` | 🔲 |

### P2 — Recursos

| ID | Pacote | Handler C++ necessário | Struct proto | Status |
|----|--------|------------------------|--------------|--------|
| 0x14 | TabComplete | `Server::HandleTabComplete(Player&, string txt, ...)` | `TabComplete` | 🔲 |
| 0x19 | ResourcePackStatus | `Server::HandleResourcePackStatus(Player&, string hash, int result)` | `ResourcePackStatus` | 🔲 |

---

## Estruturas proto faltando em `protocol.h`/`protocol.cpp`

### Clientbound
- [ ] `Respawn` (encode/decode)
- [ ] `UseBed` (encode/decode)
- [ ] `SpawnPainting` (encode/decode)
- [ ] `SpawnExpOrb` (encode/decode)
- [ ] `AttachEntity` (encode/decode)
- [ ] `EntityEffect` (encode/decode)
- [ ] `RemoveEntityEffect` (encode/decode)
- [ ] `MultiBlockChange` (encode/decode)
- [ ] `Explosion` (encode/decode)
- [ ] `Particle` (encode/decode)
- [ ] `ChangeGameState` (encode/decode)
- [ ] `SpawnGlobalEntity` (encode/decode)
- [ ] `OpenWindow` (encode/decode)
- [ ] `CloseWindowPkt` CB (encode/decode)
- [ ] `WindowProperty` (encode/decode)
- [ ] `UpdateSign` (encode/decode)
- [ ] `UpdateBlockEntity` (encode/decode)
- [ ] `SignEditorOpen` (encode/decode)
- [ ] `CombatEvent` (encode/decode)
- [ ] `Maps` (encode/decode)
- [ ] `TabComplete` (encode/decode)
- [ ] `ScoreboardObjective` (encode/decode)
- [ ] `UpdateScore` (encode/decode)
- [ ] `DisplayScoreboard` (encode/decode)
- [ ] `Teams` (encode/decode)
- [ ] `Camera` (encode/decode)
- [ ] `Title` (encode/decode)
- [ ] `PlayerListHeaderFooter` (encode/decode)
- [ ] `ResourcePackSend` (encode/decode)

### Serverbound
- [ ] `EntityAction` (decode)
- [ ] `SteerVehicle` (decode)
- [ ] `CloseWindowPkt` SB (decode)
- [ ] `CreativeInventoryAction` (decode)
- [ ] `EnchantItem` (decode)
- [ ] `UpdateSign` SB (decode)
- [ ] `PlayerAbilities` SB (decode)
- [ ] `ClientSettings` (decode)
- [ ] `ClientStatus` (decode)
- [ ] `TabComplete` SB (decode)
- [ ] `ResourcePackStatus` (decode)

---

## Funções Server.cpp faltando

### Broadcast / Envio
- [ ] `SendUpdateHealth(Player&, float, int, float)`
- [ ] `SendHeldItemChange(Player&, uint8_t)`
- [ ] `SendSetExperience(Player&, float, int, int)`
- [ ] `SendMapChunkBulk(vector<Chunk*>, bool)`
- [ ] `SendEntityEquipment(Entity&, int, Slot)`
- [ ] `SendRespawn(Player&, int, uint8_t, uint8_t, string)`
- [ ] `SendUseBed(Entity&, int, int, int)`
- [ ] `SendSpawnPainting(Entity&, string, pos, uint8_t)`
- [ ] `SendSpawnExpOrb(Entity&, int, int, int, int)`
- [ ] `SendEntityRelMove(Entity&, int8_t, int8_t, int8_t, bool)`
- [ ] `SendAttachEntity(int, int, bool)`
- [ ] `SendEntityEffect(Entity&, uint8_t, uint8_t, int, bool)`
- [ ] `SendRemoveEntityEffect(Entity&, uint8_t)`
- [ ] `SendMultiBlockChange(int, int, vector<BlockChange>)`
- [ ] `SendExplosion(float, float, float, float, vector<BlockOffset>)`
- [ ] `SendParticle(int, bool, float, float, float, float, float, float, float, int, data)`
- [ ] `SendChangeGameState(uint8_t, float)`
- [ ] `SendSpawnGlobalEntity(int, uint8_t, int, int, int)`
- [ ] `SendOpenWindow(uint8_t, string, string, uint8_t)`
- [ ] `SendCloseWindow(uint8_t)`
- [ ] `SendWindowProperty(uint8_t, int16_t, int16_t)`
- [ ] `SendUpdateSign(int, int, int, array<string,4>)`
- [ ] `SendUpdateBlockEntity(int, int, int, uint8_t, NBT)`
- [ ] `SendSignEditorOpen(int, int, int)`
- [ ] `SendCombatEvent(int, ...)`
- [ ] `SendMapData(int, byte, icons, cols, rows, int, int, data[])`
- [ ] `SendTabComplete(vector<string>)`
- [ ] `SendScoreboardObjective(string, string, uint8_t)`
- [ ] `SendUpdateScore(string, uint8_t, string, int)`
- [ ] `SendDisplayScoreboard(uint8_t, string)`
- [ ] `SendTeams(string, uint8_t, ...)`
- [ ] `SendCamera(int)`
- [ ] `SendTitle(int, ...)`
- [ ] `SendPlayerListHeaderFooter(string, string)`
- [ ] `SendResourcePack(string, string)`

### Handlers SB
- [ ] `HandleHeldItemChange(Player&, SbHeldItem)` — troca slot hotbar
- [ ] `HandleEntityAction(Player&, EntityAction)` — sneak/sprint/sleep
- [ ] `HandleSteerVehicle(Player&, SteerVehicle)` — boat/minecart
- [ ] `HandleCloseWindow(Player&, uint8_t)` — fecha janela
- [ ] `HandleCreativeAction(Player&, CreativeAction)` — creative mode
- [ ] `HandleEnchantItem(Player&, uint8_t, uint8_t)` — mesa encantamento
- [ ] `HandleUpdateSign(Player&, int, int, int, array<string,4>)` — edita placa
- [ ] `HandleAbilities(Player&, PlayerAbilities)` — creative fly
- [ ] `HandleClientSettings(Player&, ClientSettings)` — locale, view dist, etc.
- [ ] `HandleClientStatus(Player&, ClientStatus)` — **respawn (action=0)**, stats

### Lógica de jogo (Fases 2-7)
- [ ] **Fase 2**: Worldgen (Overworld/Nether/End, seed Java, biomas, estruturas)
- [ ] **Fase 3**: Física autoritativa (AABB, gravidade, água/lava, explosões, anti-fly)
- [ ] **Fase 4**: ~200 blocos (redstone, pistões, fornalha, portas, trilhos, TNT, cultivos, etc.)
- [ ] **Fase 5**: ~30 mobs + IA (pathfinding, ataque, creeper, enderman, Dragon, projéteis, veículos)
- [ ] **Fase 6**: Gameplay (fome, combate 1.8, encantos, poções, sono, vilas, conquistas, gamerules)
- [ ] **Fase 7**: Admin (~30 comandos, ops/bans, RCON, playerdata NBT, persistência total)

---

## Ordem sugerida de implementação (Fase 1 P2)

1. **UpdateHealth (0x06)** + **HeldItemChange SB (0x09)** — HUD + hotbar
2. **Respawn (0x07)** + **ClientStatus SB (0x16 action=0)** — morte/respawn
3. **EntityAction SB (0x0B)** — sneak/sprint/sleep
4. **EntityEffect (0x1D/0x1E)** — poções
5. **ChangeGameState (0x2B)** — gamemode, demo
6. **SpawnExpOrb (0x11)** + **SetExperience (0x1F)** — XP
7. **OpenWindow/CloseWindow/WindowProperty (0x2D/0x2E/0x31)** — forno, baú, etc.
8. **MultiBlockChange (0x22)** — otimização blocos
8. **Explosion (0x27)** — TNT/creeper
9. **Particle (0x2A)** — efeitos visuais
10. **UpdateSign/UpdateBlockEntity (0x33/0x35)** — placas, tile entities
11. **Scoreboard/Title/Maps** (P2) — paridade fina

---

## Critérios de "feito"

- [ ] Struct em `protocol.h`
- [ ] Encode/Decode em `protocol.cpp` (testado contra goldens se existirem)
- [ ] Handler SB em `server.cpp` (se aplicável)
- [ ] Função `SendX` / `HandleX` em `server.cpp` / `server.h`
- [ ] Teste em `test_server18.cpp` (unitário, sem socket)
- [ ] Build Release + ASan limpo
- [ ] Cliente vanilla 1.8 conecta, vê HUD correto, joga survival básico