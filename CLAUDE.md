# Project Overview

This is a personal Pokémon Emerald ROM hack ("made for Kate"), built on
[`pokeemerald-expansion`](https://github.com/rh-hideout/pokeemerald-expansion)
(RHH's fork of pret's `pokeemerald` decomp), synced with upstream master.

Remotes:
- `origin` → `cubsr/LearningRomhacking` (this project)
- `upstream` → `rh-hideout/pokeemerald-expansion`

## Current direction: 2-player co-op randomizer Nuzlocke

Scope has expanded beyond single-player QoL. The target experience is a
**randomizer Nuzlocke played simultaneously by two players**, each running
their own ROM instance, with state kept in sync across both games.

**Sync approach (decided):** Both instances run via mGBA's multi-window
mode, simulating two physical consoles connected over a link cable. On top
of that link-level connection, a custom sync layer ("netcode") is needed to
keep the two games consistent — this goes beyond what the vanilla link
protocol (trading/battling) already handles.

**Nuzlocke rule enforcement (not yet built):** For now, Nuzlocke rules
(permadeath, one-catch-per-route, no duplicate species, shared loss
tracking, etc.) are **not** enforced in game code. This section exists to
document the goal so future sessions don't have to rediscover intent —
treat it as design intent, not an implemented feature, until noted
otherwise.

### Relevant existing infrastructure

- `src/link.c`, `src/link_rfu.c`, `src/link_rfu_2.c`, `src/link_rfu_3.c`,
  `include/link.h`, `include/link_rfu.h` — pret's original GBA SIO / Wireless
  Adapter link code. This is the substrate any link-cable-based sync would
  build on; it currently only knows about vanilla link scenarios (trade,
  battle, Union Room, Mystery Gift), not custom state sync.
- `include/config/general.h` — `RANDOMIZATION_ENABLED`,
  `RANDOMIZATION_ASK_ON_NEW_GAME`, `RANDOMIZATION_DEFAULT_SIMILAR_STATS`.
  Randomizer is actively under development (see recent commit history:
  pre-game randomizer menu, randomized starters, item ball randomization,
  custom item IDs).
- `docs/tutorials/how_to_random_mon_generator.md` — existing randomizer
  tutorial/reference.
- `AgbRfu_LinkManager.c` — RFU (wireless adapter) link manager, another
  candidate transport layer if link cable emulation proves limiting.

### Open questions for future sessions

- **Seed distribution:** how does each instance agree on the same
  randomizer seed at new-game time (manual entry, exchanged over link,
  hardcoded per save pair)?
- **What crosses the wire, and when:** faints/deaths, catches, and
  route-lock state need to reach the other player's instance. Decide
  push-on-event vs. periodic sync vs. sync-on-link-connect.
- **Where Nuzlocke logic lives:** enforced in ROM (flags/scripts blocking
  illegal catches, auto-boxing fainted mons) vs. tracked by an external
  companion tool/spreadsheet that both players update manually.
- **Turn/session model:** do both players need to be connected
  simultaneously for sync to occur, or should state queue and reconcile
  on next connect?
- **Asymmetric info:** each player's game has info the other shouldn't see
  (e.g. their own route encounters before catching) — sync layer needs to
  avoid leaking this.

## Prior QoL pass (reference)

An earlier QoL review flagged several dormant `include/config/*.h` toggles
worth revisiting once the co-op/Nuzlocke work stabilizes: `I_EXP_SHARE_FLAG`
(unassigned), `P_ENABLE_MOVE_RELEARNERS` family (off), `DEXNAV_ENABLED`
(off, largest single feature gap), `POKEDEX_PLUS_HGSS` block (off). None of
these are blocking for the multiplayer work but are worth another pass
later.
