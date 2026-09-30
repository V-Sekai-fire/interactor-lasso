# lasso.elf (RFD 2287 step 2): the lasso's snapping math, guest/lasso, on godot-lite.
# lasso_core is the same library tests/lasso links natively through its Lean FFI shim.
# With the curvenet stage off, the shim is not compiled strict there, so the lasso asks for no
# contraction itself: the guest must round as tests/lasso does.
if(NOT TARGET godot_lite)
	include(${CMAKE_CURRENT_LIST_DIR}/godot_lite.cmake)
	target_compile_options(godot_lite PRIVATE -ffp-contract=off)
endif()

add_library(lasso_core STATIC EXCLUDE_FROM_ALL
	${CMAKE_SOURCE_DIR}/guest/lasso/lasso.cpp
	${CMAKE_SOURCE_DIR}/guest/lasso/lasso_api.cpp
	${CMAKE_SOURCE_DIR}/guest/lasso/checks.cpp
)
target_include_directories(lasso_core PUBLIC ${CMAKE_SOURCE_DIR}/guest/lasso)
# No contraction: rv64gc has FMA and the native build does not, so both round the same.
target_compile_options(lasso_core PRIVATE
	"SHELL:-include ${CMAKE_SOURCE_DIR}/guest/godot_lite/gdl_prelude.h"
	-ffp-contract=off -Werror=absolute-value)
target_link_libraries(lasso_core PUBLIC godot_lite)

if(COMMAND add_stage_elf)
	add_stage_elf(lasso ${CMAKE_SOURCE_DIR}/guest/lasso/main.cpp)
	target_link_libraries(lasso PRIVATE lasso_core)
endif()
