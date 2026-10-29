# Implementation Roadmap

Status: planning only. Implementation starts after owner approval of this roadmap and of decisions D1–D5 below. Effort figures are rough engineering estimates for one or two developers, not commitments.

## 1. Recommendation being planned

Option C from `ARCHITECTURE_COMPARISON.md`: an independent Worldwright codebase, with noggit3 used as a behavioural reference and, only if decision D1 allows, as a code source for specific modules. No fork of Cinder or Gold.

## 2. Decisions needed before Phase 1

| ID | Decision | Recommendation | Why it blocks |
|---|---|---|---|
| D1 | Worldwright license and reuse policy (`LICENSING_REVIEW.md` §5.3) | MIT/Apache-2.0 core, clean-room, noggit as reference only | Determines whether any noggit code can be imported |
| D2 | UI toolkit: Qt5 or Qt6 | Qt6 (LGPL) for new code; check that noggit3 Qt5 reference builds still help | Affects every UI phase; Qt5 is what noggit3 builds against here |
| D3 | Server artifact channel (`CONTENT_PIPELINE.md` §5.2) | S1-a (manual copy with parity hash) for prototype | Determines the prototype's server steps |
| D4 | Target client | 3.3.5a build 12340 only | Format writers and DBC layouts depend on it |
| D5 | Heightmap input precision | 16-bit grayscale (PNG16 or raw R16) required; 8-bit allowed as preview only | Coastline quality; avoids the 8-bit path in `MapChunk.cpp:2045` |

## 3. Phases

### Phase 0 — Research (this document set)

Done: clones of four candidates at known revisions; noggit3 Linux build verified (exit 0, 132 warnings); MCM, Portalkeeper, mod-native-instance-maps and AzerothCore read; wowdev formats fetched.

Exit criteria (met): five reports written with citations; recommendation stated; decisions D1–D5 listed.

### Phase 1 — Format core (no UI)

Scope:
- ADT (MCNK, MCVT, MCNR, MCLY, MCAL with big alpha, MCSH optional, MH2O, MFBO, MCCV), WDT (MPHD, MAIN, MAID optional), WDL (MARE, MAHO, MAOF) reader and writer.
- DBC reader/writer for Map, AreaTable, Light, LoadingScreens, MapDifficulty (12340 layouts).
- Heightmap import (16-bit) and export into MCVT.
- Test harness with round-trip tests, deterministic output, and byte-level comparison against noggit3 output for the same input where layouts overlap.

Acceptance criteria:
- A 3×3-tile island written by the library parses back identically (structure and values) in the library's reader.
- WDT MAIN marks exactly the 9 tiles. Each ADT has 256 MCNK with correct offsets (verified by an independent parser: noggit3 `MapTile` loader, built from the Phase 0 checkout).
- With a test client copy (provided by owner), `map_extractor` run on a copy with the artifact in `patch-5.MPQ` produces `maps/*.map` for the island and no error on the other maps (§CONTENT_PIPELINE 4.3).
- `vmap4_extractor`/`vmap4_assembler`/`mmaps_generator` run without error on the island (vmap/mmap generation is hypothesis H-vmap until run).

Estimate: 4–6 weeks.

Gate G1: if the written ADTs are not accepted by the extractor (or the independent parser), stop and fix before any UI work.

### Phase 2 — Project model, export, EPF

Scope:
- Project format outside any client directory (versioned JSON + binary sidecars). Autosave journal and crash recovery in this phase (not later), because project integrity underpins export.
- Export pipeline: project → export dir → deterministic EPF (manifest first, fixed timestamps; the `mod-native-instance-maps/tools/package.py` convention).
- Declarations for E1–E4 (`CONTENT_PIPELINE.md` §3) in the export manifest.
- Path-safety tests: no write outside project/export dirs (addresses P7).

Acceptance criteria:
- `island.epf` passes MCM `.content scan` with no error on a test realm, with E1/E2 extensions either implemented in MCM or stubbed with a clear diagnostic (the MCM change is a separate, reviewed change in that repository).
- Two identical exports produce byte-identical EPFs (SHA-256 equal).
- `.content build` produces a STAGED MPQ containing all island files and their SHA-256 matches the EPF contents.

Estimate: 3–4 weeks, plus MCM extension work of 2–3 weeks in the MCM repository (separate review).

### Phase 3 — Renderer spike

Scope:
- Render a loaded set of ADTs with the Phase 1 data: height, normals, MCLY textures with MCAL alpha, MCCV vertex colours, MH2O water surfaces. One OpenGL core (4.1) context, one shader set.
- Reference: noggit3 `rendering/` and `src/noggit/MapChunk.cpp` behaviour (not code, under D1-b).

Acceptance criteria:
- 9 tiles of the island visually match the same tiles rendered by noggit3 (screenshot comparison on the same data, tolerance defined by owner).
- Frame time on the owner's hardware measured and recorded; the target is set by owner in D6 (not yet decided).

Estimate: 4–6 weeks. This is the largest technical risk (R3).

Gate G2: if the renderer spike cannot match noggit3 on 9 tiles within the agreed time, re-evaluate D1 (allow noggit3 renderer code under GPL-3, option D1-a or D1-c).

### Phase 4 — Editor MVP

Scope (ordered by mission priority):
1. **Continent creation wizard:** choose name, map ID (reserved as a local placeholder; final ID from MCM lease at export), grid size, and a 16-bit heightmap or mask image. Output: a new project with ADTs.
2. **Coastline sculpting:** raise/lower brush plus a sea-level plane and a "land mask" tool that sets water and terrain from one image.
3. **Texture painting:** brush painting of MCLY layers with an asset browser (searchable, backed by a user-supplied listfile, not shipped).
4. **Object placement:** drag-and-drop from the asset browser onto the terrain; MDDF/MODF writer (new code; Cinder's writers are reference only).
5. **Undo/redo:** action stack on tile/chunk diffs (not copies of noggit action classes).
6. **Errors:** every validation failure shows the tile, chunk, and fix suggestion.

Acceptance criteria (user-facing, measurable):
- A user creates a 3×3-tile island from a 16-bit mask, sculpts a coastline, paints two textures and places 10 objects, without touching files manually. Timed by a user test; target set by owner.
- Undo/redo works for each tool across 50 consecutive actions without state drift (verified by round-trip hash).
- Killing the process mid-edit and reopening recovers the last journal state (tested with a fault-injection script).

Estimate: 8–12 weeks.

### Phase 5 — End-to-end PoC and client validation

Scope: the first development milestone from the mission, executed on a test realm and test client copy:
1. Create a small island continent in Worldwright.
2. Sculpt terrain and coastline.
3. Apply terrain textures.
4. Place existing WoW objects.
5. Save and reopen the project.
6. Export through MCM (EPF → build → activate → Portalkeeper install on a test client).
7. Generate AzerothCore map data (maps, vmaps, mmaps) and server DBCs.
8. Load and explore the continent in WoW 3.3.5a.

Acceptance criteria: each step has a pass/fail check; the client check (step 8) requires:
- Character teleports into the new map via a `game_tele` row;
- Terrain is visible and walkable on the island;
- Water shows where authored; coastline matches the mask within one chunk;
- Area name resolves (AreaTable row) and no client error appears in the log;
- The world map (M) shows the island (via `worldMaps[]`).
- Server log has no "Map file ... not found" for the island and no DBC load error.

Gate G3 (decisive for the strategy): **H1 — the 3.3.5a client loads a map ID that is not stock.** If it fails, the whole new-map design changes (it would require repurposing an unused stock map ID with a documented conflict, or a different delivery method). Test this first in Phase 5, with the smallest possible map (empty WDT + one ADT + Map.dbc row) before the full island.

Estimate: 3–4 weeks including client and server debugging.

### Phase 6 — Hardening, packaging, release

- Windows and Linux builds (CMake, reproducible; Linux verified early in Phase 1 with noggit3 as the reference).
- Documentation: operator runbook for extractor and server artifacts (P2, P3 in CONTENT_PIPELINE).
- Third-party notices (`THIRD_PARTY_NOTICES.md`) and provenance folder (`docs/provenance/`) per LICENSING_REVIEW §5.4.
- No auto-updater with binary download (see LICENSING_REVIEW §4).

Estimate: 3–4 weeks.

Total indicative effort: roughly 26–36 engineer-weeks plus MCM extension work (about 3 weeks, separate repository). This is larger than a fork's MVP (Cinder already has a wizard and renderer), and it is the cost of owning the project and export layers.

## 4. Risk register

| ID | Risk | Likelihood | Impact | Mitigation | Owner decision |
|---|---|---|---|---|---|
| R1 | Client does not load a non-stock map ID (H1) | Unknown (untested) | Critical | Gate G3 in Phase 5, smallest test first; keep fallback design (repurpose unused stock ID) documented | D4 |
| R2 | Heightmap precision too low (8-bit path) | High if unmitigated | Visual | 16-bit input required (D5) | D5 |
| R3 | Renderer from scratch takes too long | Medium–high | Schedule | Gate G2; noggit3 as behavioural reference; D1-c fallback | D1 |
| R4 | BLP encoder licensing (libimagequant is GPL-3) | High if bundled | License | Replace with permissive DXT encoder or re-implement DXT1/DXT5 directly; decide in Phase 1 | D1 |
| R5 | Extractor cannot see patch-6..9 (H4) | High if unmanaged | No terrain collision | §4.3 of CONTENT_PIPELINE; test in Phase 1 | — |
| R6 | Silent missing `.map` (H3) | Medium | Players fall through / no ground | Pre-start check script; Phase 5 log check | — |
| R7 | MCM extensions E1/E2 delayed (separate repo, separate review) | Medium | Phase 2 slip | Start MCM design review early; use stub + diagnostic path | — |
| R8 | Map.dbc/AreaTable drift between client and server | Medium | Assert or wrong names | Single generator, parity hashes (S1) | D3 |
| R9 | Scope creep into vanilla or other clients | Medium | Maintenance cost | D4 locks target | D4 |
| R10 | Stock asset redistribution by accident | Low | Legal | Export tool blocks stock-file copies (path allowlist); review in release checklist | D1 |
| R11 | Qt6 port issues on Windows | Low–medium | Schedule | Decide D2 before Phase 4; minimal Qt surface in Phase 1 (no UI) | D2 |
| R12 | Undo/recovery design flaws cause data loss | Medium | High | Journal before save; fault-injection tests in Phase 2 and 4 | — |
| R13 | Public repo governance (contributors, relicensing) | Low | Medium | DCO, `NOTICE`, `THIRD_PARTY_NOTICES.md` from day one | D1 |
| R14 | Clean-room claim challenged | Low–medium | Legal | Provenance logs; second-engineer review (LICENSING §5.4) | D1 |

## 5. Smallest useful prototype to validate the recommendation

This is deliberately narrow. It answers the three questions the recommendation depends on, without a UI:

**Prototype V0 — "one tile, one map ID, one client load"**

1. Write one ADT (flat, 16-bit height import from a synthetic mask), one WDT (only that tile flagged), one WDL, and a Map.dbc row with a non-stock ID (Phase 1 library, scripted; no UI).
2. Pack into an EPF by hand (zip with `manifest.json`), run `.content scan/install/build/activate` on a test realm (MCM as it exists today; `mapDefinitions` not yet supported, so this prototype uses **raw file entries only**). Raw entries for ADT/WDT/WDL are straightforward. A raw `DBFilesClient/Map.dbc` would replace the entire stock table rather than append a row, so for V0 ship the full stock-plus-one-row Map.dbc built from a stock copy on the test client. Whether MCM accepts a raw DBC that collides with a stock file is **untested** and must be checked first.
3. Portalkeeper installs the artifact on a test client copy.
4. Run `map_extractor` on the test client copy with the artifact at `patch-5.MPQ` (§CONTENT_PIPELINE 4.3a). Confirm `maps/<id>…map` is written for the tile.
5. Start worldserver on a test DB with the Map.dbc copy and one `game_tele` row. Teleport.

What this validates:
- H1 (non-stock map ID loads on client) — the critical question.
- H3/H4 (extractor and missing-map behaviour).
- That the file path from Worldwright-style output to MCM to Portalkeeper to client works end to end.

What it does not validate: renderer quality, UI, E1/E2 composition, vmap/mmap generation (run afterward), multi-tile performance.

Pass criteria: teleport succeeds, one tile renders, no server assert, `maps/*.map` present.
Fail handling: if H1 fails, stop and return to D4 with the alternative design; do not proceed to Phase 2.

Effort: 1–2 weeks. This is the cheapest test that could overturn the recommendation, so it should run before Phase 1 is complete if a test client and DB are available.

## 6. Acceptance criteria summary (for the owner)

| Milestone | Pass means |
|---|---|
| V0 | Non-stock map ID loads on client; `.map` generated; teleport works |
| Phase 1 | Library output accepted by extractor and an independent parser |
| Phase 2 | EPF validated by MCM; reproducible bytes; STAGED build with matching hashes |
| Phase 3 | Renderer matches noggit3 on 9 tiles within owner tolerance |
| Phase 4 | Timed user test creates, sculpts, paints, places, undoes and recovers an island |
| Phase 5 | All eight mission steps pass on test realm and client copy |
| Phase 6 | Windows and Linux builds, notices, runbook complete |

## 7. What is explicitly out of scope for this roadmap

- Vanilla / 1.12 / Turtle support.
- Any change to production AzerothCore modules, databases or environments.
- Shipping Blizzard assets.
- Client MPQ writing or any auto-updater.
- Cataclysm and later formats.
