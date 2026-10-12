cmake_minimum_required(VERSION 3.20)
project(mengfei-input-engine VERSION 0.2.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui WaylandClient Quick QuickControls2)
find_package(PkgConfig REQUIRED)
# libwayland-client：wayland-client 协议运行时库，引擎经 wl_display 直连
# gxde-wlcom 使用 zwp_input_method_v2/zwp_virtual_keyboard_v1。
# 探测方式与 wayland-scanner/xkbcommon/rime 统一：走 libwayland-dev 的
# 标准开发接口 wayland-client.pc。此前 find_package(Wayland) 实际靠
# qt6-base-dev 内部携带的 ECM find 模块（cmake/Qt6/3rdparty/
# extra-cmake-modules）解析，属 Qt 自身构建的实现细节，不对第三方工程
# 承诺可用（项目也未声明 extra-cmake-modules 构建依赖），不再依赖
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
# xkbcommon：raw keycode -> keysym 解析（键码映射骨架）
pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
# librime（Rime 中州韵）：拼音词库与候选引擎，替代内置 QMap 简易词库
pkg_check_modules(RIME REQUIRED IMPORTED_TARGET rime)

# wayland-scanner：从协议 XML 生成 C 绑定代码（libwayland-dev 提供）。
# 与 wayland-client 一致，直接从 pkg-config 取工具路径，不依赖 ECM
pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)
if(NOT WAYLAND_SCANNER)
    find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
endif()

# gxde-wlcom（wlroots 系合成器）支持的标准输入法协议，XML 与合成器内嵌
# wlroots 的 protocol 目录一致：input-method-unstable-v2 提供输入法上下文、
# 独占按键抓取与候选窗 popup surface，virtual-keyboard-unstable-v1 用于把
# 未消费按键转发回应用
set(WAYLAND_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/wayland-gen)
file(MAKE_DIRECTORY ${WAYLAND_GEN_DIR})
set(GXDE_WAYLAND_PROTOCOLS
    input-method-unstable-v2
    virtual-keyboard-unstable-v1
)
set(GXDE_WAYLAND_SOURCES "")
foreach(proto IN LISTS GXDE_WAYLAND_PROTOCOLS)
    add_custom_command(
        OUTPUT ${WAYLAND_GEN_DIR}/${proto}-client-protocol.h
               ${WAYLAND_GEN_DIR}/${proto}-protocol.c
        COMMAND ${WAYLAND_SCANNER} client-header
                ${CMAKE_CURRENT_SOURCE_DIR}/protocol/${proto}.xml
                ${WAYLAND_GEN_DIR}/${proto}-client-protocol.h
        COMMAND ${WAYLAND_SCANNER} private-code
                ${CMAKE_CURRENT_SOURCE_DIR}/protocol/${proto}.xml
                ${WAYLAND_GEN_DIR}/${proto}-protocol.c
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/protocol/${proto}.xml
        COMMENT "wayland-scanner 生成 ${proto} 客户端代码"
        VERBATIM
    )
    list(APPEND GXDE_WAYLAND_SOURCES
        ${WAYLAND_GEN_DIR}/${proto}-client-protocol.h
        ${WAYLAND_GEN_DIR}/${proto}-protocol.c)
endforeach()

qt_standard_project_setup()
qt_add_resources(QRC_FILES src/ui/qml.qrc)

qt_add_executable(mengfei-input-engine
    src/gxdeinputcore.h
    src/gxdeinputcore.cpp
    src/gxdedbusadapter.h
    src/gxdedbusadapter.cpp
    src/gxdexkbhandler.h
    src/gxdexkbhandler.cpp
    src/gxdewaylandim.h
    src/gxdewaylandim.cpp
    src/gxdepinyinengine.h
    src/gxdepinyinengine.cpp
    src/gxdesettings.h
    src/gxdesettings.cpp
    src/ui/CandidateBar.hpp
    src/ui/CandidateBar.cpp
    src/ui/SettingsPanel.hpp
    src/ui/SettingsPanel.cpp
    src/main.cpp
    ${GXDE_WAYLAND_SOURCES}
    ${QRC_FILES}
)

target_compile_definitions(mengfei-input-engine PRIVATE GXDE_VERSION="${PROJECT_VERSION}")

# wayland-scanner 生成的 *-client-protocol.h 所在目录
target_include_directories(mengfei-input-engine PRIVATE ${WAYLAND_GEN_DIR})

target_link_libraries(mengfei-input-engine PRIVATE
    Qt6::Core Qt6::DBus Qt6::Gui Qt6::WaylandClient Qt6::Quick Qt6::QuickControls2
    PkgConfig::WAYLAND_CLIENT
    PkgConfig::XKBCOMMON
    PkgConfig::RIME
)

install(TARGETS mengfei-input-engine DESTINATION bin)
install(FILES protocol/zwp-text-input-v3.xml
              protocol/input-method-unstable-v2.xml
              protocol/virtual-keyboard-unstable-v1.xml
        DESTINATION share/wayland/protocols)
