# interactor-lasso

The lasso as a godot-sandbox guest: picking a far target by a pointing cone, with Lean tests of its math.

## What it is for

The lasso snaps a pointer to a distant target inside a cone. `cmake/lasso.cmake` builds its core on the reduced engine core from `contract-guest-runtime`, `transport-meshing-pen` builds the guest ELF from it, and the same core links natively into the Lean tests. [RFD 2287](https://github.com/V-Sekai-fire/manuals-weftspun/blob/main/rfd/2287-the-first-rung-draw-and-wear-it-in-a-headset.exs) places it in the first rung.

## Test

From `tests/lasso`, with `contract-guest-runtime` checked out at its goal-manifest path:

    lake build
    lake exe tests

## Licence

MIT. See [guest/lasso/LICENSE](guest/lasso/LICENSE).
