# Grabs, FMA and the threat controller

Two classes the old version of this document described as one quick-time-event
system. They are not the same thing, and neither is a QTE.

## Recovered: FMA is scripted player animation

    CFMAController   registry 0x006BFE60   factory 0x0017D498   size 80
                     two attributes

Eighty bytes and two properties is not a struggle meter. The 107 instances in
the archive say what it actually does:

    python3 tools/dump_class.py game-iso/SHO/SH.ARC CFMAController --json -

    property 0: the message that fires it
      animEnterMirror 25, animExitMirror 24, animMirrorEnter 5,
      animMirrorExit 5, triggerFlinch 3, forceIdle 3, relayLeave 2, ...

    property 1: the state or clip it puts the player into
      EnterMirror 31, ExitMirror 31, Idle 26, IDLE 6,
      GetUpFromFall 4, StepBack 3, BEENHITFIRE 3, IGC_Stairs_End 1

So an `CFMAController` is a named hook: when this message arrives, force Travis
into that animation state. More than half of all instances in the game are the
mirror transitions, and the rest are recoveries — getting up from a fall,
stepping back, flinching, being caught by fire.

Ghost Rider's symbols place the class under
`game/modules/Objects/Fma/FMAPlayerAnim.cpp`, with the usual
`HandleAttributes` / `HandleEvents` / `MakeNew` trio and nothing suggesting a
meter or a button prompt.

## Recovered: grabs are messages

Four real messages carry the grab, and the player links three of them:

    MsgGrabStarted        MsgGrabFinished       MsgGrabCancel
    MsgSetGrabSyncState

`MsgSetGrabSyncState` is the interesting one: a grab is a two-body animation
that has to stay in step, and the enemy's own row names the clip — property 68
of `CEnemyBehaviour` is `Grabbed_2Back` and its equivalent per monster. So the
monster owns the grab animation and something synchronises the pair.

## Recovered: cThreatController is not the radio

    cThreatController   registry 0x006BFB10   factory 0x00168630   size 132
                        attributes 0x00168470, 7 properties

83 instances, one per level or so, seven properties, and the two that carry
text are message names:

    0   1.0 on 69 of 83, 0.4 on 12
    1   1.0 on 69, 0.13 on 11
    2   0.4 on 61, 1.0 on 15, 0.2 on 6
    3   0.4 on 61, 1.0 on 17, 2.0 on 5
    4   0 or -1
    5   effectEnterMirror 25, effectStartFilter 6, effectMirrorEnter 5, startFilter 2
    6   animExitMirror 24, effectStopFilter 6, animMirrorExit 5, relayMirrorExit 2

Four normalised floats and a pair of enter/exit message names, dominated by the
mirror transition and by a screen filter being started and stopped. Whatever
this class is, it is per-level tuning for an effect that turns on and off with
a message — not a per-frame proximity calculation over the monster list. The
name is Origins' own; Ghost Rider has no such class, so there is no unstripped
version to check against.

The radio is a separate thing entirely, and it is real: `Radio` is item row
number so-and-so in [ITEMS.md](ITEMS.md), `MsgUiRadioOn` / `MsgUiRadioOff`
switch it, and `RadioStaticEnable` / `RadioStaticDisable` turn its static on
and off. All four are messages. Nothing found so far computes a continuous
static level.

## Not recovered

**The struggle, if there is one.** `src/Actor/PlayerBehaviour.cpp` implements a
mash meter: 3.5 seconds, +20% a press, −15% a second, 25 damage on failure.
None of those four numbers exists in the game, and no data structure found so
far holds a meter. What `MsgGrabStarted` actually leads to has to be read out
of the player, in `FUN_001300F0` and below.

**How the mirror transition runs end to end.** Three systems name it —
`CFMAController` forces the animation, `cThreatController` starts and stops a
filter, and `MirrorReflector` in the port queues a level change. They are
clearly the same sequence, driven by messages like `effectEnterMirror` and
`animExitMirror`, and the order and timing between them is unknown.

**Whether static is continuous or binary.** Only on/off messages have been
found. The port's `1 − dist/12` falloff is a guess, and if a level is meant to
be silent until a monster is in the room, it is a wrong one.
