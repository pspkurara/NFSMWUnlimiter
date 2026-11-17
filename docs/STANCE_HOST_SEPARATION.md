# Unmodified Unlimiter compatibility plan

The integrated development build is temporary. It does **not** meet the requested
deployment model of installing an extension beside an unchanged Unlimiter ASI.
There is no evidence that source modification is unavoidable.

The current integration adds only two direct calls to the 3.3.1.1338 source:

- CustomizeSub_SetupParts appends the stance option before restoring selection.
- RideInfo_SetRandomParts notifies an observer after every completed branch.

Initialization in Init and the optional cart-name target check also assume the
same module. Save/profile, race-copy and geometry adapters otherwise hook native
PC boundaries directly. Moving those adapters does not require reimplementing
Unlimiter's part selection or its shop behavior.

The standalone host should chain the existing native CustomizeSub::Setup entry
(7BFBD0), call its current implementation, append the body-shop option, then
restore the requested selection. For randomization, wrap the two native call
sites (5E8FE0/7B4907), invoke their current target, then observe the result. Call
targets must be resolved at runtime; never encode an offset inside Unlimiter.
The cart-name adapter must likewise retain the verified current implementation.

Shared adapters should accept callbacks instead of referring to source-local
Unlimiter symbols. Initialization must run after ASI loading/native setup and
detect an integrated host so two copies never patch the same boundary. Save and
camera runtime ownership already have peer protocols; preserve those protocols.

Compatibility checks must describe data layouts/capabilities separately from
module code placement. New Unlimiter releases may change RideInfo, customization
record sizes or slots; chaining a jump alone cannot prove layout compatibility.
Unknown data layouts must not receive writes. First prove the unchanged installed
3.3.1 binary, then inspect and test other supported versions through adapters.

Required evidence before replacing the integrated build: x86 chaining/selection
fixtures, stock and Unlimiter load-order checks, unchanged dependency file hash,
native cart cancellation/purchase and save/reload, and garage/race geometry with
both owned and random cars. Keep this work separate from the running validation
session until those checks support deployment.

An experimental NFSMWStanceHost target now builds without Unlimiter implementation
headers. It chains the existing game Setup entry, observes both random-parts call
sites after their current targets return, and accepts the cart-name call target
only when owned by the loaded dependency. It currently recognizes the 3.3.1 layout
family and rejects the integrated fork to avoid duplicate hooks. It is excluded
from installation snapshots. An actual x86 E9 entry chained through MinHook now
passes owner, ECX, original-before-extension, return-value and restoration checks.
Separate ASI camera/runtime fixtures pass both stance-host/camera load orders.
Actual dependency startup and game runtime QA are still pending. Its shared
camera/frame support also needs isolation review before
calling this a standalone stance product. A successful compile is not release
evidence or proof of compatibility with newer layout families.

Runtime follow-ups reported during integration QA: the initial My Cars viewer
does not yet reflect saved stance; body-mounted race effects (light flares and
other emitters) need the body adjustment, while tire effects must follow contact
geometry. Owned My Cars stance in a custom race was confirmed by the user after
the early customization-copy fix on 2026-11-18.

Effect correction must follow attachment semantics, not whether an emitter is
above ground. Body markers need the same body transform, including front/rear
rake. Ground/contact effects must retain the final native/Unlimiter contact data;
do not add body height a second time. Inspect existing radius/contact compensation
before changing tires, smoke or skid positions, and verify emitter ownership.
