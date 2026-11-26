# A mod for OpenRAC's native port, and the top of its repository (docs/port/MODS.md, section 10):
# copy this folder, rename the target and the id, write your code.
#
#   python3 tools/fetch_sdk.py                     # the SDK sdk.lock pins, into .sdk/
#   cmake -S . -B build -G Ninja
#   cmake --build build
#   python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/megaturret_fix   # dist/rac2-megaturret-fix-0.2.0-<platform>.zip
#
# Push a tag v0.1.0 and the workflow (.github/workflows/build.yml) publishes the release the
# launcher installs from.
cmake_minimum_required(VERSION 3.20)
project(megaturret_fix LANGUAGES C CXX)

# The SDK: the release sdk.lock pins (tools/fetch_sdk.py puts it in .sdk/); OPENRAC_SDK=<dir> (an
# unpacked SDK, or sdk/ in a checkout of OpenRAC) or OpenRACMod_DIR=<dir>/cmake for another.
if(NOT OpenRACMod_DIR)
  if(DEFINED OPENRAC_SDK)
    set(OpenRACMod_DIR "${OPENRAC_SDK}/cmake")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock" lock)
    string(JSON sdk_version GET "${lock}" version)
    set(OpenRACMod_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.sdk/openrac-mod-sdk-${sdk_version}/cmake")
    if(NOT EXISTS "${OpenRACMod_DIR}/OpenRACModConfig.cmake")
      message(FATAL_ERROR "No OpenRAC mod SDK ${sdk_version}: run python3 tools/fetch_sdk.py first "
                          "(or pass -DOPENRAC_SDK=<an unpacked SDK>)")
    endif()
  endif()
endif()
find_package(OpenRACMod 1.20 REQUIRED)

# The mod's library: the moby walk (megaturret_fix.c) over the stuck-animation rule (turret_watch.c).
openrac_add_mod(megaturret_fix MANIFEST openrac-mod.json
                SOURCES src/megaturret_fix.c src/turret_watch.c)
target_include_directories(megaturret_fix PRIVATE src)

# The tests: the rule alone, and the library as the port loads it, against a host of the test's own.
include(CTest)
if(BUILD_TESTING)
  add_executable(turret_watch_test tests/turret_watch_test.c src/turret_watch.c)
  target_include_directories(turret_watch_test PRIVATE src)
  add_test(NAME turret_watch COMMAND turret_watch_test)

  add_executable(load_test tests/load_test.cpp)
  target_link_libraries(load_test PRIVATE OpenRAC::mod ${CMAKE_DL_LIBS})
  target_compile_features(load_test PRIVATE cxx_std_17)
  add_dependencies(load_test megaturret_fix)
  add_test(NAME load COMMAND load_test $<TARGET_FILE:megaturret_fix>)
endif()
