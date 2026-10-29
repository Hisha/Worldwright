# Worldwright Phase 0 — Research Findings

Status: research only. No Worldwright implementation has been started.
Companion documents: `ARCHITECTURE_COMPARISON.md`, `LICENSING_REVIEW.md`, `CONTENT_PIPELINE.md`, `IMPLEMENTATION_ROADMAP.md`.

## 1. Method and evidence base

| Source | How obtained | Revision inspected |
|---|---|---|
| noggit-cinder | `git clone --recursive` of github.com/pkuzic/noggit-cinder | `62010c5` (2026-10-12) |
| Noggit_Gold | `git clone --recursive` of github.com/AbyssalRealm/Noggit_Gold | `6566804` (2026-08-28) |
| noggit3 | `git clone --recursive` of github.com/wowdev/noggit3 | `59e58ad` (2026-04-13) |
| noggit-red | `git clone` of gitlab.com/wtxpwh/noggit-red | `4bec8c4` (2024-06-07) |
| mod-content-manager | local checkout `/home/smithkt/git/mod-content-manager` | HEAD `e9927be` |
| Portalkeeper | local `/home/smithkt/git/Portalkeeper` | local HEAD |
| mod-native-instance-maps | local `/home/smithkt/git/mod-native-instance-maps` | local HEAD |
| AzerothCore | local `/home/smithkt/aicode_folder/azerothcore-wotlk` (read-only) | `8a4e76eac` (2026-10-16) |
| WoW formats | wowdev.wiki (ADT/v18, WDT, WDL/v18), GitHub (WoWDBDefs, wow-listfile) | fetched 2026-10-29 |

Clones live outside the repo in `/home/smithkt/ww-research/` (copies of `/tmp` were abandoned because `/tmp` is a 16 GB tmpfs that hit its quota). No candidate source was modified.

Build evidence (the only build attempted):
- **noggit3 builds on Linux** (Ubuntu 26.04, GCC 15.2, CMake 4.2.3, Qt5 system packages). `cmake -S noggit3 -B build-noggit3 -DCMAKE_BUILD_TYPE=Release` configured in 7.4 s; `cmake --build` completed with exit 0 and produced `bin/noggit` (4.2 MB). 132 compiler warnings, 0 errors. Logs: `/home/smithkt/ww-research/logs/noggit3-*.log`. The binary was not launched or exercised against a client.
- **noggit-cinder, Noggit_Gold and noggit-red were not built.** Their Linux status is therefore taken from source (see §3.1).

Not verified by running: any editor UI behaviour, any client load, any AzerothCore map load. These are called out as hypotheses in §6.

## 2. Candidate snapshot

| Attribute | noggit-cinder | Noggit_Gold | noggit3 | noggit-red |
|---|---|---|---|---|
| Target | 3.3.5a **and** 1.12 (Vanilla/Turtle) | 3.3.5a | 3.3.5a (`src/noggit/ui/About.cpp:30`: "a wow map editor for 3.3.5a") | 3.3.5a + Lich King era |
| License | GPL-3.0 (`COPYING`) | GPL-3.0 (`COPYING`) | GPL-3.0 (`COPYING`) | GPL-3.0 (`COPYING`) |
| Commits in history | 34 (root is `7707675 build baseline`, empty) | 22 (no Red ancestry) | 219 | 916 |
| Last commit | 2026-10-12 | 2026-08-28 | 2026-04-13 | 2024-06-07 |
| C++ source (`src/`, approx., excl. `external`) | 395 `.cpp` in `src/noggit`, ~141k lines incl. headers | 393 `.cpp`, ~127k lines | 97 `.cpp` in `src/noggit`, ~54k lines | ~102k lines |
| Qt | Qt5 widgets (123 files) | Qt5 (121 files) | Qt5 (68 files) | Qt5 (115 files) |
| Rendering | OpenGL 4.1 core required (`src/opengl/context.cpp:24`), GLSL in `rendering/glsl/` | same lineage | OpenGL 4.1 core required (`src/opengl/context.cpp:157`), shader programs | same lineage |
| Scripting | Lua (1 file references `lua_State`) | Lua (1 file) | none in `src/` | Lua (1 file) |
| Unit tests | none in noggit tree | none | `test/math/*.cpp` (3 files) | none |
| Map creation wizard | yes, `ui/tools/MapCreationWizard` (1341 lines) | yes (1290 lines) | **no** (0 hits for MapCreation/NewMap/CreateMap) | yes (903 lines) |
| Map.dbc write | yes (WotLK and Vanilla layouts) | yes (WotLK only) | read-only (`DBC.h:32`) | yes |
| Undo/redo | `ActionManager` stack + `ActionHistoryNavigator` | yes (same lineage) | none equivalent (no `ActionManager`) | yes |
| Autosave / crash recovery | none found (0 hits in `src/noggit`) | none found | none found | none found |
| Submodules | populated | populated | none | **uninitialized** (`-` prefix in `git submodule status`) |

LOC is approximate (line counts of `.cpp`/`.hpp`/`.h` under `src/`, excluding `src/external`). The directions are reliable; exact numbers are not important.

## 3. Findings by objective

### 3.1 Architecture

**Layering.** All four are monolithic Qt5 applications. UI, world editing and file IO are not separated into libraries:
- The UI lives in `src/noggit/ui/**` (Qt widgets, `.ui` files, dock panels). Tools are in `src/noggit/ui/tools/**` and also in `src/noggit/tools/*.cpp`.
- World state is in `src/noggit/World.*`, `MapTile.*`, `MapChunk.*`, `map_index.*` (WDT/ADT index and save logic is inside `map_index.cpp`, not in a separate library).
- Client file access goes through the vendored `src/external/blizzard-archive-library` (`ClientData`, `MPQArchive`, `CASCArchive`) and `blizzard-database-library` (DBC reader).

**Global state coupling.** Counted with grep over `src/noggit`:

| Metric | noggit-cinder | Noggit_Gold | noggit3 |
|---|---|---|---|
| `extern` global declarations in headers | 20 | 20 | 12 |
| `instance()` call/definition sites | 118 | 115 | 14 |
| Files referencing `ClientData` | 27 | 25 | 0 |

Cinder and Gold are heavily tied to a process-wide client folder model (`NoggitApplication::instance()->clientData()`, `Noggit::Project::CurrentProject::get()`, globals such as `gMapDB` defined in `DBC.h:405`). noggit3 has almost none of this and reads client files through its own `MPQ.cpp`/`DBC.cpp`.

**Project model.** Cinder/Gold/Red treat the "project" as a folder inside or beside the WoW client. `ApplicationProject.h` tracks `ProjectPath`, `ProjectVersion`, and a record of MPQ patches exported to the client (`ApplicationProject.h:101`). A Worldwright project format would be new in every candidate.

**Patch output into the client.** Cinder's "Save Project to Client MPQ" (`ui/windows/noggitWindow/NoggitWindow.cpp:785`) builds a `patch-X.mpq` inside the client's `Data/` folder via `ClientData::tryCreateMPQArchive` (`blizzard-archive-library/src/ClientData.cpp:127`), which writes with `SFileCreateFile` (`MPQArchive.cpp:83`). Gold and Red inherit the same design. This is the competing publishing path Worldwright is forbidden to introduce.

**Linux build.**
- noggit3: builds (see §1). It fetches StormLib 9.30, FastNoise2, sol2, lodepng, nlohmann/json through FetchContent (`noggit3/CMakeLists.txt:717-748`).
- noggit-cinder: `CMakeLists.txt:306` has an explicit `if (UNIX)` branch (BZip2, StormLib). Its README says Linux is supported with Ubuntu packages; FetchContent Linux support is documented as "coming in the future" (README:65). Not built here.
- Noggit_Gold: `INSTALL.md` documents only Windows / Visual Studio 2022 builds; CMake has `UNIX` branches (`CMakeLists.txt:310`). Not built here.
- noggit-red: README:18 says Linux/Mac support for FetchContent is "coming in the future"; submodules are not initialized in the clone. Not built here.

**Format capabilities (from source, cross-checked against wowdev).**

| Format | noggit-cinder | Noggit_Gold | noggit3 | noggit-red | Evidence |
|---|---|---|---|---|---|
| ADT read | yes | yes | yes | yes | `MapTile.cpp:140-335` (cinder) |
| ADT write | yes; MH2O, MFBO, MCNK, MDDF/MODF | same | yes (MH2O 12 files) | yes | `MapTile.cpp:586-935` (cinder); `MapTile.cpp:912-916` writes `Water.saveToFile` |
| MCLQ (vanilla liquid) write | yes (`save_using_mclq_liquids`) | partial | read | partial | `MapTile.cpp:578-590` |
| WDT write | yes (`map_index.cpp:227` `MapIndex::save`) | yes | read | yes | `map_index.cpp:227-300` |
| WDL write | empty WDL only (`map_index.cpp:1374` `create_empty_wdl`) | same | **none** | partial | `map_index.cpp:1374` |
| WDL generation from ADT | minimap/LOD BLP export only | same | no | no | `rendering/WorldRender.cpp:2620-2650` |
| DBC read | yes (`DBCFile.cpp`, `DBC.h`) | yes | yes (`DBC.cpp`) | yes | |
| DBC write | yes, Map/Light/others | yes | no | yes | `MapCreationWizard.cpp:976-1027` |
| M2 read / render | yes | yes | yes | yes | `rendering/ModelRender.cpp` |
| M2 write | **no** | no | no | no | grep found no M2 writer |
| WMO read / render | yes | yes | yes | yes | `rendering/WMORender.cpp` |
| WMO write | **no** (placement only) | no | no | no | |
| BLP read | yes (via archive lib, `ImageBrowser`) | yes | yes | yes | |
| BLP write | yes, only via PNG2BLP for minimap/LOD export and ocean textures | same | yes (3 files) | yes | `rendering/WorldRender.cpp:2649` |
| MPQ read | yes (StormLib via archive lib) | yes | yes (StormLib) | yes | `blizzard-archive-library/src/MPQArchive.cpp` |
| MPQ write | yes, into client `Data/` | yes | no | yes | §3.5 |
| CASC read | yes (CascLib) | yes | no | yes | `CASCArchive.cpp` |

**Heightmap precision.** `MapChunk::setHeightmapImage` (`MapChunk.cpp:2045-2075`) reads each pixel with `qGray(image.pixel(x, y))` and divides by 255. Heights are therefore quantized to 256 levels per import. For a map height range of several hundred yards this gives steps of roughly 1–2 yards per import, which is visible on coastlines. 16-bit import is not supported.

**Technical debt visible in-repo.**
- `todo_1.4.md` (identical in all three) lists: alpha-layer corruption during ground editing, no ADT unloading, WMO culling gaps, hole-line artifacts between ADTs, and a crash when a clipboard model's source is deleted.
- `uid_fix_concept.md` describes a UID repair pass that the author states "does not work with parallel work by multiple editors on same map".
- Cinder's `uid_fix_concept.md` and `VANILLA-1.12-SUPPORT.md` show a large amount of vanilla-specific branching (`if (is_vanilla)` in the wizard, `save_using_mclq_liquids` in `MapTile::save`). Worldwright would carry this as dead weight.

### 3.2 New continent creation (highest priority)

Only Cinder, Gold and Red can create maps; noggit3 cannot. Cinder's flow is representative (Gold's is near-identical; the diff shows only the vanilla branch differs).

**Map ID allocation.** `DBCFile::getEmptyRecordID` (`DBCFile.cpp:303-314`) returns `max(existing ID) + 1` over the loaded Map.dbc. It is not checked against server-side Map.dbc, against other packages, or against stock client IDs outside the loaded file. The new map ID is therefore client-local and unreserved.

**Map.dbc record.** `MapCreationWizard::saveCurrentEntry` (`MapCreationWizard.cpp:976-1027`) writes:
- `InternalName` (field 1) = directory name; default `New_Map`, incremented if taken (`addNewMap`, lines 1084-1094).
- `AreaType`, `Flags` (16 = sort-by-size category), `Name` (loc), `AreaTableID` (field 22, user-typed integer, not created), `MapDescriptionAlliance/Horde`, `LoadingScreen`, `minimapIconScale`, `corpseMapID/X/Y`, `TimeOfDayOverride`, `ExpansionID`, `RaidOffset`, `NumberOfPlayers`.
- Field indexes match AzerothCore's `MapEntry` (`src/server/shared/DataStores/DBCStructure.h:1360-1390`): `linked_zone` is field 22, `expansionID` is 63, `maxPlayers` is 65. The 66-field WotLK layout is consistent between client-side Cinder and server-side AzerothCore.
- **Not written:** `MapDifficulty.dbc` (comment at line 945 "TODO save mapdifficulty.dbc"), `AreaTable.dbc` rows, `WorldMapArea.dbc`, `LoadingScreens.dbc`.

**Light.dbc.** `MapCreationWizard.cpp:949-973` adds one global Light row for the new map with `LightDB::Map = _cur_map_id` and hard-coded `LightParams` IDs `12, 13, 10, 13 (or 11 for vanilla), 4`, copied from "eastern kingdoms". These IDs reference the stock `LightParams.dbc`; they are not authored. This is a hypothesis that needs checking in the client.

**Terrain and WDT.** Creating a new map:
1. Writes a zero-byte `World\Maps\<Dir>\<Dir>.wdt` (`MapCreationWizard.cpp:904-919`).
2. Creates `World\Maps\<Dir>\<Dir>.wdt` content via `mapIndex.save()` (line 936). `MapIndex::save` (`map_index.cpp:227`) writes MVER, MPHD (flags from `mBigAlpha`, `mHasAGlobalWMO`, `_sort_models_by_size_class`), MAIN (64×64 entries), and optional MWMO/MODF for a global WMO.
3. Writes an empty WDL (`create_empty_wdl`, `map_index.cpp:1374`).
4. Writes a Map.dbc row and a Light.dbc row, then saves.

Tile creation: tiles are created when the user selects squares on the minimap-style grid. Each new tile is `MapTile` with 256 `MapChunk`s built with `nullptr` source data (`MapTile.cpp:1149-1155`), i.e. **flat zero-height terrain with default texture**. ADT files are written by `MapTile::save` (`MapTile.cpp:586`). Writing an empty map (no tiles) prompts a warning (`MapCreationWizard.cpp:886-902`) but is allowed.

**Terrain initialization / heightmap import.** Per-tile and per-chunk import from a grayscale image exists:
- `MapTile::getHeightmapImage` exports a 257×257 image (`MapTile.cpp:1157`).
- `MapTile::setHeightmapImage` / `MapChunk::setHeightmapImage` import (`MapChunk.cpp:2045`), with Set/Add/Subtract modes. Input is 8-bit effective (qGray). Node editor nodes `TileSetHeightmapImage`, `ChunkSetHeightmapImage` expose it (`ui/tools/NodeEditor/Nodes/World/...`).
- No "import whole continent heightmap and split into ADTs" workflow was found. The user would have to split and import tile by tile.

**Multi-tile editing and navigation.** Noggit has a tile-grid minimap widget (`MapView`, `_minimap_widget`), tile selection, and per-tile load/unload handling. Unloading is listed as a missing feature in `todo_1.4.md` ("There is no unloading of ADTs"). Navigation of a 64×64 world is therefore memory-bound.

**Water.** MH2O is written from `Water` objects (`MapTile.cpp:912-916`). Water tool exists (Gold README "Water Tool cursor placement"). Cinder `ui/Water.cpp` provides liquid type selection. No "ocean/sea level" global setting was found; sea level must be painted per chunk.

**Textures.** Texture layers are assigned from client MPQ textures chosen in the texture browser (`ui/texture_swapper.cpp`, `ImageBrowser`). Gold adds Auto Texture (`Noggit_Gold/README.md` features list). Alpha maps are written via MCAL (`MTEX`/`MCAL` in 5 files each).

**Objects and lighting.** MDDF/MODF are written (`MapTile.cpp:818-822` and MODF equivalent). Model/WMO browsing uses a listfile (`ui/tools/AssetBrowser`, `dist/listfile` in Gold). Lighting is edited through `Sky.cpp` and `LightTool.cpp`, which edit `Light.dbc`/`LightParams.dbc` rows.

**Saving and reopening.** Save = WDT + WDL + ADTs + DBCs, written in place into the client-side project path. Reopening works because the new map is in `Map.dbc` of the client. Multiple-edit safety relies on `uid_fix_concept.md`.

**Client compatibility and in-game loading.** Not verified in this phase. Hypotheses to test are in §6.

**Summary of what works vs. what is missing for continent creation:**

| Need | Status in Cinder/Gold | Gap |
|---|---|---|
| New map name, Map.dbc row | Works (WotLK) | ID not reserved across packages; no AreaTable/LoadingScreen/MapDifficulty creation |
| Empty WDT/WDL | Works | No continent-scale WDL from a heightmap |
| Flat ADT creation for chosen tiles | Works | Needs per-tile UI; no batch "island from mask" |
| Heightmap import | Works per tile, 8-bit | No 16-bit, no whole-continent split |
| Coastline sculpting | Terrain tools exist | No dedicated coastline/water-level tool |
| Textures / water / objects | Exist | Workflow is per-chunk, not drag-and-drop |
| Save/reopen | Works in place | No standalone project format; writes to client folder |
| Export to publishable content | Only MPQ into client `Data/` | Must be replaced by EPF export (see CONTENT_PIPELINE.md) |

### 3.3 User experience

Evaluated from source and feature lists, not from usability testing.

- **Undo/redo:** Cinder has a real action stack (`ActionManager.hpp`, `ActionHistoryNavigator`, 127 call sites). Gold has the same lineage. noggit3 has no equivalent. Worldwright should keep the action-stack idea but design it around tile/chunk diffs (see roadmap).
- **Autosave / project recovery:** none found in any candidate. Only the ADT Porter writes backups (`AdtPorterDialog.cpp:859`).
- **Drag-and-drop:** `dropEvent`/`dragEnterEvent` exist in 4–6 files per candidate; object placement is primarily via selection and brush tools.
- **Searchable asset browsers:** present (`ui/tools/AssetBrowser`, `ImageBrowser`, `ZoneIDBrowser`, filter proxies in ~23–31 UI files). These are the strongest reusable UX assets.
- **Error messages:** Cinder's 1.12 linter (Assist → "Check loaded tiles for 1.12 issues") and "missing object placeholder" are good examples. Many errors are still `QMessageBox::warning` with technical text (e.g., `NoggitWindow.cpp:815` "MPQ Name is not allowed").
- **Manual file work:** the workflow requires the user to know the client folder layout, MPQ naming (`patch-[4-9].MPQ`), and to run the patch export manually.
- **Recommendations** are in the roadmap: continent wizard driven by an image/mask, coastline tool, one-click validate-and-export, project format outside the client, recovery journal, and a single asset browser shared by textures/models/WMOs.

### 3.4 Licensing (summary; detail in LICENSING_REVIEW.md)

- All four noggit repositories are GPL-3.0 at the root. Cinder's README says it is ported from Noggit Azure, "a sibling GPL3 Noggit Red fork" (`README.md:18`), so Cinder's brushes/stamps/detail-doodad code has a second upstream.
- Vendored third-party code includes GPL-3 (libimagequant), libpng, zlib, MIT (StormLib-related in other repos, tsl), LGPL/Qt-tech-preview header (qtgradienteditor), and a blizzard-archive-library whose README is titled "DBCD-cpp" (copy-paste from another project).
- **Shipped data with unclear terms:** Gold tracks a 94.7 MB `dist/listfile/listfile.csv` (1,429,123 lines) as plain files; the upstream `wowdev/wow-listfile` repository has no LICENSE file. Both Cinder and Gold track 996 WoWDBDefs `.dbd` files, which upstream license as CC BY-SA 4.0 (Data) — share-alike and attribution apply.
- Cinder's auto-updater downloads and executes binaries from `https://updater.everwood.gg/launcher-updates/noggit-cinder/...` (`ui/windows/updater/Updater.cpp:41`). This is a third-party distribution channel that Worldwright must not inherit.

### 3.5 mod-content-manager integration (summary; detail in CONTENT_PIPELINE.md)

- EPF = ZIP with `manifest.json` (`src/ContentPackage.cpp`). Schema 1 "currently supports raw files only" (`ContentPackage.cpp:~1000`); each `content` entry is `{type:"file", source, target}` with `IsSafeRelativePath` checks. **Any file type is accepted at the path level**, so ADT/WDT/WDL/BLP/M2/WMO files can already be shipped as raw content without new code (validation of their internal correctness does not exist).
- DBC composition exists only for these 12340 tables (`src/DbcDescriptor.cpp`): Item, ItemExtendedCost, CurrencyTypes, CurrencyCategory, Spell, DungeonMap, DungeonMapChunk, WorldMapArea, WorldMapTransforms. **Map.dbc, AreaTable.dbc, Light.dbc and LoadingScreens.dbc are not supported.**
- Allocation leases (`ContentAllocationRegistry`, `ContentResourceAllocator`) exist for Item, Spell, CurrencyType, creature/gameobject templates. There is no map-ID lease.
- Output is an MPQ v1 archive (`MpqBuilder.cpp:140`, `MPQ_CREATE_ARCHIVE_V1`), hashed, staged, activated and published to `PublishDirectory` (README "Filesystem publication").
- Portalkeeper delivers the published artifact as a `WowPatch` slot (`patch-4.MPQ` to `patch-9.MPQ`) with SHA-256 verification (`Portalkeeper/README.md`, "Dynamically allocated WoW MPQs").
- The FrameForge WoWUI export (`docs/FRAMEFORGE_WOWUI.md`) is the working precedent for "external editor exports a directory, module builds EPF, MCM discovers it" — this is the pattern Worldwright should follow.
- The `mod-native-instance-maps` repository states as hard constraints: append-only composition, fixed IDs, and "No DBCs in packages" (`README.md`, constraint 6). Worldwright must respect the same boundary: Worldwright emits declarative data; MCM composes DBCs.

### 3.6 AzerothCore integration (summary; detail in CONTENT_PIPELINE.md)

Confirmed in the AzerothCore tree at `8a4e76eac`:
- Terrain collision/height is read from `DataDir/maps/MMMYYXX.map` (`src/server/game/Grids/GridTerrainLoader.cpp:31`). If the file is missing, the grid simply has no terrain data and this is logged only at debug level (lines 36-45).
- vmaps are read from `DataDir/vmaps/` and mmaps are built from `vmtree`/`vmtile` files (`tools/mmaps_generator/MapBuilder.cpp:136-170`). Both can be disabled with `vmap.enableLOS`/`vmap.enableHeight` and mmap pathing (`worldserver.conf.dist:1338-1353`).
- Server DBCs are loaded from `DataDir/dbc/` (`game/DataStores/DBCStores.cpp:269`). `Map.dbc`, `AreaTable.dbc`, `Light.dbc`, `WorldMapArea.dbc` are loaded with `LOAD_DBC` (lines 276-388). Missing or incompatible required DBCs are collected and reported at startup (lines 636-647).
- `MapMgr::CreateBaseMap` asserts the Map.dbc entry exists (`game/Maps/MapMgr.cpp:82-84`, `ASSERT(entry)`). A teleport or DB reference to an unknown map ID is therefore fatal, not a graceful error.
- The extractor reads `DBFilesClient\Map.dbc` for the Directory column, and extracts `World\Maps\<Dir>\<Dir>.wdt` and `<Dir>_<x>_<y>.adt` from a fixed MPQ list (`tools/map_extractor/System.cpp:103-115, 274-296, 976-1012`). The fixed list is `common, common-2, lichking, expansion, patch, patch-2..patch-5`. **Patch 6–9 are not read**, so a content MPQ placed in `patch-6..9` would not be seen by the stock extractor.
- The extractor writes `maps/<id>(3 digits)<y><x>.map`, not `<x><y>` (`System.cpp` output format). Naming must be produced by the tool, not by hand.
- `TC9Sidecar.h:26` defines `MAX_MAP_ID 800`. Only relevant if that sidecar is enabled.

### 3.7 Map and format references

wowdev.wiki (fetched 2026-10-29):
- ADT: map is 64×64 blocks of 16×16 chunks; file `World/Maps/<InternalMapName>/<InternalMapName>_<BlockX>_<BlockY>.adt`; `InternalMapName` is `Map.dbc` `Directory`. MCVT holds 145 floats per chunk (9×9 + 8×8). MH2O replaces MCLQ on WotLK; MFBO requires MHDR flag 1.
- WDT: MPHD flags include `adt_has_mccv` (0x2, every ADT must have MCCV), `adt_has_big_alpha` (0x4, MCAL 4096). MAIN has 4096 entries.
- WDL/v18 (3.3.5.12340): MARE per tile = 17×17 + 16×16 int16 heights; MAHO per tile = 16 uint16 hole masks; MAOF offset table. Client draws distant mountains from WDL.
- WoWDBDefs (`wowdev/WoWDBDefs`): CC BY-SA 4.0 for `definitions/`, BSD-3 for code.

## 4. Required fixes or unknowns in the candidates (short list)

1. Cinder/Gold: auto-updater, client-folder MPQ writes, vanilla branching, squashed history.
2. Gold: Windows-only documented build; 94.7 MB unlicensed listfile in-tree; history discontinuity from Red.
3. noggit3: no map creation, no WDL writer, no undo, no autosave, 132 warnings on a fresh Linux build (reproducible, recorded).
4. noggit-red: stale (2024), submodules not initialized in clone; Linux build undocumented.
5. Everywhere: no tile unloading, 8-bit heightmap import, UID repair race under multi-editor use.

## 5. Source index (file / symbol references)

Noggit (paths relative to each repo's root):
- `src/noggit/ui/tools/MapCreationWizard/Ui/MapCreationWizard.cpp` — `addNewMap` (1073), `saveCurrentEntry` (863), Light row (949), Map.dbc write (976).
- `src/noggit/DBC.h` — `MapDB` fields (62-95), `LightDB`, `gMapDB` (405).
- `src/noggit/DBCFile.cpp` — `getEmptyRecordID` (303).
- `src/noggit/map_index.cpp` — `save` (227), `saveTile` (511), `saveChanged` (537), `create_empty_wdl` (1374), `saveMaxUID` (1145), `addGlobalWmo` (1309).
- `src/noggit/MapTile.cpp` — ADT read (140-335), `initEmptyChunks` (1149), `save` (586), MH2O write (912), `getHeightmapImage` (1157).
- `src/noggit/MapChunk.cpp` — `setHeightmapImage` (2045), `getHeightmapImage` (2001).
- `src/noggit/ActionManager.hpp` / `.cpp` — undo stack.
- `src/noggit/rendering/WorldRender.cpp` — minimap/LOD BLP export (2620-2650).
- `src/noggit/ui/windows/noggitWindow/NoggitWindow.cpp` — client MPQ export (706-960).
- `src/noggit/ui/windows/updater/Updater.cpp` — update server URL (41).
- `src/external/blizzard-archive-library/src/ClientData.cpp` — `tryCreateMPQArchive` (127).
- `src/external/PNG2BLP/libimagequant/*.c` — GPL-3 header (blur.c 6-18).
- `src/opengl/context.cpp:24` (cinder), `:157` (noggit3) — OpenGL 4.1 core requirement.

noggit3:
- `src/noggit/DBC.h:32` (Map.dbc read); `src/noggit/MapHeaders.h:68-70` (WotLK MFBO/MH2O/MTXF comments); `src/noggit/ui/About.cpp:30`.
- `CMakeLists.txt:717-748` — FetchContent for StormLib, json, lodepng.

mod-content-manager (`/home/smithkt/git/mod-content-manager`):
- `src/ContentPackage.cpp` — EPF validation, raw `file` content (~985-1040), `IsSafeRelativePath` (48).
- `src/DbcDescriptor.cpp` — 12340 descriptors (Item, Spell, DungeonMap, WorldMapArea…).
- `src/WorldMapDbcComposer.cpp`, `src/ContentAllocationRegistry.cpp`, `src/MpqBuilder.cpp:140`.
- `README.md` — commands, build lifecycle, filesystem publication (261-346), DBC inspection (360), world-map DBCs (644-700).
- `docs/FRAMEFORGE_WOWUI.md` — precedent for external-editor export.

AzerothCore (`/home/smithkt/aicode_folder/azerothcore-wotlk`, read-only):
- `src/server/game/Grids/GridTerrainLoader.cpp` (20-48, 50-70, 94-120).
- `src/server/game/DataStores/DBCStores.cpp` (217-276, 269, 342, 636-647).
- `src/server/game/DataStores` — `DBCStructure.h:1360` `MapEntry`.
- `src/server/game/Maps/MapMgr.cpp` (76-90, 134-146).
- `src/server/apps/worldserver/worldserver.conf.dist` (185 DataDir, 1338-1353 vmap/mmap switches).
- `src/tools/map_extractor/System.cpp` (103-115, 274-296, 319-323, 976-1012).
- `src/tools/mmaps_generator/MapBuilder.cpp` (136-170).
- `src/server/game/TC9Sidecar/TC9Sidecar.h:26`.
- `LICENSE` (GPL-2.0); 15 source files carry AGPL headers, 1699 carry GPL-2 text.

Portalkeeper: `README.md` "Realm patches" and "Dynamically allocated WoW MPQs" (114-178); MIT (`LICENSE`).
mod-native-instance-maps: `README.md` "Hard constraints" (334-375), "Known limitations" (472-508); MIT.

## 6. Hypotheses requiring experiment (not confirmed)

| # | Hypothesis | How to test |
|---|---|---|
| H1 | A new map ID above the stock maximum (e.g. ≥ 1000) loads on the 3.3.5a client if Map.dbc, WDT and ADTs are present in a patch MPQ. | Minimal Map.dbc row + empty WDT in patch MPQ; attempt `.go` teleport. |
| H2 | Light.dbc entries copied from stock IDs (12,13,10,11/13,4) yield acceptable lighting. | Load the island in client; visually compare. |
| H3 | Server requires `maps/*.map` files for the new ID to avoid silent terrain loss; missing files produce no error at info level. | Run worldserver with and without `.map` for the test map; observe `GetHeight` behaviour. |
| H4 | `map_extractor` must be pointed at a custom MPQ list (or loose files) to see patch-6+ content. | Place ADTs in patch-6; run extractor unmodified; confirm absence; re-run with added name. |
| H5 | MCCV must be present in every ADT if `adt_has_mccv` is set in WDT MPHD. | Toggle the flag in a test WDT, observe render. |
| H6 | 8-bit heightmap quantization is visible as terracing on coastlines. | Render a 16-bit source vs. 8-bit path. |
| H7 | MCM v1 MPQ (`MPQ_CREATE_ARCHIVE_V1`) size limit (~4 GB) and file-count limit are not binding for a small island (≤ 16 ADTs). | Measure a built island MPQ. |

## 7. Open questions for the owner

1. License direction for Worldwright (GPL-3 vs MIT/Apache). This decides whether noggit code can be copied or only referenced (see LICENSING_REVIEW.md §5).
2. Target client: 3.3.5a build 12340 only (assumed throughout).
3. Whether Worldwright may write server-side `dbc/`, `maps/`, `vmaps/`, `mmaps/` into the AzerothCore DataDir, or must only emit artifacts for the operator to deploy.
4. Acceptable terrain precision (8-bit vs 16-bit input) and maximum map size for milestone 1.
