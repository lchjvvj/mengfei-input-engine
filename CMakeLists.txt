cmake_minimum_required(VERSION 3.20)
project(gxde-input-engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui WaylandClient Quick QuickControls2)
find_package(Wayland REQUIRED Client)
# xkbcommon：raw keycode -> keysym 解析（键码映射骨架）
find_package(PkgConfig REQUIRED)
pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
# librime（Rime 中州韵）：拼音词库与候选引擎，替代内置 QMap 简易词库
pkg_check_modules(RIME REQUIRED IMPORTED_TARGET rime)

qt_standard_project_setup()
qt_add_resources(QRC_FILES src/ui/qml.qrc)

qt_add_executable(gxde-input-engine
    src/gxdeinputcore.h
    src/gxdeinputcore.cpp
    src/gxdedbusadapter.h
    src/gxdedbusadapter.cpp
    src/gxdexkbhandler.h
    src/gxdexkbhandler.cpp
    src/gxdepinyinengine.h
    src/gxdepinyinengine.cpp
    src/ui/CandidateBar.hpp
    src/ui/CandidateBar.cpp
    src/main.cpp
    ${QRC_FILES}
)

target_compile_definitions(gxde-input-engine PRIVATE GXDE_VERSION="${PROJECT_VERSION}")

target_link_libraries(gxde-input-engine PRIVATE
    Qt6::Core Qt6::DBus Qt6::Gui Qt6::WaylandClient Qt6::Quick Qt6::QuickControls2
    Wayland::Client
    PkgConfig::XKBCOMMON
    PkgConfig::RIME
)

install(TARGETS gxde-input-engine DESTINATION bin)
install(FILES protocol/zwp-text-input-v3.xml DESTINATION share/wayland/protocols)
# DBus 接口描述：gxde-wlcom 侧可用 qdbusxml2cpp 生成代理代码
install(FILES protocol/org.gxde.InputEngine.xml DESTINATION share/dbus-1/interfaces)
