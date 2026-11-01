cmake_minimum_required(VERSION 3.22)

# ----------------------------------------------------------------------------
#  SWEAX NEXT — AI destekli vokal mix & mastering (JUCE / VST3)
#
#  Derleme (Windows, VS 2022):
#    cmake -G "Visual Studio 17 2022" -A x64 -B build
#    cmake --build build --config Release --target SweaxNext_VST3 SweaxTests SweaxRender
#  JUCE 7.0.12 ilk configure'da GitHub'dan indirilir; elde bir kopya varsa -DJUCE_DIR=<yol> ile o kullanılır.
#
#  Hedefler:
#    SweaxNext_VST3 / SweaxNext_Standalone  — eklenti
#    SweaxTests                              — bağımlılıksız test çatısı (Tests/Cati.h)
#    SweaxRender                             — WAV → WAV + JSON rapor (golden testler, toplu değerlendirme)
#    SweaxShots                              — editörü ekran dışında çizer: README/site görselleri ve kareleri
#
#  Mimari: Source/{Core,Analysis,Decision,DSP,Chain,State} JUCE GUI'ye BAĞIMLI DEĞİLDİR ve
#  "SweaxCore" INTERFACE hedefi olarak her yaprak hedefe (eklenti, test, araç) derlenir. Böylece
#  JUCE modül kaynakları tek bir yaprakta derlenir (statik kütüphane + modül = çift sembol tuzağı yok).
#
#  Lisans: GPLv3 (LICENSE). JUCE 7 açık kaynak kullanımda GPLv3, VST3 SDK GPLv3 seçeneğiyle bağlanır.
# ----------------------------------------------------------------------------

project(SweaxNext VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(DEFINED JUCE_DIR)
    add_subdirectory("${JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        7.0.12
        GIT_SHALLOW    TRUE
        GIT_CONFIG     core.longpaths=true)   # JUCE extras'ta 260 karakteri aşan iOS dosya yolları var
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Ortak derleme ayarları -------------------------------------------------
# /fp:precise: golden render testleri deterministik kalsın (fast-math YOK).
# Uyarılar hata: gerçek zamanlı kodda "küçük" uyarılar genellikle gerçek hatadır.
add_library(SweaxWarnings INTERFACE)
if(MSVC)
    target_compile_options(SweaxWarnings INTERFACE /W4 /permissive- /fp:precise /utf-8 /Zc:__cplusplus
        /wd4100 /wd4458 /wd4459    # kullanılmayan parametre, isim gölgeleme (JUCE başlıklarında bol)
        /wd4324)                   # alignas(64) ile önbellek satırı ayırma → "yapı dolduruldu" (kasıtlı)
else()
    target_compile_options(SweaxWarnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# --- SweaxCore: GUI'siz çekirdek (INTERFACE) --------------------------------
file(GLOB_RECURSE SWEAX_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Analysis/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Decision/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Chain/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/State/*.cpp")
file(GLOB_RECURSE SWEAX_CORE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Analysis/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Decision/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Chain/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/State/*.h")

add_library(SweaxCore INTERFACE)
target_sources(SweaxCore INTERFACE ${SWEAX_CORE_SOURCES} ${SWEAX_CORE_HEADERS})
target_include_directories(SweaxCore INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_definitions(SweaxCore INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=0)
target_link_libraries(SweaxCore INTERFACE SweaxWarnings)

# --- Eklenti -------------------------------------------------------------------
juce_add_plugin(SweaxNext
    COMPANY_NAME                "Sweax Studios"
    PLUGIN_MANUFACTURER_CODE    Swex
    PLUGIN_CODE                 Swnx
    BUNDLE_ID                   com.sweax.next
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "SWEAX NEXT"
    DESCRIPTION                 "AI vocal engine — analyze first, act only when necessary"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE      # Program Files\Common Files\VST3 yönetici ister → Tools\kur.cmd ile kopyala
    VST3_CATEGORIES             "Fx|Dynamics|EQ|Vocal")

file(GLOB_RECURSE SWEAX_PLUGIN_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Plugin*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Plugin*.h")
target_sources(SweaxNext PRIVATE ${SWEAX_PLUGIN_SOURCES})
target_compile_definitions(SweaxNext PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    # Debug eklentide processBlock tahsis kancası açık; Release'te override YOK (host'un heap'ine dokunma)
    $<$<CONFIG:Debug>:SWEAX_RT_GUARD=1>)
target_link_libraries(SweaxNext PRIVATE
    SweaxCore
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)

# --- Testler ---------------------------------------------------------------------
file(GLOB_RECURSE SWEAX_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.h")
juce_add_console_app(SweaxTests PRODUCT_NAME "SweaxTests")
# Kabuk (PluginProcessor/Editor) da testlere girer: state round-trip, latency sabitliği, RT kapsamı,
# editör aç/kapa testleri gerçek processor üzerinde koşar (spec §111-113).
target_sources(SweaxTests PRIVATE ${SWEAX_TEST_SOURCES} ${SWEAX_PLUGIN_SOURCES})
target_include_directories(SweaxTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tests")
target_compile_definitions(SweaxTests PRIVATE
    SWEAX_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures"
    SWEAX_RT_GUARD=1                       # testlerde global new/delete kancası aktif (Core/RtGuard.cpp)
    JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(SweaxTests PRIVATE
    SweaxCore
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_audio_processors            # APVTS / ParamCache testleri
    juce::juce_audio_utils
    juce::juce_gui_extra                   # PluginEditor testler içinde derlenir
    juce::juce_dsp
    juce::juce_recommended_config_flags)

# --- Araçlar: SweaxRender (WAV → WAV + JSON) -----------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Tools/SweaxRender/main.cpp")
    juce_add_console_app(SweaxRender PRODUCT_NAME "SweaxRender")
    target_sources(SweaxRender PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tools/SweaxRender/main.cpp")
    target_link_libraries(SweaxRender PRIVATE
        SweaxCore
        SweaxWarnings
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_dsp
        juce::juce_recommended_config_flags)
    target_compile_definitions(SweaxRender PRIVATE JUCE_STANDALONE_APPLICATION=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
endif()

# --- Araçlar: SweaxShots (editörü ekran dışında çizer) -----------------------------
# Gerçek processor + editör: demo vokal blok blok işlenir, timer'lar elle pompalanır, editör
# paintEntireComponent ile bir Image'a çizilir → PNG kareleri (README/site görselleri ve GIF'ler).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Tools/SweaxShots/main.cpp")
    juce_add_console_app(SweaxShots PRODUCT_NAME "SweaxShots")
    target_sources(SweaxShots PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tools/SweaxShots/main.cpp" ${SWEAX_PLUGIN_SOURCES})
    target_link_libraries(SweaxShots PRIVATE
        SweaxCore
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_dsp
        juce::juce_recommended_config_flags)
    # görseller eklentiyle aynı görünsün: açılış logosu kapalı (GPLv3 kullanımında serbest)
    target_compile_definitions(SweaxShots PRIVATE JUCE_STANDALONE_APPLICATION=1 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
endif()
