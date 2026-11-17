# iPad mini 7 原生构建

与 Mac 共用 C++20 / SDL2 游戏，不使用旧 JavaScript 原型。iPadOS 最低目标 18，限定 iPad，横屏，原生 Retina 渲染。AppIcon 复用当前游戏图标。单指点击/拖图、双指缩放、长按目标分兵、长按取消战斗法术使用 SDL 触摸事件；UIKit 帧回调在主循环返回后继续运行，切后台会停止行军、暂停音频和释放触控捕获。

2026-11-13 已从 Apple 官方下载安装 Xcode 26.6（17F113）到 `/Applications/Xcode.app`，用户已完成许可和首次启动，iOS / Simulator 26.5 SDK 与模拟器运行时可用。arm64 模拟器 Release 和未签名真机版本已编译通过，GameData 的 11,117 个运行文件逐个校验通过，图标资产目录和模拟器应用签名验证通过。iPad 原生资源打包需要 CMake 4.1 或更新版本。

目前没有可安装 IPA，也没有可用真机开发签名。用户的 iPad mini 7 暂不在身边，真机签名和安装已推迟；不要把模拟器 .app、Mac .app 或旧构建资料包当作 iPad 安装包。

环境检查：

```sh
python3 scripts/build-ios.py --preflight
```

安装 Xcode 并在 Xcode Settings → Accounts 登录你的 Apple 账号后，通过 USB 连接并信任 iPad，按 Xcode 提示开启开发者模式。使用自己的 Team ID：

```sh
python3 scripts/build-ios.py --developer-dir /Applications/Xcode.app/Contents/Developer --team YOUR_TEAM_ID
```

脚本生成 `build/ipad-device/HeroesIII.xcodeproj`，可在 Xcode 中选择 iPad 并运行。开发账号的权限、设备支持版本和签名有效期由 Apple 管理。也可先 `--simulator` 编译模拟器版本（仍需要安装相应模拟器运行时）。

归档和导出：

```sh
python3 scripts/build-ios.py --developer-dir /Applications/Xcode.app/Contents/Developer --team YOUR_TEAM_ID --archive
python3 scripts/build-ios.py --developer-dir /Applications/Xcode.app/Contents/Developer --team YOUR_TEAM_ID --archive --export-options /path/to/your/ExportOptions.plist
```

ExportOptions 必须匹配实际开发者团队、发行方式和设备。个人真机开发安装可由 Xcode 完成；分发给其他设备使用已注册设备的签名配置或 TestFlight，不能用一个未签名 IPA 任意安装。

依赖版本与 SHA256 固定在 `cmake/dependency-sources.json`；用静态 SDL2、SDL_image（Apple ImageIO）、SDL_ttf 与 FreeType，中文字体为 OFL 授权 Noto Sans SC。`scripts/stage-runtime.py` 将运行素材放入只读 GameData；用户设置/进度使用 SDL_GetPrefPath 返回的应用沙盒目录。

在 iOS 26.5 的 iPad mini（A17 Pro）模拟器已实际完成横屏启动、新游戏、触控规划路线、行军、拾取宝箱（金币 10000 → 11500）、主屏幕图标和后台恢复。修复了 ImageIO / MobileCoreServices 链接、Xcode GameData 资源复制/重新签名和 iPad 默认全屏。记录见 [模拟器验证](../../docs/ipad-bootstrap-2026-11-13.md)。此前 Mac 的合成 SDL_FINGER 回归仍作为独立证据；本轮未在 iPad 真机验证完整战斗、施法、音频及整图通关。

来源：

- [Apple Xcode 系统要求](https://developer.apple.com/xcode/system-requirements)
- [Apple 已注册设备分发](https://developer.apple.com/documentation/xcode/distributing-your-app-to-registered-devices)
- [SDL2 iOS 平台说明](https://wiki.libsdl.org/SDL2/README-ios)
