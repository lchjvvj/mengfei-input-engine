cmake_minimum_required(VERSION 3.20)
project(gxde-input-engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui WaylandClient Quick)
find_package(Wayland REQUIRED Client)

qt_standard_project_setup()

qt_add_executable(gxde-input-engine
    gxdeinputcore.h
    gxdeinputcore.cpp
    gxdepinyinengine.h
    gxdepinyinengine.cpp
    main.cpp
    main.h
)

target_link_libraries(gxde-input-engine PRIVATE Qt6::Core Qt6::Gui Qt6::WaylandClient Wayland::Client)

install(TARGETS gxde-input-engine DESTINATION bin)
# 安装wayland协议xml，用于编译protocol
install(FILES protocol/zwp-text-input-v3.xml DESTINATION share/wayland/protocols)
