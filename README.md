# gxde-input-engine

#### 介绍

**GXDE输入引擎**（Wayland，适配 gxde-wlcom / FlakeWM ）

#### 定位

独立输入法框架后端，对接 Wayland 合成器（gxde-wlcom），兼容 fcitx5 协议，同时内置基于 librime（Rime 中州韵）的拼音输入引擎，支持中文/英文切换、候选栏、预编辑文本，可集成到GXDE桌面。


#### 语言

C++ + Qt6（GXDE 生态主流），使用 wayland-client 协议，通过 zwp_input_method_v2（标准 input-method-v2）+ zwp_virtual_keyboard_v1 直连 gxde-wlcom 合成器（应用侧仍走 text-input-v3，由合成器桥接）；同时保留 org.gxde.InputEngine (gxwm协议) DBus 通道，供控制中心打开设置面板与推送光标矩形，拼音词库与候选计算由 librime（Rime 中州韵）提供


#### 使用说明

1. 原生对接  gxde-wlcom ：引擎作为 zwp_input_method_v2 客户端直连合成器，按键经 grab_keyboard 独占抓取获得，未消费的按键经 zwp_virtual_keyboard_v1 转发回焦点应用
​
2. 可选两种模式：独立内置引擎 / 作为fcitx5前端代理
​
3. 适配GXDE桌面环境，跟随GXDE主题（深色/浅色）
​
4. 拼音词库运行依赖：librime1 与 rime 词典数据（librime-data、rime-data-luna-pinyin，随 deb 包自动安装）；首次启动自动部署拼音·简化字方案并编译词典（约数秒），用户词频与个性化配置位于 ~/.config/gxde-input-engine/rime/

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
2.  gxde-input-engine  作为独立后台进程启动（systemd/user 服务），通过 wl_display_connect 直连合成器并注册为 zwp_input_method_v2 输入法（合成器未就绪时每 5 秒自动重试）

​
3. 用户点击输入框 → 应用经 text-input-v3 请求激活 → gxde-wlcom 向引擎发送 activate 事件，引擎 grab_keyboard 取得独占按键抓取

​
4. 按键由合成器路由给引擎：参与组字的按键被引擎消费，其余按键经虚拟键盘转发回应用；按键自动重复与修饰键状态由引擎按 repeat_info/modifiers 事件补齐并转发

​
5. 输入拼音 → 拼音引擎查询候选词，更新候选UI，preedit 经 set_preedit_string + commit 实时写回应用

​
6. 数字选择候选 → 汉字经 commit_string + commit 提交到Wayland应用（浏览器、GXDE控制中心等）



# 参与贡献

1.  Fork 本仓库
2.  新建分支
3.  提交代码
4.  合并到Fock 仓库主分支
5.  新建 Pull Request
