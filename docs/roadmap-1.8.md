# Roadmap 1.8 — paridade 100% com o `server.jar`

> Decisão travada: **nada de 1.9 antes da 1.8 cobrir 100% do `server.jar` 1.8.**
> `versions/v1_9+` está estacionado até lá.
> Detalhamento técnico (formatos, classes mapeadas) em `docs/vanilla-1.8.md`;
> arquitetura (threads, ABI, layout) em `docs/architecture.md`.

## Meta final (critério binário)

Cliente **vanilla 1.8 de verdade** conecta no `minecpp-1.8`, cria mundo novo e
joga **survival indistinguível do jar**: spawn, minera, crafta, sobrevive à
noite, Nether/End, mata o Ender Dragon. Sem isso, a fase não está pronta.

Padrão de prova (vale pra toda fase): bytes no fio + comportamento comparados
com o jar; `ctest` verde em Release e ASan; sem regressão nas fases anteriores.

## Feito (não reabrir sem motivo)

- [x] NBT 12 tags (round-trip byte-idêntico no `level.dat` real)
- [x] Region `.mca` (leitura/escrita aceita pelo `RegionFile` original)
- [x] Chunk 1.8 (Sections YZX, nibbles, Add, flat; boot vanilla com chunks nossos)
- [x] Codec (VarInt/string/pos dos algoritmos do `hd`/`dt`)
- [x] Pacotes handshake/status/login + play básico (ChunkData byte-idêntico)
- [x] Net: listener/poller, framing, compressão, tick 20TPS, login→play,
      streaming de chunks, chat, keepalive (aceite com cliente protocolo 47)
- [x] Thread pool work-stealing, MPSC, crypto (RSA/AES/SHA1), UUID offline

## Fase 1 — Net completa (play inteiro)

Escopo: todos os pacotes play que o cliente precisa (~100): entidades
spawn/movimento/teleporte, inventário (click/drag/creative), partículas,
sons, efeitos, scoreboard, tablist, plugins channel, resource pack,
assinatura de estatísticas, janela (crafting/furnace/villager), mapa.
Entrada: o que já temos. Saída: cliente vanilla loga, vê mundo/entidades
placeholder, abre inventário/creative sem erro no log dele (`--verbose` limpo
dos dois lados).

Progresso (tabela completa em `docs/protocol-1.8.md`):
- [x] P0: login espelha o vanilla byte a byte (brand, difficulty, abilities,
  helditem, statistics, playerlist ×2, border, windowitems, setslot, bulk
  0x26 decodado e testado; goldens em `net/goldens/`)
- [x] P1a janelas/inventário: ClickWindow 7 modos (merge, shift, número, drag
  esq/dir, double-click), Confirm + SetSlot reativo, cursor (-1,-1), HeldItem
  SB, ArmAnimation broadcast, creative ignorado em survival.
  Limites honestos: slot 0 (craft) travado sem receitas, armor sem checagem
  de tipo, drops rejeitados (sem perda) até entidades de item, maxStack 64
  (registry na Fase 4)
- [x] P1b blocos (C07 dig com timers confiados + C08 place com held validado,
  drops direto pro inventário, BreakAnim/Effect/Sound reativos, World
  server-side com evicção; provado contra o jar: dig survival + place)
- [ ] P1c entidades (spawn/movimento/metadata)
- [ ] P2: paridade fina

## Fase 2 — Worldgen (sem arquivo existente)

Escopo: Overworld (biomas, relevo, cavernas, estruturas, vilas, lagos/lava),
Nether, End; seed igual ao vanilla (`Random` Java reimplementado bit a bit);
`level.dat` escrito por nós abre no jar.
Saída: apagar `world/` e o servidor gera tudo sozinho; diff de chunks gerados
vs jar com mesma seed é idêntico (blocos + biomes + HeightMap).

## Fase 3 — Física + movimento autoritativo

Escopo: AABB/colisão, gravidade, água/lava (fluxo e nado), queda, explosões
(TNT/creeper com drop correto), fogo se espalhando/apagando, sufocamento,
knockback; reconciliação com movimento do cliente (anti-fly básico).
Saída: player anda/nada/cai/explode igual ao vanilla, tick estável com
elytra no hardware fraco (meta: nunca abaixo de 18 TPS com degradação visual).

## Fase 4 — Blocos (~200 comportamentos)

Escopo: cada bloco com tick/update/entidade associada: redstone completa
(fio, tocha, repetidor, comparador, pistões, funis, droppers, portas,
trilhos, TNT acendendo), plantações/crescimento, fornalha, água/lava,
areia/cascalho, gelo/neve, obsidian/portal, camas, bigornas, beacon,
command block (executa comandos da Fase 7).
Saída: bateria de contraptions (porta AND de pistão, farm automática, relógio
redstone) comporta igual ao jar, tick a tick.

## Fase 5 — Entidades (~30 mobs + veículos + projéteis)

Escopo: spawn/despawn por doca de chunks, IA (wander, follow, pathfinding A*,
alvo, ataque melee/distância, creeper, enderman, golem, Wither, Dragon),
projéteis (flecha/bola de fogo/poção), minecarts/barcos, XP orbs, drops,
frames/pinturas, TNT acesa, areia caindo.
Saída: noite de survival com ondas de mobs indistinguível; Dragon fight
completa a mecânica (cristais, cura, perches).

## Fase 6 — Sistemas de gameplay

Escopo: vida/fome/saturação/XP/níveis, combate (cooldown? não — 1.8 é
spam-click com cálculo vanilla), armadura/encantos, poções/efeitos,
clima (chuva/trovão), ciclo dia/noite + dormida, vilas (portas, golems,
trades), conquistas/estatísticas, fome/morte/respawn, game rules.
Saída: zerar o jogo (Wither + Dragon + conquistas pipocando).

## Fase 7 — Admin e persistência total

Escopo: ~30 comandos vanilla (gamemode, give, tp, time, weather, gamerule,
difficulty, effect, summon, setblock, testfor...), ops/bans/whitelist,
playerdata NBT completo, scoreboard.dat, villages.dat, mapas, RCON/query,
server.properties completo, logs/auditoria.
Saída: operador administra tudo sem tocar no jar; restart preserva 100%.

## Estacionado (pós-paridade, nesta ordem)

1. `v1_9` como delta sobre `v1_8` (protocolo 110, combate, End novo, elytra)
2. `v1_10` como delta sobre `v1_9`
3. online-mode (auth HTTPS Mojang; RSA/AES já prontos)
4. API nativa de plugins → bridge Paper/JNI

## Regras do track

- Fase só fecha com cliente vanilla real (python cobre regressão, não fecha fase).
- Nada de 1.9: qualquer tentação de "aproveitar e fazer multi" volta pra cá.
- Performance é critério em toda fase (20 TPS no hardware fraco é SLA).
- Descoberta nova do jar → `docs/vanilla-1.8.md` primeiro, código depois.
