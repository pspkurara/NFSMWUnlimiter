# Body duplication / inversion audit

The user clarified the Fiat symptom: the body appeared inverted underneath the
original car, rather than only the chassis lowering. This has not been reproduced
or diagnosed at runtime; do not declare it vehicle-specific or fixed from the
static findings below.

The stance bridge does not clone car models, allocate vertex/index buffers,
rewrite mesh topology, add a second body draw, or manufacture a reflection pass.
`Render` forwards each intercepted native car-render call once. `BodyPart` and
`DirectBody` replace their intercepted call sites and each forward one draw with
the same original model/part pointer. Their additional allocation is exactly one
64-byte matrix from native `eFrameMallocMatrix(1)` at 405ED0, not a body object.
The body-root adapter copies the existing native matrix once through 445090.
The package cloning in `CustomizationStanceMenu.h` concerns an FNG menu asset.

The added height transform is translation plus axle rake, with no negative scale.
Its rotation determinant is +1, and conjugating it through an invertible root
preserves that sign. Uniform height must preserve the complete native 3x3 basis.
Math fixtures check positive/negative uniform height through both ordinary and
already-reflected roots, in addition to existing scale/shear/animated-part tests.

Native PC `RenderFrontEndCars` at 750980 already renders a floor-reflection pass.
When its reflection argument is nonzero, instructions 750A5E..750A78 negate all
three components of the body's third basis row; the same native function then
adds its configured `ReflectionOffset` and calls CarRenderInfo::Render once at
750AE4. The decompiled CarRender.cpp RenderFrontEndCars agrees with this binary
evidence. Those native calls are intercepted by the bridge just like main draws;
the bridge neither adds nor requests them.

Thus static inspection does not show the requested failure mechanism of our mod
duplicating and flipping a body. It does identify native reflected geometry as a
possible source of a reversed image under a car. It does not prove that the user
was seeing that reflection, explain why the main body did not lower, or exclude
interaction with damage/part matrices, a material/shader or another mod. Runtime
matrix/pass evidence is still needed for the actual Fiat symptom.
