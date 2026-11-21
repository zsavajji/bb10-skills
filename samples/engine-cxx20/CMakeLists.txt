cmake_minimum_required(VERSION 3.20)
project(phoenix LANGUAGES C CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CROSSCOMPILING OR QNX)
  set(PHX_BB10 ON)
else()
  set(PHX_BB10 OFF)
endif()

option(PHX_SANITIZE "ASan + UBSan on the host build" ON)
add_compile_options(-Wall -Wextra)
if(PHX_SANITIZE AND NOT PHX_BB10)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# ---- libcurl as one target ----------------------------------------------------------------------
if(PHX_BB10)
  # Static, by path. zlib by its .a, BY PATH, never -lz: the phone's libz.so.2 exports a libgcc
  # unwinder that hijacks C++ exceptions. Order matters: curl, ssl, crypto, then libz.a and -lsocket.
  set(PHX_PREFIX /opt/phoenix)
  add_library(phx_curl INTERFACE)
  target_include_directories(phx_curl INTERFACE ${PHX_PREFIX}/include)
  target_compile_definitions(phx_curl INTERFACE CURL_STATICLIB)
  target_link_libraries(phx_curl INTERFACE ${PHX_PREFIX}/lib/libcurl.a ${PHX_PREFIX}/lib/libssl.a
    ${PHX_PREFIX}/lib/libcrypto.a /opt/bb10-sdk/target/qnx6/armle-v7/usr/lib/libz.a -lsocket)
else()
  find_package(CURL REQUIRED)
  add_library(phx_curl INTERFACE)
  target_link_libraries(phx_curl INTERFACE CURL::libcurl)
endif()

# ---- libwebp: the decoder only, as one static library (scripts/fetch-third-party.sh) -------------
# The files of its makefile.unix's DEC_OBJS, DSP_DEC_OBJS and UTILS_DEC_OBJS, and the demuxer. No
# encoder, no sharpyuv. Threads stay off (no WEBP_USE_THREAD). The SSE/MIPS/MSA files compile to
# empty stubs on another CPU. BB10: every CPU has NEON, and QNX has no runtime detection; without
# HAVE_CONFIG_H, dsp/cpu.h turns WEBP_USE_NEON on when the compiler defines __ARM_NEON__ (what
# -mfpu=neon does) and cpu.c's armCPUInfo then answers "yes" to kNEON on any OS but Linux's RTCD.
set(WEBP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libwebp)
set(WEBP_DEC alpha_dec buffer_dec frame_dec idec_dec io_dec quant_dec tree_dec vp8_dec vp8l_dec webp_dec)
set(WEBP_DSP alpha_processing alpha_processing_mips_dsp_r2 alpha_processing_neon alpha_processing_sse2
  alpha_processing_sse41 cpu dec dec_clip_tables dec_mips32 dec_mips_dsp_r2 dec_msa dec_neon dec_sse2
  dec_sse41 filters filters_mips_dsp_r2 filters_msa filters_neon filters_sse2 lossless
  lossless_mips_dsp_r2 lossless_msa lossless_neon lossless_sse2 lossless_sse41 rescaler rescaler_mips32
  rescaler_mips_dsp_r2 rescaler_msa rescaler_neon rescaler_sse2 upsampling upsampling_mips_dsp_r2
  upsampling_msa upsampling_neon upsampling_sse2 upsampling_sse41 yuv yuv_mips32 yuv_mips_dsp_r2
  yuv_neon yuv_sse2 yuv_sse41)
set(WEBP_UTILS bit_reader_utils color_cache_utils filters_utils huffman_utils palette
  quant_levels_dec_utils random_utils rescaler_utils thread_utils utils)
set(webp_srcs ${WEBP_DIR}/src/demux/demux.c ${WEBP_DIR}/src/demux/anim_decode.c)
foreach(f ${WEBP_DEC})
  list(APPEND webp_srcs ${WEBP_DIR}/src/dec/${f}.c)
endforeach()
foreach(f ${WEBP_DSP})
  list(APPEND webp_srcs ${WEBP_DIR}/src/dsp/${f}.c)
endforeach()
foreach(f ${WEBP_UTILS})
  list(APPEND webp_srcs ${WEBP_DIR}/src/utils/${f}.c)
endforeach()
add_library(webp STATIC ${webp_srcs})
target_compile_options(webp PRIVATE -w)
target_include_directories(webp PUBLIC ${WEBP_DIR}/src PRIVATE ${WEBP_DIR})
if(PHX_BB10)
  target_compile_options(webp PRIVATE -mfpu=neon)
endif()

# zlib, for the PNG writer: by its .a on BB10 (see above), the system's on the host
if(PHX_BB10)
  set(PHX_ZLIB /opt/bb10-sdk/target/qnx6/armle-v7/usr/lib/libz.a)
else()
  find_package(ZLIB REQUIRED)
  set(PHX_ZLIB ZLIB::ZLIB)
endif()

# ---- one library per module ---------------------------------------------------------------------
set(PHX_MODULES util net text mp4 reddit media setup core)
set(deps_util)
set(deps_net util)
set(deps_text util)
set(deps_mp4)
set(deps_reddit net util text)
set(deps_media net util mp4)
set(deps_setup util net)
set(deps_core util net text mp4 reddit media setup)

set(YYJSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/yyjson)

foreach(m ${PHX_MODULES})
  file(GLOB srcs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/${m}/*.cpp)
  if(m STREQUAL "util")
    list(APPEND srcs ${YYJSON_DIR}/yyjson.c)
    set_source_files_properties(${YYJSON_DIR}/yyjson.c PROPERTIES COMPILE_OPTIONS "-w")
  endif()
  if(srcs)
    add_library(${m} STATIC ${srcs})
    set(vis PUBLIC)
  else()
    add_library(${m} INTERFACE)  # no sources yet: headers only
    set(vis INTERFACE)
  endif()
  target_include_directories(${m} ${vis} ${CMAKE_CURRENT_SOURCE_DIR}/src)
  foreach(d ${deps_${m}})
    target_link_libraries(${m} ${vis} ${d})
  endforeach()
  if(m STREQUAL "util")
    target_include_directories(util PRIVATE ${YYJSON_DIR})
  elseif(m STREQUAL "net")
    target_link_libraries(net ${vis} phx_curl)
  elseif(m STREQUAL "media")
    target_link_libraries(media ${vis} webp ${PHX_ZLIB})
  endif()
endforeach()

# ---- the daemon ---------------------------------------------------------------------------------
add_executable(phoenixd src/main.cpp)
target_compile_definitions(phoenixd PRIVATE PHX_VERSION="${PROJECT_VERSION}")
target_link_libraries(phoenixd PRIVATE core)
target_compile_definitions(core PRIVATE PHX_VERSION="${PROJECT_VERSION}")

# ---- tests: tests/<module>/*_test.cpp, one program each -----------------------------------------
if(NOT PHX_BB10)
  enable_testing()
  foreach(m ${PHX_MODULES})
    add_custom_target(${m}_tests)
    file(GLOB tests CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/${m}/*_test.cpp)
    foreach(t ${tests})
      get_filename_component(file ${t} NAME_WE)       # json_test, http_live_test
      string(REGEX REPLACE "_test$" "" name ${file})  # json, http_live
      set(exe ${m}_${name}_test)
      add_executable(${exe} ${t})
      target_include_directories(${exe} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
      target_compile_definitions(${exe} PRIVATE
        PHX_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
      target_link_libraries(${exe} PRIVATE ${m})
      add_dependencies(${m}_tests ${exe})
      add_test(NAME ${m}.${name} COMMAND ${exe})
      if(file MATCHES "_live_test$")
        set_tests_properties(${m}.${name} PROPERTIES LABELS live)
      endif()
    endforeach()
  endforeach()
endif()
