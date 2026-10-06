cmake_minimum_required(VERSION 3.20)
project(gxde-input-engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui WaylandClient Quick QuickControls2)
find_package(Wayland REQUIRED Client)

qt_standard_project_setup()
qt_add_resources(QRC_FILES src/ui/qml.qrc)

qt_add_executable(gxde-input-engine
    src/gxdeinputcore.h
    src/gxdeinputcore.cpp
    src/gxdepinyinengine.h
    src/gxdepinyinengine.cpp
    src/ui/CandidateBar.hpp
    src/ui/CandidateBar.cpp
    src/main.cpp
    ${QRC_FILES}
)

target_link_libraries(gxde-input-engine PRIVATE
    Qt6::Core Qt6::Gui Qt6::WaylandClient Qt6::Quick Qt6::QuickControls2
    Wayland::Client
)

install(TARGETS gxde-input-engine DESTINATION bin)
install(FILES protocol/zwp-text-input-v3.xml DESTINATION share/wayland/protocols)
