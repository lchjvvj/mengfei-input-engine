# gxde-input-engine

#### 介绍

GXDE 输入引擎（Wayland，适配 gxde-wlcom / FlakeWM / Treeland）

#### 定位

独立输入法框架后端，对接 Wayland 合成器（gxde-wlcom），兼容 fcitx5 协议，同时自带简易输入法模块，支持中文/英文切换、候选栏、预编辑文本，可集成到GXDE桌面。


#### 语言

C++ + Qt6（GXDE 生态主流），使用 wayland-client 协议，支持  text-input-v3 （现代Wayland输入标准）


#### 使用说明

1. 原生对接  gxde-wlcom ，复用合成器的 text-input-v3
​
2. 可选两种模式：独立内置引擎 / 作为fcitx5前端代理
​
3. 适配GXDE桌面环境，跟随GXDE主题（深色/浅色）

#### 自动构建

构建脚本位于[./build-deb](./build-deb), 这是个shell脚本，用于在调试时生成安装包，便于在调试机器上轻松部署与卸载。


首先先修改脚本权限: 

```bash
chmod a+x ./build-deb
```


然后以下上参数帮助:

```bash
用法: ./build-deb <选项>

选项:
  -b, --binary          仅构建二进制包（默认行为）
    -d, --install-deps    先安装构建依赖(读 debian/control)，再构建
    -c, --clean           仅清理构建产物后退出
    -h, --help            打印帮助信息
```

初次编译建议执行:

```bash
./build-deb -d    # 安装依赖并构建
```

以后就可以不用安装依赖了:

```bash
./build-deb    # 直接构建
```

构建完成后清理中间产物: 

```bash
./build-deb -c
```

#### 查看运行状态

# 用户会话启用服务
systemctl --user daemon-reload
systemctl --user enable --now gxde-input-engine.service

# 查看运行状态
systemctl --user status gxde-input-engine.service

# 查看日志
journalctl --user -u gxde-input-engine.service -f

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


#### 配套：生成Wayland协议代码说明

需要安装 wayland-scanner ，从 zwp-text-input-v3.xml 生成协议代码：

wayland-scanner private-code zwp-text-input-v3.xml protocol/text-input-v3.c

wayland-scanner client-header zwp-text-input-v3.xml protocol/text-input-v3.h

#### 参与贡献

1.  Fork 本仓库
2.  新建分支
3.  提交代码
4.  新建 Pull Request
