# Apocalypse spectacle mode

Apocalypse is an explicit, reversible visit-only spectacle. Hold **E** continuously for three
seconds while walking with no open window. The warning bar fills before anything changes;
releasing early cancels. A second three-second hold restores the ordinary zoo. A short E release
still reads an information board.

Activation records one deterministic public-side keeper gate from every occupied outdoor
habitat, including its exact fence kind, condition and gate flags, and then removes those edges.
The two sealed snake terraria are the defined indoor exception. The property perimeter and shared
habitat walls are never candidates. Seed 1 currently opens 119 gates for 607 of 609 animals.

Residents first route normally to the inside of their own opening, respecting blocking props,
fences and species water ability. Only the final adjacent step crosses the recorded open edge.
Shelter occupants are brought outdoors. The existing loose-animal behaviour then applies.
Dangerous loose animals are indexed in 32 m cells; a visitor group panics only for a dangerous
animal within 28 m, retains its members and uses the ordinary routed exit plan. Ending the mode
calms only fleeing groups.

The persistent red status remains visible while the mode is active. Cinema and map travel are
unavailable, binoculars are lowered, and pausing or losing the eligible walking state cancels a
partial hold. Pause itself remains available.

## Restoration and saves

Every participating animal retains its original habitat id. On restoration it is placed at the
nearest deterministic passable, prop-safe and unoccupied tile of that habitat; it is crated only
if no safe tile exists. Recorded gates are restored byte-for-byte. Births in an active habitat
join the participant list; a participant that died is simply absent at restoration.

Save format 4 stores the active flag, exact former fences and original homes. Loading reconstructs
habitats with the gates temporarily closed, then reopens the recorded edges. Invalid, perimeter or
mismatched breach data rejects the save instead of creating a half-restored zoo. Inactive saves
and their deterministic state hashes remain unchanged apart from the format version.

## Performance and verification

The first implementation caused the player collision world for the 1,284 × 648-tile estate to be
rebuilt synchronously after all gates changed. `Walker` now retains the collision shape id of each
fence and removes or restores only the recorded gate solids. A full-estate regression requires
each transition to complete in under two seconds and checks the exact shape-count delta. Fence
render batches update at two 128-tile cells per frame even after the read-only plant construction
cache has been released; buildings and vegetation are not rebuilt.

`tests/core/ApocalypseTests.cpp` covers deterministic selection, public reachability, active
save/load, movement in map bounds, exact restoration, repeatability and damaged-save refusal.
`tests/game/WalkerTests.cpp` covers the incremental full-estate collision update.
`tests/game/apocalypse.script` exercises early cancellation, activation and restoration through
the same held-key input path as a player and captures each state.
