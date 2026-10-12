#include "gxdewaylandim.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <utility>

#include <QByteArray>
#include <QDebug>
#include <QHash>
#include <QImage>
#include <QList>
#include <QSocketNotifier>
#include <QString>
#include <QTimer>

#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>

// wayland-scanner 由 protocol/*.xml 生成（XML 与 gxde-wlcom 内嵌
// wlroots 的 protocol 目录保持一致，事件签名以合成器实现为准）
#include "input-method-unstable-v2-client-protocol.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"

#include "gxdeinputcore.h"
#include "gxdepinyinengine.h"
#include "ui/CandidateBar.hpp"

namespace {
// Wayland 连接失败/断开后的重试间隔：合成器可能在启动或重启，不宜忙轮询
constexpr int kReconnectIntervalMs = 5000;
// wl_seat 绑定版本上限：seat 仅作为 get_input_method /
// create_virtual_keyboard 的参数使用，不消费其键盘能力
constexpr uint32_t kSeatVersionMax = 9;
// wl_compositor 绑定版本上限：v3 起提供 set_buffer_scale，
// 候选窗按输出缩放提交缓冲时需要
constexpr uint32_t kCompositorVersionMax = 6;
// wl_output 绑定版本：v2 起提供 scale 事件，足够候选窗决定渲染缩放
constexpr uint32_t kOutputVersionMax = 2;
}

class GXDEWaylandIM::Private
{
public:
    Private(GXDEWaylandIM *qq, GXDEInputCore *corePtr, GXDEPinyinEngine *pinyinPtr,
            CandidateBar *uiPtr);
    ~Private();

    // ---- 连接管理 ----
    bool connectToCompositor();
    void disconnectFromCompositor();
    // 延迟到下一轮事件循环再断开：不能在 Wayland 事件回调（dispatch 栈）
    // 或 QSocketNotifier 自身回调里销毁 display/notifier
    void teardownAsync(const QString &reason);
    void scheduleReconnect();
    void flushRequests();
    void closePendingFds();

    // ---- 协议对象建立 ----
    void setupInputMethod();
    void setupVirtualKeyboard();
    // 候选窗 popup surface：需要 compositor 与 inputMethod 都就绪
    //（全局对象到达顺序不定，两路 setup 都会尝试）
    void setupPopupSurface();

    // ---- 合成器事件处理 ----
    void handleActivate();
    void handleDeactivate();
    void handleKeymap(uint32_t format, int32_t fd, uint32_t size);
    void handleKey(uint32_t time, uint32_t key, uint32_t state);
    void handleModifiers(uint32_t depressed, uint32_t latched,
                         uint32_t locked, uint32_t group);
    void handleRepeatInfo(int32_t rate, int32_t delay);
    void processKeyEvent(uint32_t time, uint32_t key, uint32_t state);
    void forwardKey(uint32_t time, uint32_t key, uint32_t state);

    // ---- 状态写回 ----
    void scheduleStateFlush();
    void flushState();

    // ---- 候选窗（zwp_input_popup_surface_v2）----
    // 把 pendingFrame 拷贝进新建的 wl_shm 缓冲提交到 popup surface；
    // 空帧提交空缓冲，候选窗随内容隐藏
    void commitPopupFrame();
    // popup surface 所在输出的 scale 作为候选窗渲染缩放，变化时
    // 通知 CandidateBar 按新缩放重绘
    void updatePopupScale();

    // ---- Wayland 静态监听器（data 指向本 Private）----
    static void registryGlobal(void *data, wl_registry *registry, uint32_t name,
                               const char *interface, uint32_t version);
    static void registryGlobalRemove(void *data, wl_registry *registry, uint32_t name);
    static void seatName(void *data, wl_seat *seat, const char *name);
    static void seatCapabilities(void *data, wl_seat *seat, uint32_t caps);
    static void imActivate(void *data, zwp_input_method_v2 *im);
    static void imDeactivate(void *data, zwp_input_method_v2 *im);
    static void imSurroundingText(void *data, zwp_input_method_v2 *im,
                                  const char *text, uint32_t cursor, uint32_t anchor);
    static void imTextChangeCause(void *data, zwp_input_method_v2 *im, uint32_t cause);
    static void imContentType(void *data, zwp_input_method_v2 *im,
                              uint32_t hint, uint32_t purpose);
    static void imDone(void *data, zwp_input_method_v2 *im);
    static void imUnavailable(void *data, zwp_input_method_v2 *im);
    static void grabKeymap(void *data, zwp_input_method_keyboard_grab_v2 *grab,
                           uint32_t format, int32_t fd, uint32_t size);
    static void grabKey(void *data, zwp_input_method_keyboard_grab_v2 *grab,
                        uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
    static void grabModifiers(void *data, zwp_input_method_keyboard_grab_v2 *grab,
                              uint32_t serial, uint32_t depressed, uint32_t latched,
                              uint32_t locked, uint32_t group);
    static void grabRepeatInfo(void *data, zwp_input_method_keyboard_grab_v2 *grab,
                               int32_t rate, int32_t delay);
    static void popupTextInputRectangle(void *data, zwp_input_popup_surface_v2 *popup,
                                        int32_t x, int32_t y, int32_t width, int32_t height);
    static void popupSurfaceEnter(void *data, wl_surface *surface, wl_output *output);
    static void popupSurfaceLeave(void *data, wl_surface *surface, wl_output *output);
    static void outputGeometry(void *data, wl_output *output, int32_t x, int32_t y,
                               int32_t physicalWidth, int32_t physicalHeight,
                               int32_t subpixel, const char *make, const char *model,
                               int32_t transform);
    static void outputMode(void *data, wl_output *output, uint32_t flags,
                           int32_t width, int32_t height, int32_t refresh);
    static void outputDone(void *data, wl_output *output);
    static void outputScale(void *data, wl_output *output, int32_t factor);
    static void bufferRelease(void *data, wl_buffer *buffer);

    // 监听器结构体：字段顺序必须与协议 XML 的事件声明顺序一致
    static const struct wl_registry_listener kRegistryListener;
    static const struct wl_seat_listener kSeatListener;
    static const struct zwp_input_method_v2_listener kInputMethodListener;
    static const struct zwp_input_method_keyboard_grab_v2_listener kKeyboardGrabListener;
    static const struct zwp_input_popup_surface_v2_listener kInputPopupListener;
    static const struct wl_surface_listener kPopupSurfaceListener;
    static const struct wl_output_listener kOutputListener;
    static const struct wl_buffer_listener kBufferListener;

    GXDEWaylandIM *q;
    GXDEInputCore *core;
    GXDEPinyinEngine *pinyin;
    // 候选窗渲染器：其 frameChanged 帧经 popup surface 提交显示
    CandidateBar *ui;

    // Wayland 连接与协议对象（断开后全部置空，重连时重建）
    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    wl_seat *seat = nullptr;
    wl_compositor *compositor = nullptr;
    wl_shm *shm = nullptr;
    zwp_input_method_manager_v2 *imManager = nullptr;
    zwp_virtual_keyboard_manager_v1 *vkManager = nullptr;
    zwp_input_method_v2 *inputMethod = nullptr;
    zwp_input_method_keyboard_grab_v2 *keyboardGrab = nullptr;
    zwp_virtual_keyboard_v1 *virtualKeyboard = nullptr;
    // 候选窗：专用 wl_surface + input_popup 角色包装（合成器负责放置）
    wl_surface *popupSurface = nullptr;
    zwp_input_popup_surface_v2 *inputPopup = nullptr;

    // 已绑定全局对象的注册表名字（0 表示未绑定）：global_remove 时
    // 这几个被移除意味着连接不可用，需要重连；输出等其它全局对象
    // 变化不影响连接（输出热插拔单独处理）
    uint32_t boundSeatName = 0;
    uint32_t boundImManagerName = 0;
    uint32_t boundVkManagerName = 0;
    uint32_t boundCompositorName = 0;
    uint32_t boundShmName = 0;
    // wl_compositor 实际绑定版本：v3 起才有 set_buffer_scale 请求
    uint32_t compositorVersion = 0;

    // 输出缩放跟踪：popup surface 的 enter/leave 事件给出 wl_output 代理，
    // 据此查询 scale 决定候选窗渲染与提交的缓冲缩放（HiDPI）
    QHash<uint32_t, wl_output *> outputsByName;  // 注册表名字 -> 代理
    QHash<wl_output *, int32_t> outputScales;    // 代理 -> scale（缺省 1）
    QList<wl_output *> popupOutputs;             // popup surface 已进入的输出
    int32_t popupScale = 1;                      // 当前候选窗渲染/提交缩放

    // Qt 事件循环接入
    QSocketNotifier *readNotifier = nullptr;
    QSocketNotifier *writeNotifier = nullptr;
    QTimer reconnectTimer;   // 断线重试（单次触发）
    QTimer repeatTimer;      // 按键自动重复（单次触发：先 delay，后按 rate 间隔重启）
    bool teardownScheduled = false;

    // 状态写回
    bool flushScheduled = false;  // singleShot(0) 合并标志：一轮多次变化只 commit 一次
    uint32_t serial = 0;          // 已收到 done 事件计数，commit 请求的协议 serial
    QString preeditText;          // 当前 preedit；pending 状态每次 commit 后清零，需全量重发
    QByteArray pendingCommit;     // 待上屏文本（UTF-8），commit 后清空

    // 候选窗最新帧：popup surface 未就绪（compositor 全局对象晚到/重连中）
    // 时暂存，就绪后补提交；空帧表示候选窗隐藏
    QImage pendingFrame;

    // 按键抓取状态
    bool vkHasKeymap = false;     // 协议约束：虚拟键盘发 key/modifiers 前必须先发 keymap
    int deferredKeymapFd = -1;    // vk 未就绪时暂存的合成器 keymap fd，就绪后补发
    uint32_t deferredKeymapSize = 0;
    QList<int> pendingFds;        // 已入队消息引用的 fd，flush 成功后才能关闭
    uint32_t repeatKey = 0;       // 正在自动重复的按键（evdev keycode）
    uint32_t repeatTime = 0;      // 重复事件的合成时间戳：沿用原事件时基递增，避免跳变
    int repeatRate = 0;           // 重复速率（字符/秒），0 表示禁用重复
    int repeatDelay = 0;          // 按下到开始重复的延迟（毫秒）
};

const struct wl_registry_listener GXDEWaylandIM::Private::kRegistryListener = {
    registryGlobal,
    registryGlobalRemove,
};

const struct wl_seat_listener GXDEWaylandIM::Private::kSeatListener = {
    seatCapabilities,
    seatName,
};

const struct zwp_input_method_v2_listener GXDEWaylandIM::Private::kInputMethodListener = {
    imActivate,
    imDeactivate,
    imSurroundingText,
    imTextChangeCause,
    imContentType,
    imDone,
    imUnavailable,
};

const struct zwp_input_method_keyboard_grab_v2_listener GXDEWaylandIM::Private::kKeyboardGrabListener = {
    grabKeymap,
    grabKey,
    grabModifiers,
    grabRepeatInfo,
};

const struct zwp_input_popup_surface_v2_listener GXDEWaylandIM::Private::kInputPopupListener = {
    popupTextInputRectangle,
};

const struct wl_surface_listener GXDEWaylandIM::Private::kPopupSurfaceListener = {
    popupSurfaceEnter,
    popupSurfaceLeave,
};

// wl_output 绑定在 v2（scale 是 v2 事件），name/description（v4）
// 留空即可：服务端不会向低版本绑定发送
const struct wl_output_listener GXDEWaylandIM::Private::kOutputListener = {
    outputGeometry,
    outputMode,
    outputDone,
    outputScale,
};

const struct wl_buffer_listener GXDEWaylandIM::Private::kBufferListener = {
    bufferRelease,
};

GXDEWaylandIM::GXDEWaylandIM(GXDEInputCore *core, GXDEPinyinEngine *pinyin,
                             CandidateBar *ui, QObject *parent)
    : QObject(parent), d(new Private(this, core, pinyin, ui))
{
}

GXDEWaylandIM::~GXDEWaylandIM()
{
    delete d;
}

void GXDEWaylandIM::start()
{
    if (!d->connectToCompositor())
        d->scheduleReconnect();
}

GXDEWaylandIM::Private::Private(GXDEWaylandIM *qq, GXDEInputCore *corePtr,
                                GXDEPinyinEngine *pinyinPtr, CandidateBar *uiPtr)
    : q(qq), core(corePtr), pinyin(pinyinPtr), ui(uiPtr)
{
    reconnectTimer.setSingleShot(true);
    QObject::connect(&reconnectTimer, &QTimer::timeout, q, [this] {
        if (!display && !connectToCompositor())
            scheduleReconnect();
    });

    // 按键自动重复：合成器把按键路由给输入法后停止了自身的重复
    //（keyboard_repeat_stop），由这里按 repeat_info 补齐。单次触发模式：
    // 按下时 start(delay)，每次重复后按 1000/rate 重启
    repeatTimer.setSingleShot(true);
    QObject::connect(&repeatTimer, &QTimer::timeout, q, [this] {
        if (repeatRate <= 0)
            return;
        const int interval = qMax(1, 1000 / repeatRate);
        repeatTime += static_cast<uint32_t>(interval);
        const bool consumed = core->handleKeyEvent(repeatKey, true);
        if (!consumed)
            forwardKey(repeatTime, repeatKey, WL_KEYBOARD_KEY_STATE_PRESSED);
        repeatTimer.start(interval);
    });

    // preedit 写回：拼音引擎 preeditUpdate 出站链路
    QObject::connect(pinyin, &GXDEPinyinEngine::preeditUpdate, q,
                     [this](const QString &text) {
        preeditText = text;
        scheduleStateFlush();
    });
    // 上屏写回：core 的 textCommitted 出站链路
    QObject::connect(core, &GXDEInputCore::textCommitted, q,
                     [this](const QString &text) {
        pendingCommit.append(text.toUtf8());
        scheduleStateFlush();
    });
    // 候选窗帧：CandidateBar（QPainter 渲染）出帧后拷贝进 wl_shm 缓冲，
    // 经 zwp_input_popup_surface_v2 提交显示；空帧提交空缓冲隐藏候选窗
    QObject::connect(ui, &CandidateBar::frameChanged, q, [this](const QImage &frame) {
        pendingFrame = frame;
        commitPopupFrame();
    });
}

GXDEWaylandIM::Private::~Private()
{
    disconnectFromCompositor();
}

bool GXDEWaylandIM::Private::connectToCompositor()
{
    if (display)
        return true;

    // 独立连接：不依赖 Qt WaylandClient 的私有 display，与设置面板
    //（Qt 窗口）的连接互不干扰（fcitx5 同款做法）
    display = wl_display_connect(nullptr);
    if (!display) {
        qWarning() << "[GXDEWaylandIM] 无法连接 Wayland 合成器（WAYLAND_DISPLAY 不可用？）";
        return false;
    }

    registry = wl_display_get_registry(display);
    if (registry)
        wl_registry_add_listener(registry, &kRegistryListener, this);
    // 首次阻塞 roundtrip：等全局对象列表送达，registryGlobal 回调里完成
    // manager/seat 绑定并触发 setupInputMethod/setupVirtualKeyboard
    if (!registry || wl_display_roundtrip(display) < 0) {
        qWarning() << "[GXDEWaylandIM] 注册表 roundtrip 失败，错误码:"
                   << (display ? wl_display_get_error(display) : -1);
        disconnectFromCompositor();
        return false;
    }

    const int fd = wl_display_get_fd(display);
    readNotifier = new QSocketNotifier(fd, QSocketNotifier::Read);
    QObject::connect(readNotifier, &QSocketNotifier::activated, q, [this] {
        // fd 可读时 dispatch 不会长时间阻塞：内部完成读入与事件分发
        wl_display_dispatch(display);
        if (wl_display_get_error(display) != 0) {
            teardownAsync(QStringLiteral("Wayland 连接错误"));
            return;
        }
        flushRequests();
    });
    // 写通知仅在发送缓冲区满（EAGAIN）时启用，平时关闭避免空转
    writeNotifier = new QSocketNotifier(fd, QSocketNotifier::Write);
    writeNotifier->setEnabled(false);
    QObject::connect(writeNotifier, &QSocketNotifier::activated, q,
                     [this] { flushRequests(); });

    if (inputMethod)
        qInfo() << "[GXDEWaylandIM] 已连接合成器，zwp_input_method_v2 就绪";
    else
        qWarning() << "[GXDEWaylandIM] 合成器未提供 zwp_input_method_manager_v2，"
                      "输入法不会生效（等待全局对象出现后重连）";
    flushRequests();
    return true;
}

void GXDEWaylandIM::Private::disconnectFromCompositor()
{
    repeatTimer.stop();
    // notifier 可能正处于自身回调栈中：deleteLater 延迟销毁
    if (readNotifier) {
        readNotifier->setEnabled(false);
        readNotifier->deleteLater();
        readNotifier = nullptr;
    }
    if (writeNotifier) {
        writeNotifier->setEnabled(false);
        writeNotifier->deleteLater();
        writeNotifier = nullptr;
    }
    closePendingFds();
    if (deferredKeymapFd >= 0) {
        close(deferredKeymapFd);
        deferredKeymapFd = -1;
        deferredKeymapSize = 0;
    }
    // wl_display_disconnect 会释放连接上的全部客户端代理：协议对象只需置空，
    // 合成器侧在客户端断开后自动清理输入法/虚拟键盘/popup surface 资源
    if (display)
        wl_display_disconnect(display);
    display = nullptr;
    registry = nullptr;
    seat = nullptr;
    compositor = nullptr;
    shm = nullptr;
    imManager = nullptr;
    vkManager = nullptr;
    inputMethod = nullptr;
    keyboardGrab = nullptr;
    virtualKeyboard = nullptr;
    popupSurface = nullptr;
    inputPopup = nullptr;
    boundSeatName = 0;
    boundImManagerName = 0;
    boundVkManagerName = 0;
    boundCompositorName = 0;
    boundShmName = 0;
    compositorVersion = 0;
    // 输出代理随连接一并失效：只清跟踪表，不发 destroy 请求
    //（pendingFrame 保留：重连重建 popup surface 后补提交）
    outputsByName.clear();
    outputScales.clear();
    popupOutputs.clear();
    popupScale = 1;

    vkHasKeymap = false;
    serial = 0;
    preeditText.clear();
    pendingCommit.clear();
    repeatKey = 0;
    repeatTime = 0;
    repeatRate = 0;
    repeatDelay = 0;
}

void GXDEWaylandIM::Private::teardownAsync(const QString &reason)
{
    if (teardownScheduled)
        return;
    teardownScheduled = true;
    qWarning() << "[GXDEWaylandIM]" << reason << ","
               << kReconnectIntervalMs << "ms 后重连";
    // 不能在 Wayland 事件回调（dispatch 栈）或 notifier 自身回调里直接
    // 销毁 display/notifier，延迟到下一轮事件循环
    QTimer::singleShot(0, q, [this] {
        teardownScheduled = false;
        disconnectFromCompositor();
        scheduleReconnect();
    });
}

void GXDEWaylandIM::Private::scheduleReconnect()
{
    if (!reconnectTimer.isActive())
        reconnectTimer.start(kReconnectIntervalMs);
}

void GXDEWaylandIM::Private::flushRequests()
{
    if (!display)
        return;
    const int ret = wl_display_flush(display);
    if (ret >= 0) {
        // 全部请求已写入套接字：消息中引用的 keymap fd 可以安全关闭了
        if (writeNotifier)
            writeNotifier->setEnabled(false);
        closePendingFds();
        return;
    }
    if (errno == EAGAIN) {
        // 套接字缓冲区满：等待可写通知后续传，此时 fd 不能提前关闭
        if (writeNotifier)
            writeNotifier->setEnabled(true);
        return;
    }
    teardownAsync(QStringLiteral("刷新请求失败: %1").arg(strerror(errno)));
}

void GXDEWaylandIM::Private::closePendingFds()
{
    for (const int fd : std::as_const(pendingFds))
        close(fd);
    pendingFds.clear();
}

void GXDEWaylandIM::Private::setupInputMethod()
{
    if (!imManager || !seat || inputMethod)
        return;
    inputMethod = zwp_input_method_manager_v2_get_input_method(imManager, seat);
    zwp_input_method_v2_add_listener(inputMethod, &kInputMethodListener, this);
    setupPopupSurface();
}

void GXDEWaylandIM::Private::setupVirtualKeyboard()
{
    if (!vkManager || !seat || virtualKeyboard)
        return;
    virtualKeyboard =
        zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(vkManager, seat);
    // keymap 事件先于 vk 就绪时暂存的 fd：现在补发
    //（协议要求发 key/modifiers 前必须先设置 keymap）
    if (deferredKeymapFd >= 0) {
        zwp_virtual_keyboard_v1_keymap(virtualKeyboard,
                                       WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                                       deferredKeymapFd, deferredKeymapSize);
        pendingFds.append(deferredKeymapFd);
        deferredKeymapFd = -1;
        deferredKeymapSize = 0;
        vkHasKeymap = true;
    }
}

void GXDEWaylandIM::Private::setupPopupSurface()
{
    if (!compositor || !inputMethod || popupSurface)
        return;
    // 专用 surface + input_popup 角色：合成器负责把它放置在焦点文本
    // 输入区域附近（下方放不下翻转到上方，横向夹取到输出内），并且
    // 只在输入法激活状态显示——引擎不再需要光标坐标与自行定位
    popupSurface = wl_compositor_create_surface(compositor);
    wl_surface_add_listener(popupSurface, &kPopupSurfaceListener, this);
    inputPopup = zwp_input_method_v2_get_input_popup_surface(inputMethod, popupSurface);
    zwp_input_popup_surface_v2_add_listener(inputPopup, &kInputPopupListener, this);
    // popup 就绪前（compositor 全局对象晚到/重连）缓存的候选帧补提交
    if (!pendingFrame.isNull())
        commitPopupFrame();
}

void GXDEWaylandIM::Private::commitPopupFrame()
{
    if (!popupSurface || !shm)
        return;  // 帧已缓存在 pendingFrame，setupPopupSurface 后补提交
    if (pendingFrame.isNull()) {
        // 空帧：解除缓冲挂接，候选窗随内容隐藏
        wl_surface_attach(popupSurface, nullptr, 0, 0);
        wl_surface_damage(popupSurface, 0, 0, INT32_MAX, INT32_MAX);
        wl_surface_commit(popupSurface);
        flushRequests();
        return;
    }

    QImage frame = pendingFrame;
    if (frame.format() != QImage::Format_ARGB32_Premultiplied)
        frame = frame.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int width = frame.width();
    const int height = frame.height();
    const int stride = width * 4;
    const int size = stride * height;

    // 匿名内存文件作为 wl_shm 池数据源：每帧新建 pool+buffer（候选帧
    // 按按键节奏产生，频率低），pool 建完 buffer 即销毁，缓冲由合成器
    // release 事件后销毁代理，避免复用同步问题
    const int fd = memfd_create("gxde-im-popup", MFD_CLOEXEC);
    if (fd < 0) {
        qWarning() << "[GXDEWaylandIM] memfd_create 失败:" << strerror(errno);
        return;
    }
    if (ftruncate(fd, size) < 0) {
        qWarning() << "[GXDEWaylandIM] ftruncate 失败:" << strerror(errno);
        close(fd);
        return;
    }
    void *map = mmap(nullptr, static_cast<size_t>(size), PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        qWarning() << "[GXDEWaylandIM] mmap 候选帧缓冲失败:" << strerror(errno);
        close(fd);
        return;
    }
    if (frame.bytesPerLine() == stride) {
        memcpy(map, frame.constBits(), static_cast<size_t>(size));
    } else {
        // QImage 行末可能有对齐填充：逐行拷贝成紧凑 stride
        for (int y = 0; y < height; ++y)
            memcpy(static_cast<char *>(map) + static_cast<size_t>(y) * stride,
                   frame.constScanLine(y), static_cast<size_t>(stride));
    }
    munmap(map, static_cast<size_t>(size));

    wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
                                                  WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    wl_buffer_add_listener(buffer, &kBufferListener, this);
    // fd 要等消息真正写入套接字（flush 成功）后才能关闭，与 keymap fd 同约定
    pendingFds.append(fd);

    wl_surface_attach(popupSurface, buffer, 0, 0);
    // set_buffer_scale 是 wl_surface v3 请求：低版本绑定时跳过
    //（帧按 scale=1 渲染，逻辑尺寸即物理尺寸）。scale 跨 commit 保持，
    // 缩放变化（含降回 1）都必须显式重发
    if (compositorVersion >= 3)
        wl_surface_set_buffer_scale(popupSurface, popupScale);
    wl_surface_damage(popupSurface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(popupSurface);
    flushRequests();
}

void GXDEWaylandIM::Private::updatePopupScale()
{
    // popup surface 只会在一个输出上显示：取首个进入输出的 scale
    int32_t scale = 1;
    if (!popupOutputs.isEmpty())
        scale = outputScales.value(popupOutputs.first(), 1);
    if (scale == popupScale)
        return;
    popupScale = scale;
    // 通知渲染器按新缩放重绘：frameChanged 回来后经 commitPopupFrame
    // 以新尺寸缓冲提交（set_buffer_scale 保证逻辑位置/尺寸不变）
    const qint64 frameBefore = pendingFrame.cacheKey();
    ui->setDevicePixelRatio(popupScale);
    // 系数与渲染端一致时（如断线重连把 popupScale 重置回 1）不会出
    // 新帧，需重提交缓存帧，让 buffer_scale 与帧的实际缩放对齐
    if (pendingFrame.cacheKey() == frameBefore)
        commitPopupFrame();
}

void GXDEWaylandIM::Private::handleActivate()
{
    core->activateContext();
    // 独占按键抓取：此后合成器把 seat 的键盘事件路由给输入法而非直达应用。
    // grab 对象跨 activate/deactivate 保留，只创建一次
    if (!keyboardGrab) {
        keyboardGrab = zwp_input_method_v2_grab_keyboard(inputMethod);
        zwp_input_method_keyboard_grab_v2_add_listener(keyboardGrab,
                                                       &kKeyboardGrabListener, this);
    }
    flushRequests();
    qDebug() << "[GXDEWaylandIM] 输入上下文激活";
}

void GXDEWaylandIM::Private::handleDeactivate()
{
    repeatTimer.stop();
    // 先清拼音状态：触发空 preedit 写回并隐藏候选窗（空帧提交空缓冲，
    // 合成器同时按协议隐藏 popup surface），再关闭输入上下文
    pinyin->clear();
    core->deactivateContext();
    flushRequests();
    qDebug() << "[GXDEWaylandIM] 输入上下文失活";
}

void GXDEWaylandIM::Private::handleKeymap(uint32_t format, int32_t fd, uint32_t size)
{
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || fd < 0 || size == 0) {
        qWarning() << "[GXDEWaylandIM] 无法处理的 keymap 格式:" << format;
        if (fd >= 0)
            close(fd);
        return;
    }

    // mmap 读出完整 keymap 字符串（size 含结尾 NUL）
    void *map = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        qWarning() << "[GXDEWaylandIM] mmap keymap 失败:" << strerror(errno);
        close(fd);
        return;
    }
    const QByteArray keymapString(static_cast<const char *>(map),
                                  static_cast<int>(size));
    munmap(map, size);

    // core 侧同步布局：替换 RMLVO 默认 keymap，保证 keysym 解析与合成器一致
    core->loadKeymap(keymapString);

    if (virtualKeyboard) {
        // 同一个 fd 转发给虚拟键盘：消息真正写入套接字（flush 成功）前
        // 不能关闭，先挂入 pendingFds
        zwp_virtual_keyboard_v1_keymap(virtualKeyboard,
                                       WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, size);
        pendingFds.append(fd);
        vkHasKeymap = true;
        flushRequests();
    } else {
        // vk 未就绪（vkManager 全局对象晚到）：暂存 fd，setupVirtualKeyboard 补发
        if (deferredKeymapFd >= 0)
            close(deferredKeymapFd);
        deferredKeymapFd = fd;
        deferredKeymapSize = size;
    }
}

void GXDEWaylandIM::Private::handleKey(uint32_t time, uint32_t key, uint32_t state)
{
    // 任何新按键事件都先停掉旧按键的自动重复
    repeatTimer.stop();
    processKeyEvent(time, key, state);
}

void GXDEWaylandIM::Private::processKeyEvent(uint32_t time, uint32_t key, uint32_t state)
{
    const bool pressed = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
    // 引擎决定消费与否：中文模式下参与组字的按键被消费，其余一律透传
    const bool consumed = core->handleKeyEvent(key, pressed);
    // 未消费的按键经虚拟键盘转发：合成器识别其来自输入法
    //（keyboard_is_from_input_method）并直通焦点应用
    if (!consumed)
        forwardKey(time, key, state);
    // 合成器已对路由给输入法的按键停止自动重复，这里补齐：
    // 先 delay 后按 rate 周期（是否可重复由 xkb 语义决定）
    if (pressed && repeatRate > 0 && core->keyRepeats(key)) {
        repeatKey = key;
        repeatTime = time;
        repeatTimer.start(repeatDelay);
    }
}

void GXDEWaylandIM::Private::forwardKey(uint32_t time, uint32_t key, uint32_t state)
{
    // 协议约束：keymap 之前发 key 会被合成器按 no_keymap 错误断开连接
    if (!virtualKeyboard || !vkHasKeymap)
        return;
    zwp_virtual_keyboard_v1_key(virtualKeyboard, time, key, state);
    flushRequests();
}

void GXDEWaylandIM::Private::handleModifiers(uint32_t depressed, uint32_t latched,
                                             uint32_t locked, uint32_t group)
{
    // 同步 xkb 修饰状态：保证 Shift+字母等组合键的 keysym 解析与合成器一致
    core->updateModifiers(depressed, latched, locked, group);
    // 合成器把修饰键路由给输入法后不再通知 seat，
    // 必须经虚拟键盘转发，否则应用侧 Ctrl/Shift/Alt 状态丢失
    if (virtualKeyboard && vkHasKeymap) {
        zwp_virtual_keyboard_v1_modifiers(virtualKeyboard, depressed, latched,
                                          locked, group);
        flushRequests();
    }
}

void GXDEWaylandIM::Private::handleRepeatInfo(int32_t rate, int32_t delay)
{
    // 协议约定：负值非法；rate 为 0 时禁用重复
    repeatRate = rate > 0 ? rate : 0;
    repeatDelay = delay > 0 ? delay : 0;
    if (repeatRate == 0)
        repeatTimer.stop();
}

void GXDEWaylandIM::Private::scheduleStateFlush()
{
    if (flushScheduled || !inputMethod)
        return;
    flushScheduled = true;
    // 同一轮事件循环内的多次状态变化合并为一次 commit：协议要求 serial
    // 与 done 计数一致，同一轮连发多次 commit 会因 serial 不匹配被丢弃
    QTimer::singleShot(0, q, [this] { flushState(); });
}

void GXDEWaylandIM::Private::flushState()
{
    flushScheduled = false;
    if (!inputMethod)
        return;
    if (!pendingCommit.isEmpty()) {
        zwp_input_method_v2_commit_string(inputMethod, pendingCommit.constData());
        pendingCommit.clear();
    }
    // pending 状态每次 commit 后清零：preedit 每轮都要全量重发，
    // 不发送 set_preedit_string 即表示清空应用侧预编辑
    if (!preeditText.isEmpty()) {
        const QByteArray utf8 = preeditText.toUtf8();
        // 协议约定：cursor_begin/cursor_end 是 UTF-8 缓冲区内的字节偏移
        const int cursor = utf8.size();
        zwp_input_method_v2_set_preedit_string(inputMethod, utf8.constData(),
                                               cursor, cursor);
    }
    zwp_input_method_v2_commit(inputMethod, serial);
    flushRequests();
}

void GXDEWaylandIM::Private::registryGlobal(void *data, wl_registry *registry,
                                            uint32_t name, const char *interface,
                                            uint32_t version)
{
    auto *self = static_cast<Private *>(data);
    if (strcmp(interface, zwp_input_method_manager_v2_interface.name) == 0) {
        self->imManager = static_cast<zwp_input_method_manager_v2 *>(wl_registry_bind(
            registry, name, &zwp_input_method_manager_v2_interface, qMin(version, 1u)));
        self->boundImManagerName = name;
        self->setupInputMethod();
    } else if (strcmp(interface, zwp_virtual_keyboard_manager_v1_interface.name) == 0) {
        self->vkManager = static_cast<zwp_virtual_keyboard_manager_v1 *>(wl_registry_bind(
            registry, name, &zwp_virtual_keyboard_manager_v1_interface, qMin(version, 1u)));
        self->boundVkManagerName = name;
        self->setupVirtualKeyboard();
    } else if (strcmp(interface, wl_seat_interface.name) == 0 && !self->seat) {
        // 只使用第一个 seat：与 gxde-wlcom 默认 seat0 单座位场景一致
        self->seat = static_cast<wl_seat *>(wl_registry_bind(
            registry, name, &wl_seat_interface, qMin(version, kSeatVersionMax)));
        self->boundSeatName = name;
        wl_seat_add_listener(self->seat, &kSeatListener, self);
        self->setupInputMethod();
        self->setupVirtualKeyboard();
    } else if (strcmp(interface, wl_compositor_interface.name) == 0) {
        // 候选窗 popup surface 的 wl_surface 由它创建
        self->compositorVersion = qMin(version, kCompositorVersionMax);
        self->compositor = static_cast<wl_compositor *>(wl_registry_bind(
            registry, name, &wl_compositor_interface, self->compositorVersion));
        self->boundCompositorName = name;
        self->setupPopupSurface();
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        // 候选帧缓冲数据源（wl_shm v1 即够用）
        self->shm = static_cast<wl_shm *>(wl_registry_bind(
            registry, name, &wl_shm_interface, 1u));
        self->boundShmName = name;
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        // 绑定全部输出：popup surface 的 enter/leave 事件给出输出代理，
        // 据此查询 scale 决定候选窗渲染缩放（v2 起才有 scale 事件）
        auto *output = static_cast<wl_output *>(wl_registry_bind(
            registry, name, &wl_output_interface, qMin(version, kOutputVersionMax)));
        self->outputsByName.insert(name, output);
        wl_output_add_listener(output, &kOutputListener, self);
    }
}

void GXDEWaylandIM::Private::registryGlobalRemove(void *data, wl_registry *registry,
                                                  uint32_t name)
{
    Q_UNUSED(registry);
    auto *self = static_cast<Private *>(data);
    // 输出热插拔：释放对应代理并重算候选窗缩放，不影响输入法连接
    const auto it = self->outputsByName.find(name);
    if (it != self->outputsByName.end()) {
        wl_output *output = it.value();
        self->outputsByName.erase(it);
        self->outputScales.remove(output);
        self->popupOutputs.removeAll(output);
        wl_output_destroy(output);
        self->updatePopupScale();
        return;
    }
    // 已绑定的核心全局对象被移除意味着连接不可用，整体重连
    if (name != self->boundSeatName && name != self->boundImManagerName
        && name != self->boundVkManagerName && name != self->boundCompositorName
        && name != self->boundShmName)
        return;
    self->teardownAsync(QStringLiteral("已绑定的全局对象被移除: %1").arg(name));
}

void GXDEWaylandIM::Private::seatName(void *data, wl_seat *seat, const char *name)
{
    Q_UNUSED(data);
    Q_UNUSED(seat);
    qDebug() << "[GXDEWaylandIM] seat:" << name;
}

void GXDEWaylandIM::Private::seatCapabilities(void *data, wl_seat *seat, uint32_t caps)
{
    Q_UNUSED(data);
    Q_UNUSED(seat);
    Q_UNUSED(caps);
    // 键盘能力经输入法按键抓取（grab_keyboard）提供，无需在 seat 上绑定 wl_keyboard
}

void GXDEWaylandIM::Private::imActivate(void *data, zwp_input_method_v2 *im)
{
    Q_UNUSED(im);
    static_cast<Private *>(data)->handleActivate();
}

void GXDEWaylandIM::Private::imDeactivate(void *data, zwp_input_method_v2 *im)
{
    Q_UNUSED(im);
    static_cast<Private *>(data)->handleDeactivate();
}

void GXDEWaylandIM::Private::imSurroundingText(void *data, zwp_input_method_v2 *im,
                                               const char *text, uint32_t cursor,
                                               uint32_t anchor)
{
    Q_UNUSED(data);
    Q_UNUSED(im);
    Q_UNUSED(text);
    Q_UNUSED(cursor);
    Q_UNUSED(anchor);
    // 周边文本用于候选改写（delete_surrounding_text），当前引擎不支持，忽略
}

void GXDEWaylandIM::Private::imTextChangeCause(void *data, zwp_input_method_v2 *im,
                                               uint32_t cause)
{
    Q_UNUSED(data);
    Q_UNUSED(im);
    Q_UNUSED(cause);
    // 文本变化原因：供输入法语境判断，当前无消费场景，忽略
}

void GXDEWaylandIM::Private::imContentType(void *data, zwp_input_method_v2 *im,
                                           uint32_t hint, uint32_t purpose)
{
    Q_UNUSED(data);
    Q_UNUSED(im);
    Q_UNUSED(hint);
    Q_UNUSED(purpose);
    // 内容类型（密码框/URL 等）：暂不据此切换输入行为，忽略
}

void GXDEWaylandIM::Private::imDone(void *data, zwp_input_method_v2 *im)
{
    Q_UNUSED(im);
    // 协议约定：commit 请求的 serial 等于已收到的 done 事件计数，
    // 不匹配的 commit 会被合成器丢弃 pending 状态
    static_cast<Private *>(data)->serial++;
}

void GXDEWaylandIM::Private::imUnavailable(void *data, zwp_input_method_v2 *im)
{
    Q_UNUSED(im);
    // 合成器宣告此输入法对象不可用（seat 移除或被其它输入法取代）：
    // 整体重连，重新绑定全局对象并 get_input_method
    static_cast<Private *>(data)->teardownAsync(QStringLiteral("输入法不可用"));
}

void GXDEWaylandIM::Private::grabKeymap(void *data,
                                        zwp_input_method_keyboard_grab_v2 *grab,
                                        uint32_t format, int32_t fd, uint32_t size)
{
    Q_UNUSED(grab);
    static_cast<Private *>(data)->handleKeymap(format, fd, size);
}

void GXDEWaylandIM::Private::grabKey(void *data,
                                     zwp_input_method_keyboard_grab_v2 *grab,
                                     uint32_t serial, uint32_t time, uint32_t key,
                                     uint32_t state)
{
    Q_UNUSED(grab);
    Q_UNUSED(serial);
    static_cast<Private *>(data)->handleKey(time, key, state);
}

void GXDEWaylandIM::Private::grabModifiers(void *data,
                                           zwp_input_method_keyboard_grab_v2 *grab,
                                           uint32_t serial, uint32_t depressed,
                                           uint32_t latched, uint32_t locked,
                                           uint32_t group)
{
    Q_UNUSED(grab);
    Q_UNUSED(serial);
    static_cast<Private *>(data)->handleModifiers(depressed, latched, locked, group);
}

void GXDEWaylandIM::Private::grabRepeatInfo(void *data,
                                             zwp_input_method_keyboard_grab_v2 *grab,
                                             int32_t rate, int32_t delay)
{
    Q_UNUSED(grab);
    static_cast<Private *>(data)->handleRepeatInfo(rate, delay);
}

void GXDEWaylandIM::Private::popupTextInputRectangle(void *data,
                                                      zwp_input_popup_surface_v2 *popup,
                                                      int32_t x, int32_t y,
                                                      int32_t width, int32_t height)
{
    Q_UNUSED(data);
    Q_UNUSED(popup);
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(width);
    Q_UNUSED(height);
    // 文本输入矩形（popup surface 局部坐标）仅作布局提示：候选窗的
    // 放置与屏幕边界避让由合成器完成（贴着文本输入矩形、放不下翻转），
    // 当前渲染不需要据此调整内容
}

void GXDEWaylandIM::Private::popupSurfaceEnter(void *data, wl_surface *surface,
                                                wl_output *output)
{
    Q_UNUSED(surface);
    auto *self = static_cast<Private *>(data);
    self->popupOutputs.append(output);
    self->updatePopupScale();
}

void GXDEWaylandIM::Private::popupSurfaceLeave(void *data, wl_surface *surface,
                                                wl_output *output)
{
    Q_UNUSED(surface);
    auto *self = static_cast<Private *>(data);
    self->popupOutputs.removeAll(output);
    self->updatePopupScale();
}

void GXDEWaylandIM::Private::outputGeometry(void *data, wl_output *output, int32_t x,
                                             int32_t y, int32_t physicalWidth,
                                             int32_t physicalHeight, int32_t subpixel,
                                             const char *make, const char *model,
                                             int32_t transform)
{
    Q_UNUSED(data);
    Q_UNUSED(output);
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(physicalWidth);
    Q_UNUSED(physicalHeight);
    Q_UNUSED(subpixel);
    Q_UNUSED(make);
    Q_UNUSED(model);
    Q_UNUSED(transform);
    // 位置/物理尺寸/transform 由合成器用于放置 popup，候选窗渲染只关心 scale
}

void GXDEWaylandIM::Private::outputMode(void *data, wl_output *output, uint32_t flags,
                                         int32_t width, int32_t height, int32_t refresh)
{
    Q_UNUSED(data);
    Q_UNUSED(output);
    Q_UNUSED(flags);
    Q_UNUSED(width);
    Q_UNUSED(height);
    Q_UNUSED(refresh);
    // 分辨率/刷新率与候选窗渲染无关
}

void GXDEWaylandIM::Private::outputDone(void *data, wl_output *output)
{
    Q_UNUSED(data);
    Q_UNUSED(output);
    // 一轮输出属性更新结束：scale 事件已即时处理
}

void GXDEWaylandIM::Private::outputScale(void *data, wl_output *output, int32_t factor)
{
    auto *self = static_cast<Private *>(data);
    self->outputScales[output] = factor > 0 ? factor : 1;
    // 缩放变化只对 popup surface 当前所在输出有意义
    if (self->popupOutputs.contains(output))
        self->updatePopupScale();
}

void GXDEWaylandIM::Private::bufferRelease(void *data, wl_buffer *buffer)
{
    Q_UNUSED(data);
    // 合成器已用完该帧缓冲：销毁代理，释放客户端资源
    wl_buffer_destroy(buffer);
}
