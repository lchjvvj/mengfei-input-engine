#include "gxdewaylandim.h"

#include <cerrno>
#include <cstring>
#include <utility>

#include <QByteArray>
#include <QDebug>
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

namespace {
// Wayland 连接失败/断开后的重试间隔：合成器可能在启动或重启，不宜忙轮询
constexpr int kReconnectIntervalMs = 5000;
// wl_seat 绑定版本上限：seat 仅作为 get_input_method /
// create_virtual_keyboard 的参数使用，不消费其键盘能力
constexpr uint32_t kSeatVersionMax = 9;
}

class GXDEWaylandIM::Private
{
public:
    Private(GXDEWaylandIM *qq, GXDEInputCore *corePtr, GXDEPinyinEngine *pinyinPtr);
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

    // 监听器结构体：字段顺序必须与协议 XML 的事件声明顺序一致
    static const struct wl_registry_listener kRegistryListener;
    static const struct wl_seat_listener kSeatListener;
    static const struct zwp_input_method_v2_listener kInputMethodListener;
    static const struct zwp_input_method_keyboard_grab_v2_listener kKeyboardGrabListener;

    GXDEWaylandIM *q;
    GXDEInputCore *core;
    GXDEPinyinEngine *pinyin;

    // Wayland 连接与协议对象（断开后全部置空，重连时重建）
    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    wl_seat *seat = nullptr;
    zwp_input_method_manager_v2 *imManager = nullptr;
    zwp_virtual_keyboard_manager_v1 *vkManager = nullptr;
    zwp_input_method_v2 *inputMethod = nullptr;
    zwp_input_method_keyboard_grab_v2 *keyboardGrab = nullptr;
    zwp_virtual_keyboard_v1 *virtualKeyboard = nullptr;

    // 已绑定全局对象的注册表名字（0 表示未绑定）：global_remove 时
    // 只有这三个被移除才需要重连，输出等其它全局对象变化不影响连接
    uint32_t boundSeatName = 0;
    uint32_t boundImManagerName = 0;
    uint32_t boundVkManagerName = 0;

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
    seatName,
    seatCapabilities,
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

GXDEWaylandIM::GXDEWaylandIM(GXDEInputCore *core, GXDEPinyinEngine *pinyin,
                             QObject *parent)
    : QObject(parent), d(new Private(this, core, pinyin))
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
                                GXDEPinyinEngine *pinyinPtr)
    : q(qq), core(corePtr), pinyin(pinyinPtr)
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

    // preedit 写回：与 DBus PreeditChanged 信号同源（拼音引擎 preeditUpdate）
    QObject::connect(pinyin, &GXDEPinyinEngine::preeditUpdate, q,
                     [this](const QString &text) {
        preeditText = text;
        scheduleStateFlush();
    });
    // 上屏写回：与 DBus CommitText 信号同源（core 的 textCommitted 出站链路）
    QObject::connect(core, &GXDEInputCore::textCommitted, q,
                     [this](const QString &text) {
        pendingCommit.append(text.toUtf8());
        scheduleStateFlush();
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

    // 独立连接：不依赖 Qt WaylandClient 的私有 display，与候选栏 UI 的
    // 连接互不干扰（fcitx5 同款做法）
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
    // 合成器侧在客户端断开后自动清理输入法/虚拟键盘资源
    if (display)
        wl_display_disconnect(display);
    display = nullptr;
    registry = nullptr;
    seat = nullptr;
    imManager = nullptr;
    vkManager = nullptr;
    inputMethod = nullptr;
    keyboardGrab = nullptr;
    virtualKeyboard = nullptr;
    boundSeatName = 0;
    boundImManagerName = 0;
    boundVkManagerName = 0;

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
    // 先清拼音状态：触发空 preedit 写回并隐藏候选栏（与 DBus Deactivate
    // 的处理一致），再关闭输入上下文
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
    }
}

void GXDEWaylandIM::Private::registryGlobalRemove(void *data, wl_registry *registry,
                                                  uint32_t name)
{
    Q_UNUSED(registry);
    auto *self = static_cast<Private *>(data);
    // 只关心已绑定的三个全局对象；输出等其它全局对象的增删
    //（如显示器热插拔）不影响输入法连接
    if (name != self->boundSeatName && name != self->boundImManagerName
        && name != self->boundVkManagerName)
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
