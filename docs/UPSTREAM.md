# Defects found outside this repository

Zoo Simulator does not change CNA or cna-engine. What it finds there is written down here, with
what the game does meanwhile, so the owner can pass it on.

## ZOO-UP-001: cna-engine's shadow map stores skinned casters far too near the sun

Found 2026-11-13 (cna-engine `04ab7b3`, EasyGL renderer on a Radeon 780M).

**What was seen.** Every animal was drawn in its own shadow: dull, flat, the same brown on the
sunlit and the far side, with speckles that followed the coat texture. A test model with a strong
normal map (a grid of bumps) looked exactly like the model without it. With the shadow map off,
or with skinned models not receiving it, the same models are lit, shaded and show the normal map.
Evidence: `docs/evidence/animal-self-shadow-2026-11-13.jpg` (before; after; after, with the test
normal map).

**Diagnosis (from reading the code; the map itself was not read back).**
`cna-engine/src/Graphics/ShadowMap.cpp` writes depth as a colour: a mid grey faded to white by
fog over the map's depth, so the stored value runs 0.5..1 (`SuggestedBias` says so). Rigid casters
go through a `BasicEffect` with texturing off. Skinned casters go through a stock `SkinnedEffect`
with the grey as emissive and **no texture set**. `SkinnedEffect` always multiplies its texture
in, and CNA's EasyGL renderer binds opaque black for an unset texture of a classic stock effect
(`EasyGLRenderer.cpp`, the comment "graphics shared cleanup GSC-0004": XNA parity). The grey is
multiplied away, a skinned caster stores `fog` instead of `0.5 + 0.5 * fog`, and receivers read
that as a surface up to half the map's depth nearer the sun. The shadow it throws on the ground
is still in the right place; the model itself, and anything between it and the sun, is shadowed.

**Suggested correction in cna-engine** (not applied from here): give the shadow map's skinned
effect a 1x1 white texture.

```cpp
// ShadowMap::ShadowMap, after skinned_ is set up
white_ = std::make_unique<Xna::Texture2D>(device, 1, 1);   // a member, kept alive with the map
const Microsoft::Xna::Framework::Color texel(255, 255, 255, 255);
white_->SetData(&texel, 0, 1);
skinned_->setTextureProperty(white_.get());
```

A test for it: a skinned plate above a floor must leave a sunlit skinned receiver at the same
height unshadowed (the engine's own calibration scene, with a one-bone skin).

**What the game does meanwhile.** `src/game/SunShadows.cpp` sets the shadow map up as
`Enhanced::EnableShadows` does and then turns shadow reception off for the two skinned effects.
Animals are lit by the sun and still throw their shadows on the ground. The cost: an animal is no
longer dimmed when it stands under a tree or in a shelter's shadow, and a skinned caster can
still darken a rigid surface that lies between it and the sun (a branch above an animal). When
cna-engine is corrected, `EnableSunShadows` goes back to `Enhanced::EnableShadows`.

## ZOO-UP-002: `--screenshot` writes the back buffer's alpha into the PNG

Found 2026-11-14 with the lion's mane (alpha-tested hair cards). `AutomatedRun::BeginDraw`
(`cna-engine/src/Graphics/AutomatedRun.cpp`) captures with default `CaptureOptions`, so
`opaque` is false and the PNG keeps whatever alpha the frame left in the back buffer. An
alpha-tested surface writes its texture's alpha (0.5..1 where it passes the test), and an
image viewer composites those pixels over white: on the screenshot the dark brown mane looked
pale grey, on screen it is brown. The game's own scripted shots already ask for
`.opaque = true` (`ZooGame.cpp`, `ZooGameVisit.cpp`).

**Suggested correction in cna-engine** (not applied from here): `capture.opaque = true;` in
`AutomatedRun::BeginDraw`, or a `--screenshot-alpha` switch for the rare caller that wants the
alpha. **What the game does meanwhile:** nothing in the game; screenshots of hair or leaves are
flattened to RGB before anyone judges them (the evidence sheets are JPEG, which has no alpha).

## ZOO-UP-003: imported glTF models are culled inside out when single-sided (observed)

Found 2026-11-14 while looking for the cause of ZOO-UP-002. With `doubleSided` switched off in
a copy of `lion.glb`, the game's viewer drew the lion inside out (paw pads and the inside of
the mouth seen through the body), although the file's winding agrees with its normals for
99.8 % of the triangles. `Enhanced/ModelImport.cpp` copies `doubleSided` but never sets
`Material::frontFace`, which defaults to `FrontFace::Clockwise` (XNA's), while glTF front faces
are counter-clockwise. Not investigated further. **What the game does meanwhile:** every animal
model is exported double-sided, so nothing is culled; the cost is the back faces' fragments.
