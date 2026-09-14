# vanilla 1.8 — ground truth (decompilado + mundo gerado)

> Nada aqui é chute. Tudo foi extraído do `server-1.8.jar` (release 1.8,
> 2014-09-01, protocolo 47, classes ofuscadas) via Vineflower 1.12.0
> e validado contra mundo gerado pelo próprio jar.
> Nossa implementação C precisa fazer round-trip byte-compatível com isso.

Reproduzir decompile:
```
unzip -o server-1.8.jar -d /tmp/mc-full -x 'com/*' 'org/*' 'io/*' 'gnu/*' 'javax/*' 'assets/*' 'META-INF/*'
java -jar vineflower-slim-1.12.0.jar /tmp/mc-full/<Classe>.class /tmp/out
```

Regenerar mundo de referência:
```
mkdir ref && cp server-1.8.jar ref/ && cd ref
echo "eula=true" > eula.txt
printf 'server-port=25566\nonline-mode=false\nlevel-type=FLAT\ngenerator-settings=2;7,2x3,2;1\nlevel-name=world\n' > server.properties
timeout 25 java -jar server-1.8.jar nogui
```

## 1. Mapa obf → real (MVP mundo/chunks)

| Classe | É | Evidência |
|---|---|---|
| `gd` | `NBTBase` | factory `createNewByType` com cases 0–11 |
| `ge` | `NBTPrimitive` (base numérica) | `extends gd`, `fu/fq/fs/... extends ge` |
| `fr/fm/gb/fu/fw/fs/fq/fl/gc/fv/fn/ft` | End/Byte/Short/Int/Long/Float/Double/ByteArray/String/List/Compound/IntArray | ids 0–11 no factory, ver §2 |
| `fz` | `CompressedStreamTools` | `read/write` com `GZIPInput/OutputStream`, `a(DataInput, fx)` |
| `fx`/`fy` | `NBTSizeTracker` (ilimitado + limitado) | `fx.a` = tracker infinito passado no read |
| `gg` | `JsonToNBT` (mojangson) | strings `Invalid tag encountered`, `Unbalanced brackets` |
| `bfv` | `RegionFile` | setores 4096, header 8192, `GZIPInputStream`, ver §3 |
| `bfx` | `RegionFileCache` | `"r." + (x>>5) + "." + (z>>5) + ".mca"`, `x & 31`, cache 256 |
| `bfy` | `AnvilChunkLoader` | `"Chunk file at x,z is missing level data"`, lê `Level`/`Sections`, ver §4 |
| `bft` | `OldChunkLoader` (conversor McRegion) | lê `Blocks/Data/...` flat no root (altura 128, 8 loops de 16) — só legado |
| `bqn`/`bqj` | `SaveFormatOld` / `AnvilSaveConverter` | `level.dat`, `DIM-1`/`DIM1`, versão `19133` |
| `bqo` | `WorldInfo` (dentro de `level.dat/Data`) | `SpawnX`, `GameType`, `generatorName` |
| `bfn` | `NibbleArray` | `new bfn(bytes, 7)` para Data/BlockLight/SkyLight |
| `hd` | `PacketBuffer` | `VarInt` + `DataInput` (rede, pós-MVP) |

## 2. NBT (base `gd`, big-endian, do factory + métodos `a(DataOutput)`/`a(DataInput)`)

| id | classe | payload exato |
|---|---|---|
| 0 | `fr` End | zero bytes |
| 1 | `fm` Byte | `writeByte/readByte` (1 byte) |
| 2 | `gb` Short | `writeShort` (2 bytes BE) |
| 3 | `fu` Int | `writeInt` (4 bytes BE) |
| 4 | `fw` Long | `writeLong` (8 bytes BE) |
| 5 | `fs` Float | `writeFloat` (4 bytes BE) |
| 6 | `fq` Double | `writeDouble` (8 bytes BE) |
| 7 | `fl` ByteArray | `writeInt(len)` + `write(bytes)`; read: `readInt` + `readFully` |
| 8 | `gc` String | `writeUTF` (u16 len + modified UTF-8) |
| 9 | `fv` List | `writeByte(elemId)` + `writeInt(len)` + elementos **sem** nome/tag |
| 10 | `fn` Compound | sequência `tag(id, nome, payload)` + `TAG_End(0)`; get: `b(key,10)` has-compound, `f`=getInt, `k`=getByteArray, `c`=getList, `n`=getBool, `g`=getLong, `d`=getByte, `l`=getIntArray, `m`=getCompound |
| 11 | `ft` IntArray | `writeInt(len)` + `len × writeInt` |

Arquivo (`fz`): root = `TAG_Compound` nome `""` (u16 len 0), tudo dentro de **gzip**.
`level.dat` validado: 1029 bytes ungzipped, root tag 10, nome vazio, contém `Data`.

## 3. Region `.mca` (classe `bfv`)

- Arquivo = `r.<rx>.<rz>.mca`, chunk `(cx,cz)` ∈ região `(cx>>5, cz>>5)`, slot `(cx&31, cz&31)`.
- Header 8192 bytes: `1024 × u32 BE` offsets `[0,4096)` + `1024 × u32 BE` timestamps `[4096,8192)`.
- Offset entry: `sector = v>>8`, `count = v&0xFF`; vazio = 0. Timestamp = u32 BE (epoch s).
- Chunk em `sector*4096`: `u32 BE len` + `u8 version` + `payload(len-1)`.
  `version`: 1 = gzip, 2 = **zlib** (vanilla 1.8 escreve 2). `len` conta version+pzload.
- `if (file.length < 4096)`, header incompleto → preenche; escrita sempre múltipla de 4096
  (`& 4095` padding com zero). Leitura valida `len > 4096*count` → null (chunk ausente).
- Cache (`bfx`): máx 256 arquivos abertos, depois fecha tudo (`a()`).

## 4. Chunk Anvil 1.8 (classe `bfy`, write + read confirmados)

Root: `TAG_Compound("", { Level: Compound, "V": Byte(1) })`.
`Level`:
- `xPos, zPos: Int` — `LastUpdate: Long` — `HeightMap: Int[256]` — `TerrainPopulated: Bool`
- `Sections: List<Compound>` — `Biomes: Byte[256]` — `Entities: List` (+ TileEntities/TileTicks no load)
- Load exige `Level` compound e `Sections` lista (`is missing level data, skipping` senão).

Section (só salva se não-vazia):
- `Y: Byte(y>>4 & 0xFF)` — `Blocks: Byte[4096]` — `Data: Byte[2048]` (nibbles via `bfn`)
- `Add: Byte[2048]` **só se presente** (ids > 255) — `BlockLight: Byte[2048]`
- `SkyLight: Byte[2048]` (ou zerado com mesmo length se dimensão sem céu)

## 6. Descobertas de implementação (fn HashMap + nibbles + índices)

- `fn` (Compound) = `Map c = Maps.newHashMap()` → **ordem das chaves no arquivo
  é hash-order, NÃO ordem de escrita**. Comparar NBT vanilla com
  `minecpp_nbt_equal` (sem ordem); nossa escrita usa ordem canônica do save.
- `V:Byte(1)` fica DENTRO de `Level` (primeiro no save); root = só `{Level}`.
- Slot de item: NBT ausente = **1 byte `0x00`** (hd.a((fn)null)); presente =
  NBT root completo (hd.h lê com budget 2MB). Sem prefixo u16!
- S25 BreakAnim: `varint eid + pos + BYTE stage`; stage é **contador
  crescente** (observado 0..17), **`255` (-1) = remove**. Não é 0-9!
- C07 dig: `varint status + pos + byte face`; statuses 0,1,2 =
  START,ABORT,STOP (enum mm); survival acumula dano por tick, FINISH colhe.
- C08 place: `pos + byte dir + slot + cursor×3`; usa o held DO SERVIDOR
  (pacote divergente = resync SetSlot); **exige movimento prévio**
  (hasMoved) e alcance < 8 blocos (dist² < 64).
- `bff` (ChunkNibbleArray, 2048 B): `idx = y<<8|z<<4|x`, byte `idx>>1`,
  nibble baixo se `idx` par. Mesmo índice linear dos `Blocks[4096]`
  (`x=idx&15, y=idx>>8&15, z=idx>>4&15`).
- id 12 bits = `Blocks[i] | Add<<8`; meta = Data nibble.
- HeightMap/Biomes: índice `x + z*16`. Ordem de save do Level: V, xPos, zPos,
  LastUpdate, HeightMap, TerrainPopulated, LightPopulated, InhabitedTime,
  Sections[{Y,Blocks,Data,Add?,BlockLight,SkyLight}], Biomes, Entities,
  TileEntities, TileTicks? (só se houver). `Add` só se algum id > 255.

## 7. Referência em `test-data/vanilla-1.8-flat/`

Gerado pelo jar com `level-type=FLAT` (bedrock+2 dirt+grass), parado logo após `Done`:
- `level.dat` (636 B, sha256 `1f27b5…3bd352d`) — parseado: root 10, tem `Data`/`generatorName=flat`.
- `region/r.-1.-1.mca` (16384 B = 4 setores, sha256 `ccd5c9…a0fe6e`) — 2 chunks;
  slot testado: `sector=3, len=256, version=2(zlib)` → NBT root 10 com `Level`+`Sections`+`xPos`.

Critério de pronto do MVP mundo/chunks: nosso C lê esses 2 arquivos e reescreve
byte-idêntico (tirando timestamps), e o `server-1.8.jar` abre nosso output sem log de erro.

Prova executada (region): nossa lib escreveu `r.0.0.mca` com o chunk real no slot
(5,7); o `bfv` (RegionFile original do jar) abriu e devolveu 11786 bytes de NBT
válido (`root=10`, `Level`, `Sections`, `xPos=-1/zPos=-12`). Escrita compatível
confirmada com o próprio código vanilla.

## 8. Aceite MVP (boot vanilla com chunks 100% nossos)

`r.-1.-1.mca` reescrito só com nossa stack (flat gen + region write), spawn
apontado para (-8,4,-184), boot `nogui`: `Done` sem `Exception`, sem
`Chunk file at`, sem `wrong location`. O vanilla manteve nossos 2 chunks
intactos (mesmo payload/timestamp) e gerou 310 vizinhos ao redor (312 total).

## 9. Protocolo (hd PacketBuffer + pacotes reais capturados)

- `hd.e()/f()` VarInt/VarLong: 7 bits LE/byte, MSB continua, limite 5/10 bytes
  (`>5/>10` = "too big"). `hd.b(int/long)` escrita com `>>>`.
- `hd.c(max)`: VarInt nbytes, teto `nbytes <= max*4`, rejeita negativo, UTF-8.
  Escrita: teto 32767 bytes.
- `dt` (BlockPos) u64: x:26 (bits 38..63), y:12 (26..37), z:26 (0..25), signed.
  Encode `(x&2^26-1)<<38|(y&2^12-1)<<26|(z&2^26-1)`; decode com shifts
  aritméticos (`x = (long)v>>38`, `y = v<<26>>52`, `z = v<<38>>38`).
- ChunkData 0x21 (dos bytes reais, não do decompile): id + int x,z + bool
  continuous + u16BE mask + varint dataLen + data; por section (bit asc):
  4096×**u16LE(id<<4|meta)** + 2048 blocklight + 2048 skylight; +256 biomes
  se continuous. Seção do flat: 12288 + 256 = 12544 bytes.
- Goldens em `test-data/vanilla-1.8-flat/net/` (conteúdo do frame sem len):
  status_response (JSON 1.8/proto 47), pong, joingame (eid 74, flat),
  spawnpos (285,4,645), poslook, keepalive, chunkdata (27,30,mask 1).
  Capturados via socket contra o jar (handshake→status→login offline).
- Nosso ChunkData encodado é byte-idêntico ao do vanilla (memcmp em teste).
- Nosso handshake encodado foi aceito pelo jar (respondeu JSON de status).

## 10. Net jogável (aceite com cliente falando protocolo 47)

- Framing: `varint(len) + varint(id) + payload`; com compressão:
  `varint(len) + varint(dataLen) + (zlib se dataLen>0, cru se 0)`.
  Payload < threshold vai cru com `dataLen=0` (observado: 45B LoginSuccess).
- Ordem do login offline: Handshake → LoginStart → SetCompression(256) →
  LoginSuccess → JoinGame → TimeUpdate → SpawnPosition → PlayerPosLook →
  burst de ChunkData (view-dist 4 = 81 colunas).
- Cliente que esquece `dataLen` pós-compressão recebe close por frame
  corrupto (comportamento correto, igual ao vanilla).
- KeepAlive do server a cada 10s, kick em 30s sem resposta.
- Criptografia no fio (RSA/AES) implementada e testada, mas só usada em
  online-mode (deferred: falta auth HTTPS com a Mojang).
