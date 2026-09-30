# lasso.elf (RFD 2287 step 2): the lasso's snapping math, guest/lasso, on godot-lite.
# lasso_core is the same library tests/lasso links natively through its Lean FFI shim.
# With the curvenet stage off, the shim is not compiled strict there, so the lasso asks for no
# contraction itself: the guest must round as tests/lasso does.
get_filename_component(LASSO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
# The runtime is a sibling checkout in the manifest layout.
if(NOT DEFINED GUEST_RUNTIME_ROOT)
	get_filename_component(GUEST_RUNTIME_ROOT "${LASSO_ROOT}/../../2-contract/guest-runtime" ABSOLUTE)
endif()
if(NOT TARGET godot_lite)
	include(${GUEST_RUNTIME_ROOT}/cmake/godot_lite.cmake)
	target_compile_options(godot_lite PRIVATE -ffp-contract=off)
endif()

add_library(lasso_core STATIC EXCLUDE_FROM_ALL
	${LASSO_ROOT}/guest/lasso/lasso.cpp
	${LASSO_ROOT}/guest/lasso/lasso_api.cpp
	${LASSO_ROOT}/guest/lasso/checks.cpp
)
target_include_directories(lasso_core PUBLIC ${LASSO_ROOT}/guest/lasso)
# No contraction: rv64gc has FMA and the native build does not, so both round the same.
target_compile_options(lasso_core PRIVATE
	"SHELL:-include ${GUEST_RUNTIME_ROOT}/guest/godot_lite/gdl_prelude.h"
	-ffp-contract=off -Werror=absolute-value)
target_link_libraries(lasso_core PUBLIC godot_lite)

if(COMMAND add_stage_elf)
	add_stage_elf(lasso ${LASSO_ROOT}/guest/lasso/main.cpp)
	target_link_libraries(lasso PRIVATE lasso_core)
endif()
