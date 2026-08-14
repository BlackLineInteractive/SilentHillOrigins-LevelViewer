# Items and weapons

    CInventoryItemDef   registry 0x006CC278   factory 0x0018C8A8   size 236
                        ctor 0x0018C8F8       attributes 0x0018C300, 37 properties
    CPickupItem         registry 0x006CC2A0   factory 0x00191FD8   size 288

The item table is not in the executable. It is 164 `CInventoryItemDef` objects
placed in the archive like any other level object, one per item, each carrying
all 37 properties. That means the whole thing — damage, durability, clip sizes,
sounds, string-table ids — can be read out of the retail disc without touching
a disassembler:

    python3 tools/dump_class.py game-iso/SHO/SH.ARC CInventoryItemDef \
        --json docs/generated/sho_inventory.json

The 37 matches `sho_attrmap.json`, which finds 37 attribute cases at
`0x0018C300`, so the record is complete rather than truncated.

## Recovered: what the 164 rows are

Property 0 is the category, and it partitions the table cleanly:

    0   46   keys, notes-adjacent props, the torch, the radio, NVGs
    1   10   consumables: HealthDrink, EnergyDrink, HealthKit, Ampoule, and the ammo boxes
    2   35   melee weapons and throwables
    3    7   firearms
    4   52   documents (Notes_*)
    5   14   accolades: Brawler, Butcher, Cartographer, Collector, Explorer,
             Fireman, Shooter, Sprinter, Stalker, Saviour, Vincent, Ambassador, Dog, Weapons

Properties 2, 3, 5 and 6 are string-table ids — `inventory_Shotgun_name`,
`_namep`, `_short`, `_long` — so the display text comes out of `Strings.<code>`
and is already localised. Property 35 is the world model
(`Torch`, `Radio`, `HO_K_ER`), and 36 the document body for notes.

## Recovered: the weapon table

Property 17 is damage, 18 is durability in hits, 16 is rounds per clip and 12
the reserve the player can carry. Firearms are category 3, everything else in
the list is category 2.

| weapon | dmg | hits | clip | reserve | range | 19 |
|---|---|---|---|---|---|---|
| ButchersGreatSword | 190 | ∞ | | | | 15 |
| TV, Typewriter, FilingCabinet, Toolbox, CurtainWeights | 100 | 1 | | | | 15 |
| Bottle, Toaster | 75 | 1 | | | | 15 |
| BarStool, Lamp, LonelyMoonGauntlets | 50 | 1 / ∞ | | | | 15 |
| FireAxe | 40 | ∞ | | | | 25 |
| Katana | 40 | 15 | | | | 25 |
| MeatCleaver | 40 | 10 | | | | 25 |
| PlasticCrate | 40 | 1 | | | | 15 |
| ButchersKnife | 30 | 8 | | | | 25 |
| MeatGaff | 30 | 10 | | | | 35 |
| MeatHook | 30 | 12 | | | | 35 |
| MonkeyWrench | 30 | 15 | | | | 25 |
| PitchFork | 25 | 8 | | | | 30 |
| Poker | 25 | 12 | | | | 30 |
| Shovel | 25 | 9 | | | | 35 |
| CrowBar, Hammer | 20 | 12 / 10 | | | | 25 |
| CutThroatRazor, ScrewDriver | 20 | 7 / 6 | | | | 30 |
| Spear | 20 | 10 | | | | 35 |
| BrokenWood, PoolCue | 20 | 4 | | | | 40 / 51 |
| Baton | 18 | 7 | | | | 25 |
| Scalpel | 17 | 8 | | | | 25 |
| BrokenPole, DripStand, LightStand | 15 | 8 / 7 | | | | 45 / 40 / 30 |
| Fists | 10 | 5 | | | | 15 |
| Shotgun | 90 | | 2 | 32 | 5 | 15 |
| Rifle | 80 | | 4 | 6 | 100 | 15 |
| Magnum | 65 | | 6 | 6 | 10 | 15 |
| M1911Pistol | 25 | | 8 | 8 | 15 | 15 |
| TargetPistol | 20 | | 6 | 6 | 15 | 15 |
| TeslaRifle | 15 | | — | 50 | 10 | 15 |
| AK47 | 12 | | 18 | 30 | 15 | 15 |

Durability 0 means the weapon never breaks: `ButchersGreatSword`, `FireAxe`,
`LonelyMoonGauntlets` and `TeslaRifle`, all four of them unlockables or boss
gear. Durability 1 is the household objects that shatter on the first hit —
that mechanic is exactly what the data says, no inference needed.

Property 20 is the break sound and it groups the weapons by material:
`travis_swordbreak` for bladed, `travis_batbreak` for blunt,
`travis_gunbreak` for firearms, and object-specific ones like `bulk_porttv`
and `bulk_toaster`. Properties 29, 30 and 31 are fire, reload and dry-fire
(`travis_shotgunshot`, `travis_shotgunreload`, `travis_9mmempty`); the Tesla
rifle has its own three.

Properties 27 and 28 are 1.0 on everything melee and 0.9 / 0.3 on the pistols,
rifle and AK47 — the shotgun and the Tesla stay at 1.0. Two movement
multipliers, most likely walk and aim, but that pairing is read off the numbers
rather than out of the code.

## Not recovered

**Property 19.** It runs 15 for fists and throwables, 25–35 for ordinary melee
and 40–51 for the long thin ones — `PoolCue` 51, `BrokenPole` 45, `BrokenWood`
and `DripStand` 40. That ordering is reach, or something that behaves exactly
like reach, and nothing has confirmed it. Same for 24 and 25 on firearms: 24
orders as range (Rifle 100, Shotgun 5) and 25 as its opposite, but neither has
been matched to the code that reads it.

**Indices 7–11, 13–15, 21–23, 26, 32–34.** Mostly constant across the table,
so they are defaults nobody touched, and they are unread.

**What the port does with all of this.** `src/Inventory/` and
`src/Combat/CombatSystem.cpp` do not load `sho_inventory.json`. They carry a
hand-written item list with invented numbers, and its weapon ids
(`Weapon_KitchenKnife`, `Weapon_HuntingRifle`, `Weapon_AssaultRifle`) are not
the game's — the game calls them `ButchersKnife`, `Rifle` and `AK47`. Wiring
the real table in is the next job in this file, and it is a small one: the data
is already extracted.

**Reload, aim cone and durability loss.** `MsgUiReload` and `MsgWeaponBroken`
are real messages and `travis_9mmempty` is a real dry-fire cue, so the shape of
the system is right. The ±15° aim cone and "one durability point per hit" in
`CombatSystem` are assumptions.
