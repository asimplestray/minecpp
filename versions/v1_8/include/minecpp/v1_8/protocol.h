#pragma once

// Pacotes 1.8 (protocolo 47) — encode/decode sobre id+payload (sem framing de
// conexão, sem compressão/criptografia: isso é da futura camada net).
// Formatos verificados contra bytes reais do server-1.8.jar em
// test-data/vanilla-1.8-flat/net/ + `hd`/`dt` decompilados.
//
// ChunkData 0x21 (ground truth dos bytes reais):
//   id + int x + int z + bool continuous + u16BE mask + varint dataLen + data
//   data por section (bit order, Y asc): 4096×u16LE(id<<4|meta) + 2048
//   blocklight + 2048 skylight; + 256 biomes se continuous.

#include <cstdint>
#include <string>
#include <vector>

#include "minecpp/core/buf.h"
#include "minecpp/v1_8/chunk.h"

namespace minecpp::v18::proto {

inline constexpr int kProtocolVersion = 47;
inline constexpr size_t kMaxFrame = 2u << 20;  // 2MB, teto anti-OOM

enum class State { kHandshake = 0, kStatus = 1, kLogin = 2, kPlay = 3 };

// IDs serverbound/clientbound por estado (só os implementados).
enum : int32_t {
  // Handshake (SB)
  kSbHandshake = 0x00,
  // Status
  kCbStatusResponse = 0x00,
  kCbStatusPong = 0x01,
  kSbStatusRequest = 0x00,
  kSbStatusPing = 0x01,
  // Login
  kCbLoginDisconnect = 0x00,
  kCbLoginSuccess = 0x02,
  kCbLoginCompression = 0x03,
  kSbLoginStart = 0x00,
  // Play (CB)
  kCbKeepAlive = 0x00,
  kCbJoinGame = 0x01,
  kCbChat = 0x02,
  kCbTimeUpdate = 0x03,
  kCbSpawnPosition = 0x05,
  kCbPlayerPosLook = 0x08,
  kCbHeldItem = 0x09,
  kCbChunkData = 0x21,
  kCbBulk = 0x26,
  kCbSetSlot = 0x2F,
  kCbWindowItems = 0x30,
  kCbStatistics = 0x37,
  kCbPlayerList = 0x38,
  kCbAbilities = 0x39,
  kCbPlugin = 0x3F,
  kCbPlayDisconnect = 0x40,
  kCbDifficulty = 0x41,
  kCbBorder = 0x44,
  kCbUpdateHealth = 0x06,
  kCbExperience = 0x1F,
  // Play (SB)
  kSbKeepAlive = 0x00,
  kSbChat = 0x01,
  kSbFlying = 0x03,
  kSbPosition = 0x04,
  kSbLook = 0x05,
  kSbPosLook = 0x06,
  kSbHeldItemSB = 0x09,
  kSbArmAnim = 0x0A,
  kSbCloseWindow = 0x0D,
  kSbClickWindow = 0x0E,
  kSbConfirmTxn = 0x0F,
  kSbCreativeAction = 0x10,
  kSbPluginMessage = 0x17,
  // Play SB blocos (P1b)
  kSbDigging = 0x07,
  kSbBlockPlace = 0x08,
  // Play SB entidades (P1c: só decode por enquanto)
  kSbUseEntity = 0x02,
  // Play CB entidades (P1c)
  kCbSpawnObject = 0x0E,
  kCbSpawnMob = 0x0F,
  kCbSpawnPlayer = 0x0C,
  kCbCollectItem = 0x0D,
  kCbVelocity = 0x12,
  kCbDestroy = 0x13,
  kCbRelMove = 0x15,
  kCbLook = 0x16,
  kCbRelMoveLook = 0x17,
  kCbTeleport = 0x18,
  kCbHeadLook = 0x19,
  kCbEntityStatus = 0x1A,
  kCbMetadata = 0x1C,
  kCbProperties = 0x20,
  kCbExpOrb = 0x11,
  kCbEquipment = 0x04,
  // Play CB blocos (P1b)
  kCbBlockChange = 0x23,
  kCbBlockAction = 0x24,
  kCbBreakAnim = 0x25,
  kCbEffect = 0x28,
  kCbSound = 0x29,
  // Play CB janelas/entidades (lote P1)
  kCbAnimation = 0x0B,
  kCbOpenWindow = 0x2D,
  kCbCloseWindow = 0x2E,
  kCbWindowProp = 0x31,
  kCbConfirmTxn = 0x32,
  // Play CB P1 faltando
  kCbRespawn = 0x07,
  kCbUseBed = 0x0A,
  kCbSpawnPainting = 0x10,
  kCbAttachEntity = 0x1B,
  kCbEntityEffect = 0x1D,
  kCbRemoveEntityEffect = 0x1E,
  kCbEntityRelMove0 = 0x14,
  kCbMultiBlockChange = 0x22,
  kCbExplosion = 0x27,
  kCbParticle = 0x2A,
  kCbChangeGameState = 0x2B,
  kCbSpawnGlobalEntity = 0x2C,
  kCbUpdateSign = 0x33,
  kCbUpdateBlockEntity = 0x35,
  kCbSignEditorOpen = 0x36,
  kCbCombatEvent = 0x42,
  // Play CB P2 (scoreboard, titles, maps, resources)
  kCbMaps = 0x34,
  kCbTabComplete = 0x3A,
  kCbScoreboardObjective = 0x3B,
  kCbUpdateScore = 0x3C,
  kCbDisplayScoreboard = 0x3D,
  kCbTeams = 0x3E,
  kCbCamera = 0x43,
  kCbTitle = 0x45,
  kCbPlayerListHeaderFooter = 0x47,
  kCbResourcePackSend = 0x48,
  // Play SB P1 faltando
  kSbEntityAction = 0x0B,
  kSbSteerVehicle = 0x0C,
  kSbEnchantItem = 0x11,
  kSbUpdateSign = 0x12,
  kSbPlayerAbilities = 0x13,
  kSbClientSettings = 0x15,
  kSbClientStatus = 0x16,
  // Play SB P2
  kSbTabComplete = 0x14,
  kSbResourcePackStatus = 0x19,
};

struct Handshake {
  int32_t proto = kProtocolVersion;
  std::string host;
  uint16_t port = 25565;
  int32_t next = 1;  // 1=status, 2=login
};

struct StatusResponse {
  std::string json;  // opaco (sem lib JSON no core)
};

struct Ping {
  int64_t payload = 0;
};

struct LoginStart {
  std::string name;  // max 16 chars
};

struct LoginSuccess {
  std::string uuid;  // "xxxxxxxx-xxxx-..." string
  std::string name;
};

struct Disconnect {
  std::string reason;  // JSON chat
};

struct SetCompression {
  int32_t threshold = -1;  // -1 desliga
};

struct KeepAlive {
  int32_t id = 0;  // VarInt em 1.8
};

struct JoinGame {
  int32_t eid = 0;
  uint8_t mode = 0;
  int8_t dimension = 0;
  uint8_t difficulty = 0;
  uint8_t max_players = 0;
  std::string level_type;
  bool reduced_debug = false;
};

struct SpawnPosition {
  int32_t x = 0, y = 0, z = 0;
};

struct PlayerPosLook {
  double x = 0, y = 0, z = 0;
  float yaw = 0, pitch = 0;
  uint8_t flags = 0;
};

struct ChunkData {
  int32_t x = 0, z = 0;
  bool continuous = true;
  uint16_t mask = 0;
  std::vector<uint8_t> data;
};

struct Chat {
  std::string msg;  // SB max 100 chars; CB JSON
};

struct TimeUpdate {
  int64_t age = 0, time = 0;
};

struct Flying {
  bool on_ground = false;
};

struct SbPosition {
  double x = 0, y = 0, z = 0;
  bool on_ground = false;
};

struct SbLook {
  float yaw = 0, pitch = 0;
  bool on_ground = false;
};

struct SbPosLook {
  double x = 0, y = 0, z = 0;
  float yaw = 0, pitch = 0;
  bool on_ground = false;
};

struct PluginMessage {
  std::string channel;  // max 20
  std::vector<uint8_t> data;
};

// ---- lote P0 (login limpo, goldens em net/goldens/) ----

struct ServerDifficulty {
  uint8_t difficulty = 1;
};

struct PlayerAbilities {
  uint8_t flags = 0;
  float fly_speed = 0.05f, walk_speed = 0.1f;
};

struct HeldItemChange {
  uint8_t slot = 0;
};

struct Statistics {
  std::vector<std::pair<std::string, int32_t>> stats;  // vazio = zerado
};

struct PlayerListProp {
  std::string name, value;
  bool has_sig = false;
  std::string sig;
};

struct PlayerListEntry {
  uint8_t uuid[16] = {};
  std::string name;
  std::vector<PlayerListProp> props;
  int32_t gamemode = 0, ping = 0;
  bool has_display = false;
  std::string display;
};

struct PlayerListItem {
  int32_t action = 0;  // 0 ADD, 4 REMOVE (só esses implementados)
  std::vector<PlayerListEntry> players;
};

struct WorldBorder {  // action 3 INITIALIZE (única implementada)
  int32_t action = 3;
  double center_x = 0, center_z = 0;
  double old_size = 6e7, new_size = 6e7;
  int64_t speed = 0;
  int32_t portal_boundary = 29999984, warn_a = 5, warn_b = 15;
};

struct Slot {  // slotdata 1.8 (esquema do hd: NBT ausente = byte 0x00)
  Slot() : id(-1), count(0), damage(0), nbt(nullptr) {}
  int16_t id;
  uint8_t count;
  int16_t damage;
  minecpp_nbt_tag_t *nbt;  // owned; null = sem tag
};

void FreeSlot(Slot &s);  // libera NBT e zera (id=-1)
bool CopySlot(Slot &dst, const Slot &src);  // deep-copy; false=NOMEM (dst zerado)
bool SlotsSame(const Slot &a, const Slot &b);

struct WindowItems {
  uint8_t window = 0;
  std::vector<Slot> slots;
};

struct SetSlot {
  int8_t window = 0;  // byte com sinal no fio (jh); -1 = cursor
  int16_t slot = -1;
  Slot item;
};

struct UpdateHealth {
  float health = 20, saturation = 5;
  int32_t food = 20;
};

struct SetExperience {
  float bar = 0;
  int32_t level = 0, total = 0;
};

struct BulkChunk {
  int32_t x = 0, z = 0;
  uint16_t mask = 0;
  std::vector<uint8_t> data;  // seções + biomas (ver 0x26)
};

struct MapChunkBulk {
  bool sky_light = true;
  std::vector<BulkChunk> chunks;  // vanilla manda 10 por pacote
};

// ---- janelas/inventário (P1a) ----

struct ClickWindow {
  uint8_t window = 0;
  int16_t slot = 0;  // -999 = fora da janela
  uint8_t button = 0;
  int16_t action = 0;  // id de transação (eco no confirm)
  uint8_t mode = 0;  // 0..6
  Slot clicked;  // item que o cliente acha que clicou (validação)
};

struct ConfirmTransaction {
  uint8_t window = 0;
  int16_t action = 0;
  bool accepted = false;
};

struct OpenWindow {
  uint8_t window = 0;
  std::string type;  // "minecraft:chest", ...
  std::string title;  // JSON chat
  uint8_t slots = 0;
};

struct CloseWindowPkt {  // CB 0x2E e SB 0x0D (só id da janela)
  uint8_t window = 0;
};

struct WindowProperty {
  uint8_t window = 0;
  int16_t prop = 0, value = 0;
};

struct Animation {
  int32_t eid = 0;
  uint8_t action = 0;  // 0 swing, ...
};

struct CreativeAction {
  int16_t slot = 0;
  Slot item;
};

struct SbHeldItem {
  int16_t slot = 0;  // 0..8 (short no SB, byte no CB)
};

// ---- blocos (P1b) ----

struct BlockDig {
  int32_t status = 0;  // varint no fio (classe it: e()): 0 start, 1 cancel,
                       // 2 finish, 3 drop-item, 4 eat/shoot
  int32_t x = 0, y = 0, z = 0;
  uint8_t face = 0;
};

struct BlockPlace {
  int32_t x = 0, y = 0, z = 0;  // -1 = usar item no ar
  uint8_t dir = 255;
  Slot held;
  uint8_t cx = 0, cy = 0, cz = 0;  // cursor 0..15
};

struct BlockChangePkt {
  int32_t x = 0, y = 0, z = 0;
  int32_t idmeta = 0;  // id<<4|meta
};

struct BlockAction {
  int32_t x = 0, y = 0, z = 0;
  uint8_t b1 = 0, b2 = 0;
  int32_t block = 0;
};

struct BreakAnim {
  int32_t eid = 0;
  int32_t x = 0, y = 0, z = 0;
  int8_t stage = 0;  // 0..9 progresso, -1 remove (vanilla: writeByte)
};

struct Effect {
  int32_t id = 0;  // 2001 = quebra bloco (partículas+som no cliente)
  int32_t x = 0, y = 0, z = 0;
  int32_t data = 0;  // 2001: id do bloco
  bool norel = false;
};

struct SoundEffect {
  std::string name;
  int32_t x = 0, y = 0, z = 0;  // ×8
  float volume = 1, pitch = 1;
};

// ---- entidades (P1c) ----

// Tipos mob 1.8 usados: 50 creeper, 51 skeleton, 52 spider, 54 zombie,
// 55 slime, 58 enderman, 90 pig, 91 sheep, 92 cow, 93 chicken, 94 squid.
// Objetos: 1 boat, 2 item, 10 minecart, 70 falling block.

// Entrada de metadata: index 0..31, tipo no alto do byte-chave.
struct MetaEntry {
  uint8_t index = 0;
  uint8_t type = 0;  // 0 u8, 1 i16, 2 i32, 3 f32, 4 string, 5 slot, 6 pos, 7 rot
  int64_t i = 0;
  double f = 0;
  std::string s;
  Slot item;
  int32_t px = 0, py = 0, pz = 0;  // tipo 6
  float rx = 0, ry = 0, rz = 0;  // tipo 7
};

struct Metadata {
  std::vector<MetaEntry> entries;
};

struct SpawnMob {
  int32_t eid = 0;
  uint8_t type = 90;
  int32_t x = 0, y = 0, z = 0;  // fixo ×32
  uint8_t yaw = 0, pitch = 0, head = 0;
  int16_t vx = 0, vy = 0, vz = 0;
  Metadata meta;  // inline após velocidade (golden confirma)
};

struct SpawnPlayer {
  int32_t eid = 0;
  uint8_t uuid[16] = {};
  int32_t x = 0, y = 0, z = 0;  // fixo ×32
  uint8_t yaw = 0, pitch = 0;
  int16_t held = 0;  // item na mão (id, 0 = vazio)
  Metadata meta;  // inline (golden confirma)
};

struct SpawnObject {
  int32_t eid = 0;
  uint8_t type = 2;  // 2 = item dropado
  int32_t x = 0, y = 0, z = 0;  // fixo ×32
  uint8_t pitch = 0, yaw = 0;
  int32_t data = 0;  // item: 1
  bool has_vel = false;
  int16_t vx = 0, vy = 0, vz = 0;
};

struct DestroyEntities {
  std::vector<int32_t> eids;
};

struct EntityTeleport {
  int32_t eid = 0;
  int32_t x = 0, y = 0, z = 0;  // fixo ×32
  uint8_t yaw = 0, pitch = 0;
  bool on_ground = false;
};

struct EntityRelMove {
  int32_t eid = 0;
  int8_t dx = 0, dy = 0, dz = 0;  // delta ×32 (short? não: 3×i8 + ground)
  bool on_ground = false;
};

struct EntityLook {
  int32_t eid = 0;
  uint8_t yaw = 0, pitch = 0;
  bool on_ground = false;
};

struct EntityRelMoveLook {
  int32_t eid = 0;
  int8_t dx = 0, dy = 0, dz = 0;
  uint8_t yaw = 0, pitch = 0;
  bool on_ground = false;
};

struct Velocity {
  int32_t eid = 0;
  int16_t vx = 0, vy = 0, vz = 0;
};

struct HeadLook {
  int32_t eid = 0;
  uint8_t head = 0;
};

struct EntityStatus {
  int32_t eid = 0;  // i32 no fio 1.8!
  uint8_t status = 0;
};

struct Equipment {
  int32_t eid = 0;
  int32_t slot = 0;
  Slot item;
};

struct CollectItem {
  int32_t collector = 0, collected = 0;
};

struct ExpOrb {
  int32_t eid = 0;
  int32_t x = 0, y = 0, z = 0;  // fixo ×32
  int32_t count = 0;
};

struct UseEntity {  // SB 0x02 (só decode; ação na Fase 5)
  int32_t target = 0;
  int32_t action = 0;  // 0 interact, 1 attack, 2 interact-at
  float hx = 0, hy = 0, hz = 0;  // só se action == 2
};

struct PropertyMod {
  uint8_t uuid[16] = {};
  double amount = 0;
  int8_t op = 0;
};

struct Property {
  std::string key;
  double value = 0;
  std::vector<PropertyMod> mods;
};

struct Properties {
  int32_t eid = 0;
  std::vector<Property> props;
};

// ---- P1 faltando: Clientbound ----

struct Respawn {
  int32_t dimension = 0;
  uint8_t difficulty = 0;
  uint8_t gamemode = 0;
  std::string level_type;
};

struct UseBed {
  int32_t eid = 0;
  int32_t x = 0, y = 0, z = 0;
};

struct SpawnPainting {
  int32_t eid = 0;
  std::string title;
  int32_t x = 0, y = 0, z = 0;
  uint8_t direction = 0;
};

struct AttachEntity {
  int32_t vehicle = 0;
  int32_t rider = 0;
  bool leash = false;
};

struct EntityEffect {
  int32_t eid = 0;
  uint8_t effect_id = 0;
  uint8_t amplifier = 0;
  int32_t duration = 0;
  bool hide_particles = false;
};

struct RemoveEntityEffect {
  int32_t eid = 0;
  uint8_t effect_id = 0;
};

struct EntityRelMove0 {
  int32_t eid = 0;
};

struct MultiBlockChange {
  int32_t chunk_x = 0, chunk_z = 0;
  struct Record {
    uint16_t packed = 0;  // (x<<12)|(z<<8)|y
    int32_t block_id = 0;
  };
  std::vector<Record> records;
};

struct Explosion {
  float x = 0, y = 0, z = 0;
  float strength = 0;
  struct Offset {
    int8_t dx = 0, dy = 0, dz = 0;
  };
  std::vector<Offset> records;
  float player_motion_x = 0, player_motion_y = 0, player_motion_z = 0;
};

struct Particle {
  int32_t id = 0;
  bool long_distance = false;
  float x = 0, y = 0, z = 0;
  float ox = 0, oy = 0, oz = 0;
  float speed = 0;
  int32_t count = 0;
  std::vector<int32_t> data;  // opcional, depende do id
};

struct ChangeGameState {
  uint8_t reason = 0;  // 0=no respawn, 1=end raining, 2=begin raining, 3=gamemode, 4=win, 5=demo, 6=arrow hit
  float value = 0;
};

struct SpawnGlobalEntity {
  int32_t eid = 0;
  uint8_t type = 0;  // 1=lightning
  int32_t x = 0, y = 0, z = 0;
};

struct UpdateSign {
  int32_t x = 0, y = 0, z = 0;
  std::string lines[4];
};

struct UpdateBlockEntity {
  int32_t x = 0, y = 0, z = 0;
  uint8_t action = 0;
  minecpp_nbt_tag_t *nbt = nullptr;
};

struct SignEditorOpen {
  int32_t x = 0, y = 0, z = 0;
};

struct CombatEvent {
  int32_t event = 0;  // 0=enter combat, 1=end combat, 2=entity dead
  int32_t duration = 0;
  int32_t entity_id = 0;
  int32_t player_id = 0;
  std::string death_message;
};

// ---- P1 faltando: Serverbound ----

struct EntityAction {
  int32_t eid = 0;
  int32_t action = 0;  // 1=sneak, 2=unsneak, 3=leave bed, 4=sprint, 5=unsprint, 6=jump horse
  int32_t jump_boost = 0;
};

struct SteerVehicle {
  float sideways = 0, forward = 0;
  uint8_t flags = 0;  // 0x1=jump, 0x2=unmount
};

struct EnchantItem {
  uint8_t window = 0;
  uint8_t enchantment = 0;
};

struct SbUpdateSign {
  int32_t x = 0, y = 0, z = 0;
  std::string lines[4];
};

struct SbPlayerAbilities {
  uint8_t flags = 0;
  float fly_speed = 0, walk_speed = 0;
};

struct ClientSettings {
  std::string locale;
  uint8_t view_distance = 0;
  int32_t chat_mode = 0;
  bool chat_colors = false;
  uint8_t skin_parts = 0;
  int32_t main_hand = 0;  // 0=left, 1=right (1.9+)
};

struct ClientStatus {
  int32_t action = 0;  // 0=respawn, 1=request stats, 2=taking inventory achievement
};

// ---- P2: Scoreboard, Titles, Maps, Resources ----

// Clientbound P2
struct MapData {
  int32_t map_id = 0;
  uint8_t scale = 0;
  std::vector<int32_t> icons;  // icon data
  int32_t columns = 0, rows = 0;
  int32_t x = 0, z = 0;
  std::vector<uint8_t> data;
};

struct CbTabComplete {
  std::vector<std::string> matches;
};

struct ScoreboardObjective {
  std::string name;
  std::string value;  // display name
  uint8_t action = 0;  // 0=create, 1=remove, 2=update display
};

struct UpdateScore {
  std::string name;  // score name (entity name)
  uint8_t action = 0;  // 0=create/update, 1=remove
  std::string objective;
  int32_t value = 0;
};

struct DisplayScoreboard {
  uint8_t position = 0;  // 0=list, 1=sidebar, 2=below name
  std::string name;  // objective name
};

struct Teams {
  std::string name;
  uint8_t mode = 0;  // 0=create, 1=remove, 2=update, 3=add players, 4=remove players
  std::string display_name;
  std::string prefix;
  std::string suffix;
  uint8_t friendly_fire = 0;
  uint8_t name_tag_visibility = 0;
  uint8_t color = 0;
  std::vector<std::string> players;  // usernames
};

struct Camera {
  int32_t entity_id = 0;
};

struct Title {
  int32_t action = 0;  // 0=set title, 1=set subtitle, 2=set times, 3=hide, 4=reset
  std::string text;  // JSON chat for title/subtitle
  int32_t fade_in = 0, stay = 0, fade_out = 0;  // for action=2
};

struct PlayerListHeaderFooter {
  std::string header;
  std::string footer;
};

struct ResourcePackSend {
  std::string url;
  std::string hash;  // SHA-1
};

// Serverbound P2
struct SbTabComplete {
  std::string text;
  bool has_position = false;
  int32_t x = 0, y = 0, z = 0;
};

struct ResourcePackStatus {
  std::string hash;
  int32_t result = 0;  // 0=success, 1=decline, 2=failed download, 3=accepted
};

bool Encode(const Handshake &, minecpp_writer_t *);
bool Decode(Handshake &, minecpp_reader_t *);
bool Encode(const StatusResponse &, minecpp_writer_t *);
bool Decode(StatusResponse &, minecpp_reader_t *);
bool Encode(const Ping &, minecpp_writer_t *);
bool Decode(Ping &, minecpp_reader_t *);
bool Encode(const LoginStart &, minecpp_writer_t *);
bool Decode(LoginStart &, minecpp_reader_t *);
bool Encode(const LoginSuccess &, minecpp_writer_t *);
bool Decode(LoginSuccess &, minecpp_reader_t *);
bool Encode(const Disconnect &, minecpp_writer_t *);
bool Decode(Disconnect &, minecpp_reader_t *);
bool Encode(const SetCompression &, minecpp_writer_t *);
bool Decode(SetCompression &, minecpp_reader_t *);
bool Encode(const KeepAlive &, minecpp_writer_t *);
bool Decode(KeepAlive &, minecpp_reader_t *);
bool Encode(const JoinGame &, minecpp_writer_t *);
bool Decode(JoinGame &, minecpp_reader_t *);
bool Encode(const SpawnPosition &, minecpp_writer_t *);
bool Decode(SpawnPosition &, minecpp_reader_t *);
bool Encode(const PlayerPosLook &, minecpp_writer_t *);
bool Decode(PlayerPosLook &, minecpp_reader_t *);
bool Encode(const ChunkData &, minecpp_writer_t *);
bool Decode(ChunkData &, minecpp_reader_t *);
bool Encode(const Chat &, minecpp_writer_t *);
bool Decode(Chat &, minecpp_reader_t *, int max_chars);
bool Encode(const TimeUpdate &, minecpp_writer_t *);
bool Decode(TimeUpdate &, minecpp_reader_t *);
bool Encode(const Flying &, minecpp_writer_t *);
bool Decode(Flying &, minecpp_reader_t *);
bool Encode(const SbPosition &, minecpp_writer_t *);
bool Decode(SbPosition &, minecpp_reader_t *);
bool Encode(const SbLook &, minecpp_writer_t *);
bool Decode(SbLook &, minecpp_reader_t *);
bool Encode(const SbPosLook &, minecpp_writer_t *);
bool Decode(SbPosLook &, minecpp_reader_t *);
bool Encode(const PluginMessage &, minecpp_writer_t *);
bool Decode(PluginMessage &, minecpp_reader_t *);
bool Encode(const ServerDifficulty &, minecpp_writer_t *);
bool Decode(ServerDifficulty &, minecpp_reader_t *);
bool Encode(const PlayerAbilities &, minecpp_writer_t *);
bool Decode(PlayerAbilities &, minecpp_reader_t *);
bool Encode(const HeldItemChange &, minecpp_writer_t *);
bool Decode(HeldItemChange &, minecpp_reader_t *);
bool Encode(const Statistics &, minecpp_writer_t *);
bool Decode(Statistics &, minecpp_reader_t *);
bool Encode(const PlayerListItem &, minecpp_writer_t *);
bool Decode(PlayerListItem &, minecpp_reader_t *);
bool Encode(const WorldBorder &, minecpp_writer_t *);
bool Decode(WorldBorder &, minecpp_reader_t *);
bool Encode(const Slot &, minecpp_writer_t *);
bool Decode(Slot &, minecpp_reader_t *);
bool Encode(const WindowItems &, minecpp_writer_t *);
bool Decode(WindowItems &, minecpp_reader_t *);
bool Encode(const SetSlot &, minecpp_writer_t *);
bool Decode(SetSlot &, minecpp_reader_t *);
bool Encode(const UpdateHealth &, minecpp_writer_t *);
bool Decode(UpdateHealth &, minecpp_reader_t *);
bool Encode(const SetExperience &, minecpp_writer_t *);
bool Decode(SetExperience &, minecpp_reader_t *);
bool Encode(const MapChunkBulk &, minecpp_writer_t *);
bool Decode(MapChunkBulk &, minecpp_reader_t *);
bool Encode(const ClickWindow &, minecpp_writer_t *);
bool Decode(ClickWindow &, minecpp_reader_t *);
bool Encode(const ConfirmTransaction &, minecpp_writer_t *);
bool Decode(ConfirmTransaction &, minecpp_reader_t *);
bool Encode(const OpenWindow &, minecpp_writer_t *);
bool Decode(OpenWindow &, minecpp_reader_t *);
bool Encode(const CloseWindowPkt &, minecpp_writer_t *);
bool Decode(CloseWindowPkt &, minecpp_reader_t *);
bool Encode(const WindowProperty &, minecpp_writer_t *);
bool Decode(WindowProperty &, minecpp_reader_t *);
bool Encode(const Animation &, minecpp_writer_t *);
bool Decode(Animation &, minecpp_reader_t *);
bool Encode(const CreativeAction &, minecpp_writer_t *);
bool Decode(CreativeAction &, minecpp_reader_t *);
bool Encode(const SbHeldItem &, minecpp_writer_t *);
bool Decode(SbHeldItem &, minecpp_reader_t *);
bool Encode(const BlockDig &, minecpp_writer_t *);
bool Decode(BlockDig &, minecpp_reader_t *);
bool Encode(const BlockPlace &, minecpp_writer_t *);
bool Decode(BlockPlace &, minecpp_reader_t *);
bool Encode(const BlockChangePkt &, minecpp_writer_t *);
bool Decode(BlockChangePkt &, minecpp_reader_t *);
bool Encode(const BlockAction &, minecpp_writer_t *);
bool Decode(BlockAction &, minecpp_reader_t *);
bool Encode(const BreakAnim &, minecpp_writer_t *);
bool Decode(BreakAnim &, minecpp_reader_t *);
bool Encode(const Effect &, minecpp_writer_t *);
bool Decode(Effect &, minecpp_reader_t *);
bool Encode(const SoundEffect &, minecpp_writer_t *);
bool Decode(SoundEffect &, minecpp_reader_t *);
bool Encode(const SpawnMob &, minecpp_writer_t *);
bool Decode(SpawnMob &, minecpp_reader_t *);
bool Encode(const SpawnPlayer &, minecpp_writer_t *);
bool Decode(SpawnPlayer &, minecpp_reader_t *);
bool Encode(const SpawnObject &, minecpp_writer_t *);
bool Decode(SpawnObject &, minecpp_reader_t *);
bool Encode(const DestroyEntities &, minecpp_writer_t *);
bool Decode(DestroyEntities &, minecpp_reader_t *);
bool Encode(const EntityTeleport &, minecpp_writer_t *);
bool Decode(EntityTeleport &, minecpp_reader_t *);
bool Encode(const EntityRelMove &, minecpp_writer_t *);
bool Decode(EntityRelMove &, minecpp_reader_t *);
bool Encode(const EntityLook &, minecpp_writer_t *);
bool Decode(EntityLook &, minecpp_reader_t *);
bool Encode(const EntityRelMoveLook &, minecpp_writer_t *);
bool Decode(EntityRelMoveLook &, minecpp_reader_t *);
bool Encode(const Velocity &, minecpp_writer_t *);
bool Decode(Velocity &, minecpp_reader_t *);
bool Encode(const HeadLook &, minecpp_writer_t *);
bool Decode(HeadLook &, minecpp_reader_t *);
bool Encode(const EntityStatus &, minecpp_writer_t *);
bool Decode(EntityStatus &, minecpp_reader_t *);
bool Encode(const Equipment &, minecpp_writer_t *);
bool Decode(Equipment &, minecpp_reader_t *);
bool Encode(const CollectItem &, minecpp_writer_t *);
bool Decode(CollectItem &, minecpp_reader_t *);
bool Encode(const ExpOrb &, minecpp_writer_t *);
bool Decode(ExpOrb &, minecpp_reader_t *);
bool Encode(const UseEntity &, minecpp_writer_t *);
bool Decode(UseEntity &, minecpp_reader_t *);
bool Encode(const Metadata &, minecpp_writer_t *);
bool Decode(Metadata &, minecpp_reader_t *);
bool Encode(const Properties &, minecpp_writer_t *);
bool Decode(Properties &, minecpp_reader_t *);

// P1 faltando: Clientbound encode/decode
bool Encode(const Respawn &, minecpp_writer_t *);
bool Decode(Respawn &, minecpp_reader_t *);
bool Encode(const UseBed &, minecpp_writer_t *);
bool Decode(UseBed &, minecpp_reader_t *);
bool Encode(const SpawnPainting &, minecpp_writer_t *);
bool Decode(SpawnPainting &, minecpp_reader_t *);
bool Encode(const AttachEntity &, minecpp_writer_t *);
bool Decode(AttachEntity &, minecpp_reader_t *);
bool Encode(const EntityEffect &, minecpp_writer_t *);
bool Decode(EntityEffect &, minecpp_reader_t *);
bool Encode(const RemoveEntityEffect &, minecpp_writer_t *);
bool Decode(RemoveEntityEffect &, minecpp_reader_t *);
bool Encode(const EntityRelMove0 &, minecpp_writer_t *);
bool Decode(EntityRelMove0 &, minecpp_reader_t *);
bool Encode(const MultiBlockChange &, minecpp_writer_t *);
bool Decode(MultiBlockChange &, minecpp_reader_t *);
bool Encode(const Explosion &, minecpp_writer_t *);
bool Decode(Explosion &, minecpp_reader_t *);
bool Encode(const Particle &, minecpp_writer_t *);
bool Decode(Particle &, minecpp_reader_t *);
bool Encode(const ChangeGameState &, minecpp_writer_t *);
bool Decode(ChangeGameState &, minecpp_reader_t *);
bool Encode(const SpawnGlobalEntity &, minecpp_writer_t *);
bool Decode(SpawnGlobalEntity &, minecpp_reader_t *);
bool Encode(const UpdateSign &, minecpp_writer_t *);
bool Decode(UpdateSign &, minecpp_reader_t *);
bool Encode(const UpdateBlockEntity &, minecpp_writer_t *);
bool Decode(UpdateBlockEntity &, minecpp_reader_t *);
bool Encode(const SignEditorOpen &, minecpp_writer_t *);
bool Decode(SignEditorOpen &, minecpp_reader_t *);
bool Encode(const CombatEvent &, minecpp_writer_t *);
bool Decode(CombatEvent &, minecpp_reader_t *);

// P1 faltando: Serverbound encode/decode
bool Encode(const EntityAction &, minecpp_writer_t *);
bool Decode(EntityAction &, minecpp_reader_t *);
bool Encode(const SteerVehicle &, minecpp_writer_t *);
bool Decode(SteerVehicle &, minecpp_reader_t *);
bool Encode(const EnchantItem &, minecpp_writer_t *);
bool Decode(EnchantItem &, minecpp_reader_t *);
bool Encode(const SbUpdateSign &, minecpp_writer_t *);
bool Decode(SbUpdateSign &, minecpp_reader_t *);
bool Encode(const SbPlayerAbilities &, minecpp_writer_t *);
bool Decode(SbPlayerAbilities &, minecpp_reader_t *);
bool Encode(const ClientSettings &, minecpp_writer_t *);
bool Decode(ClientSettings &, minecpp_reader_t *);
bool Encode(const ClientStatus &, minecpp_writer_t *);
bool Decode(ClientStatus &, minecpp_reader_t *);

// P2: Clientbound encode/decode
bool Encode(const MapData &, minecpp_writer_t *);
bool Decode(MapData &, minecpp_reader_t *);
bool Encode(const CbTabComplete &, minecpp_writer_t *);
bool Decode(CbTabComplete &, minecpp_reader_t *);
bool Encode(const ScoreboardObjective &, minecpp_writer_t *);
bool Decode(ScoreboardObjective &, minecpp_reader_t *);
bool Encode(const UpdateScore &, minecpp_writer_t *);
bool Decode(UpdateScore &, minecpp_reader_t *);
bool Encode(const DisplayScoreboard &, minecpp_writer_t *);
bool Decode(DisplayScoreboard &, minecpp_reader_t *);
bool Encode(const Teams &, minecpp_writer_t *);
bool Decode(Teams &, minecpp_reader_t *);
bool Encode(const Camera &, minecpp_writer_t *);
bool Decode(Camera &, minecpp_reader_t *);
bool Encode(const Title &, minecpp_writer_t *);
bool Decode(Title &, minecpp_reader_t *);
bool Encode(const PlayerListHeaderFooter &, minecpp_writer_t *);
bool Decode(PlayerListHeaderFooter &, minecpp_reader_t *);
bool Encode(const ResourcePackSend &, minecpp_writer_t *);
bool Decode(ResourcePackSend &, minecpp_reader_t *);

// P2: Serverbound encode/decode
bool Encode(const SbTabComplete &, minecpp_writer_t *);
bool Decode(SbTabComplete &, minecpp_reader_t *);
bool Encode(const ResourcePackStatus &, minecpp_writer_t *);
bool Decode(ResourcePackStatus &, minecpp_reader_t *);

// Frame sem compressão: varint(len) + varint(id) + payload.
// Unframe valida teto e devolve id + view do payload (sem cópia).
bool FrameEncode(int32_t id, const uint8_t *payload, size_t n,
                 minecpp_writer_t *out);
bool FrameDecode(minecpp_reader_t *r, int32_t *id, minecpp_reader_t *payload);

// ChunkData <-> Chunk (seções não-nulas, Y asc; exige tamanhos exatos).
bool ChunkDataBuild(const Chunk &c, bool continuous, ChunkData *out);
bool ChunkDataApply(const ChunkData &p, Chunk *c);
// Bulk: monta de chunks (biomas obrigatórios) / fatia em ChunkDatas.
bool BulkBuild(const std::vector<const Chunk *> &chunks, bool sky,
               MapChunkBulk *out);
bool BulkSplit(const MapChunkBulk &p, std::vector<ChunkData> *out);

}  // namespace minecpp::v18::proto
