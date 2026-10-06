#include "gxdepinyinengine.h"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGlobalStatic>
#include <QStandardPaths>

#include <rime_api.h>
#include <xkbcommon/xkbcommon-keysyms.h>

// PIMPL：rime_api.h 的类型只在实现文件出现，与 GXDEInputCore 的风格一致
class GXDEPinyinEngine::Private
{
public:
    RimeApi *api = nullptr;     // librime vtable，由 rime_get_api() 提供
    RimeSessionId session = 0;  // 0 表示会话未建立，所有操作视为无效

    // 最近一次 RimeContext 的缓存，供各 getter 使用
    QString preedit;            // 预编辑串（已分音节，如 "ni hao"）
    QStringList candidates;     // 当前页候选
    QByteArray selectKeys;      // 方案选词键 menu/alternative_select_keys（默认方案通常为空）
    int selectedIndex = 0;      // 页内高亮候选索引
    int pageSize = 5;           // 方案 menu/page_size，Rime 默认 5
    int pageNo = 0;             // 当前页号（0 起始）
    bool isLastPage = true;     // 是否最后一页
};

namespace {

// librime 通知回调：部署进度、方案加载等事件写入日志，便于排查词库问题
void rimeNotificationHandler(void *context, RimeSessionId sessionId,
                             const char *messageType, const char *messageValue)
{
    Q_UNUSED(context);
    Q_UNUSED(sessionId);
    qDebug().noquote() << "[Rime]" << messageType << messageValue;
}

// 系统级方案/词典目录：Debian/Ubuntu 的 librime-data 装到 /usr/share/rime-data，
// 其他发行版（如 Arch）常见为 /usr/share/rime/data
QString rimeSharedDataDir()
{
    static const char *const candidates[] = {
        "/usr/share/rime-data",
        "/usr/local/share/rime-data",
        "/usr/share/rime/data",
        "/usr/local/share/rime/data",
    };
    for (const char *dir : candidates) {
        if (QFileInfo::exists(QLatin1String(dir)))
            return QLatin1String(dir);
    }
    return QLatin1String(candidates[0]);
}

// 用户级数据目录：存放用户词频、部署产物（build/）与个人 Rime 配置
QString rimeUserDataDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QLatin1String("/gxde-input-engine/rime");
}

// 首次运行写入的方案列表补丁：只启用朙月拼音·简化字（luna_pinyin_simp），
// 与旧版内置词库的简体输出一致，并避免 default.yaml 默认方案列表引用
// 未安装的方案数据（bopomofo/cangjie5 等）导致整体部署失败
const char *const kDefaultCustomYaml =
    "# 由 gxde-input-engine 首次运行时写入。\n"
    "# 仅启用朙月拼音·简化字；如需其他方案，安装对应 rime-data-* 包后\n"
    "# 在 schema_list 中补充，删除用户目录下 build/ 重新部署即可。\n"
    "patch:\n"
    "  schema_list:\n"
    "    - schema: luna_pinyin_simp\n";

// librime 全局生命周期：进程内只 initialize/deploy 一次，
// Q_GLOBAL_STATIC 保证懒加载与线程安全；进程退出时 finalize
struct RimeGlobal
{
    RimeApi *api = nullptr;

    RimeGlobal()
    {
        api = rime_get_api();
        if (!api) {
            qCritical() << "[Rime] rime_get_api 失败，拼音输入不可用";
            return;
        }

        const QString sharedDir = rimeSharedDataDir();
        const QString userDir = rimeUserDataDir();
        QDir().mkpath(userDir);

        // 用户未自定义过 default.custom.yaml 时才写入默认补丁，不覆盖用户配置
        const QString customFile = userDir + QLatin1String("/default.custom.yaml");
        if (!QFileInfo::exists(customFile)) {
            QFile file(customFile);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text))
                file.write(kDefaultCustomYaml);
            else
                qWarning() << "[Rime] 写入默认方案补丁失败:" << customFile;
        }

        // traits 指向本地字符串对象，librime 在 initialize 期间完成拷贝，
        // 保持存活到调用返回即可
        const QByteArray sharedDirUtf8 = sharedDir.toUtf8();
        const QByteArray userDirUtf8 = userDir.toUtf8();
        const QByteArray appName = QByteArrayLiteral("rime.gxde-input-engine");
        const QByteArray distVersion = QByteArrayLiteral(GXDE_VERSION);

        RIME_STRUCT(RimeTraits, traits);
        traits.shared_data_dir = sharedDirUtf8.constData();
        traits.user_data_dir = userDirUtf8.constData();
        traits.app_name = appName.constData();
        traits.distribution_name = "GXDE";
        traits.distribution_code_name = "gxde-input-engine";
        traits.distribution_version = distVersion.constData();

        api->setup(&traits);
        api->set_notification_handler(rimeNotificationHandler, nullptr);
        api->initialize(&traits);

        // 首次运行（或词典数据更新后）需要部署方案、编译词典，
        // 阻塞等待完成，避免会话建立后查不到候选
        if (api->start_maintenance(False))
            api->join_maintenance_thread();

        qInfo().noquote() << "[Rime] 初始化完成，方案目录:" << sharedDir
                          << "用户目录:" << userDir;
    }

    ~RimeGlobal()
    {
        if (api)
            api->finalize();
    }
};

Q_GLOBAL_STATIC(RimeGlobal, g_rime)

} // namespace

GXDEPinyinEngine::GXDEPinyinEngine(QObject *parent)
    : QObject(parent), d(new Private)
{
    d->api = g_rime()->api;
    if (!d->api)
        return;

    d->session = d->api->create_session();
    if (d->session == 0) {
        qCritical() << "[Rime] 创建输入会话失败，拼音输入不可用";
        return;
    }

    char schemaId[128] = {0};
    if (d->api->get_current_schema(d->session, schemaId, sizeof(schemaId)))
        qDebug().noquote() << "[Rime] 当前输入方案:" << schemaId;
    else
        qWarning() << "[Rime] 无可用输入方案，请安装 Rime 词典数据"
                      "（Debian/Ubuntu/GXDE: librime-data）";
}

GXDEPinyinEngine::~GXDEPinyinEngine()
{
    if (d->api && d->session != 0)
        d->api->destroy_session(d->session);
    delete d;
}

void GXDEPinyinEngine::appendChar(char ch)
{
    sendKey(static_cast<unsigned char>(ch));
}

void GXDEPinyinEngine::backspace()
{
    sendKey(XKB_KEY_BackSpace);
}

void GXDEPinyinEngine::clear()
{
    if (d->api && d->session != 0)
        d->api->clear_composition(d->session);
    syncState();
}

bool GXDEPinyinEngine::selectCandidate(int idx)
{
    if (!d->api || d->session == 0 || idx < 0 || idx >= d->candidates.size())
        return false;

    bool handled = false;
    if (idx < d->selectKeys.size()) {
        // 方案配置了选词键（如 "1234567890"）：一键选中当前页第 idx 个候选
        handled = d->api->process_key(d->session,
                                      static_cast<unsigned char>(d->selectKeys.at(idx)), 0);
    } else {
        // 默认方案未配置选词键：用 Up/Down 把高亮移到目标候选后以空格确认
        //（高亮与目标同在当前页，移动不会跨页）
        const int keysym = d->selectedIndex < idx ? XKB_KEY_Down : XKB_KEY_Up;
        for (int step = qAbs(idx - d->selectedIndex); step > 0; --step) {
            if (!d->api->process_key(d->session, keysym, 0))
                return false;
        }
        handled = d->api->process_key(d->session, XKB_KEY_space, 0);
    }
    if (!handled)
        return false;

    syncState();
    return true;
}

// 上一个候选（Rime 内按方案 key_binder 处理页内移动/跨页）
void GXDEPinyinEngine::selectPrev()
{
    if (d->candidates.isEmpty()) return;
    sendKey(XKB_KEY_Up);
}

// 下一个候选
void GXDEPinyinEngine::selectNext()
{
    if (d->candidates.isEmpty()) return;
    sendKey(XKB_KEY_Down);
}

void GXDEPinyinEngine::pageUp()
{
    if (d->candidates.isEmpty()) return;
    sendKey(XKB_KEY_Page_Up);
}

void GXDEPinyinEngine::pageDown()
{
    if (d->candidates.isEmpty()) return;
    sendKey(XKB_KEY_Page_Down);
}

QString GXDEPinyinEngine::getRawPinyin() const
{
    return d->preedit;
}

QStringList GXDEPinyinEngine::getCandidates() const
{
    return d->candidates;
}

int GXDEPinyinEngine::getSelectedIndex() const
{
    return d->selectedIndex;
}

int GXDEPinyinEngine::getPageSize() const
{
    return d->pageSize;
}

int GXDEPinyinEngine::getCurrentPage() const
{
    if (d->candidates.isEmpty()) return 0;
    return d->pageNo + 1;
}

int GXDEPinyinEngine::getTotalPages() const
{
    if (d->candidates.isEmpty()) return 0;
    // librime 只告知"是否最后一页"，无法预知总页数；
    // 非最后一页时按 当前页 + 1 估算，保证 UI 页码可见且可翻页
    return d->pageNo + 1 + (d->isLastPage ? 0 : 1);
}

void GXDEPinyinEngine::sendKey(int keysym)
{
    if (!d->api || d->session == 0)
        return;
    d->api->process_key(d->session, keysym, 0);
    syncState();
}

// 读取上屏文本与输入上下文，更新缓存并通知 UI
void GXDEPinyinEngine::syncState()
{
    if (!d->api || d->session == 0) {
        // 引擎未就绪：输出空状态，候选栏隐藏
        d->preedit.clear();
        d->candidates.clear();
        d->selectKeys.clear();
        d->selectedIndex = 0;
        emit preeditUpdate(QString());
        emit candidateUpdate({}, 0);
        emit pageInfoUpdate(0, 0);
        return;
    }

    // 先取已上屏文本，走 commitWord -> GXDEInputCore::commitString 上屏链路
    RIME_STRUCT(RimeCommit, commit);
    if (d->api->get_commit(d->session, &commit)) {
        const QString text = commit.text ? QString::fromUtf8(commit.text) : QString();
        d->api->free_commit(&commit);
        if (!text.isEmpty())
            emit commitWord(text);
    }

    RIME_STRUCT(RimeContext, ctx);
    if (d->api->get_context(d->session, &ctx)) {
        d->preedit = ctx.composition.preedit
                         ? QString::fromUtf8(ctx.composition.preedit) : QString();
        d->candidates.clear();
        for (int i = 0; i < ctx.menu.num_candidates; ++i) {
            if (ctx.menu.candidates && ctx.menu.candidates[i].text)
                d->candidates << QString::fromUtf8(ctx.menu.candidates[i].text);
        }
        d->selectKeys = ctx.menu.select_keys ? QByteArray(ctx.menu.select_keys)
                                             : QByteArray();
        d->selectedIndex = qMax(0, ctx.menu.highlighted_candidate_index);
        if (ctx.menu.page_size > 0)
            d->pageSize = ctx.menu.page_size;
        d->pageNo = ctx.menu.page_no;
        d->isLastPage = (ctx.menu.is_last_page != 0);
        d->api->free_context(&ctx);
    }

    emit preeditUpdate(d->preedit);
    emit candidateUpdate(d->candidates, d->selectedIndex);
    emit pageInfoUpdate(getCurrentPage(), getTotalPages());
}
