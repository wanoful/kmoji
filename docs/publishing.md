# 发布与 AUR 维护

源码：https://github.com/wanoful/kmoji

## 发布新版本

1. 同步修改 `CMakeLists.txt` 和 `data/kmoji.conf` 中的版本号。
2. 构建并执行 `python tests/integration.py`，提交变更。
3. 创建版本标签（例如 `v0.1.0`）并推送到 GitHub，再创建对应 Release。
4. 使用下面的脚本生成 AUR 文件并构建包，将包和校验文件上传至 Release。

版本标签发布后不要移动，否则源码归档的校验值会改变。
首版二进制在 Arch Linux x86_64 上构建；其他架构尚未验证。

## 首次提交 AUR

AUR 保存 `PKGBUILD` 和 `.SRCINFO`，不上传源码或二进制包。
先注册 [AUR 账号](https://aur.archlinux.org/register)，在账号设置中添加 SSH 公钥。
认证和提交规则见 [ArchWiki](https://wiki.archlinux.org/title/AUR_submission_guidelines)。

```sh
# 在 kMoji 源码目录中运行；AUR 仓库与源码仓库分开
git -c init.defaultBranch=master clone ssh://aur@aur.archlinux.org/kmoji.git ../kmoji-aur
python packaging/prepare-aur.py 0.1.0 ../kmoji-aur
cd ../kmoji-aur
makepkg -s
# 可选但推荐：安装 namcap 后检查 PKGBUILD 和成品包
namcap PKGBUILD kmoji-0.1.0-1-x86_64.pkg.tar.zst
git add PKGBUILD .SRCINFO
git commit -m 'Initial release: kmoji 0.1.0'
git push -u origin master
```

首次推送前确认包名尚未被占用，并检查生成的文件。不要把 `src/`、`pkg/`、源码归档
或 `.pkg.tar.zst` 提交到 AUR。软件包安装后，用户在 KDE 会话中运行 `kmoji-setup` 启用。

## 更新 AUR

先发布新的 GitHub 标签，再拉取 AUR 仓库，重新运行 `prepare-aur.py`，构建验证后提交
`PKGBUILD` 和 `.SRCINFO`。只修改打包方式时增加 `pkgrel`，无需发布新上游版本。
脚本生成文件时会将 `pkgrel` 重置为 1；维护已有版本的打包修订时请手动调整。

已有可发布版本但尚未配置 AUR 账号时，也可以先生成到普通目录：

```sh
python packaging/prepare-aur.py 0.1.0 ../kmoji-aur
```

完成 SSH 配置后，再将这两个文件复制到正式 AUR checkout 提交。
