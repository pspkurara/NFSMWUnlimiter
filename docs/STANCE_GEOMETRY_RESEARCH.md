# Geometry evidence for automatic fender boundaries

`tools/inspect_stance_geometry.py` is a read-only offline report, not an automatic
calibrator. It reads installed GEOMETRY.BIN files and never changes game data or
enables a plane. Example:

```powershell
python tools/inspect_stance_geometry.py --game 'C:\Games\Most Wanted' --cars FIATCOUPE PUNTO --output geometry-report.json
```

The report records selected BASE/BODY/brake/wheel LOD-A solids, material effect
and vertex counts, stream stride, finite position bounds, normal-length range,
and the asset pivot matrix/determinant. Only confirmed 36-byte effects 0/4/5 are
decoded as positions/normals; other formats remain marked unsupported. This is
not a general geometry exporter. Its triangle report checks aligned u16 index
payloads, zero tail padding, material vertex partitions, index range and
containment in declared material AABBs. It reports derived triangle bounds without
exporting the source vertices or indices.
Do not feed its bounding boxes directly into production fender clipping.

On these 17 solids, 64,918 triangles passed the checks. Multi-material Fiat BASE
indices are relative to the complete vertex stream, not reset for each material;
the material's contiguous vertex partition still bounds its references. Some
payloads have two zero alignment bytes and Punto AABBs include a 1 cm margin.
Exact AABB equality would incorrectly reject those native surfaces.
`python -m unittest discover -s tests -p stance_geometry_tests.py` covers valid
partitions, wrong local indices, out-of-stream indices, expanded bounds and
nonzero/incorrect tail padding. These checks do not prove a fender shape or its
runtime coordinate transform.

Read-only measurements on the installed REDUX assets, 2026-11-17:

| Solid | Vertices | Measured longitudinal extent | Pivot determinant |
| --- | ---: | --- | ---: |
| FIATCOUPE_KIT00_BODY_A | 3195 | 1.3421 .. 2.1301 | approximately -1 |
| FIATCOUPE_BASE_A | 13203 | -2.1307 .. 2.0293 | approximately -1 |
| PUNTO_KIT00_BODY_A | 6954 | -2.0213 .. 2.0158 | +1 |
| PUNTO_BASE_A | 1118 | -1.9398 .. 1.1211 | -1 |

The Fiat's standard BODY is a front-region component; BASE contains most of the
car. Other Fiat kits have different BODY coverage. A universal boundary detector
therefore cannot treat BODY alone as a complete shell, or infer a missing rear
fender from its bounding box. It must collect the actual selected BASE and kit
submissions, their material/triangle surfaces and transforms, then evaluate each
wheel separately. Fenderless/unsupported cases must remain uncut.

Both Fiat solids contain an asset pivot reflecting the Z basis; the Punto BASE
also has a negative determinant (a different basis). These are existing file
values, not matrices created by the stance bridge. A negative stored pivot is
not itself proof of a broken asset: the engine's use of that pivot and the actual
submitted transform still require runtime evidence. In particular this finding
does not prove the cause of the user's inverted Fiat image or explain why its
main body did not lower. Do not add an unconditional Fiat axis flip as a fix.

## PC pivot-use follow-up

The inspected native eModel::GetPivotMatrix at 4FB7D0 reads model+0xC, then
returns solid+0x40. Its direct PC caller is SimpleModelAnim::Animate, 73D3C0;
eView::Render calls that routine at 6DAA2B with the submitted local/world matrix.
The animation routine compares solid+0x10 against exactly two configured hashes
before obtaining/modifying the pivot and composing it into the matrix.

Read-only process inspection on 2026-11-17 confirmed both live entries at
904B44/904B58 were their native defaults: 8A9B575D (windmill) and 63555FF2
(donut sign). Fiat BASE is FC93B979 and standard BODY is B5385233; neither
matches. Thus the inspected native simple-animation path does not apply the
stored Fiat reflection pivot. This narrows that hypothesis; it does not prove
that every graphics mod or all other renderer paths ignore pivot metadata.

The offline report keeps positions in their stored coordinate frame and reports
pivot metadata separately. It must not silently pre-multiply all car vertices by
the stored pivot. Runtime mesh/transform capture is still needed before matching
an automatic arch contour against the rendered fender.

The format facts were cross-checked against the primary
[NFS-ModTools MostWantedSolidReader](https://github.com/NFSTools/NFS-ModTools/blob/master/Common/Geometry/MostWantedSolidReader.cs):
104-byte shading groups, effect transitions delimiting vertex streams, 128-byte
vertex-data alignment, and CarShader positions/normals. Local decompilation's PC
eSolidPlat.hpp explicitly warns that its platform structure comes from another
game/platform; it is insufficient evidence for a production memory layout.

Next implementation evidence needed: verified PC runtime mesh/material layout,
runtime triangle-buffer binding and coordinate/pivot use; measured arch contours around
native axle references; a boundary representation that preserves lower/inner
wheel geometry. The existing single half-space cannot represent a general wheel
arch, so automatic fitting must not silently approximate every car with it.

## PC runtime CPU mesh access

Further native disassembly establishes a CPU path without reading a WRITEONLY
GPU vertex buffer. At 6E03F9/6E0409 the queue item+8 points to a slot containing
the platform geometry pointer. The platform's material count/table are +10/+14;
each material record is 68h bytes. CPU indices are at platform+24 with total
index count +2C; the native loader sets them at 6E3FC8 and uploads to its index
buffer at 6E3FE9. Each material's +44 is the first index and +40 its triangle
count, matching DrawIndexedPrimitive arguments at 6E05E2..6E05FC. +5C records
the material index count. +3C must not be misread as an index count.

Material +4C/+50 give CPU vertex source/byte length; +54 is the GPU buffer and
+58 the complete stream vertex count. Effect descriptor +34 points to stride at
descriptor+8. At 6E40E7..6E40F3 subsequent materials sharing the previous effect
have their CPU pointer/size cleared while retaining the shared GPU buffer and
stream count. Resolve their source from the preceding record in the same
contiguous effect run, with table, effect and buffer identity checks. Never
read backwards outside the containing platform table.

A bounded read-only inspection of the running validation process found 66
queue-resident material records (19,681 triangles) satisfying table membership,
index count/range, shared source, 36-byte stride and buffer-length checks. Shared
materials referenced their first stream record as expected. These are resident
queue entries, potentially stale: they do NOT prove the currently selected car,
actual draw ownership, all vertex formats or frontend instance identity. A cached
car pointer from an earlier BodyTrace had already been reused by UI allocations,
demonstrating why production capture must occur inside the verified submission
scope rather than dereferencing previous log addresses. Runtime copying still
needs resource lifetime/selected model correspondence and per-draw transforms.

## Tyre-only deformation evidence, 2026-11-18

The offline report also includes FRONT_TIRE_A, material names, diffuse map indices
and their resolved texture hashes. Texture entries are eight bytes (hash plus
unused word), as verified against the primary
[SolidReader](https://github.com/NFSTools/NFS-ModTools/blob/master/Common/Geometry/SolidReader.cs)
and the material indices in MostWantedSolidReader. These identities are evidence,
not a production tyre classifier.

Punto's RIM_KIT00_GUN_METAL/CHROME and TIRE_KIT00_SIDEWALL all resolve to
689AE908. A diffuse-texture whitelist would deform the rim. Fiat's three
materials are all named `Material`, so material-name matching alone also fails.

The report measures the minimum distance of the entire triangle surface to
local Y (wheel axle), plus maximum vertex radius. Including triangle interiors
rejects back caps whose corner vertices lie outside the actual tyre circumference.

| Stock LOD-A surface | Minimum radius (m) | Maximum radius (m) |
| --- | ---: | ---: |
| Fiat back cap, E38806E3 | 0 | 0.34067 |
| Fiat rim, 51CEE106 | 0 | 0.21900 |
| Fiat outer ring, 8BAC1B72 | 0.21123 | 0.30700 |
| Punto rim gun metal | 0 | 0.29956 |
| Punto tyre sidewall | 0.22819 | 0.33113 |
| Punto tyre back cap | 0 | 0.38397 |
| Punto tyre tread | 0.32916 | 0.33113 |

These two cases support using radial topology alongside material identities.
Neither the largest material AABB nor maximum vertex radius identifies the tyre:
the caps have larger corner radii. This is still insufficient evidence for every
MOD wheel, displaced axle, disconnected tyre mesh or LOD. No live mesh mutation
or stretch setting is enabled by this work. Required next steps remain exact
draw ownership, wheel-local coordinates, tyre/rim boundary selection, smooth
radial taper with corrected normals, per-draw buffer isolation and save/menu ABI
extension. Shared mesh data must never be edited in place.

REDUX's WHEELS/WHEELS9 files in the current test copy start with `dono` rather
than the supported native chunk directory. They are not decoded by this offline
reader; runtime capture must validate the selected wheel rather than substituting
the stock-car wheel measurements above. The generated report remains local and
contains only metadata/bounds, never copied vertex or texture payloads.
