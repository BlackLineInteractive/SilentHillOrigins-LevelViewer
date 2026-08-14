# Monsters

    CEnemyBehaviour   registry 0x006B4B78   factory 0x0012B630   size 11664
                      ctor 0x00127718       vtable 0x0068CFE8

Three classes derive from it, and only three:

    CCalibanBehaviour   0x0011F2E0
    CFlaurosBehaviour   0x0012BF50
    CSadDaddyBehaviour

There is no `CNurseBehaviour` or `CCarrionBehaviour`. Every ordinary monster in
the game is the *same* class with a different row of properties, which is why
the tuning is in the archive rather than in the executable.

## Recovered: the fourteen monsters, and their numbers

`SH.ARC` holds fourteen `CEnemyBehaviour` definitions, each with 84 properties
of its own plus the components it inherits. Dumped whole into
[`sho_enemies.json`](../../docs/generated/sho_enemies.json):

    python3 tools/dump_class.py game-iso/SHO/SH.ARC CEnemyBehaviour \
        --json docs/generated/sho_enemies.json

    2Back            2BackNoTNF        Ariel              Butcher
    CalibanBoss      Carrion           InvisibleMan       Momma
    PsychoNurse      PsychoNurseSan    SadDaddyTongue     StraightJacket
    StraightJacketAlt                  StraightJacketBoss

`CCharacterBehaviour` property 8 is health — it is the only property that
orders the way difficulty does, and the bosses sit where you would expect:

    PsychoNurse, PsychoNurseSan                     100
    Carrion, InvisibleMan, SadDaddyTongue           125
    StraightJacket, StraightJacketAlt               125
    Ariel                                           150
    2Back, 2BackNoTNF, StraightJacketBoss           200
    Butcher, Momma                                  375
    CalibanBoss                                     450

Several `CEnemyBehaviour` properties are plainly angles and distances, and they
separate into two groups that make the design legible even before the code is
read. Ordinary monsters carry a cone (index 14 = 45°, 22 = 90°, 18 = 120°) and
short ranges (15 = 3.0, 17 = 7.5, 20 = 5.0). The scripted ones —
`CalibanBoss`, `Momma`, `SadDaddyTongue` — carry 180° and 1000.0 in the same
slots, which is a designer switching the sensory model off rather than widening
it. `Butcher` is between the two: 80.0 where others have 3.0 or 15.0.

    index    2Back   Carrion   Butcher   CalibanBoss
       14     45.0      45.0      45.0        180.0
       15      3.0       3.0      80.0       1000.0
       17      7.5       7.5    1000.0       1000.0
       18    120.0     120.0      90.0        180.0
       19     15.0      20.0      80.0       1000.0
       22     90.0      90.0      60.0        180.0

Property 51 is 75.0 on all fourteen and property 78 is 45.0 on all fourteen:
defaults nobody touched.

Strings in the row name the assets: property 68 is the grab animation
(`Grabbed_2Back`), 79 is a spotted cue (`2back_spotted`), and
`CCharacterBehaviour` 9–12 are impact and spawn sounds (`2back_impact`,
`rdspawn`).

## Not recovered

**Which index is which.** The table above says "plainly an angle" because 45,
90, 120 and 180 in adjacent slots are not accidental — but nothing here has
been matched against the code that reads it. Ghost Rider dispatches
`CEnemyBehaviour`'s attributes through a 51-entry jump table at `0x001294A0`
with a field offset per index; the equivalent in Origins has not been located
(`sho_attrmap.json` finds only the vtable-7 stub at `0x0012BA50`). Finding it
is what turns the numbers above into named fields, and it is the single highest
-value thing left in this file.

**The AI itself.** `FUN_00121AF0` (425 lines), `FUN_001285A8` (394) and
`FUN_001291D0` (169) have not been read. The state machine in
`src/Actor/EnemyBehaviour.cpp` — patrol, investigate, alert, chase, attack — is
a guess at what a survival-horror monster does.

**Sight and hearing.** The port hard-codes a 50° cone, 4.5 m unlit, 14 m with
the torch on, and noise radii of 1.5 / 4 / 9 / 30 m. None of those seven
numbers is from the game. The real ones are in the fourteen rows above,
unlabelled, and in whatever `FUN_00121AF0` does with them.

**Knockdown and the finishing stomp.** Four to six seconds down, 25% health on
standing up, 1.2 m to stomp: invented. The mechanic is real — it is in the
game — but every constant attached to it here was made up.

**How a monster reaches the player.** `Characters::MsgPingAwareness` and
`MsgSetAllEnemiesAware` are real messages (see
`docs/executables/MESSAGES.md`), so awareness is broadcast rather than polled.
The port polls.
