cmake_minimum_required(VERSION 3.21)
project(RegemarkPresent VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

# DeckLink needs the Blackmagic Desktop Video SDK headers at build time.
# Download it from blackmagicdesign.com/developer and point this at the SDK root.
set(DECKLINK_SDK_DIR "" CACHE PATH "Path to the Blackmagic DeckLink SDK root")

set(SOURCES
    src/core/Types.cpp
    src/core/RichText.cpp
    src/core/ScreenConfig.cpp
    src/core/Library.cpp
    src/core/AppSettings.cpp
    src/render/TextRenderer.cpp
    src/render/MediaPool.cpp
    src/render/ScreenRenderer.cpp
    src/show/ShowController.cpp
    src/show/ShowTimer.cpp
    src/output/FrameWorker.cpp
    src/output/DisplayOutput.cpp
    src/output/NdiOutput.cpp
    src/output/OutputEngine.cpp
    src/import/SongImporter.cpp
    src/ui/MainWindow.cpp
    src/ui/SlideEditor.cpp
    src/ui/ScreenSetupDialog.cpp
    src/ui/MediaBin.cpp
    src/ui/Widgets.cpp
    src/ui/Theme.cpp
    src/ui/SongSelectBrowser.cpp
    src/ui/TimerPanel.cpp
)

if(DECKLINK_SDK_DIR)
    if(WIN32)
        # The Windows SDK ships an IDL; generate the COM header + GUIDs with MIDL.
        set(DL_GEN ${CMAKE_BINARY_DIR}/decklink)
        file(MAKE_DIRECTORY ${DL_GEN})
        add_custom_command(
            OUTPUT ${DL_GEN}/DeckLinkAPI_h.h ${DL_GEN}/DeckLinkAPI_i.c
            COMMAND midl /nologo /env x64 /h DeckLinkAPI_h.h /iid DeckLinkAPI_i.c
                    /I ${DECKLINK_SDK_DIR}/Win/include
                    ${DECKLINK_SDK_DIR}/Win/include/DeckLinkAPI.idl
            WORKING_DIRECTORY ${DL_GEN}
            DEPENDS ${DECKLINK_SDK_DIR}/Win/include/DeckLinkAPI.idl)
        list(APPEND SOURCES src/output/DeckLinkOutput.cpp ${DL_GEN}/DeckLinkAPI_i.c)
        set(DL_INCLUDE ${DL_GEN} ${DECKLINK_SDK_DIR}/Win/include)
    else()
        list(APPEND SOURCES src/output/DeckLinkOutput.cpp
             ${DECKLINK_SDK_DIR}/Linux/include/DeckLinkAPIDispatch.cpp)
        set(DL_INCLUDE ${DECKLINK_SDK_DIR}/Linux/include)
    endif()
else()
    list(APPEND SOURCES src/output/DeckLinkStub.cpp)
endif()

# Everything except main() lives in a static library so tests can link it.
add_library(rpcore STATIC ${SOURCES})
target_include_directories(rpcore PUBLIC src)
target_link_libraries(rpcore PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Multimedia)
target_compile_definitions(rpcore PUBLIC RP_VERSION="${PROJECT_VERSION}")

# Optional: SongSelect opens inside the app when Qt WebEngine is available;
# otherwise it falls back to the system browser + watching Downloads.
find_package(Qt6 QUIET COMPONENTS WebEngineWidgets)
if(Qt6WebEngineWidgets_FOUND)
    target_link_libraries(rpcore PUBLIC Qt6::WebEngineWidgets)
    target_compile_definitions(rpcore PRIVATE RP_HAVE_WEBENGINE=1)
    message(STATUS "SongSelect: embedded browser (Qt WebEngine)")
else()
    message(STATUS "SongSelect: system browser fallback (Qt WebEngine not found)")
endif()

qt_add_executable(RegemarkPresent WIN32 src/main.cpp resources/resources.qrc)
if(WIN32)
    target_sources(RegemarkPresent PRIVATE resources/app.rc)  # .exe icon
endif()
target_link_libraries(RegemarkPresent PRIVATE rpcore)

if(DECKLINK_SDK_DIR)
    target_include_directories(rpcore PRIVATE ${DL_INCLUDE})
    target_compile_definitions(rpcore PRIVATE RP_HAVE_DECKLINK=1)
    if(WIN32)
        target_link_libraries(rpcore PUBLIC ole32 oleaut32)
    else()
        target_link_libraries(rpcore PUBLIC dl pthread)
    endif()
endif()

option(RP_BUILD_TESTS "Build the self-tests" ON)
if(RP_BUILD_TESTS)
    enable_testing()
    qt_add_executable(rp_selftest tests/selftest.cpp)
    target_link_libraries(rp_selftest PRIVATE rpcore)
    add_test(NAME selftest COMMAND rp_selftest ${CMAKE_BINARY_DIR}/selftest-out)
    set_tests_properties(selftest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

install(TARGETS RegemarkPresent RUNTIME DESTINATION .)
