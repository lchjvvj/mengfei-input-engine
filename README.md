# gxde-input-engine

#### 介绍

GXDE 输入引擎（Wayland，适配 gxde-wlcom / FlakeWM / Treeland）

#### 定位

独立输入法框架后端，对接 Wayland 合成器（gxde-wlcom），兼容 fcitx5 协议，同时自带简易输入法模块，支持中文/英文切换、候选栏、预编辑文本，可集成到GXDE桌面。


#### 语言

C++ + Qt6（GXDE 生态主流），使用 wayland-client 协议，支持  text-input-v3 （现代Wayland输入标准）


#### 整体架构

gxde-input-engine
├─ gxde-input-core        # 核心输入管理
│  ├─ wayland text-input-v3 协议实现（和gxde-wlcom通信）
│  ├─ 预编辑文本(preedit)、候选词(candidate)、提交文本commit
│  └─ 输入法上下文管理（多窗口、激活状态）
├─ gxde-input-method      # 内置输入法模块（可插拔）
│  ├─ 拼音引擎（简易）
│  └─ 英文/半角符号
├─ gxde-input-ui          # 候选栏UI（Qt6 Wayland窗口，置顶跟随光标）
└─ plugin                 # 插件接口，可加载fcitx5输入法插件

#### 使用说明

1. 原生对接  gxde-wlcom ，复用合成器的 text-input-v3
​
2. 可选两种模式：独立内置引擎 / 作为fcitx5前端代理
​
3. 适配GXDE桌面环境，跟随GXDE主题（深色/浅色）

#### 运行流程

1. gxde-wlcom 合成器启动

​
2.  gxde-input-engine  作为独立后台进程启动（systemd/user 服务）

​
3. 用户点击输入框 → gxde-wlcom发送 text_input enter 事件

​
4. 引擎激活，监听键盘事件（从gxde-wlcom接收按键）

​
5. 输入拼音 → 拼音引擎查询候选词，更新候选UI

​
6. 数字选择候选 → 提交汉字到Wayland应用（浏览器、GXDE控制中心等）


#### 参与贡献

1.  Fork 本仓库
2.  新建分支
3.  提交代码
4.  新建 Pull Request
