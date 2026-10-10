-- SPDX-License-Identifier: MIT
import Lake
open Lake DSL System

package LassoTests where
  leanOptions := #[⟨`autoImplicit, false⟩]

-- Every dependency is pinned in a V-Sekai-fire repo or fork (AGENTS.md rule 1); plausible first,
-- so the witness search does not bring in another copy.
require plausible from git
  "https://github.com/V-Sekai-fire/plausible" @ "v4.34.0"
require «plausible-witness-dag» from git
  "https://github.com/V-Sekai-fire/plausible-witness-dag" @ "f18818941e8914b110f85ec330889a4785c01bf1"

/-- Every `.cpp` under `dir`, sorted, so the object names do not depend on the walk order. -/
def cppUnder (dir : FilePath) : IO (Array FilePath) := do
  let all ← dir.walkDir
  return (all.filter (·.extension == some "cpp")).qsort (·.toString < ·.toString)

-- guest/lasso and the godot-lite shim it runs on, compiled the way cmake/lasso.cmake compiles them
-- for the guest, plus the shim.
extern_lib lasso_ffi pkg := do
  let root := pkg.dir / ".." / ".."
  -- The godot-lite shim is contract-guest-runtime's, a sibling checkout in the manifest layout.
  let runtime := root / ".." / ".." / "2-contract" / "guest-runtime"
  let gdl := runtime / "guest" / "godot_lite"
  let core := runtime / "vendor" / "godot-core-subset"
  let lasso := root / "guest" / "lasso"
  let flags := #["-std=c++17", "-O2", "-fPIC", "-ffp-contract=off", "-Werror=absolute-value", "-w",
    "-I", gdl.toString, "-I", core.toString, "-I", lasso.toString, "-include", (gdl / "gdl_prelude.h").toString]
  let sources := (← cppUnder gdl) ++ (← cppUnder core) ++
    #[lasso / "lasso.cpp", lasso / "lasso_api.cpp", lasso / "checks.cpp"]
  let mut objs := #[]
  for src in sources do
    let rel := (src.toString.drop (root.toString.length + 1)).toString.map (fun c => if c == '/' then '_' else c)
    objs := objs.push (← buildO (pkg.buildDir / "ffi" / s!"{rel}.o") (← inputTextFile src) #[] flags "clang++")
  let shimFlags := #["-std=c++17", "-O2", "-fPIC", "-I", (← getLeanIncludeDir).toString, "-I", lasso.toString]
  objs := objs.push (← buildO (pkg.buildDir / "ffi" / "shim.o") (← inputTextFile (pkg.dir / "ffi" / "shim.cpp"))
    #[] shimFlags "clang++")
  buildStaticLib (pkg.staticLibDir / nameToStaticLib "lasso_ffi") objs

lean_lib LassoTests

@[default_target]
lean_exe tests where
  root := `Main
  -- The C++ is built by the system compiler against its C++ runtime and glibc; the toolchain's own
  -- sysroot has neither, so those libraries are named by the paths the system compiler reports.
  moreLinkArgs := run_io do
    let lib (name : String) : IO String := do
      let out ← IO.Process.output { cmd := "clang++", args := #[s!"-print-file-name={name}"] }
      pure out.stdout.trimAscii.toString
    pure #[← lib "libstdc++.so.6", ← lib "libm.so.6", ← lib "libc.so.6", "-Wl,--allow-shlib-undefined"]
