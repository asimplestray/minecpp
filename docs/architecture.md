# minecpp — Architecture

> Servidor alternativo de Minecraft em C + C++ híbrido.
> Alvo: multi-versão (1.8+), um binário por versão, multithreading real desde o dia 1,
> baixo consumo, multiplataforma (Linux + Windows).

Referência vanilla: `server-1.8.jar` (protocolo 47). Não distribuir código dele aqui,
usar só como oráculo de comportamento + gerador de mundos para testes.

---

## 1. Decisões travadas

| # | Decisão | Escolha |
|---|---------|---------|
| 1 | Distribuição | **Um binário por versão.** Repo monolito, release por versão (`minecpp-1.8-linux-x64.zip`, `minecpp-1.9-win64.zip`, ...). Sem tradução runtime entre versões (sem ViaVersion interno). |
| 2 | Layout | `src/core/` (agnóstico) + `versions/vX_Y/` na raiz. Cada versão implementa em C++ e expõe ABI C. |
| 3 | Reuso | **Interface C + composição, não herança C++.** `v1_9` linka os objetos de `v1_8` e sobrescreve só o delta. Sem `class V19 : public V18`. |
| 4 | Threading | **Pipeline real, não region-threading (Folia).** Tick orquestradora + pool work-stealing + I/O async. Semântica de mundo único preservada. |
| 5 | Hooks | Event bus + scheduler no `core` desde já, custo zero sem plugin. API nativa C primeiro, bridge Paper/JNI depois. |
| 6 | Linguagem | **Híbrido:** C17 no caminho quente (buffers, varint, NBT, region I/O, filas), C++20 na orquestração (world, server, versões). |
| 7 | MVP | Mundo + chunks: NBT + region `.mca` + modelo de chunk 1.8 + gerador flat + tool de validação contra mundo vanilla. Sem net/play ainda. |

### Por que não herança C++ entre versões

Cadeia `v1_10 : v1_9 : v1_8` funciona até a 1.12 e explode na **1.13 (The Flattening)**:
acaba `id:meta`, entra `namespaced + blockstate + palette`, chunk e protocolo reescritos.
Herança profunda = fragile base class + virtual em hot path + refator impossível.

Solução: duas famílias base, mesma interface:

- `Base_Legacy (v1_8 .. v1_12)` — raiz é `v1_8`, resto é delta.
- `Base_Flattened (v1_13+)` — nova raiz quando chegarmos lá, mesma `version_api_t`.

Cada pasta de versão contém `delta.md` listando o que mudou em relação à anterior.
Nada de cópia full.

Sub-versões (1.8–1.8.9 = protocolo 47) são uma família só: `v1_8` cobre `1.8.x`.

---

## 2. Layout do repo

```
minecpp/
  CMakeLists.txt            # root: options por versão, Threads, warnings, output
  cmake/                    # helpers (warnings.cmake, platform.cmake no futuro)
  docs/architecture.md      # este arquivo
  server-1.8.jar            # referência vanilla, NÃO compilar junto (gitignore no release)
  src/core/                 # agnóstico de versão, linkado por todas
    include/minecpp/core/
      platform.h            # OS/CPU detect, export macros, cache-line, affinity
      version_api.h         # ABI C que toda versão implementa
      event.h               # event bus (hooks p/ futuro Paper)
      scheduler.h           # scheduler tick + async tasks
      thread_pool.h         # job system work-stealing
      mem.h                 # arenas, pools, buffers zero-copy
    src/
      platform.c
      event.c
      scheduler.c
      thread_pool.c
  versions/
    v1_8/                   # BASE LEGACY — implementação completa
      CMakeLists.txt        # lib minecpp_v1_8 + exe minecpp-1.8
      include/minecpp/v1_8/...
      src/version.cpp       # preenche version_api_t da 1.8
      src/main.cpp          # main do binário 1.8
      delta.md              # vazio (é a base)
    v1_9/                   # DELTA — linka v1_8, sobrescreve o que mudou
      CMakeLists.txt        # lib minecpp_v1_9 + exe minecpp-1.9
      src/version.cpp
      src/main.cpp
      delta.md
    v1_10/                  # idem, linka v1_9
      ...
```

Regra: `minecpp-1.9` linka `minecpp_v1_9 + minecpp_v1_8 + minecpp_core`.
`minecpp-1.10` linka `+ minecpp_v1_9 ...` em cascata. Nunca `#include` interno
de outra versão, só via `version_api_t` + símbolos `v1_8_*` explicitamente re-exportados.

---

## 3. `version_api_t` (ABI C estável)

`src/core/include/minecpp/core/version_api.h` define o contrato. Core nunca chama
código de versão diretamente, só via ponteiros. Exemplo resumido:

```c
typedef struct minecpp_version_api {
  int protocol_version;      // 47 = 1.8.x, 110 = 1.9.4, 210 = 1.10.x
  const char *version_name;  // "1.8.x"
  // blocos / chunks (MVP)
  int  (*block_get)(int x, int y, int z, const void *chunk);
  void (*chunk_serialize)(const void *chunk, minecpp_buffer_t *out);
  void (*chunk_deserialize)(void *chunk, const uint8_t *data, size_t len);
  // protocolo / net (pós-MVP)
  // tick / física (pós-MVP)
} minecpp_version_api_t;
```

Cada `versions/vX_Y/src/version.cpp` expõe:

```cpp
extern "C" const minecpp_version_api_t* minecpp_v1_8_api(void);
```

`v1_9` faz:

```cpp
extern "C" const minecpp_version_api_t* minecpp_v1_9_api(void) {
  static minecpp_version_api_t api = *minecpp_v1_8_api(); // herda tudo
  api.protocol_version = 110;
  api.chunk_serialize = v1_9_chunk_serialize; // só o que mudou
  return &api;
}
```

Isso é a "herança" — cópia de struct + override pontual. Explícito, sem vtable,
funciona em C, testável, `constexpr`-friendly.

---

## 4. Threading — o problema real e a solução

### 4.1 Sintoma medido

CPU 8c/8t @ 2.5 GHz, cache privado por core, single-thread fraco:

- parado: 20 TPS
- andando (survival): 15 TPS
- elytra: 7 TPS

Diagnóstico: não é lógica de jogo, é **chunk streaming no tick thread**.
Andar gera ~4–9 chunks/s. Elytra gera 30–80 chunks/s (view-distance 10 = 441
chunks ao redor, tudo invalida em segundos). No vanilla/Paper típico o tick faz:
load disco → parse NBT → generate → light → serialize → zlib → enqueue net,
tudo ou muito disso no caminho crítico de 50 ms. Single-thread fraco colapsa.

Elytra é o pior caso: rajada de I/O + CPU + compressão + banda ao mesmo tempo.

### 4.2 Princípios (CPU fraco em single, monstro em multi)

1. **Tick thread nunca bloqueia.** Só orquestra e faz commit serial. Orçamento
   fixo 50 ms. Tudo que pode demorar >1 ms vai para workers ou I/O async.
2. **Paralelismo por estágio, não por região.** Mantém semântica de mundo único
   (diferente do Folia). Ordem determinística via fase compute (paralela,
   read-only) + fase commit (serial, rápida).
3. **Cache privado por core = localidade é lei.** Chunk é unidade de
   ownership: um worker é dono de um chunk por vez, dados contíguos,
   structs quentes alinhadas em 64 B, sem false sharing, sem lock no hot path.
   Filas MPSC por worker + work-stealing só quando ocioso.
4. **Prioridade + prefetch direcional.** Elytra não se resolve só com threads,
   se resolve com prever para onde o player vai. Fila de chunks com 3 níveis
   + cone de prefetch na direção da velocidade.
5. **Degradação graciosa.** Se estourar 50 ms, corta carga (reduz view-distance
   efetiva, adia compressão de chunks distantes), nunca deixa TPS cair para 7.
   TPS é SLA, qualidade visual é variável.

### 4.3 Arquitetura de threads

```
[net-io 1..2] ──parse──> [tick 1] ──jobs──> [workers N = hc - 2] ──done──> [tick commit]
     ^                       |                         |                        |
     |                   event bus               chunk pipeline            scheduler
     └────────────────── send queue (comprimido pronto) ───────────────────┘
[disk-io async] <── load/store region .mca (epoll/IOCP, nunca na tick)
```

- **tick (1 thread, afinada, alta prioridade):** avança relógio 50 ms, drena
  pacotes parseados, monta job graph do tick, executa commit, drena send queue.
- **net-io (1–2 threads):** `recv` + varint parse + fila MPSC para tick.
  `send` consome buffers já comprimidos. Zero `malloc` no loop (arenas).
- **workers (N = hardware_concurrency − 2, mínimo 2):** chunk gen, lighting,
  NBT parse/serialize, zlib, física/entidades (fase compute). Work-stealing.
- **disk-io (async, não thread dedicada no Linux):** `io_uring`/`epoll` (Linux),
  `IOCP` (Windows), abstraído em `core/platform.h`. Tick nunca faz `read()`.

Na CPU 8/8: 1 tick + 1 net-io + 6 workers. Elytra escala em 6 workers em vez
de 1 tick.

### 4.4 Pipeline de chunks (o que salva a elytra)

```
request(prio) -> disk async -> parse NBT (worker) -> generate (worker)
  -> light (worker) -> serialize+zlib (worker) -> send queue -> net-io
```

- Deduplicação: mesmo chunk pedido 2x = 1 job (hash de coord).
- Prioridades: `P0` = raio imediato (player + 2), `P1` = cone de prefetch
  (direção × velocidade × 2 s à frente, elytra = cone longo e estreito),
  `P2` = resto do view-distance. P0 preempta P2.
- Orçamento por tick: ex. máx 8 compressões + 4 gerações por tick; resto fica
  na fila. Andar parece igual, elytra vê chunks distantes com LOD/borda em vez
  de travar TPS.
- Cancelamento: se player mudou de direção, jobs P2 fora do cone são
  cancelados antes de comprimir (economiza zlib, que é caro em 2.5 GHz).

### 4.5 Regras de código concorrente

- Nenhum `malloc`/`free` no tick ou no parse de pacote. Arenas por thread + pools.
- Nenhum mutex no hot path. MPSC lock-free tick←workers, SPSC workers→net.
- Dados de chunk: SoA onde for quente, `alignas(64)` em headers de fila/contador.
- Determinismo: workers nunca escrevem no estado vivo, escrevem em delta;
  tick aplica deltas em ordem fixa. Replay do tick = mesmos deltas.

Métricas que vamos expor desde o MVP (mesmo sem net): tempo por estágio
(disk/parse/gen/light/compress), profundidade de fila P0/P1/P2, ticks >50 ms,
chunks/s. Sem isso estamos cegos no hardware fraco.

---

## 5. MVP — Mundo + Chunks (1.8)

Escopo fechado, sem rede ainda:

1. `core/mem`, `core/platform`, `core/thread_pool`, `core/event` (stub funcional).
2. `NBT` em C: big-endian, parse/serialize, testes contra `level.dat` gerado pelo `server-1.8.jar`.
3. `region` em C: `.mca` (32×32 chunks, zlib deflate), load/store async-ready (API sync primeiro, assinatura já async).
4. `chunk 1.8`: 16×256×16, 16 sections de 16³, `Blocks` + `Add` + `Data` + `BlockLight` + `SkyLight`, `HeightMap`, `Biomes`.
5. Gerador flat (bedrock + dirt + grass) + tool `nbt-dump` / `region-dump` para diff contra vanilla.
6. Testes: round-trip NBT, round-trip region, chunk serialize gera bytes que o vanilla abre sem corrupt.

Fora do MVP: handshake/login/play, compressão de protocolo, criptografia,
entidades, física, inventário. Entram em `docs/protocol-1.8.md` depois.

---

## 6. Multiplataforma

- Nada de `epoll.h` / `windows.h` fora de `src/core/src/platform_*.c`.
  Todo o resto inclui só `minecpp/core/platform.h`.
- `Threads::Threads` via CMake. No Windows: Winsock2 init + `IOCP` backend;
  no Linux: `epoll`/`io_uring` backend. Mesma assinatura.
- `CMAKE_C_STANDARD 17`, `CMAKE_CXX_STANDARD 20`. Compila com GCC 13+, Clang 16+,
  MSVC 19.36+. Warnings como erro no CI (`/W4` + `-Wall -Wextra -Werror`).
- Fim de linha LF, UTF-8, nada de path com `\` hardcoded. Pastas de versão
  sempre `v1_8` (sem ponto) por causa de targets CMake e includes.

---

## 7. Hooks para futuro Paper (custo zero agora)

`core/event.h`:

```c
typedef enum { MINECPP_EV_TICK, MINECPP_EV_CHUNK_LOAD, MINECPP_EV_PACKET_IN, ... } minecpp_ev_t;
void minecpp_event_sub(minecpp_ev_t ev, minecpp_cb_t cb, void *ctx);
void minecpp_event_emit(minecpp_ev_t ev, const void *payload);
```

Sem subscriber = um `if (count==0) return`. Quando existir o bridge JNI, ele é
só mais um subscriber que sobe a JVM e repassa. Scheduler com `delay_ticks` +
`async_worker` já cobre 90% do que plugin Bukkit precisa (tarefas sync/async).

API nativa `minecpp_plugin.h` (C, `dlopen`/`LoadLibrary`) vem antes de qualquer
tentativa JNI. JNI só depois do servidor 1.8 estável com 20 TPS em elytra no
hardware fraco — senão repetimos o problema de performance com overhead de JVM.

---

## 8. Roadmap

Track detalhado com critérios de aceite em `docs/roadmap-1.8.md`
(1.8 com paridade 100% antes de qualquer 1.9).

- [x] Arquitetura + CMake esqueleto (este commit)
- [x] core: platform + mem + thread_pool + event (compila Linux+Windows, testes)
  - [x] thread_pool work-stealing real (sync.h POSIX+Win32, rings P0/P1/P2, stats)
  - [x] buf codec (VarInt/string/pos, algoritmos do `hd` vanilla)
  - [ ] mem arenas/pools (quando o hot path exigir)
  - [ ] net async (epoll/IOCP) + compressão + criptografia (pós-protocolo)
- [x] NBT em C (`src/core/nbt`, round-trip byte-idêntico no level.dat + chunk vanilla, ASan/UBSan limpo)
- [x] region `.mca` em C (framing+setores, vanilla-compat provada com o bfv original)
- [x] chunk 1.8 em C++ (Sections YZX, nibbles, Add, flat gen) + `minecpp-dump`
- [x] MVP mundo/chunks ACEITO: boot vanilla com 2 chunks 100% nossos, zero erro
- [ ] Benchmark chunk pipeline no hardware 8c/2.5GHz: meta 20 TPS andando, ≥18 elytra com degradação visual, nunca 7
- [x] Protocolo 1.8 — codec + pacotes (handshake/status/login/play, ChunkData 0x21
  byte-idêntico ao vanilla). Sockets/criptografia ficam p/ a camada net.
- [ ] v1_9 como delta (documentar `delta.md`, linkar `v1_8`)
- [ ] v1_10 idem
- [ ] Plugin API nativa → só então avaliar bridge Paper

Critério de pronto do MVP: gerar mundo flat, fechar, reabrir no `server-1.8.jar`
sem erro, e importar mundo vanilla sem corrupt. Mais teste de carga do pipeline.
