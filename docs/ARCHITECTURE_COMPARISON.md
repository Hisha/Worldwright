# Architecture Comparison — Fork Cinder, Fork Gold, or Start Independently

Evidence references point to `WORLDWRIGHT_RESEARCH.md` and to files inspected in the clones under `/home/smithkt/ww-research/`.

## 1. Options

- **A. Fork noggit-cinder** (`pkuzic/noggit-cinder`, `62010c5`).
- **B. Fork Noggit_Gold** (`AbyssalRealm/Noggit_Gold`, `6566804`).
- **C. Build Worldwright independently**, using documented formats (wowdev.wiki) and selectively reusing license-compatible libraries. noggit3 is treated as an optional reference or code source.

A fork of **noggit3** (upstream 3.3.5a) was evaluated as a sub-option of A/B because it is the cleanest Noggit base. It is discussed under C.

## 2. Scoring criteria and weights

Each criterion is scored 1 (poor) to 5 (good) for the option as a starting point. Weights reflect the mission: user-friendliness, continent creation, mod-content-manager boundary, and independence.

| Criterion | Weight | Why it matters |
|---|---|---|
| Continent creation foundation | 5 | Highest-priority feature |
| Fit with mod-content-manager boundary (no in-client MPQ writes) | 5 | Hard constraint |
| Project model independent of client folder | 4 | Required for save/reopen and export |
| Modern UX ability (undo, recovery, drag-drop, search) | 4 | Primary requirement |
| Code health / technical debt | 4 | Long-term maintainability |
| Licensing clarity and cleanliness | 4 | Constraint; public repository |
| Linux build support (verified) | 3 | Constraint |
| Independence from upstream decisions | 4 | Constraint |
| Development effort to MVP | 3 | Planning |
| Performance / renderer maturity | 3 | Existing renderers are mature |

## 3. Scores

| Criterion (weight) | A. Fork Cinder | B. Fork Gold | C. Independent (noggit3 as reference) |
|---|---|---|---|
| Continent creation foundation (5) | 3 — wizard + Map.dbc + WDT/WDL exist, but ID allocation and AreaTable/Light/MapDifficulty are ad hoc | 3 — same as Cinder minus vanilla branch | 4 — designed around map IDs as leases and EPF composition from day one |
| MCM boundary (5) | 1 — in-client MPQ writer, auto-updater, `ClientData` throughout | 2 — same in-client writer design inherited | 5 — no writer; EPF export from project |
| Project model (4) | 2 — "project" = client folder | 2 — same | 5 — defined by Worldwright |
| UX ability (4) | 3 — ActionManager undo, asset browser; no recovery | 3 — same, plus auto-texture and saved chunks | 4 — can be designed in; cost is building the base |
| Code health (4) | 2 — 20 globals, 118 `instance()` sites, vanilla branching, updater | 2 — same coupling; 22-commit history | 4 — fresh base; only chosen modules imported |
| Licensing (4) | 3 — GPL-3 root, but second upstream (Noggit Azure), updater, unlicensed data | 3 — GPL-3 root, unlicensed 94 MB listfile, history discontinuity | 4 — choice is open; reuse controlled |
| Linux build (3) | 3 — UNIX branch exists; not built here | 2 — Windows-first docs; not built here | 4 — stack chosen with Linux as first target |
| Independence (4) | 2 — depends on a single third-party fork and its updater | 2 — depends on a single fork with thin history | 5 — owns roadmap |
| Effort to MVP (3) | 4 — features exist | 4 — features exist | 2 — must build rendering, model/WMO loader, UI |
| Performance / renderer (3) | 4 — mature GL 4.1 renderer | 4 — same | 3 — new renderer risk, mitigated by reusing noggit3 shaders/logic as reference |

Weighted totals (out of 5.0, rounded):

| | A | B | C |
|---|---|---|---|
| Weighted score (sum of weight × score ÷ 39) | **2.6** | **2.6** | **4.1** |

The effort criterion is the only one where forking wins clearly. It is outweighed by the hard constraints (MCM boundary, no in-client publishing, project model) that forks would have to undo everywhere.

## 4. Code-level evidence behind the scores

### 4.1 Client folder is the project (blocks A and B)

- `MapCreationWizard.cpp:904-919` writes the new WDT directly into `project_path / world/maps/<name>` where `project_path` is the Noggit project, which is a client folder. `MapIndex::save` (`map_index.cpp:227`) writes into the same tree.
- `NoggitWindow.cpp:785-960` "Save Project to Client MPQ" creates `patch-X.mpq` in the client's `Data/` through `ClientData::tryCreateMPQArchive` (`ClientData.cpp:127`). That path violates "no competing client patch distribution mechanism".
- `ApplicationProject.h:101-138` tracks exported MPQ records inside the editor's project object, coupling publication state to editing state.

Making Cinder/Gold follow the MCM boundary means deleting the export feature, its UI (`NoggitWindow.cpp` ~700-960), its archive-writing path (`ClientData` write functions), and the project model. That is most of the publishing layer.

### 4.2 Coupling to globals and singletons (blocks A and B on maintainability)

Counted over `src/noggit`:
- Cinder: 20 `extern` globals, 118 `instance()` sites, 27 files touch `ClientData`.
- Gold: 20, 115, 25.
- noggit3: 12, 14, 0.

`gMapDB` is defined in `DBC.h:405` and used across the wizard, the minimap widget and the DBC layer. Any new project system in a fork would have to route around these singletons or keep them.

### 4.3 Vanilla branching and debt (penalises A)

- Cinder's wizard has `is_vanilla` branches for Map.dbc layout (`MapCreationWizard.cpp:951-1025`), a 42-field vanilla layout in `DBC.h:89-95`, and `save_using_mclq_liquids` in `MapTile::save` (`MapTile.cpp:578-590`).
- Cinder's `VANILLA-1.12-SUPPORT.md` documents a "stability overhaul" of parsers for vanilla data.

Worldwright targets 3.3.5a build 12340 only (research §7). Keeping vanilla support means maintaining two sets of format writers for no benefit.

### 4.4 History and provenance (penalises A and B)

- Cinder root commit `7707675 build baseline` is empty; history is a re-published snapshot. Cinder's own README says it is ported from Noggit Azure (`README.md:18`).
- Gold's history does not contain Red's last commit: `git merge-base --is-ancestor 4bec8c4 HEAD` fails in Gold. Gold's README describes a lineage (Noggit → Red → Gold) that its Git graph does not show. Attribution must be rebuilt from the README rather than from history.
- noggit-red's submodules are uninitialized in the clone (`git submodule status` shows `-` for all six). Red is also the most stale (2024-06-07).

### 4.5 Linux support (verified for noggit3 only)

- noggit3: configured and built on Ubuntu 26.04 with GCC 15.2, Qt5 system packages, CMake 4.2.3. Exit 0, 132 warnings, 0 errors. `bin/noggit` produced.
- Cinder: `CMakeLists.txt:306` has UNIX branch; Linux is documented with `apt install` line (README:147-149), but FetchContent is "coming in the future" for Linux (README:65). Not built.
- Gold: Windows-first docs; UNIX branch exists (`CMakeLists.txt:310`). Not built.
- Red: Linux FetchContent "coming in the future" (README:18).

The noggit3 build demonstrates the Linux path is feasible. It does not prove the editor runs; that is an open item.

### 4.6 Renderer maturity (favours A/B, and reusable by C)

- All four require OpenGL 4.1 core (`opengl/context.cpp`). Shaders live in `rendering/glsl/` (cinder: `wmo_frag.glsl`, `terrain_frag.glsl`) and the GL wrapper library in `src/opengl/` (`shader.hpp`, `texture.hpp`, `scoped.hpp`).
- noggit3's renderer is the cleanest (12 files use `opengl::program`; no `glBegin` immediate-mode calls). Cinder/Gold/Red have one `glBegin` file each.
- Reusing renderer code is a licensing question (see LICENSING_REVIEW.md §5), not a technical blocker.

### 4.7 Undo and UX (favours A/B)

Cinder/Gold have `ActionManager` with 127 call sites (`ActionManager.cpp`, `MapView.cpp`, tools). noggit3 has none. An independent build would need its own undo design. Recommendation: design undo around tile/chunk diffs rather than copying the action objects, because the existing action classes capture editor state and are tightly bound to `MapView`.

## 5. Option C: how it would be built

Rather than a rewrite of all of Noggit, C takes a layered approach:

| Layer | Source | Notes |
|---|---|---|
| Project model, map-ID leases, EPF export | New code | Owns the mission-critical features |
| WDT/WDL/ADT/MH2O/MCNK read and write | New code, validated against wowdev and noggit3 behaviour | Writers are small and well-specified; write tests with round-trip and extractor checks |
| DBC read/write (Map, AreaTable, Light, LoadingScreens, MapDifficulty) | New code, or the MIT/BSD-style reader pattern from `blizzard-database-library` if license-compatible | Only a handful of tables needed |
| MPQ read | StormLib (MIT) via FetchContent, as noggit3 does | Read-only in the editor |
| BLP read/write | PNG2BLP (GPL-3 copy, or re-implement the DXT path) | Decision in LICENSING_REVIEW.md |
| Rendering | New Qt/OpenGL renderer; noggit3 renderer as behavioural reference | Largest risk; see roadmap R3 |
| M2/WMO loading | noggit3 loader as reference | Reuse depends on license decision |
| UI shell | Qt5 or Qt6 (decision in roadmap) | Dock/tool panels designed fresh around recovery and continent wizard |
| Undo | New action-stack design on tile/chunk diffs | |
| Validation and export | Calls MCM build tooling via EPF contract | No MPQ writing |

## 6. Recommendation

**Option C: start Worldwright independently**, with noggit3 as the primary reference and an optional code source, and with no fork of Cinder or Gold.

Reasons, in order of weight:

1. **Hard constraints are violated by the forks at their core.** Cinder and Gold write MPQs into the client (`NoggitWindow.cpp:785-960`), ship an auto-updater that runs third-party binaries (`Updater.cpp:41`), and model the project as a client folder. Removing these is not a cleanup; it is replacing the publishing and project layers.
2. **Continent creation needs new domain logic in any case.** Map IDs must be leased and composed through mod-content-manager (no map lease exists there today), AreaTable/Light/LoadingScreen/MapDifficulty rows must be authored, and the server requires matching `Map.dbc`/`AreaTable.dbc`. The Cinder wizard covers a fraction of this (research §3.2).
3. **Debt would be inherited.** 20 globals, 115–118 singleton sites, vanilla branching, 8-bit heightmap import, and tile-unload gaps are all in Cinder/Gold.
4. **Independence.** Gold has a 22-commit public history with no Red ancestry; Cinder has a single-maintainer fork with an external update server. Both would make Worldwright dependent on upstream decisions the constraints forbid.

When Option A or B would win: if the goal were a vanilla-plus-3.3.5 editor, or a quick 3.3.5 patch editor that keeps the client-folder workflow. Neither is the Worldwright mission.

Why not fork noggit3 instead? It is the cleanest base (no updater, no MPQ writes, low singleton count, Linux build verified) and would be the right choice for an editor that only needs the existing feature set. Its costs for Worldwright are: no map creation, no WDL writer, no undo, no recovery, and a Qt5 widget codebase that is not designed around a project. Those are the features Worldwright most needs. Forking it would still require building the project and export layers, so the fork saves only the renderer and format readers. Those can be re-used from noggit3 under GPL-3 if the license decision allows it (see LICENSING_REVIEW.md §5 decision gate D1).

## 7. Risks specific to the recommendation

| Risk | Impact | Mitigation |
|---|---|---|
| Rendering from scratch takes longer than planned | Milestone slips | Start rendering spike with noggit3 as reference; roadmap R3 gate |
| Reusing noggit3 code forces GPL-3 on Worldwright | License mismatch with MCM (MIT) | Make D1 decision before importing any code; prefer re-implementation from wowdev docs |
| Parser correctness on 3.3.5 ADTs | Corrupt terrain on load | Round-trip tests against noggit3 and AzerothCore map_extractor output |
| Qt5 vs Qt6 | Future porting cost | Decide in roadmap P0; Qt5 is what noggit3 builds against on Linux here |
