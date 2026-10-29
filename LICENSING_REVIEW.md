# Licensing Review

This is an engineering licensing review, not legal advice. Items marked **verify** need a legal read before public release.

## 1. Root licenses (verified in repository files)

| Repository | License file | Declared license | Notes |
|---|---|---|---|
| noggit-cinder | `COPYING` (GPLv3 text), `README.md:48-50` | GPL-3.0 | Also states it is ported from Noggit Azure (`README.md:18`). |
| Noggit_Gold | `COPYING`, `README.md:89-93` | GPL-3.0 | Asks to keep notices and source availability for redistributed builds. |
| noggit3 | `COPYING`, `README.md:1-3` | GPL-3.0 | Source headers say "licensed under GNU General Public License (version 3)" (208 occurrences in `src/noggit`). |
| noggit-red | `COPYING`, `README.md:1-3` | GPL-3.0 | Root of the lineage (per Gold README). |
| mod-content-manager | `LICENSE` | MIT, Copyright (c) 2026 Kevin Smith | Compatible with a GPL-3 Worldwright as a separate program. |
| Portalkeeper | `LICENSE` | MIT, Copyright (c) 2026 Kevin Smith | |
| mod-native-instance-maps | `LICENSE` | MIT, Copyright (c) 2026 Kevin Smith | Its README says it "is not a DBC patcher". |
| AzerothCore (local copy) | `LICENSE` | GPL-2.0 (text dated June 1991) | 15 source files carry AGPL headers and 1699 carry GPL-2 text (`grep` over `src/`). Mixed-header tree: **verify** upstream statement before linking any code. Worldwright will not link AzerothCore. |

## 2. Third-party code bundled in each candidate

Verified by reading vendored `src/external/*` in the clones.

| Component | Repos | License evidence | Risk for reuse |
|---|---|---|---|
| libimagequant (PNG2BLP) | cinder, gold, red | Header of `blur.c` (`src/external/PNG2BLP/libimagequant/blur.c:6-18`): "free software: ... GNU General Public License ... version 3 ... or (at your option) any later version." | GPL-3-or-later. Compatible with GPL-3 Worldwright; **incompatible with MIT Worldwright** if copied. Upstream pngquant also offers commercial licensing; the bundled copy is GPL-3 (**verify** upstream before relying on alternative terms). |
| libpng | cinder, gold, red | `PNG2BLP/libpng/LICENSE` ("libpng license") | Permissive; attribution required. |
| zlib | cinder, gold, red | `zlib.h` header (zlib license text) | Permissive. |
| png++ | cinder, gold, red | `pngpp/png.hpp` header, BSD-style with no-endorsement clause | Permissive; attribution. |
| libtxc_dxtn | cinder, gold, red | `txc_dxtn.h` header, MIT-style permission grant | Permissive; attribution. |
| tsl::robin_map | cinder, gold, red | `tsl/robin_map.h`, MIT license text | Permissive. |
| imgui / qtimgui | cinder, gold, red | `qtimgui/imgui/*` headers reference a license at end of file | **Verify** exact text before reuse (not extracted in this review). |
| qtgradienteditor | cinder, gold, red | Header: Nokia copyright, "`$QT_BEGIN_LICENSE:LGPL$`", and "contained in the Technology Preview License Agreement" | **Verify.** The "Technology Preview" wording is Qt-era and may not match LGPL. Do not reuse without a legal read. |
| glm | cinder, gold, red | Only Sun-copyright snippet in `ext/scalar_ulp.inl`; no license file vendored | **Verify.** Upstream glm is MIT, but no license text is in the tree. |
| blizzard-archive-library | cinder, gold, red | No license file. README header reads "# DBCD-cpp" (copy-paste from another project). | **Verify** upstream license of the actual library before reuse. Treat as unlicensed until checked. |
| blizzard-database-library | cinder, gold, red | No license file. Source headers (e.g. `include/external/ByteStream.h`) say "License as published by the Free Software Foundation ... version 3". README header also "DBCD-cpp". | Treat as GPL-3 by header evidence; **verify** provenance. |
| StormLib | noggit3 (FetchContent v9.30), mod-content-manager (`third_party/StormLib/LICENSE`) | MIT, Copyright 1999–2013 Ladislav Zezula (verified in mod-content-manager) | Permissive; safe for any Worldwright license. |
| CascLib | cinder, gold, red (`cmake/FindCascLib.cmake`) | Not vendored in-tree; expected MIT (**verify**) | |
| Qt5 | all | LGPL-3 (open-source edition). Dynamic linking required for LGPL convenience | Use Qt under LGPL terms; Qt6 is also LGPL. |
| lodepng, sol2, nlohmann/json, FastNoise2 | noggit3 FetchContent (scripting only) | Upstream MIT/Zlib; not vendored | Fetched at build time; keep attribution. |
| Lua 5.1 / LuaJIT | noggit3, cinder (`README.md:58-61`) | MIT (LuaJIT); Lua 5.1 MIT | Permissive. |

Action: any Worldwright build must ship a `THIRD_PARTY_NOTICES.md` and must not vendor a component whose license is marked **verify** until it is cleared.

## 3. Shipped data files (non-code) — significant

These are not covered by the repo's code license.

| Artifact | Where | Upstream license | Obligation |
|---|---|---|---|
| WoWDBDefs `definitions/*.dbd` (996 files) | `noggit-cinder/dist/definitions`, `Noggit_Gold/dist/definitions` (tracked as plain files: `git ls-files` = 996 each) | Data: **CC BY-SA 4.0**; code: BSD-3-Clause (`wowdev/WoWDBDefs` LICENSE.md, fetched) | Attribution and share-alike for the `.dbd` data. Shipping them inside a GPL-3 repo is possible if the data stays CC BY-SA 4.0 with attribution. Prefer fetching at build time instead. |
| Community listfile `listfile.csv` (94,703,984 bytes, 1,429,123 lines) | `Noggit_Gold/dist/listfile/listfile.csv` (tracked, 100644) | `wowdev/wow-listfile` repository root has **no LICENSE file** in the fetched listing. It is a community-maintained name list for Blizzard archive contents. | **Unclear / treat as all-rights-reserved until verified.** Do not redistribute in Worldwright. Worldwright should use a user-supplied or build-time-fetched listfile and state that origin. |
| Themes `dist/themes/Dark`, `McNet` | cinder, gold | No license file in-tree | **Verify** authorship and terms. |
| Blizzard game assets | not in repo | Blizzard copyright | Must not be shipped. Gold states this (`README.md:41, 73`). Keep that rule. |

Community listfile names and WoWDBDefs schema are useful for the editor, but the safe design is: Worldwright fetches or lets the user point to them; the repo does not contain them.

## 4. The Cinder-specific risks

- `ui/windows/updater/Updater.cpp:41` downloads updates from `https://updater.everwood.gg/launcher-updates/noggit-cinder/...`. This is a third-party update channel that downloads and runs binaries. It is incompatible with "no competing distribution mechanism" and with a clean supply chain. It must not be inherited in any form.
- `README.md:18`: ported from Noggit Azure. Azure's license is GPL-3 per Cinder's own statement, but its provenance is not in Cinder's history (root commit is empty). Provenance must be rebuilt before any copy.

## 5. GPL obligations and decision gate D1

### 5.1 What GPL-3 requires if Worldwright copies or links noggit code

If Worldwright copies or links any GPL-3 noggit code (for example renderer shaders, M2/WMO loaders, or parsers from noggit3):
- The combined work must be distributed under GPL-3.0 (or GPL-3.0-or-later). MIT or Apache-2.0 for the combined binary is not possible.
- Source for the whole program must be provided with the binary (GPL-3 §6).
- Modified files must carry a prominent change notice (GPL-3 §5a) and retain the original copyright notices.
- The license text must accompany distributions (GPL-3 §4–5).
- Upstream attribution must stay intact (Gold README "Keep applicable license notices, copyright statements, upstream attribution, and source availability intact").

### 5.2 What is not triggered

- Reading noggit source to understand behaviour is not itself a license event, but copying expression, structure or code from it is. A clean-room protocol is needed to claim independence (see §5.4).
- Exchanging files with mod-content-manager (EPF, MPQ, JSON) is communication between separate programs. Worldwright does not link or include MCM code. This is generally treated as arm's-length; **verify** if Worldwright ever imports MCM code.
- Using AzerothCore as a reference (reading `src/tools/map_extractor`, `GridTerrainLoader.cpp`) is reading GPL-2 code. Copying it would trigger GPL-2 obligations (and AGPL headers in some files would be stricter). Describe behaviour in the docs; do not copy.

### 5.3 Decision gate D1 (must be made before code import)

| Option | License of Worldwright | What it allows | What it forbids |
|---|---|---|---|
| D1-a: GPL-3.0-or-later, may import noggit3 code | GPL-3.0-or-later | Directly reuse noggit3 renderer/readers | Cannot combine with MIT-only parts; bundling libimagequant is fine |
| D1-b: MIT (or Apache-2.0), clean implementation | MIT / Apache-2.0 | Matches MCM and Portalkeeper; permissive for downstream | No copying of noggit code; libimagequant must be replaced (or dropped) with a permissive BLP encoder |
| D1-c: MIT for core, GPL-3 for optional noggit-derived module | Mixed, separated by directory | Keeps core permissive; isolates GPL parts | Build complexity; users must understand which parts are GPL |

**Recommendation: D1-b** for the core (matching MCM and Portalkeeper), with noggit used only as a behavioural reference and for documented formats. This keeps the independent public repository free of GPL copyleft. It costs effort on the renderer and BLP path (see roadmap R3, R4). If the renderer spike shows the clean implementation is not achievable on the timeline, switch to D1-c and isolate the noggit-derived module.

### 5.4 Clean-room protocol (for D1-b)

1. Specifications come from wowdev.wiki (ADT, WDT, WDL, M2, WMO, BLP, MPQ, DBC) and WoWDBDefs schemas. Cite the page and revision.
2. Engineers who have read noggit source for a topic do not write the implementation for that topic alone. A second engineer validates against wowdev and AzerothCore behaviour. Record this in `docs/provenance/`.
3. No file-level copying from noggit, Cinder, Gold, Red, or AzerothCore. Snippets with licensing notices are forbidden.
4. Tests compare Worldwright output against behaviour (round-trip, extractor parse, file bytes), not against copied code.

## 6. Redistribution rules for Worldwright outputs

- Worldwright outputs (projects, EPFs) must not contain stock Blizzard assets. A continent that references stock textures or models by path is allowed (the client already has them). Copying stock BLP/M2/WMO/ADT files into a published EPF is redistribution of Blizzard assets and is not allowed.
- ADTs derived from stock tiles (for example when porting an existing zone) are derivative of Blizzard data. Keep such content out of public artifacts, or keep it local to the operator.
- Original terrain, textures and models created by the user are the user's own; Worldwright should record authorship in the project metadata and make no claim over it.
- mod-native-instance-maps already demonstrates the rule "ships artwork and a manifest; no DBCs in packages". Worldwright follows the same rule.

## 7. Maintaining an independent public repository

- Create a new repository. Do not fork Cinder, Gold, Red, or noggit3 unless D1-a is chosen.
- Add `NOTICE` and `THIRD_PARTY_NOTICES.md` listing every reference and any imported component, with its license and source commit.
- Attribution to the Noggit lineage (Noggit3 contributors, Skarn, Titi/T1ti, Varenroth, RussianBias, Adspartan, Abyssal Realm) should be in `docs/CREDITS.md` even when no code is copied, since their work informed the design. This is courtesy, not a license requirement, unless code is reused.
- Use DCO sign-off or a CLA for contributors so relicensing decisions stay possible.
- Do not publish the `updater.everwood.gg` channel or any auto-update binary mechanism.
- Keep the community listfile and WoWDBDefs out of the repo; fetch at build time with the license noted in the fetch script.
- Qt is LGPL: ship dynamic linkage and the LGPL notices. Do not statically link Qt in a release binary without an LGPL-compliant plan.
- Dependencies to add with permissive licenses: StormLib (MIT), a permissive BLP encoder (to be chosen; see roadmap R4), libpng/zlib.

## 8. Summary table

| Item | Status | Action |
|---|---|---|
| Cinder code | GPL-3, but provenance and updater problems | Do not fork |
| Gold code | GPL-3, history not linked to Red, 94 MB unlicensed listfile in tree | Do not fork |
| noggit3 code | GPL-3, clean header state | Reference; D1 decision for any copy |
| noggit-red code | GPL-3, stale, submodules missing | Reference only |
| libimagequant | GPL-3-or-later | Do not bundle under MIT |
| qtgradienteditor, glm, blizzard-*-library | **verify** | Do not reuse until cleared |
| WoWDBDefs data | CC BY-SA 4.0 | Fetch at build time or attribute |
| wow-listfile | no LICENSE file | Do not redistribute |
| StormLib | MIT | Safe |
| mod-content-manager, Portalkeeper, mod-native-instance-maps | MIT | Safe to interoperate by file contract |
| AzerothCore | GPL-2 (plus AGPL headers) | Reference only; do not copy |
| Blizzard assets | copyright Blizzard | Never ship |
