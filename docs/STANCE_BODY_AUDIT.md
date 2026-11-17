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

PC damage-path follow-up: 74EA6E obtains GetPartMatrix, 74EA76 copies it,
and 74EAAB jumps to 74EC99. The shared call at 74EC9B therefore already passes
both damage and ordinary body submissions through BodyPart. A separate missing
damage RenderPart hook does not explain the symptom.

To capture the unresolved runtime case, set `[Debug] StanceBodyMatrices=1` in
the tested installation's `scripts/NFSMWUnlimiterSettings.ini`, then restart.
The bridge appends `BodyTrace` lines to LOCALAPPDATA/NFSMWCustomizationLab/bridge.log.
Each distinct car/view/height/reflection combination is sampled once, capped at
64 combinations per process. Each records native body, biased root, height delta,
conjugated world delta and up to eight part input/output pairs (including the BODY
slot). Full 4x4 values, determinants, damage pointer and pass identity distinguish
an inverse rejection, reflection geometry, missing height data and a changed part
basis without adding draws or mutating native geometry. Default is OFF; no file
I/O or diagnostic matrix calculations occur on the disabled path. The trace is
diagnostic evidence only and does not fix the Fiat issue by itself.

Uniform-height follow-up: HeightTransform previously rejected equal front/rear
heights when the wheelbase was zero, missing or nonfinite, although translation
requires no axle references. The bridge also skipped every height operation when
the ecar layout pointer was absent. Equal finite heights now produce translation
before axle validation, and a missing layout can use this path. Unequal heights
still require valid axle data; the bridge does not invent a vehicle wheelbase.
Fixtures cover both lowering and raising with zero/nonfinite axle positions and
reject nonfinite heights. This fixes that identifiable dependency defect; it is
not proof that the Fiat's native attributes are invalid or its symptom is fixed.

The subsequent geometry investigation found reflection pivots in the original
Fiat asset metadata, but the native simple-animation caller uses that matrix only
for two scenery hashes, neither matching Fiat. Live hash-table reads confirmed
the default targets. See [pivot-use evidence](STANCE_GEOMETRY_RESEARCH.md#pc-pivot-use-follow-up).
An unconditional vehicle axis correction is therefore unsupported by this lead.

## First live Fiat trace, 2026-11-17

The private validation installation running source 5b5f760 recorded 48 distinct
FIATCOUPE pass/height combinations, with equal axle heights approximately -0.025
through -0.325. There were 32 nonreflection and 16 floor-reflection snapshots.
`tools/analyze_body_trace.py <bridge.log> --output <report.json>` checked all
192 recorded part input/output pairs. Each output matched input multiplied by
the recorded world-height delta within 2e-5, and every determinant retained its
sign. Native nonreflection passes had positive orientation; negative native
orientation was confined to reflection passes in this recording. The normal
part translations changed with the requested height, rather than remaining at
the original height or becoming reflected inside this bridge.

This rules out a newly introduced reflection in those sampled bridge outputs;
it does not identify the visible upside-down object, prove complete mesh/part
coverage, or exclude later renderer/other-plugin changes. Do not describe the
reported symptom as fixed without visual confirmation. Raw process addresses
and the private game's log are retained outside the repository. Four parser
fixtures cover preserved native reflections, an incorrect translation, process
ID-counter restarts and missing world-transform evidence:
`python -m unittest discover -s tests -p body_trace_tests.py`.
