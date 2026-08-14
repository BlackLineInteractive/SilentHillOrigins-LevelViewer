# The message table

Everything in the Climax engine talks through `RWS::CEventHandler`, and the
wiring is by **name** — `TODO.md` §4b establishes that from Ghost Rider's
`RegisterMsg(CEventId&, const char*, const char*)` and
`LinkMsg(CEventId&, const char*, unsigned short)`.

What was missing was the names themselves. In the stripped Origins binary a
message is a global with no text in it — `DAT_006B4BF8` — and the code that
sends it says nothing about what it is. There is exactly one place the name
appears: the interning call that fills the global in, once, at startup.

    FUN_001FE780(CEventId *id, const char *name, const char *subname)

`FUN_001FE780` hashes `name`, looks it up in the global id table, and either
adopts the existing entry or makes a new one — the same de-duplication Ghost
Rider's `RegisterMsg` performs. So each `DAT_006Bxxxx` has one registration
site, and finding it names the global.

`tools/event_ids.py` does that as a text search over `decomp/sles_r5900/`,
resolving the second argument through the ELF. It recovers **206 messages**,
committed as [`sho_event_ids.json`](../generated/sho_event_ids.json) and emitted
as `SHO-port/include/SHO/Core/EventIds.h`.

That list is closed. A message name that is not in it is one we invented, and
nothing in the original answers to it.

---

## What the 206 cover

The whole engine, not just gameplay: the frame ticks (`iMsgRunningTick`,
`iMsgPausedTick`, `iMsgRenderScene`), the front end (`MsgUiNavigateUp`,
`MsgUiSelectOk`, `MsgUiTorchOn`), zones (`ZoneActivate`, `ZonePreCache`,
`ZoneLoadTransitions`), cameras (`CamMsgShakeCamera`, `CamMsgTnfStart`),
cutscenes (`IGCStart`, `IGCSkipRequest`), the save path (`MsgSaveGameData`,
`MsgLoadGameData`) and eleven cheat messages.

Two groups are worth flagging:

* **`Tutorial*` — eight names (`TutorialBeenHit`, `TutorialComboLevel1`,
  `TutorialPickedUpEssence`, `TutorialPenanceGaugeHalf`,
  `TutorialLowEssenceReached`, `TutorialLinkGaugeFull`, `TutorialHasGrabbed`,
  `TutorialEnemyDazed`) are Ghost Rider's, not Origins'.** Essence, the penance
  gauge and the link gauge are Ghost Rider mechanics; Origins has none of them.
  They are registered by `CPlayerBehaviour` anyway, because the class is shared
  between the two games. Do not port them.
* `race_start_line_trigger`, `boss_trigger`, `enemy_trigger`, `player_trigger`
  are lower case and unlike everything else — trigger *type* names rather than
  messages, interned through the same table.

---

## The two sides of the wiring

Three helpers do all of it, and telling them apart matters because two of them
look alike:

| | |
|---|---|
| `FUN_001FE780(id, name, sub)` | give a global its name — the registration above |
| `FUN_001FF350(handler, id, ?, prio)` | **link**: this handler wants the message. Priority is `0x8000` everywhere in the player |
| `FUN_001FED20(handler, id)` | **unlink**: walk the id's listener list, drop the entry whose handler matches |

So a constructor is the place to read what a class listens to, and the
destructor is the mirror image. `CPlayerBehaviour`'s destructor `FUN_0012D250`
(vtable `0x0068E588` slot 3) unlinks exactly what the constructor linked; it is
not the attach handler.

## The player's own table, read out of `FUN_0012C3B0`

**Registered — 22 messages the player publishes:**

    PlayerActivate          PlayerDeactivate        KillPlayerWithExplosion
    PlayerTorchOn           PlayerTorchOff          PlayerTorchCreated
    PlayerHeartBeat1        PlayerHeartBeat2        OneShotDestroyed
    RadioStaticEnable       RadioStaticDisable
    ScreenStaticEnable      ScreenStaticDisable     RespotPlayer
    TutorialBeenHit         TutorialComboLevel1     TutorialPickedUpEssence
    TutorialPenanceGaugeHalf  TutorialLowEssenceReached
    TutorialLinkGaugeFull   TutorialHasGrabbed      TutorialEnemyDazed

**Linked — 33 messages the player answers to:**

    iMsgRunningTick         iMsgPausedTick          iMsgSetPausedMode
    MsgActivate             MsgDeactivate
    GetPlayer               RespotPlayer            FMARespotPlayer
    EnablePlayerControl     EnablePlayerRender
    EnablePlayerCombat      DisablePlayerCombat
    GetPlayerAttackAnim     KillPlayerWithExplosion
    PlayerTorchOn           PlayerTorchOff          PlayerTorchCreated
    RadioStaticEnable       RadioStaticDisable
    ScreenStaticEnable      ScreenStaticDisable     OneShotDestroyed
    PlayerHeartBeat1        PlayerHeartBeat2
    MsgItemEquipped         MsgWeaponBroken
    MsgGrabStarted          MsgGrabFinished
    CheatMsg_OneHitKill     CheatMsg_Invincibility  CheatMsg_DoubleSpeed
    CheatMsg_ReplenishHealth  CheatMsg_PlayerSuicide

Four things this settles that were previously assumed:

* **`iMsgRunningTick` is the player's update.** Travis is not called from a
  game loop that knows about him; he is a listener like everything else, and
  the per-frame entry point is a message. Whatever runs the state machine hangs
  off that link.
* **The torch, the radio static and the screen static are three separate
  systems**, each with its own on/off pair, and each both sent *and* listened
  to by the player — the on/off pair is how anything else in the level drives
  them.
* **The eight `Tutorial*` are registered but never linked.** Nothing in Origins
  listens to them; they survive only because the class is shared with Ghost
  Rider. Do not port them.
* **There is no `CameraCut` message.** A camera change is not announced to the
  player, and nothing in the 206 is a cut notification. `PlaneTrigger` names
  the two cameras directly (§4b), so any player-side response to a cut has to
  come from the camera manager, not from a message.

---

## What it makes readable

A function that was a wall of `DAT_006Bxxxx` becomes plain. `FUN_001777E8`, the
zone activation the scene queue reaches through command `0x08`
(`TODO.md` §6f), is:

```
CZone::Activate(zone, spawnName):
    if (zone->flags & 1) return                 // already up
    zone->activeName = spawnName ? spawnName : zone->name
    bind the zone's resource to that name       // FUN_00203D58
    send ZonePreCache(zone)                     // 0x006BFD98
    instantiate the zone's objects              // FUN_00178B60, four fields at +0x6C
    send GetPlayer                              // 0x006B4AE0, reply is the player
    if no player replied:
        send FindPlayerSpawners                 // 0x006BF3D0
    send ZoneActivate(zone)                     // 0x006BFD80
    ... read the level script's "Msg" attribute, register it as a message
        and send it once; then its "Dat" attribute ...
    zone->flags |= 1
```

The `GetPlayer` / `FindPlayerSpawners` pair is the answer to "where does Travis
come from on a level change": the zone asks whether a player already exists and
only falls back to the spawners when nobody answers. That is why walking
between rooms keeps one Travis instead of respawning him.

---

## Regenerating

    python3 tools/event_ids.py

Reads `decomp/sles_r5900/` and `game-iso/SHO/SLES_551.47`; writes
`docs/generated/sho_event_ids.json` and `SHO-port/include/SHO/Core/EventIds.h`.
