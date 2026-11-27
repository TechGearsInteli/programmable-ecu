# Build do computador (sem placa): testes do nucleo da ECU e simulador da HMI.
#
# O jeito mais simples de usar e:   python tools/host_build.py
# (ele acha um compilador C, configura, compila e roda os testes).
#
# Os firmwares das placas NAO passam por aqui: eles usam PlatformIO
# (src/firmware/hmi/s3 e src/firmware/motor-bench).
cmake_minimum_required(VERSION 3.20)
project(programmable_ecu_host LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ECU_BUILD_TESTS "Testes do nucleo (Unity)" ON)
option(ECU_BUILD_SIM "Simulador LVGL da HMI" ON)

include(FetchContent)
# Dependencias fixadas em versao. SOURCE_SUBDIR inexistente: so baixa, nao
# usa o CMakeLists delas (compilamos os .c que precisamos logo abaixo).
FetchContent_Declare(unity
  GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
  GIT_TAG v2.6.0
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR _sem_cmake)
FetchContent_Declare(lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG v9.2.2
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR _sem_cmake)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  set(ECU_WARNINGS -Wall -Wextra -Wshadow -Wno-unused-parameter)
elseif(MSVC)
  set(ECU_WARNINGS /W4 /wd4100)
endif()

# ---------------------------------------------------------------------------
# Nucleo portatil (C99, sem Arduino e sem LVGL): o mesmo codigo roda no ESP do
# motor, no motor virtual do simulador e nos testes.
# ---------------------------------------------------------------------------
file(GLOB ECU_CORE_SRC CONFIGURE_DEPENDS src/firmware/common/src/*.c)
add_library(ecu_core OBJECT ${ECU_CORE_SRC})
target_include_directories(ecu_core PUBLIC src/firmware/common/include)
target_compile_options(ecu_core PRIVATE ${ECU_WARNINGS})

if(ECU_BUILD_TESTS)
  enable_testing()
  FetchContent_MakeAvailable(unity)
  add_library(unity OBJECT ${unity_SOURCE_DIR}/src/unity.c)
  target_include_directories(unity PUBLIC ${unity_SOURCE_DIR}/src)

  file(GLOB ECU_TESTS CONFIGURE_DEPENDS src/firmware/common/test/test_*.c)
  foreach(test_src ${ECU_TESTS})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE ecu_core unity)
    target_compile_options(${test_name} PRIVATE ${ECU_WARNINGS})
    add_test(NAME ${test_name} COMMAND ${test_name})
  endforeach()
endif()

# ---------------------------------------------------------------------------
# Simulador da HMI: LVGL 9.2.2 + as telas de src/firmware/hmi/ui + motor virtual
# ---------------------------------------------------------------------------
if(ECU_BUILD_SIM)
  FetchContent_MakeAvailable(lvgl)
  file(GLOB_RECURSE LVGL_SRC CONFIGURE_DEPENDS ${lvgl_SOURCE_DIR}/src/*.c)
  add_library(lvgl OBJECT ${LVGL_SRC})
  target_include_directories(lvgl PUBLIC ${lvgl_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/firmware/hmi)
  target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
  if(WIN32)
    # Liga a janela do Windows (driver oficial do LVGL) no lv_conf.h
    target_compile_definitions(lvgl PUBLIC HMI_SIM_WINDOWS=1)
  endif()
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lvgl PRIVATE -w)
  endif()

  file(GLOB HMI_UI_SRC CONFIGURE_DEPENDS
    src/firmware/hmi/ui/src/*.c
    src/firmware/hmi/ui/src/fonts/*.c)
  add_library(hmi_ui OBJECT ${HMI_UI_SRC})
  target_include_directories(hmi_ui PUBLIC src/firmware/hmi/ui/include)
  target_link_libraries(hmi_ui PUBLIC lvgl ecu_core)
  target_compile_options(hmi_ui PRIVATE ${ECU_WARNINGS})

  file(GLOB HMI_SIM_SRC CONFIGURE_DEPENDS src/software/hmi-sim/*.c)
  add_executable(hmi_sim ${HMI_SIM_SRC})
  target_link_libraries(hmi_sim PRIVATE hmi_ui lvgl ecu_core)
  target_compile_options(hmi_sim PRIVATE ${ECU_WARNINGS})
  if(WIN32)
    target_link_libraries(hmi_sim PRIVATE user32 gdi32 imm32)
  endif()

  if(ECU_BUILD_TESTS)
    # Teste ponta a ponta: roda os cenarios de tela sem janela, contra o motor
    # virtual, conferindo o que a tela mostra, e salva as capturas.
    add_test(NAME hmi_e2e
      COMMAND hmi_sim --headless --cenario todos --saida ${CMAKE_BINARY_DIR}/capturas)
  endif()
endif()
