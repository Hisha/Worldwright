# Content Pipeline — Worldwright, mod-content-manager, Portalkeeper, AzerothCore

Status: design. Nothing here is implemented. Facts are cited to repository files; hypotheses are marked **H**.

## 1. Integration boundary

The mission constraints fix the boundary:
- **Worldwright** owns world editing, project files, validation and the authoring of declarative content.
- **mod-content-manager (MCM)** owns discovery, composition of DBCs with ID leases, cumulative MPQ build, hashing, activation and publication.
- **Portalkeeper** owns delivery of published artifacts to the client (`WowPatch`, `patch-4..9`, SHA-256).
- **AzerothCore** owns the server runtime and consumes server-side data (DBC, `maps`, `vmaps`, `mmaps`, world DB rows).

Worldwright never writes into a WoW client folder and never builds an MPQ. The file contract between Worldwright and MCM is an EPF (ZIP with `manifest.json`), the same contract that FrameForge already uses (`mod-content-manager/docs/FRAMEFORGE_WOWUI.md`: "FrameForge Export → module/content/WoWUI → module EPF builder → module.epf → existing content/*.epf discovery").

```
Worldwright project (outside client)
    │  export (Worldwright)
    ▼
export dir: manifest + world/ (WDT, WDL, ADT) + textures (BLP) + models (M2/WMO, optional) + dbc-declarations.json
    │  EPF builder (Worldwright tool; mirrors mod-native-instance-maps/tools/package.py)
    ▼
continent.epf  (ZIP, manifest.json first, deterministic)
    │  MCM discovery / install / build (existing lifecycle)
    ▼
cumulative Realm-Content-NNNNNN.mpq  →  STAGED → ACTIVE → PublishDirectory
    │  Portalkeeper WowPatch (patch-4..9, SHA-256 verified)
    ▼
client Data/patch-N.MPQ

Parallel server path (no MCM support today; see §5):
export dir → server artifacts: dbc/Map.dbc, dbc/AreaTable.dbc, dbc/Light.dbc → worldserver DataDir/dbc
built client MPQ → map_extractor (see §4.3) → maps/*.map → vmap4_extractor/assembler → vmaps/* → mmaps_generator → mmaps/*
world DB rows (game_tele, optional creatures/graveyards via MCM server rows) → worldserver
```

## 2. What exists today, and what each side already does

| Capability | Where | Status |
|---|---|---|
| EPF discovery, validation, install selection | MCM `ContentPackage.cpp`, `ContentPackageRegistry.cpp` | Works |
| Raw file entries (`type: "file"`) of any extension, path-safe | MCM `ContentPackage.cpp` (~985–1040) | Works. Schema 1 "currently supports raw files only" |
| Cumulative MPQ v1 build with StormLib | MCM `src/MpqBuilder.cpp:140` | Works |
| SHA-256, lifecycle STAGED/ACTIVE/SUPERSEDED, publication | MCM README "Build lifecycle", "Filesystem publication" | Works |
| DBC composition: Item, ItemExtendedCost, CurrencyTypes/Category, Spell | MCM `DbcDescriptor.cpp` | Works |
| DBC composition: DungeonMap, DungeonMapChunk, WorldMapArea, WorldMapTransforms, with durable ID leases | MCM `WorldMapDbcComposer.cpp`, README "Native world map DBCs" | Works (12340 only) |
| Map.dbc, AreaTable.dbc, Light.dbc, LoadingScreens.dbc composition | none | **Missing** |
| Map-ID allocation lease | none (leases exist for item/spell/currency/template IDs; `ContentResourceAllocator.cpp`) | **Missing** |
| Server DBC/map artifacts | none (MCM server support is DB rows, not files under `DataDir`) | **Missing** |
| Portalkeeper WowPatch delivery | Portalkeeper README 114–178 | Works (patch-4..9 only) |
| FrameForge export pattern | MCM `docs/FRAMEFORGE_WOWUI.md` | Works, precedent |

## 3. Proposed EPF extensions (backward-compatible)

All extensions follow the existing pattern: a new optional top-level field, schema bump only where needed, existing EPFs unchanged. Each is "declaration, not bytes", like `worldMaps[]`.

### 3.1 E1 — `mapDefinitions[]` (Map.dbc, 12340, 66 fields)

Declares the new map's client record. MCM composes `DBFilesClient/Map.dbc` append-only with a durable lease, exactly like `WorldMapArea`.

```json
"mapDefinitions": [{
  "mapId": "lease",
  "directory": "WorldwrightIsland",
  "name": {"enUS": "Ashgrove Isle"},
  "areaType": 0,
  "expansionId": 2,
  "areaTableId": "lease",
  "loadingScreen": 0,
  "minimapIconScale": 1.0,
  "maxPlayers": 0
}]
```

Rules:
- `mapId` is allocated by MCM as a `map` resource kind, with `first_build` provenance, like items and spells. It must not collide with the stock Map.dbc maximum (computed from the registered baseline) or with another package's lease.
- Field order and strings are written by the composer, not by Worldwright. `directory` must match ADT/WDT paths exactly (wowdev: ADT path uses Map.dbc `Directory`).
- Fields not mapped above (`MapDescriptionAlliance/Horde`, `CorpseMapID`, etc.) take the stock-row convention, written explicitly, not omitted.
- The 66-field layout is identical in AzerothCore (`DBCStructure.h:1360`) and in Noggit Cinder's WotLK writer (`DBC.h:62-95`), so the field map is cross-checked.

### 3.2 E2 — `areaTables[]` (AreaTable.dbc)

Required because `MCNK` area IDs in the ADTs must resolve to `AreaTable` rows for zone names, and because `Map.dbc` references `AreaTableID` (field 22). Composed with an area-ID lease. Each row: `areaId`, `parentAreaId`, `areaBit`, `flags`, `name`, `exploreLevel`, `ambient`, `zoneMusic`, etc. Layout to be taken from `DBCStructure.h` and verified against stock AreaTable.dbc (baseline required).

### 3.3 E3 — `lightRows[]` (Light.dbc, optional)

Only references stock `LightParams.dbc` IDs; never invents parameters (consistent with mod-native-instance-maps constraint "No invented transforms"). Cinder's current behaviour uses hard-coded stock IDs (12, 13, 10, 11/13, 4) (`MapCreationWizard.cpp:949-973`); that value is a hypothesis (H2) to test before adopting.

### 3.4 E4 — `terrain[]` (validated raw content)

ADT/WDT/WDL files are already expressible as raw `file` content. The missing piece is validation. Add a declaration so MCM can check them before build:

```json
"terrain": [{
  "map": "lease:mapDefinitions[0]",
  "wdt": "world/maps/WorldwrightIsland/WorldwrightIsland.wdt",
  "wdl": "world/maps/WorldwrightIsland/WorldwrightIsland.wdl",
  "tiles": [{"x": 31, "y": 31, "adt": "world/maps/WorldwrightIsland/WorldwrightIsland_31_31.adt"}],
  "wdtFlags": {"bigAlpha": true, "mccv": true}
}]
```

Validations MCM should enforce (new, all deterministic):
- every `tiles[]` entry exists in the EPF and in the manifest; the WDT MAIN table marks exactly those tiles as present (`map_index.cpp` writes MAIN; reading in `MapTile.cpp`);
- ADT file name equals `<Directory>_<x>_<y>.adt` (wowdev ADT/v18: "World/Maps/<InternalMapName>/<InternalMapName>_<BlockX>_<BlockY>.adt");
- each ADT has MVER=18, MHDR, 256 MCNK, MCVT of 145 floats per chunk;
- if WDT `adt_has_mccv` is set, every ADT has MCCV on every chunk (H5);
- MH2O/MFBO chunks consistent with MHDR offsets;
- all `MMDX`/`MWMO` names resolve to either an EPF file or a stock client file (existence check against the baseline listfile supplied to MCM, not checked by hash);
- sizes below the per-file limit that MCM already enforces for raw content (none currently; to be added with a generous cap).

No terrain bytes are interpreted for building; MCM only validates and then copies, as it does for raw files.

### 3.5 E5 — `models[]` and `textures[]` (no new mechanism)

BLP and M2/WMO files ship as raw `file` entries (already supported). Recommended additional validation: BLP header and DXT size, M2 version (WotLK), WMO version, and listfile-style path normalisation (`Interface\...` vs `Interface/...`, already handled by `ContentBuildPaths::Target` in MCM).

### 3.6 E6 — `worldMaps[]` (continent world-map entry)

For a continent, the `M` key world-map needs `WorldMapArea` rows. MCM's existing `worldMaps[].areas[]` composer covers this. The `dungeonMapId` reference is `-1`/`0` for non-instance areas, which MCM already accepts (`README.md` "Native world map DBCs", and `mod-native-instance-maps` "Known limitations"). Use it without changes.

### 3.7 Summary of required MCM changes

| ID | Change | Type | Backward compatible? |
|---|---|---|---|
| E1 | `mapDefinitions[]` + `map` lease kind | new optional field + allocator kind | Yes |
| E2 | `areaTables[]` + `areaTable` lease kind | new optional field + allocator kind | Yes |
| E3 | `lightRows[]` (stock refs only) | new optional field | Yes |
| E4 | `terrain[]` validator | new validation stage; raw copy unchanged | Yes |
| E5 | BLP/M2/WMO header checks | validation only | Yes |
| E6 | none (reuse `worldMaps[]`) | — | — |
| S1 | server artifact output (see §5) | new build output kind + parity manifest | Yes if additive |

## 4. Client-side constraints

### 4.1 Map ID and client loading (H1, to test)

Map ID must exist in the client's `Map.dbc` with `Directory` pointing at files present in the same cumulative MPQ or a lower-priority MPQ. Whether IDs above the stock maximum load is not established in this research (H1). The server side also has a ceiling only if the optional TC9 sidecar is enabled (`TC9Sidecar.h:26`, `MAX_MAP_ID 800`).

### 4.2 Lighting and sky (H2)

`Light.dbc` rows per map; stock IDs as currently used by Cinder. Not verified.

### 4.3 Patch slots and extractor visibility

- Portalkeeper's WowPatch mode allocates `patch-4.MPQ` to `patch-9.MPQ` (Portalkeeper README 135–145). Six slots are shared with all realm content. The MCM cumulative artifact is one slot.
- AzerothCore's `map_extractor` reads only `common, common-2, lichking, expansion, patch, patch-2, patch-3, patch-4, patch-5` (`src/tools/map_extractor/System.cpp:103-115`). **If Portalkeeper places the realm MPQ at `patch-6..9`, the stock extractor will not see the terrain** (H4). Options:
  - (a) Run the extractor against a local copy of the client with the artifact copied to `patch-5.MPQ` (only for extraction, never in production). Simple; uses only stock tooling.
  - (b) Extend `CONF_mpq_list` in a copy of the tool (not in the production module tree). Requires a fork of AzerothCore tooling; keep as fallback.
  - Chosen for the prototype: (a).

### 4.4 MPQ format

MCM builds `MPQ_CREATE_ARCHIVE_V1` (`MpqBuilder.cpp:140`). Version-1 archives have historical size and file-count limits. An island of ≤ 16 ADTs with textures should be far below those limits; measure in the prototype (H7).

### 4.5 Client files that must not be redistributed

Stock ADTs, textures and models are Blizzard copyright. Worldwright may reference stock paths. Any copy of stock assets into an EPF is redistribution and is out of scope. See LICENSING_REVIEW.md §6.

## 5. Server-side requirements (AzerothCore)

Confirmed by reading `/home/smithkt/aicode_folder/azerothcore-wotlk` at `8a4e76eac` (read-only):

| Requirement | Source | Consequence for pipeline |
|---|---|---|
| `Map.dbc` row with the same ID as the client | `MapMgr.cpp:82-84` (`ASSERT(entry)`) | Missing row = assert at map creation; server must receive the same `Map.dbc` (or a server-side version of E1). |
| `AreaTable.dbc` loaded as a required store | `DBCStores.cpp:276` (LOAD_DBC AreaTable) | E2 needed on server too. Per-area lookup behaviour not traced in this phase (H-area). |
| `Light.dbc` | `DBCStores.cpp:338` | E3 needed on server too. |
| `WorldMapArea.dbc` | `DBCStores.cpp:388` (server loads it) | Covered by `worldMaps[]` for the client; server copy must match. |
| DBCs under `DataDir/dbc/` | `DBCStores.cpp:269` (`dataPath + "dbc/"`) | S1: publish server DBC artifacts with parity hash. |
| `maps/MMMYYXX.map` terrain | `GridTerrainLoader.cpp:31`; missing file = debug log only (lines 36-45) | Must be generated from ADTs. Silent failure risk (H3). |
| `vmaps/` | `GridTerrainLoader.cpp:121-140`; toggled by `vmap.enableLOS`/`vmap.enableHeight` (`worldserver.conf.dist:1345-1353`) | Generate with `vmap4_extractor` + `vmap4_assembler`. |
| `mmaps/` | `mmaps_generator/MapBuilder.cpp:136-170` (maps discovered from `*.vmtree` and `*.vmtile`) | mmaps need vmaps first. Pathing optional: `mmap` settings in conf. |
| Teleport destination | `game_tele` table loaded at `ObjectMgr.cpp:9761` | World-DB row `game_tele` pointing at new map ID and coordinates. |
| Map entry for dungeon checks | `MapMgr.cpp:138-146` `PlayerCannotEnter` | Continents (non-instance) need no `instance_template`. |

### 5.1 Map extraction output naming

`map_extractor` writes `maps/<id 3 digits><y 2 digits><x 2 digits>.map` (`System.cpp` output format, `ExtractMapsFromMpq` at 976–1012). Worldwright must not name `.map` files itself. The extractor is the authority.

### 5.2 Server artifact channel (S1)

MCM today publishes only client MPQs. Server DBCs and generated terrain are not MCM outputs. Options:
- **S1-a (recommended for prototype):** Worldwright's export also writes `server/dbc/*.dbc` and a manifest with SHA-256 for each file. The operator copies them into `DataDir/dbc/` manually, and `worldserver` startup validates them. No MCM change.
- **S1-b (later):** MCM gains an additive "server data artifact" output with the same lifecycle and a `.content server-artifact status` command. Requires a design review in the MCM repository.

Neither option modifies existing AzerothCore modules or production environments during research.

## 6. Operational sequence (prototype target)

1. Worldwright exports the island project to `export/` with manifest, WDT/WDL/ADT, BLP, `dbc-declarations.json`, and server DBC candidates.
2. Worldwright EPF builder packages `export/` into `island.epf` (deterministic ZIP, manifest first, fixed timestamps — the mod-native-instance-maps convention in `tools/package.py`).
3. `.content scan` → `.content install <island-key>` → `.content build` → `.content activate <n>` in a **test** realm (never production).
4. Portalkeeper installs `Realm-Content-NNNNNN.mpq` as a WowPatch slot on a test client.
5. A copy of the client is used as the extractor input. The artifact is copied to `patch-5.MPQ` for extraction only (§4.3a). `map_extractor`, `vmap4_extractor`, `vmap4_assembler`, `mmaps_generator` run on the copy and write into a test `DataDir`.
6. Server DBCs from step 1 are copied into the test `DataDir/dbc/`.
7. Insert a test `game_tele` row for the island. Start `worldserver` against the test DB.
8. Teleport from the client (`.tele` or `.go`), check terrain, collision, water, textures, and area names.

## 7. Risks and unknowns specific to this pipeline

| ID | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| P1 | Map ID collision between client lease and server Map.dbc | Medium | Assert on map creation | Single source of truth for Map.dbc generated by Worldwright; parity hash on both sides |
| P2 | Extractor blind to patch-6..9 | High if unmanaged | Missing `.map`, no terrain collision | §4.3 options; document in operator runbook |
| P3 | Silent missing `.map` files | Medium | Players fall through or see no terrain | Pre-start check script counting `maps/*.map` vs ADT count |
| P4 | Lighting wrong with stock Light IDs | Medium | Visual only | H2 test; E3 optional |
| P5 | 8-bit heightmap input → coastline terracing | High | Visual quality | Roadmap R2 requires 16-bit input path |
| P6 | Portalkeeper six-slot limit shared with other content | Low–medium | Install failure | Single cumulative MCM artifact (already how MCM works) |
| P7 | Worldwright accidentally writes into a client folder | Low if designed out | Violates constraints | No client path in Worldwright configuration; tests assert no writes outside project and export dirs |
| P8 | Server DBC drift from client DBC | Medium | Runtime mismatch | Parity manifest (S1) |
