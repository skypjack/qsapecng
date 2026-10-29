# Engine regression tests (no Qt needed). Enable with -DQSAPECNG_BUILD_TESTS=ON,
# run with: ctest --test-dir build --output-on-failure

set(ENGINE_SRCS
  ${PROJECT_SOURCE_DIR}/src/model/circuit.cpp
  ${PROJECT_SOURCE_DIR}/src/model/metacircuit.cpp
  ${PROJECT_SOURCE_DIR}/src/model/numericsolve.cpp
  ${PROJECT_SOURCE_DIR}/src/parser/crc_circuit.cpp
  ${PROJECT_SOURCE_DIR}/src/parser/spice_circuit.cpp
  ${PROJECT_SOURCE_DIR}/src/parser/ir_circuit.cpp
  ${PROJECT_SOURCE_DIR}/src/logger/logger.cpp
)

find_package(Threads REQUIRED)

function(qsapecng_test name)
  add_executable(${name} ${ARGN} ${ENGINE_SRCS})
  target_include_directories(${name} PRIVATE
    ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/src)
  target_link_libraries(${name} PRIVATE Boost::headers Threads::Threads)
  target_compile_definitions(${name} PRIVATE
    BOOST_BIND_GLOBAL_PLACEHOLDERS BOOST_ALLOW_DEPRECATED_HEADERS)
  # checked iterators turn container misuse into a deterministic failure
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_definitions(${name} PRIVATE _GLIBCXX_DEBUG)
  elseif(MSVC)
    # MSVC checks iterators in the Debug configuration
    target_compile_options(${name} PRIVATE /bigobj)
  endif()
endfunction()

qsapecng_test(metacircuit_compress_test metacircuit_compress_test.cpp)
add_test(NAME issue6_compress COMMAND metacircuit_compress_test)

qsapecng_test(solve_test solve_test.cpp)
add_test(NAME issue6_active_bandpass
  COMMAND solve_test ${CMAKE_CURRENT_SOURCE_DIR}/data/issue6-active-bandpass.info
    "( - C1 * R2 * V1 ) * s / C1 * C2 * R1 * R2 * s^2 + ( C1 * R1 + C2 * R2 ) * s + 1"
    "( - 1 ) * s / 1 * s^2 + 2 * s + 1")
add_test(NAME rc_lowpass_netlist
  COMMAND solve_test ${CMAKE_CURRENT_SOURCE_DIR}/data/rc-lowpass-2nd-order.crc
    "V1 / ( - C1 * C2 * R1 * R2 ) * s^2 + ( ( - C2 * R2 ) + ( - C1 * R1 ) + ( - C2 * R1 ) ) * s + ( - 1 )"
    "1 / ( - 1e-06 ) * s^2 + ( - 0.003 ) * s + ( - 1 )")
# op-amp with open-loop gain 200000 and GBW 1 MHz (inverting x10):
# DC gain -9.99945, closed-loop pole at 90.9 kHz
add_test(NAME opamp_finite_gain
  COMMAND solve_test ${CMAKE_CURRENT_SOURCE_DIR}/data/opamp-finite-gain.info
    "A1 * R2 * V1 / ( ( - 0.031831 * R2 ) + ( - 0.031831 * R1 ) ) * s + ( ( - R2 ) + ( - A1 * R1 ) + ( - R1 ) )"
    "1 / ( - 1.7507e-07 ) * s + ( - 0.100005 )")
# one-pin voltage source (saved as V from ground to the pin, type 22)
add_test(NAME single_pin_source
  COMMAND solve_test ${CMAKE_CURRENT_SOURCE_DIR}/data/single-pin-source.info
    "V1 / C1 * R1 * s + 1"
    "1 / 0.001 * s + 1")
# real R (Cp = 1 nF) and real C (ESR = 10 ohm) in the RC low-pass above:
# H = (ESR + 1/sC) / (R || 1/sCp + ESR + 1/sC)
add_test(NAME real_passives
  COMMAND solve_test ${CMAKE_CURRENT_SOURCE_DIR}/data/real-passives.info
    "C1 * R1 * V1 * s^2 + ( 1e+09 * C1 * V1 + 0.1 * R1 * V1 ) * s + 1e+08 * V1 / C1 * R1 * s^2 + ( 1e+09 * C1 + 0.1 * R1 + 1e+08 * C1 * R1 ) * s + 1e+08"
    "1 * s^2 + 1.1e+06 * s + 1e+11 / 1 * s^2 + 1.011e+08 * s + 1e+11")
# the multi-core solver gives the same result as a single thread, term for
# term, and stops when cancelled
qsapecng_test(parallel_test parallel_test.cpp)
add_test(NAME parallel_files
  COMMAND parallel_test
    ${CMAKE_CURRENT_SOURCE_DIR}/data/issue6-active-bandpass.info
    ${CMAKE_CURRENT_SOURCE_DIR}/data/opamp-finite-gain.info
    ${CMAKE_CURRENT_SOURCE_DIR}/data/real-passives.info
    ${CMAKE_CURRENT_SOURCE_DIR}/data/rc-lowpass-2nd-order.crc
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/mfb-bandpass.info
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/twin-t-notch.info
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/sallen-key-lowpass.info)
add_test(NAME parallel_grid COMMAND parallel_test grid 3)
add_test(NAME exact_sums COMMAND parallel_test exactsum)
add_test(NAME term_limit COMMAND parallel_test limit)
add_test(NAME term_reuse
  COMMAND parallel_test reuse ${CMAKE_CURRENT_SOURCE_DIR}/data)

# the symbolic engine against an independent numeric (MNA) solver
qsapecng_test(mna_check mna_check.cpp
  ${PROJECT_SOURCE_DIR}/src/functor/rpoly.cpp
  ${PROJECT_SOURCE_DIR}/src/functor/rpoly-adapter.cpp)
# fixed seeds: 400 random circuits each (R, G, L, C, sources, controlled
# sources; then op-amps ideal and real, transformers, coupled coils, real
# R/L/C, current sources)
add_test(NAME mna_fuzz_basic COMMAND mna_check fuzz 400 11)
add_test(NAME mna_fuzz_extended COMMAND mna_check fuzz2 400 12)
# the SPICE export against ngspice (passes with a note when ngspice is missing)
add_test(NAME spice_export COMMAND mna_check spice 120 7)
# the solver against the one it replaced (tests/reference_solver.h): the
# same trees in the same order on one and three threads, the same terms
# with the same coefficients and the same printed result
add_test(NAME reference_fuzz_basic COMMAND mna_check reference 300 11)
add_test(NAME reference_fuzz_extended COMMAND mna_check reference 300 12 ext)
file(GLOB reference_circuits
  ${CMAKE_CURRENT_SOURCE_DIR}/../examples/*.info
  ${CMAKE_CURRENT_SOURCE_DIR}/data/*.info
  ${CMAKE_CURRENT_SOURCE_DIR}/data/*.crc)
add_test(NAME reference_files
  COMMAND mna_check reference-file ${reference_circuits})
# the numeric result without the trees (metacircuit::numeric) against the
# digit result from the trees
add_test(NAME numeric_fuzz_basic COMMAND mna_check numeric 300 21)
add_test(NAME numeric_fuzz_extended COMMAND mna_check numeric 300 22 ext)
add_test(NAME numeric_files
  COMMAND mna_check numeric-file ${reference_circuits})
# a circuit too big for the tree solve (4 billion trees; kept in data/big,
# out of the lists above): the numeric solve against MNA
add_test(NAME numeric_big_ladder
  COMMAND mna_check file-numeric ${CMAKE_CURRENT_SOURCE_DIR}/data/big/ladder7-real-lc.info)
# the same ladder bridged by capacitors (elliptic-like, about 1e12 trees):
# its coefficients span 320 decades, beyond what a double holds once the
# result is made monic; the plots' root finder hung on it
add_test(NAME numeric_big_bridged
  COMMAND mna_check file-numeric ${CMAKE_CURRENT_SOURCE_DIR}/data/big/ladder7-bridged.info)
add_test(NAME roots_big_ladder
  COMMAND mna_check roots-file ${CMAKE_CURRENT_SOURCE_DIR}/data/big/ladder7-real-lc.info)
add_test(NAME roots_big_bridged
  COMMAND mna_check roots-file ${CMAKE_CURRENT_SOURCE_DIR}/data/big/ladder7-bridged.info)
set_tests_properties(roots_big_ladder roots_big_bridged numeric_big_bridged
  PROPERTIES TIMEOUT 120)
foreach(circuit
    data/issue6-active-bandpass.info data/opamp-finite-gain.info
    data/real-passives.info data/single-pin-source.info
    data/rc-lowpass-2nd-order.crc
    ../examples/opamp-inverting.info ../examples/rc-lowpass-2nd-order.info
    ../examples/rlc-bandpass.info ../examples/sallen-key-lowpass.info)
  get_filename_component(stem ${circuit} NAME)
  string(REPLACE "../" "" where ${circuit})
  string(REGEX REPLACE "/.*" "" where ${where})
  add_test(NAME mna_file_${where}_${stem}
    COMMAND mna_check file ${CMAKE_CURRENT_SOURCE_DIR}/${circuit})
endforeach()
