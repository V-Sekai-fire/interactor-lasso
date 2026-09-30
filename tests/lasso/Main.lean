-- SPDX-License-Identifier: MIT
import PlausibleWitnessDag
import LassoTests.Ffi

/-!
# Tests for the lasso guest (guest/lasso)

The C++ is linked natively through `LassoTests.Ffi`; Lean is never in the running path. The unit
checks are the guest's own (`checks.cpp`, each with its control). Each property is a
plausible-witness-dag query for a candidate that breaks it, over deterministic candidates derived
from an index. The real code must come back `.provablyNone` inside `searchWidth`; its control, which
plants the defect the property rules out, must come back `.found`. `.budgetHit` is a failure.
-/

open PlausibleWitnessDag LassoTests

namespace LassoTests

def floats (xs : List Float) : FloatArray := ⟨xs.toArray⟩
def get (a : FloatArray) (i : Nat) : Float := a.get! i

/-- How far the deterministic readback looks: the width of every search. -/
def searchWidth : Nat := 4000

def mix (c k : Nat) : Nat := (c * 2654435761 + k * 40503 + 12345) % 1000003
def valueAt (c k : Nat) (lo hi : Float) : Float := lo + (hi - lo) * (mix c k).toFloat / 1000003.0

-- ── Vectors and the random view, a rotation matrix by Gram-Schmidt (never angles) ──────────────

structure V3 where
  x : Float
  y : Float
  z : Float

def V3.add (a b : V3) : V3 := ⟨a.x + b.x, a.y + b.y, a.z + b.z⟩
def V3.scale (a : V3) (s : Float) : V3 := ⟨a.x * s, a.y * s, a.z * s⟩
def V3.dot (a b : V3) : Float := a.x * b.x + a.y * b.y + a.z * b.z
def V3.cross (a b : V3) : V3 := ⟨a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x⟩
def V3.len (a : V3) : Float := Float.sqrt (a.dot a)
def V3.neg (a : V3) : V3 := a.scale (-1.0)

/-- A frame: three orthonormal columns and an origin. -/
structure Frame where
  x : V3
  y : V3
  z : V3
  o : V3

def Frame.apply (f : Frame) (l : V3) : V3 :=
  f.o.add ((f.x.scale l.x).add ((f.y.scale l.y).add (f.z.scale l.z)))

def Frame.toArray (f : Frame) : FloatArray :=
  floats [f.x.x, f.x.y, f.x.z, f.y.x, f.y.y, f.y.z, f.z.x, f.z.y, f.z.z, f.o.x, f.o.y, f.o.z]

def vecAt (c k : Nat) (lo hi : Float) : V3 := ⟨valueAt c k lo hi, valueAt c (k + 1) lo hi, valueAt c (k + 2) lo hi⟩

/-- A proper rotation from two random vectors, and a random origin; the identity when degenerate. -/
def frameAt (c : Nat) : Frame :=
  let a := vecAt c 100 (-1.0) 1.0
  let b := vecAt c 103 (-1.0) 1.0
  let o := vecAt c 106 (-5.0) 5.0
  let e1 := a.scale (1.0 / a.len)
  let b' := b.add (e1.scale (-(b.dot e1)))
  if a.len < 0.1 || b'.len < 0.1 then ⟨⟨1, 0, 0⟩, ⟨0, 1, 0⟩, ⟨0, 0, 1⟩, o⟩
  else
    let e2 := b'.scale (1.0 / b'.len)
    ⟨e1, e2, e1.cross e2, o⟩

def pi : Float := 3.14159265358979323846

/-- r metres ahead of the frame, theta radians off its -z axis, phi around it. -/
def ahead (r theta phi : Float) : V3 :=
  ⟨r * Float.sin theta * Float.cos phi, r * Float.sin theta * Float.sin phi, -r * Float.cos theta⟩

def targetsOf (ps : List V3) : FloatArray :=
  floats (ps.flatMap fun p => [p.x, p.y, p.z, 0.3, 1.0, 1.0, 1.0])

/-- A permutation of 0..n-1 from the candidate index (a deterministic Fisher-Yates). -/
def permAt (c n : Nat) : Array Nat := Id.run do
  let mut a := (List.range n).toArray
  for i in [0:n] do
    let k := n - 1 - i
    if k > 0 then
      let j := mix c (200 + k) % (k + 1)
      a := a.swapIfInBounds k j
  return a

/-- Values increasing by at least `gapLo`, dealt to target slots by a permutation. -/
def spread (c base : Nat) (n : Nat) (start gapLo gapHi : Float) : Array Float × Array Nat := Id.run do
  let perm := permAt c n
  let mut sorted : Array Float := #[]
  let mut v := start
  for k in [0:n] do
    v := v + valueAt c (base + k) gapLo gapHi
    sorted := sorted.push v
  let mut slots := Array.replicate n 0.0
  for k in [0:n] do
    slots := slots.set! perm[k]! sorted[k]!
  return (slots, perm)

def snapOf (f : Frame) (ps : List V3) : Int × Int :=
  let r := Ffi.snap f.toArray (targetsOf ps) (-1.0) 0.0 0.0 0
  if r.size == 4 then ((get r 0).toInt64.toInt, (get r 1).toInt64.toInt) else (-2, -2)

-- ── Properties, each with a control that plants the defect ─────────────────────────────────────

/-- At one range, the cone picks the target nearest its axis, then the next nearest. Control: the
axis offsets of the pick and one other target swapped in the call, the expectation kept. -/
def nearestBreaks (broken : Bool) (c : Nat) : Bool :=
  let n := 2 + mix c 0 % 5
  let r := valueAt c 1 0.5 5.0
  let (theta, perm) := spread c 10 n 0.01 0.002 0.2
  let phis := (List.range n).map fun i => valueAt c (20 + i) 0.0 (2.0 * pi)
  let best := perm[0]!
  let other := perm[1 + mix c 30 % (n - 1)]!
  let sent := if broken then (theta.swapIfInBounds best other) else theta
  let f := frameAt c
  let ps := (List.range n).map fun i => f.apply (ahead r sent[i]! phis[i]!)
  let (first, second) := snapOf f ps
  !(first == best && second == perm[1]!)

/-- At one angle off the axis, the nearer target wins. Control: the ranges of the pick and one
other target swapped in the call, the expectation kept. -/
def nearerBreaks (broken : Bool) (c : Nat) : Bool :=
  let n := 2 + mix c 40 % 5
  let theta := valueAt c 41 0.02 1.0
  let (range, perm) := spread c 50 n 0.3 0.02 1.0
  let phis := (List.range n).map fun i => valueAt c (60 + i) 0.0 (2.0 * pi)
  let best := perm[0]!
  let other := perm[1 + mix c 70 % (n - 1)]!
  let sent := if broken then (range.swapIfInBounds best other) else range
  let f := frameAt c
  let ps := (List.range n).map fun i => f.apply (ahead sent[i]! theta phis[i]!)
  let (first, second) := snapOf f ps
  !(first == best && second == perm[1]!)

/-- From a target straight ahead, the joystick moves the snap to the neighbour on that side of the
view, for any view. Control: the view rolled a quarter turn about its forward axis, the unrolled
expectation kept. -/
def redirectBreaks (broken : Bool) (c : Nat) : Bool :=
  let f := frameAt c
  let d := valueAt c 80 1.0 4.0
  let s := d * valueAt c 81 0.1 0.5
  let locals : List V3 := [⟨0, 0, -d⟩, ⟨s, 0, -d⟩, ⟨-s, 0, -d⟩, ⟨0, s, -d⟩, ⟨0, -s, -d⟩]
  let ts := targetsOf (locals.map f.apply)
  let view := if broken then ({ f with x := f.y, y := f.x.neg } : Frame) else f
  let pick (dx dy : Float) : Int := (Ffi.redirect 0.0 view.toArray ts dx dy).toInt64.toInt
  !(pick 1 0 == 1 && pick (-1) 0 == 2 && pick 0 1 == 3 && pick 0 (-1) == 4 && pick 0 0 == 0)

def firstViolation (breaks : Nat → Bool) (steps : Nat) : Option Nat :=
  (List.range steps).find? breaks

def query (name : String) (breaks : Nat → Bool) : IO TraceEntry := do
  let readback : Nat → Readback (Option Nat) := fun steps =>
    match firstViolation breaks steps with
    | some w => { value := some w, found := true, witnessIdx := w, budgetHit := false }
    | none => { value := none, found := false, budgetHit := (firstViolation breaks searchWidth).isSome }
  let (_, _, trace) ← resolve name (fun _ c => breaks c) readback
  pure trace

def properties : List (String × (Bool → Nat → Bool)) := [
  ("at one range the pick is the target nearest the axis, then the next (control: two axis offsets swapped)",
    nearestBreaks),
  ("at one angle the nearer target wins (control: two ranges swapped)", nearerBreaks),
  ("the joystick moves the snap to the neighbour on that side of any view (control: the view rolled a quarter turn)",
    redirectBreaks) ]

end LassoTests

-- `--write=<file>` also writes the checks' text, which Gate 12 compares with the guest's.
open LassoTests in
def main (args : List String) : IO UInt32 := do
  let mut bad := 0
  let text ← Ffi.checkAll
  for a in args do
    if a.startsWith "--write=" then IO.FS.writeFile (a.drop 8).toString text
  let lines := (text.splitOn "\n").filter (· ≠ "")
  for l in lines do
    IO.println s!"{if l.startsWith "PASS " then "ok  " else "FAIL"} {l}"
    unless l.startsWith "PASS " do bad := bad + 1
  -- An empty read is not a pass: checks.cpp has seven checks.
  if lines.length != 7 then
    IO.println s!"FAIL the guest's checks: {lines.length} lines, want 7"
    bad := bad + 1
  for (name, breaks) in properties do
    let real ← query name (breaks false)
    let control ← query s!"control: {name}" (breaks true)
    let realOk := real.outcome == .provablyNone
    let controlOk := match control.outcome with | .found _ => true | _ => false
    IO.println s!"{if realOk && controlOk then "ok  " else "FAIL"} {name}: real {repr real.outcome}, control {repr control.outcome}"
    unless realOk && controlOk do bad := bad + 1
  let live ← Ffi.livePoints
  IO.println s!"{if live == 0 then "ok  " else "FAIL"} no LassoPoint outlives its call across the searches: {live} alive"
  unless live == 0 do bad := bad + 1
  IO.println s!"{lines.length} checks and {properties.length} properties over {searchWidth} candidates each, {bad} failures"
  return if bad == 0 then 0 else 1
