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
