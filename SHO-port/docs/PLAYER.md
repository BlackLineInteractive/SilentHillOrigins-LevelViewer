# Travis

`CPlayerBehaviour`, the class the player is an instance of.

    registry  0x006B4C50
    factory   0x00136CC8      allocates 0x1BF0 = 7152 bytes, returns object + 4
    ctor      0x0012C3B0
    vtable    0x0068E588

Three vtable slots are overridden, and they are where reading should start:

    slot  3   0x0012D250   teardown: unlinks every message the ctor linked
    slot  5   0x00137230   per-frame work
    slot  7   0x00137210   attribute handling
    slot 19   0x00136D18

## Recovered: how Travis is wired in

The constructor registers 22 messages and links 33. The names come from
`docs/executables/MESSAGES.md`, which recovers all 206 message names in the
build; the constants are in `include/SHO/Core/EventIds.h`.

Registered — what Travis sends:

    PlayerActivate          PlayerDeactivate        KillPlayerWithExplosion
    PlayerTorchOn           PlayerTorchOff          PlayerTorchCreated
    PlayerHeartBeat1        PlayerHeartBeat2        OneShotDestroyed
    RadioStaticEnable       RadioStaticDisable
    ScreenStaticEnable      ScreenStaticDisable     RespotPlayer

plus eight `Tutorial*` messages that are Ghost Rider's and that nothing in
Origins listens to. They survive because the class is shared between the two
games. Do not port them.

Linked — what Travis answers to:

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

Four things fall out of that list.

**`iMsgRunningTick` is the update.** Travis is not called from a loop that
knows about him; he is a listener like everything else in the engine, and the
per-frame entry point is a message at priority `0x8000`. Whatever runs the
state machine hangs off that link.

**The torch, the radio static and the screen static are three separate
systems.** Each has its own enable/disable pair, and each pair is both sent and
listened to by the player — which is how a trigger elsewhere in the level
drives them.

**Combat, control and rendering are three independent switches.**
`EnablePlayerCombat` / `DisablePlayerCombat`, `EnablePlayerControl` and
`EnablePlayerRender` are what cutscenes and scripted sequences use to take
pieces of Travis away without removing him.

**There is no camera-cut message.** Nothing in the 206 announces a camera
change. `PlaneTrigger` names the outgoing and incoming camera directly (see
`docs/TODO.md` §4b), so any response Travis has to a cut comes from the camera
manager, not from a message. The port's `PlayerBehaviour::OnEvent` listens for
a `"CameraCut"` that does not exist.

## Recovered: slot 5, the per-frame path

`FUN_00137230` is short and points at everything else:

    FUN_00123028()                       // shared character update
    (virtual through +0x34, slot 0x7c)   // gets the frame
    send CamMsgReceiveTrackingDetails    // hands the camera where Travis is
    FUN_00130028(this)                   // -> the state machine

So the camera does not go looking for Travis. He publishes his tracking details
every frame and the camera receives them, which fits `CConstraintCamera`
computing its vertical aim at run time rather than carrying an authored one
(`docs/TODO.md` §4a).

`FUN_00130028` resolves an animation dictionary on first use, builds a matrix
and then reaches `FUN_001300F0` — 5088 bytes, the largest function in the
player, and **not yet read**. It is the state machine.

## Not recovered

Everything below is what `src/Actor/PlayerBehaviour.cpp` and
`src/Actor/CharacterController.cpp` currently do. None of it came out of the
binary.

**Stamina.** The port drains 15 units a second while running, regenerates 20
walking and 35 standing, and locks running out below 25. The system is real —
`Stamina`, `ReduceStamina` and `Fatigue` are strings in the executable, and
`Fatigue`/`Stamina` sit in what looks like the save-data field list — but
none of those four numbers is. They were picked to feel right.

**The state set.** Idle / Walk / Run / Aim / Attack / Exhausted / Grappled /
Dead is a reasonable guess at a survival-horror character and nothing more. The
real set is whatever `FUN_001300F0` switches on.

**The direction latch across a camera cut.** Implemented against the
non-existent `CameraCut` message. The behaviour itself — Travis keeping his
world direction until the stick is re-centred — is described in
`docs/TODO.md` §4c as something the original does and this does not; it is
still worth having, but it has to be driven from the camera manager.

**Grapple timings.** 3.5 seconds to escape, +20% a press, −15% a second. See
[GRAPPLE.md](GRAPPLE.md): `CFMAController` is 80 bytes with two attributes, and
neither of them has been read.

**Radio static as a distance falloff.** The port computes
`1 − dist/12` against the nearest monster. `RadioStaticEnable` and
`RadioStaticDisable` are messages, not a continuous level, so at minimum the
shape is wrong: something decides on and off. `cThreatController` carries the
per-level numbers and they are in the archive — see [GRAPPLE.md](GRAPPLE.md).

## What to read next

`FUN_001300F0`, in order of what it unblocks: the state set, the input mapping,
the movement speeds and the stamina constants are all in there, and all four are
currently invented. `FUN_0012E2A0` (533 lines) and `FUN_001335B8` (217) are the
next two down.
