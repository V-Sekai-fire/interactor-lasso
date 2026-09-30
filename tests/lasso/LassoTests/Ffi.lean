-- SPDX-License-Identifier: MIT
/-!
# guest/lasso, through tests/lasso/ffi/shim.cpp

A transform crosses as 12 floats (basis columns x, y, z, then the origin); targets as 7 floats each
(x, y, z, size, snapping power, visible, snap locked). Indices cross as floats, -1 for none.
-/

namespace LassoTests.Ffi

@[extern "lasso_lean_snap"] opaque snap : @& FloatArray → @& FloatArray → Float → Float → Float → UInt8 → FloatArray
@[extern "lasso_lean_redirect"] opaque redirect : Float → @& FloatArray → @& FloatArray → Float → Float → Float
@[extern "lasso_lean_live_points"] opaque livePoints : IO UInt32
@[extern "lasso_lean_check_all"] opaque checkAll : IO String

end LassoTests.Ffi
