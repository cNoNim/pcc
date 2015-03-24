cmake_minimum_required(VERSION 3.0)
project(pcc VERSION 1.2.0)

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

include(GetTriple)
get_target_triple(
    PCC_TARGET_TRIPLE
    PCC_TARGET_ARCH
    PCC_TARGET_VENDOR
    PCC_TARGET_OS)
set(PCC_TARGET_TRIPLE ${PCC_TARGET_TRIPLE} CACHE STRING "Target triple")

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/mip
    ${CMAKE_CURRENT_SOURCE_DIR}/cc/driver
    ${CMAKE_CURRENT_SOURCE_DIR}/arch/${PCC_TARGET_ARCH}
    ${CMAKE_CURRENT_SOURCE_DIR}/os/${PCC_TARGET_OS})

add_definitions(
	-DGCC_COMPAT
	-DPCC_DEBUG
    -DTARGET="${PCC_TARGET_TRIPLE}"
    -DVERSION="${PROJECT_VERSION}"
    -Dmach_${PCC_TARGET_ARCH}
    -Dos_${PCC_TARGET_OS}
    -DVERSSTR="${PROJECT_NAME} ${PROJECT_VERSION}")

add_executable(cc
	cc/cc/cc.c
	cc/driver/strlist.c
	cc/driver/xalloc.c)

find_package(BISON)
find_package(FLEX)

add_executable(mkext
	mip/mkext.c
	mip/common.c
	arch/${PCC_TARGET_ARCH}/table.c)
set_target_properties(mkext PROPERTIES COMPILE_DEFINITIONS MKEXT)
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/external.c ${CMAKE_CURRENT_BINARY_DIR}/external.h
	COMMAND mkext
	DEPENDS mkext)

BISON_TARGET(cgram cc/ccom/cgram.y ${CMAKE_CURRENT_BINARY_DIR}/cgram.c)
FLEX_TARGET(cscan cc/ccom/scan.l ${CMAKE_CURRENT_BINARY_DIR}/cscan.c)
ADD_FLEX_BISON_DEPENDENCY(cscan cgram)

add_executable(ccom
	arch/${PCC_TARGET_ARCH}/code.c
	arch/${PCC_TARGET_ARCH}/local.c
	arch/${PCC_TARGET_ARCH}/local2.c
	arch/${PCC_TARGET_ARCH}/order.c
	arch/${PCC_TARGET_ARCH}/table.c
	cc/ccom/builtins.c
	cc/ccom/gcc_compat.c
	cc/ccom/init.c
	cc/ccom/inline.c
	cc/ccom/main.c
	cc/ccom/optim.c
	cc/ccom/pftn.c
	cc/ccom/stabs.c
	cc/ccom/symtabs.c
	cc/ccom/trees.c
	mip/common.c
	mip/compat.c
	mip/match.c
	mip/optim2.c
	mip/reader.c
	mip/regs.c
	mip/unicode.c
	external.c
	${BISON_cgram_OUTPUTS}
	${FLEX_cscan_OUTPUTS})
target_include_directories(ccom PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/cc/ccom)

add_executable(cpp
	mip/compat.c
	cc/cpp/cpp.c
	cc/cpp/cpc.c
	cc/cpp/token.c)

BISON_TARGET(cxxgram cc/cxxcom/cgram.y ${CMAKE_CURRENT_BINARY_DIR}/cxxgram.c)
FLEX_TARGET(cxxscan cc/cxxcom/scan.l ${CMAKE_CURRENT_BINARY_DIR}/cxxscan.c)
ADD_FLEX_BISON_DEPENDENCY(cxxscan cxxgram)

add_executable(cxxcom
	arch/${PCC_TARGET_ARCH}/code.c
	arch/${PCC_TARGET_ARCH}/local.c
	arch/${PCC_TARGET_ARCH}/local2.c
	arch/${PCC_TARGET_ARCH}/order.c
	arch/${PCC_TARGET_ARCH}/table.c
	cc/cxxcom/builtins.c
    cc/cxxcom/gcc_compat.c
    cc/cxxcom/init.c
    cc/cxxcom/inline.c
    cc/cxxcom/main.c
    cc/cxxcom/cxxcode.c
    cc/cxxcom/optim.c
    cc/cxxcom/pftn.c
    cc/cxxcom/stabs.c
    cc/cxxcom/symtabs.c
    cc/cxxcom/trees.c
    mip/common.c
	mip/compat.c
	mip/match.c
	mip/optim2.c
	mip/reader.c
	mip/regs.c
	mip/unicode.c
    external.c
    ${BISON_cxxgram_OUTPUTS}
    ${FLEX_cxxscan_OUTPUTS})
target_include_directories(cxxcom PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/cc/cxxcom)