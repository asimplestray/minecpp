#pragma once

// Servidor 1.8 jogável (offline-mode): handshake→status→login→play.
// Threading (ver docs/architecture.md §4):
// - net thread: poller + framing/compressão/criptografia (bytes <-> pacotes).
// - tick thread (main): state machines, mundo, timers. Nunca bloqueia em I/O.
// - workers: jobs de chunk (region+NBT+ChunkData). Completions via tick_q.
// Filas MPSC; tick drena tudo por iteração (50ms).

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "minecpp/core/mqueue.h"
#include "minecpp/v1_8/protocol.h"

namespace minecpp::v18 {

// ---- mensagens net -> tick ----
struct MsgIn {
  enum class Kind { Connected, Packet, Disconnected, ChunkReady } kind;
  uint32_t conn = 0;
  int32_t id = 0;  // Packet
  std::vector<uint8_t> payload;  // Packet
  // ChunkReady (de worker): Chunk NBT já parseado (ownership; null=missing).
  int32_t cx = 0, cz = 0;
  Chunk *chunk = nullptr;
  bool missing = false;
};

// ---- mensagens tick -> net ----
struct MsgOut {
  enum class Kind { Send, EnableCompression, Close } kind;
  uint32_t conn = 0;
  int32_t id = 0;  // Send
  std::vector<uint8_t> payload;  // Send
  int32_t threshold = -1;  // EnableCompression
};

struct ServerConfig {
  uint16_t port = 25565;
  std::string world_dir = "world";
  int view_distance = 5;  // raio em chunks (quadrado)
  int32_t compression_threshold = 256;  // -1 desliga
  std::string motd = "A Minecraft Server";
  int max_players = 20;
  bool verbose = false;
};

struct Player {
  enum class State { Handshake, Status, Login, Play };
  static constexpr int kInvSize = 45;  // 0 resultado, 1-4 craft, 5-8 armor,
                                       // 9-35 main, 36-44 hotbar
  ~Player();  // libera NBT dos slots
  uint32_t conn = 0;
  State state = State::Handshake;
  std::string name;
  std::string uuid;
  uint8_t uuid_raw[16] = {};
  double x = 0, y = 0, z = 0;
  float yaw = 0, pitch = 0;
  bool on_ground = false;
  int32_t eid = 0;
  int32_t dimension = 0;  // 0=overworld, -1=nether, 1=end
  std::unordered_set<int64_t> loaded, inflight;
  int32_t keepalive_id = 0;
  bool keepalive_pending = false;
  uint64_t keepalive_tick = 0;
  // Inventário (janela 0). Cursor = item no mouse (SetSlot -1,-1).
  proto::Slot inv[kInvSize];
  proto::Slot cursor;
  int selected = 0;  // hotbar 0..8
  bool drag_active = false;
  uint8_t drag_button = 0;  // 0 esquerdo, 1 direito
  std::vector<int> drag_slots;
  // Escavação em andamento (C07 start → finish/cancel).
  bool digging = false;
  int32_t dig_x = 0, dig_y = 0, dig_z = 0;
  // Players conhecidos (SpawnPlayer já enviado) + última pos transmitida.
  std::unordered_set<uint32_t> known_players;
  double bsx = 0, bsy = 0, bsz = 0;
};

// Entidade server-side (mob passivo ou item dropado). Sem IA (Fase 5);
// física só para itens.
struct Entity {
  ~Entity();
  enum class Kind { Mob, Item };
  int32_t eid = 0;
  Kind kind = Kind::Mob;
  uint8_t mob_type = 90;  // pig
  double health = 10;
  proto::Slot stack;  // Kind::Item
  double x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0;
  float yaw = 0, pitch = 0;
  int age = 0;  // item: pickup após 10, despawn 6000
  int far_ticks = 0;  // mob longe de players
  bool dead = false;
  std::unordered_set<uint32_t> seen_by;  // conns com spawn enviado
  double sx = 0, sy = 0, sz = 0;  // último teleporte
};

// Mundo server-side (dono tick): chunks adotados dos jobs. Sem save ainda —
// restart perde mudanças (documentado). Evicção LRU quando passa do teto.
class World {
 public:
  static constexpr size_t kMaxChunks = 1024;
  ~World();
  Chunk *Get(int32_t cx, int32_t cz) const;
  bool Has(int32_t cx, int32_t cz) const;
  void Adopt(Chunk *c);  // toma ownership (substitui)
  // Evicta até caber, poupando colunas em `keep`. Devolve nº evictados.
  size_t Evict(const std::unordered_set<int64_t> &keep);
  size_t Size() const;
  // Bloco global; -1 fora do chunk carregado (não carrega).
  int32_t GetBlock(int x, int y, int z, int32_t *meta) const;
  // Retorna false se chunk ausente.
  bool SetBlock(int x, int y, int z, int32_t id, int32_t meta);

 private:
  std::unordered_map<int64_t, std::unique_ptr<Chunk>> chunks_;
};

inline int64_t ChunkKey(int32_t cx, int32_t cz) {
  return (static_cast<int64_t>(cx) << 32) ^ static_cast<uint32_t>(cz);
}

class Server {
 public:
  Server(const ServerConfig &cfg, minecpp_mqueue_t *tick_q,
         minecpp_mqueue_t *net_q);
  // Lê spawn de level.dat. false = mundo inválido (msg em err).
  bool Init(std::string *err);
  // Chamados pela tick (e testes): pacotes/eventos já roteados.
  Player *AddPlayer(uint32_t conn);
  void RemovePlayer(uint32_t conn);
  Player *Find(uint32_t conn);
  void OnPacket(uint32_t conn, int32_t id, const uint8_t *payload, size_t n);
  void OnChunkReady(MsgIn *m);  // toma ownership
  void OnTick(uint64_t tick);  // drena tick_q + timers
  size_t PlayerCount() const;
  size_t EntityCount() const { return entities_.size(); }
  World &GetWorld() { return world_; }
  int32_t SpawnX() const { return spawn_x_; }
  int32_t SpawnY() const { return spawn_y_; }
  int32_t SpawnZ() const { return spawn_z_; }
  void Send(uint32_t conn, int32_t id, const uint8_t *p, size_t n);

 private:
  // Broadcast de pacote pronto p/ quem tem a coluna carregada (exceto `skip`).
  void SendToLoaded(int32_t cx, int32_t cz, int32_t id, const uint8_t *p,
                    size_t n, uint32_t skip = 0);
  void Kick(Player &p, const std::string &reason);
  void BroadcastChat(const std::string &from, const std::string &msg);
  void SendSlot(uint32_t conn, int slot, const proto::Slot &item);
  void SendCursor(uint32_t conn, const proto::Slot &cursor);
  void SendConfirm(uint32_t conn, int16_t action, bool accepted);
  void ProcessClick(Player &p, const proto::ClickWindow &c);
  void HandleDig(Player &p, const proto::BlockDig &d);
  void HandlePlace(Player &p, const proto::BlockPlace &d);
  // Insere stack no inventário (merge 9..44, depois vazio). Devolve o que
  // NÃO coube (count>0). Manda SetSlots dos alterados.
  int InventoryAdd(Player &p, const proto::Slot &stack);
  // Resync de bloco: manda estado atual p/ quem tem a coluna.
  void ResyncBlock(int x, int y, int z);
  // ---- entidades (P1c) ----
  int NextEid() { return next_eid_++; }
  Entity *SpawnMob(uint8_t type, double x, double y, double z, double health);
  Entity *SpawnItem(double x, double y, double z, const proto::Slot &stack,
                    double vx, double vy, double vz);
  void DestroyEntity(Entity &e);  // broadcast + apaga
  void EntityTick();  // física itens, pickup, despawn, spawn passivo
  void SendEntityTo(Entity &e, Player &viewer);  // spawn + meta + props
  void UpdateVisibility(Player &p);  // players↔players + entidades novas
  proto::Metadata MobMetadata(const Entity &e) const;
  proto::Metadata PlayerMetadata() const;
  int CountMobsNear(double x, double z, double r) const;
  uint32_t NextRand();
  void OnJoin(Player &p);
  void RequestChunks(Player &p);
  void SubmitChunkJob(Player &p, int32_t cx, int32_t cz);
  void HandleHandshake(Player &p, const uint8_t *d, size_t n);
  void HandleStatus(Player &p, int32_t id, const uint8_t *d, size_t n);
  void HandleLogin(Player &p, int32_t id, const uint8_t *d, size_t n);
  void HandlePlay(Player &p, int32_t id, const uint8_t *d, size_t n);
// ---- entidades (P1c) broadcast helpers ----
  void HandleUseEntity(Player &p, const proto::UseEntity &u);
  void SendEntityMetadata(const Entity &e);
  void SendEntityVelocity(const Entity &e);
  void SendEntityEquipment(const Entity &e, int32_t slot, const proto::Slot &item);
  void SendEntityStatus(const Entity &e, uint8_t status);
  void SendPlayerHeadLook(const Player &p);
  // ---- P1 faltando: handlers ----
  void SendHeldItemChange(Player &p, uint8_t slot);
  void SendSetExperience(Player &p, float bar, int level, int total);
  void SendRespawn(Player &p, int32_t dimension, uint8_t difficulty, uint8_t gamemode, const std::string &level_type);
  void SendUseBed(Entity &e, int32_t x, int32_t y, int32_t z);
  void SendSpawnPainting(Entity &e, const std::string &title, int32_t x, int32_t y, int32_t z, uint8_t direction);
  void SendSpawnExpOrb(Entity &e, int32_t x, int32_t y, int32_t z, int32_t count);
  void SendEntityRelMove(Entity &e, int8_t dx, int8_t dy, int8_t dz, bool on_ground);
  void SendAttachEntity(int32_t vehicle, int32_t rider, bool leash);
  void SendEntityEffect(Entity &e, uint8_t effect_id, uint8_t amplifier, int32_t duration, bool hide_particles);
  void SendRemoveEntityEffect(Entity &e, uint8_t effect_id);
  void SendMultiBlockChange(int32_t cx, int32_t cz, const std::vector<proto::MultiBlockChange::Record> &records);
  void SendExplosion(float x, float y, float z, float strength, const std::vector<proto::Explosion::Offset> &records,
                     float px, float py, float pz);
  void SendParticle(int32_t id, bool long_distance, float x, float y, float z, float ox, float oy, float oz,
                    float speed, int32_t count, const std::vector<int32_t> &data);
  void SendChangeGameState(uint8_t reason, float value);
  void SendSpawnGlobalEntity(int32_t eid, uint8_t type, int32_t x, int32_t y, int32_t z);
  void SendOpenWindow(uint8_t window, const std::string &type, const std::string &title, uint8_t slots);
  void SendCloseWindow(uint8_t window);
  void SendWindowProperty(uint8_t window, int16_t prop, int16_t value);
  void SendUpdateSign(int32_t x, int32_t y, int32_t z, const std::string lines[4]);
  void SendUpdateBlockEntity(int32_t x, int32_t y, int32_t z, uint8_t action);
  void SendSignEditorOpen(int32_t x, int32_t y, int32_t z);
  void SendCombatEvent(int32_t event, int32_t duration, int32_t entity_id, int32_t player_id, const std::string &death_message);
  // SB handlers P1 faltando
  void HandleHeldItemChange(Player &p, const proto::SbHeldItem &h);
  void HandleEntityAction(Player &p, const proto::EntityAction &a);
  void HandleSteerVehicle(Player &p, const proto::SteerVehicle &s);
  void HandleCloseWindow(Player &p, uint8_t window);
  void HandleCreativeAction(Player &p, const proto::CreativeAction &c);
  void HandleEnchantItem(Player &p, const proto::EnchantItem &e);
  void HandleUpdateSign(Player &p, const proto::SbUpdateSign &s);
  void HandleAbilities(Player &p, const proto::SbPlayerAbilities &a);
  void HandleClientSettings(Player &p, const proto::ClientSettings &c);
  void HandleClientStatus(Player &p, const proto::ClientStatus &c);
  // ---- P2: handlers ----
  void SendMapData(int32_t map_id, uint8_t scale, const std::vector<int32_t> &icons,
                   int32_t columns, int32_t rows, int32_t x, int32_t z, const std::vector<uint8_t> &data);
  void SendTabComplete(const std::vector<std::string> &matches);
  void SendScoreboardObjective(const std::string &name, const std::string &value, uint8_t action);
  void SendUpdateScore(const std::string &name, uint8_t action, const std::string &objective, int32_t value);
  void SendDisplayScoreboard(uint8_t position, const std::string &name);
  void SendTeams(const std::string &name, uint8_t mode, const std::string &display_name,
                 const std::string &prefix, const std::string &suffix,
                 uint8_t friendly_fire, uint8_t name_tag_visibility, uint8_t color,
                 const std::vector<std::string> &players);
  void SendCamera(int32_t entity_id);
  void SendTitle(int32_t action, const std::string &text, int32_t fade_in, int32_t stay, int32_t fade_out);
  void SendPlayerListHeaderFooter(const std::string &header, const std::string &footer);
  void SendResourcePackSend(const std::string &url, const std::string &hash);
  void HandleTabComplete(Player &p, const proto::SbTabComplete &t);
  void HandleResourcePackStatus(Player &p, const proto::ResourcePackStatus &r);

  std::string StatusJson() const;

  ServerConfig cfg_;
  minecpp_mqueue_t *tick_q_ = nullptr;
  minecpp_mqueue_t *net_q_ = nullptr;
  std::unordered_map<uint32_t, std::unique_ptr<Player>> players_;
  std::unordered_map<int32_t, std::unique_ptr<Entity>> entities_;
  uint64_t rng_ = 0x9E3779B97F4A7C15ull;
  World world_;
  int32_t spawn_x_ = 8, spawn_y_ = 4, spawn_z_ = 8;
  int64_t world_time_ = 0;
  int32_t next_eid_ = 1;
  uint64_t tick_ = 0;
  int submits_this_tick_ = 0;
};

// Job de chunk p/ pool (arg mallocado; SEMPRE responde, mesmo com missing).
struct ChunkJob {
  std::string region_dir;
  uint32_t conn = 0;
  int32_t cx = 0, cz = 0;
  int32_t dimension = 0;  // 0=overworld, -1=nether, 1=end
  minecpp_mqueue_t *reply = nullptr;  // tick_q
};

void ChunkJobMain(void *arg);

// Libera MsgIn (incl. Chunk de ChunkReady não-processado).
void FreeMsgIn(MsgIn *m);

// ---- net thread (dona de sockets; fala com tick via filas) ----
class NetThread {
 public:
  NetThread(uint16_t port, minecpp_mqueue_t *tick_q, minecpp_mqueue_t *net_q);
  ~NetThread();
  bool Start(std::string *err);  // binda; false = porta ocupada etc.
  void Stop();  // sinaliza + join
  void SetVerbose(bool v) { verbose_ = v; }
  void Run();  // loop (chamado na thread)

 private:
  struct Conn;
  void CloseConn(uint32_t id, const char *why);
  uint16_t port_;
  minecpp_mqueue_t *tick_q_ = nullptr;
  minecpp_mqueue_t *net_q_ = nullptr;
  std::atomic<bool> stop_{false};
  bool started_ = false;
  bool verbose_ = false;
  std::thread worker_;
  // (resto do estado vive em server.cpp, só na net thread)
  struct Impl;
  Impl *impl_ = nullptr;
};

}  // namespace minecpp::v18
