# kMoji

为 KDE Plasma 6 / Wayland 制作的表情面板。Qt 6 界面，通过 Fcitx5 直接提交文字，
不模拟按键、不改写剪贴板。界面与输入法插件分别运行。

紧凑布局参考 Windows 表情面板与 [Smile](https://github.com/mijorus/smile)：搜索框、
表情网格、底部图标分类栏。肤色和更多选项按需展开，多选组合栏仅在选中后出现。

![浅色界面](docs/light.png)

## 功能

- Unicode 17.0：1,914 个基础 emoji，附带支持的肤色变体。
- 九个分类、全部浏览、收藏和最近使用。
- 中文、英文、无声调拼音搜索；支持粘贴关键词。
- 六种肤色选择、多个 emoji 组合、连续输入。
- 跟随系统 / 浅色 / 深色外观。
- 收藏、最近使用、肤色和外观保存在 `~/.config/kmoji/kmoji.conf`。
- 焦点切换后取消输入会话；密码和敏感输入框不触发面板。

## 使用

| 操作 | 功能 |
| --- | --- |
| `Win + .` / `Ctrl + Alt + .` | 在文本框中打开面板 |
| 输入文字 | 搜索；中文输入法状态下可直接键入拼音，例如 `xiongmao` |
| `Ctrl + V` | 粘贴搜索文字 |
| 方向键 / `PageUp` / `PageDown` | 浏览表情 |
| `Tab` / `Shift + Tab` | 切换分类 |
| 点击 / `Enter` | 输入当前表情；已有组合时，Enter 输入组合 |
| `Shift + 点击` / `Shift + Enter` | 将表情加入组合；再按 Enter 或点“输入”提交 |
| 右键 / `Ctrl + D` | 收藏 / 取消收藏 |
| 右上角 `···` | 展开连续输入与外观选项 |
| 连续输入 | 提交后保留面板 |
| `Esc` | 取消并关闭 |
| 更多选项中的“外观” | 依次切换浅色、深色、跟随系统 |
| 分类标题旁的手掌图标 | 展开肤色选择 |

搜索默认在左侧当前分类内进行。需要搜索全部 emoji 时选择“全部表情”。
最近使用的记录保存基础 emoji，展示与插入时使用当前选择的肤色。

## 构建与预览

依赖：C++20、CMake、Ninja、Fcitx5 开发文件、Qt 6 Widgets/Network、彩色 emoji 字体。
Arch 的包为 `base-devel cmake ninja fcitx5 qt6-base qt6-wayland noto-fonts-emoji`。

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local" -DCMAKE_INSTALL_LIBDIR=lib
cmake --build build -j4
./build/kmoji --preview
```

源码附带离线数据，无须在构建或使用时访问网络。

## 安装与启用

安装脚本额外需要 Python、PyGObject、`busctl` 和 KDE 的 `qdbus6`。只安装到当前用户的
`~/.local`，不需要 sudo。安装前备份文件到 `~/.local/state/kmoji/backup-*`。

```sh
# 仅安装，保留当前输入法与快捷键
python scripts/install.py

# 安装、添加 KWin 窗口规则、停用旧的 I'm Emoji Picker，并重启 Fcitx5
python scripts/install.py --activate
```

Fcitx5 设置 → 附加组件 → kMoji 可以修改呼出快捷键。
KWin 规则使面板不获取焦点，并显示在 Popup 层；原文本框继续持有输入法会话。
桌面会话中若绑定了别的应用的相同快捷键，需要解除冲突。

回退至某次安装前的版本与设置：

```sh
python scripts/restore.py ~/.local/state/kmoji/backup-YYYYMMDD-HHMMSS
```

恢复会还原该备份内的 Fcitx5 / KWin 配置；请注意备份之后的手动配置也会被还原。

## 验证

```sh
python tests/integration.py
```

测试启动私有 D-Bus 和 Fcitx5，使用离屏 Qt 界面，不操作当前桌面输入法。
覆盖直接提交、ZWJ / 肤色序列、连续输入、失焦与销毁后拒绝提交、敏感输入框、
过期会话、英文 / 中文 / 拼音搜索、分类浏览、多表情组合、收藏与历史持久化。

安装后，在桌面终端运行真实 Qt 文本框测试（额外需要 Qt Gui 私有开发头文件）：

```sh
cmake -S . -B build -DKMOJI_BUILD_DESKTOP_SMOKE=ON
cmake --build build -j4
QT_IM_MODULE=fcitx ./build/kmoji-desktop-smoke
```

这会短暂打开一个测试文本框，经 Qt 的输入法接口发送测试按键，验证面板冷启动、
搜索和直接提交，并保留文本框原有内容。它不会向其他应用模拟全局键盘输入。

导出界面预览：

```sh
QT_QPA_PLATFORM=offscreen ./build/kmoji --screenshot /tmp/kmoji.png
```

## 结构与边界

- `src/addon.cpp`：Fcitx5 原生模块，在输入法事件循环中处理键盘和 `commitString()`。
  使用弱引用和会话编号校验输入目标。进程间通信通过权限 0600 的 Unix socket，
  并校验对端用户 ID；断开、超时或失焦后放弃会话。
- `src/picker.cpp`：独立 Qt 6 界面，负责检索、绘制与偏好设置。列表使用委托绘制，
  不为每个 emoji 创建控件。首次触发时启动，后续触发复用进程。被插件启动时不创建
  自己的 Qt 输入法上下文，避免干扰目标应用。
- `scripts/generate-data.py`：从 Unicode 17 / CLDR 48 生成分类、中文标签与肤色映射，
  利用 ICU `uconv` 生成拼音索引。仅重新生成数据时需要联网和 `uconv`。

首版面向 KDE Plasma 6 / Wayland，其他桌面未验证。目标应用需正确接入 Fcitx5。
面板打开时键盘用于搜索和选择，拼音不会转换成汉字；可以直接用拼音、英文或粘贴中文搜索。
当前没有颜文字、自定义标签、混合肤色编辑，以及光标旁定位；面板由 KWin 居中显示。
Unicode 新增字符能否正常显示取决于系统 emoji 字体。

代码为 MIT 许可；Unicode / CLDR 数据使用 [Unicode 数据许可](data/UNICODE-LICENSE.txt)。
