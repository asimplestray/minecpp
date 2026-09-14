# Protocolo 1.8 (47) — mapa de pacotes play

> IDs verificados contra goldens em `test-data/vanilla-1.8-flat/net/goldens/`
> (capturados via socket do `server.jar`; ordem de chegada em `_order.txt`).
> Convenções: `v` = VarInt, `s` = String, `u8/u16/i32/i64/f32/f64` BE,
> `pos` = u64 26/12/26, `bool` = u8. `*` = golden decodificado nesta sessão.

## Ordem do login vanilla (observada, espelhar no OnJoin)

`LoginSuccess → JoinGame → Brand → Difficulty → SpawnPos → Abilities →
HeldItem → Statistics → PlayerList ×2 → PosLook → Border → TimeUpdate →
WindowItems → SetSlot → MapChunkBulk ×N (10 chunks cada)`

## Clientbound (server → client)

| id | Nome | Formato | Pri | Golden |
|----|------|---------|-----|--------|
| 0x00 | KeepAlive | `v id` | feito | keepalive.bin |
| 0x01 | JoinGame | `i32 eid, u8 mode, i8 dim, u8 diff, u8 max, s level, bool reduced` | feito | joingame.bin |
| 0x02 | Chat | `s json` | feito | — |
| 0x03 | TimeUpdate | `i64 age, i64 time` | feito | (burst) |
| 0x04 | EntityEquipment | `v eid, v slot, slotdata` | P1 | — |
| 0x05 | SpawnPosition | `pos` | feito | spawnpos.bin |
| 0x06 | UpdateHealth | `f32 hp, v food, f32 sat` | **P0** | — |
| 0x07 | Respawn | `i32 dim, u8 diff, u8 mode, s level` | P1 | — |
| 0x08 | PlayerPositionLook | `f64×3, f32×2, u8 flags` | feito | poslook.bin |
| 0x09 | HeldItemChange | `u8 slot` | **P0** | cb_09_1.bin* |
| 0x0A | UseBed | `v eid, pos` | P1 | — |
| 0x0B | Animation | `v eid, u8 anim` | P1 | — |
| 0x0C | SpawnPlayer | `v eid, uuid16, i32 x,y,z (fixo /32), u8 yaw,pitch, v held, metadata` | P1 | — |
| 0x0D | CollectItem | `v collected, v collector` | P1 | — |
| 0x0E | SpawnObject | `v eid, u8 type, i32 x,y,z, u8 pitch,yaw, i32 data + vel?` | P1 | — |
| 0x0F | SpawnMob | `v eid, u8 type, i32 x,y,z, u8 yaw,pitch,head, vel?, metadata` | P1 | cb_0f_*.bin |
| 0x10 | SpawnPainting | `v eid, s title, pos, u8 dir` | P1 | — |
| 0x11 | SpawnExpOrb | `v eid, i32 x,y,z, v count` | P1 | — |
| 0x12 | EntityVelocity | `v eid, i16 x,y,z` | P1 | cb_12_*.bin |
| 0x13 | DestroyEntities | `v count, v eid×n` | P1 | — |
| 0x14 | Entity (move relativo 0) | `v eid` | P1 | — |
| 0x15 | EntityRelativeMove | `v eid, i8 dx,dy,dz, bool ground` | P1 | cb_15_*.bin |
| 0x16 | EntityLook | `v eid, u8 yaw,pitch, bool ground` | P1 | cb_16_*.bin |
| 0x17 | EntityLookMove | `v eid, i8 dx,dy,dz, u8 yaw,pitch, bool ground` | P1 | cb_17_*.bin |
| 0x18 | EntityTeleport | `v eid, i32 x,y,z, u8 yaw,pitch, bool ground` | P1 | cb_18_*.bin |
| 0x19 | EntityHeadLook | `v eid, u8 head` | P1 | cb_19_*.bin |
| 0x1A | EntityStatus | `i32 eid(!), u8 status` | P1 | — |
| 0x1B | AttachEntity | `i32 vehicle, i32 rider, bool leash` | P1 | — |
| 0x1C | EntityMetadata | `v eid, metadata blob (termina 0x7F)` | P1 | cb_1c_*.bin |
| 0x1D | EntityEffect | `v eid, u8 effect, u8 amp, v dur, bool hide` | P1 | — |
| 0x1E | RemoveEntityEffect | `v eid, u8 effect` | P1 | — |
| 0x1F | SetExperience | `f32 bar, v level, v total` | **P0** | — |
| 0x20 | EntityProperties | `v eid, i32 count, (s key, f64 val, v mods)×n` | P1 | cb_20_*.bin |
| 0x21 | ChunkData | `i32 x,z, bool cont, u16 mask, v len, data` (blocos u16LE!) | feito | chunkdata.bin |
| 0x22 | MultiBlockChange | `i32 cx,cz, v count, (u16 packed)×n` | P1 | — |
| 0x23 | BlockChange | `pos, v id<<4\|meta` | feito | cb_23_*.bin + ao vivo |
| 0x24 | BlockAction | `pos, u8 b1, u8 b2, v block` | feito | — |
| 0x25 | BlockBreakAnim | `v eid, pos, u8 stage` | feito | — |
| 0x26 | MapChunkBulk | `bool sky, v count, (i32 x,z, u16 mask)×n, data` | **P0** | cb_26_*.bin* |
| 0x27 | Explosion | `f32 x,y,z, f32 str, v count, rec×n, vel` | P1 | — |
| 0x28 | Effect | `i32 id, pos, i32 data, bool norel` | feito | — |
| 0x29 | SoundEffect | `s name, i32 x,y,z (×8), f32 vol,pitch` | feito | goldens ao vivo |
| 0x2A | Particle | `i32 id, bool dist, f32 x,y,z, f32 ox,oy,oz, f32 speed, i32 n, [data]` | P1 | — |
| 0x2B | ChangeGameState | `u8 reason, f32 value` | P1 | — |
| 0x2C | SpawnGlobalEntity | `v eid, u8 type, i32 x,y,z` | P1 | — |
| 0x2D | OpenWindow | `u8 id, s type, s title, u8 slots` | P1 | — |
| 0x2E | CloseWindow | `u8 id` | P1 | — |
| 0x2F | SetSlot | `i8 win, i16 slot, slotdata` | **P0** | cb_2f_1.bin* |
| 0x30 | WindowItems | `u8 win, i16 count, slotdata×n` | **P0** | cb_30_1.bin* |
| 0x31 | WindowProperty | `u8 win, i16 prop, i16 val` | P1 | — |
| 0x32 | ConfirmTransaction | `u8 win, i16 action, bool ok` | P1 | — |
| 0x33 | UpdateSign | `pos, s×4` | P1 | — |
| 0x34 | Maps | `v mapid, v scale, v icons, v cols, v rows, i32 x,z, u8 data[]` | P2 | — |
| 0x35 | UpdateBlockEntity | `pos, u8 action, NBT` | P1 | — |
| 0x36 | SignEditorOpen | `pos` | P1 | — |
| 0x37 | Statistics | `v count, (s nome, v valor)×n` | **P0** | cb_37_1.bin* |
| 0x38 | PlayerListItem | `v action, v count, ...` (ADD: uuid16,s,props,v,v,bool[,s]) | **P0** | cb_38_*.bin* |
| 0x39 | PlayerAbilities | `u8 flags, f32 fly, f32 walk` | **P0** | cb_39_1.bin* |
| 0x3A | TabComplete | `v count, s×n` | P2 | — |
| 0x3B | ScoreboardObjective | `s name, s val, u8 action` | P2 | — |
| 0x3C | UpdateScore | `s name, u8 action, s obj, [v val]` | P2 | — |
| 0x3D | DisplayScoreboard | `u8 pos, s name` | P2 | — |
| 0x3E | Teams | `s name, u8 mode, ...` | P2 | — |
| 0x3F | PluginMessage | `s canal, bytes` (server manda `MC|Brand`+`"vanilla"`) | **P0** | cb_3f_1.bin* |
| 0x40 | Disconnect | `s json` | feito | — |
| 0x41 | ServerDifficulty | `u8 diff` | **P0** | cb_41_1.bin* |
| 0x42 | CombatEvent | `v event, ...` | P1 | — |
| 0x43 | Camera | `v eid` | P2 | — |
| 0x44 | WorldBorder | `v action, ...` (INIT: f64×4, vlong, v×3) | **P0** | cb_44_1.bin* |
| 0x45 | Title | `v action, ...` | P2 | — |
| 0x47 | PlayerListHeaderFooter | `s header, s footer` | P2 | — |
| 0x48 | ResourcePackSend | `s url, s hash` | P2 | — |

`slotdata` (1.8, classe hd): `i16 id (-1 vazio) [+ u8 count, i16 damage, (0x00 | NBT root)]`.

## Serverbound (client → server)

| id | Nome | Formato | Pri |
|----|------|---------|-----|
| 0x00 | KeepAlive | `v id` | feito |
| 0x01 | Chat | `s (max 100)` | feito |
| 0x02 | UseEntity | `v target, v action, [f32 x,y,z]` | P1 |
| 0x03 | Flying | `bool ground` | feito |
| 0x04 | Position | `f64×3, bool ground` | feito |
| 0x05 | Look | `f32 yaw,pitch, bool ground` | feito |
| 0x06 | PositionLook | `f64×3, f32×2, bool ground` | feito |
| 0x07 | BlockDig | `v status, pos, u8 face` | feito | testado no jar |
| 0x08 | PlayerBlockPlacement | `pos, u8 dir, slot, u8 cx,cy,cz` | feito | testado no jar |
| 0x09 | HeldItemChange | `v slot` | P1 |
| 0x0A | ArmAnimation | (vazio) | P1 |
| 0x0B | EntityAction | `v eid, v action, v jumpboost` | P1 |
| 0x0C | SteerVehicle | `f32 side,fwd, u8 flags` | P1 |
| 0x0D | CloseWindow | `u8 id` | P1 |
| 0x0E | ClickWindow | `u8 win, i16 slot, u8 btn, i16 action, u8 mode, slotdata` | P1 |
| 0x0F | ConfirmTransaction | `u8 win, i16 action, bool ok` | P1 |
| 0x10 | CreativeInventoryAction | `i16 slot, slotdata` | P1 |
| 0x11 | EnchantItem | `u8 win, u8 ench` | P1 |
| 0x12 | UpdateSign | `pos, s×4` | P1 |
| 0x13 | Abilities | `u8 flags, f32 fly,walk` | P1 |
| 0x14 | TabComplete | `s txt, [bool pos, pos]` | P2 |
| 0x15 | ClientSettings | `s locale, u8 view, v chatmode, bool colors, u8 skin, v hand?` | P1 |
| 0x16 | ClientStatus | `v action (0 respawn, 1 stats)` | P1 |
| 0x17 | PluginMessage | `s canal, bytes` | feito (ignora) |
| 0x19 | ResourcePackStatus | `s hash, v result` | P2 |

## Prioridades

- **P0**: login limpo + HUD correto sem erro no cliente. É o lote desta entrega.
- **P1**: interação com o mundo (quebrar/colocar, inventário, entidades visíveis).
  Próxima entrega, na ordem: janelas/inventário → blocos → entidades.
- **P2**: paridade fina (scoreboard, títulos, mapas, pacotes ópticos).
