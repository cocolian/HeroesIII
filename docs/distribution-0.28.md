# 0.28 安装与平台状态

后续环境更新（2026-11-13）：Xcode 26.6 已安装并完成首次启动，iOS 26.5 SDK 和 arm64 模拟器编译通过；iPad 真机签名/安装因设备暂不在身边推迟。当前状态见 [iPad 构建说明](../native/ios/README.md)。下文保留 0.28 安装包制作时的验证记录；旧 ZIP 未包含后续 iOS 构建修复。

## MacBook

- [PKG 安装包](../dist/HeroesIII-0.28.0-macOS-universal.pkg)：双击后按 Installer 操作，安装到“应用程序”。
- [DMG 磁盘映像](../dist/HeroesIII-0.28.0-macOS-universal.dmg)：打开后将 HeroesIII.app 拖到 Applications。
- Apple Silicon 与 Intel universal；最低 macOS 14，Retina，F4/F11/Cmd+F 全屏。Intel 在本机 Rosetta 验证，没有另一台 Intel Mac 的实机证明。

程序包含完整当前运行素材、音效/音乐、OFL 中文备用字体和静态依赖，只链接 macOS 系统库。资源从应用包 GameData 读取，禁用了源工程路径回退。设置和进度写入 SDL_GetPrefPath 对应的用户目录，Mac 为 `~/Library/Application Support/HeroesIII/`，不会写进安装包。当前用户此前未开始游玩，本轮没有开发旧存档迁移功能。

包使用本地临时签名并通过 codesign 严格校验，没有 Developer ID 身份或 Apple 公证，属于本地开发测试发行；尚未在另一台 Mac 验证 Gatekeeper 首次安装体验。没有关闭任何系统安全设置。游戏原版功能与画面一致性仍未完成。

## iPad mini 7

[原生构建资料 ZIP](../dist/HeroesIII-0.28.0-iPad-native-build-kit.zip)包含共享 C++ 源码、CMake iPadOS 配置、图标、运行素材和固定 SHA256 的依赖源码缓存。它不是 IPA，不能直接安装到 iPad。构建目标最低 iPadOS 18，iPad 横屏，Retina；实际 iOS 分支仍须通过 Xcode 编译、签名和真机验收。

触控层支持单指点击和拖图、双指缩放、长按部队目标进行拆分，以及长按取消选中的战斗法术。使用原生 SDL_FINGER 事件，禁止同时生成触摸鼠标事件，避免一次点击执行两次。UIKit 通过 SDL 的 iOS 动画回调驱动帧，返回 SDL_main 后对象保持存活；后台释放触控、停止行军、暂停声音并保留状态。鼠标和外接键盘入口保留。

本轮仅在 Mac 的 1133×744 点窗口用合成手指事件验证这些交互，包括新局→宝箱、手指最终释放位置、两指不误点、拆分、施法取消和后台恢复。不能据此宣称 iPad 已运行。图标导出为不带 alpha 的 PNG，供 Xcode 资产目录编译。

此机只有 Command Line Tools，没有 iPhoneOS/Simulator SDK，`security find-identity -p codesigning` 返回 0 个有效身份。App Store 获取 Xcode 时显示要求 macOS 26.6，安装被拒绝。Apple 的版本表显示 Xcode 26.6 可运行于 macOS 26.2–26.x，可从开发者下载页取得兼容版本；是否支持用户 iPad 的实际系统版本还要在设备连接后确认。

需要用户完成 Apple 账号相关设置后继续：

1. 从 Apple Developer 下载兼容的完整 Xcode，首次打开完成组件与许可步骤。
2. 在 Xcode Settings → Accounts 登录自己的 Apple 账号，通过 USB 连接并信任 iPad mini 7。
3. 确认需要的 iPadOS SDK/运行时，按设备提示启用开发者模式。
4. 用自己的 Team ID 运行 [原生构建脚本说明](../native/ios/README.md)，再由 Xcode 安装到该设备；分发 IPA 时使用相应注册设备配置或 TestFlight。

没有代用户接受 Xcode 许可、创建证书、导出私钥、上传 Apple 服务或发布应用。

## 本轮验证

发行版通过 284 核心、14 资源格式、22 音频检查及 273 张界面回归。Intel slice 的 284 核心检查、音频检查和 46 张重点流程通过 Rosetta；ASan/UBSan 核心/格式/音频以及 46 张重点流程通过。自动练习图仍第 18 天四战完成。静态 SDL2 的流式重采样在整文件一次提交时会遗漏最后缓存块，现对完整 WAV 使用整缓冲转换，27 首音乐时长检查均通过，未降低误差门槛。

应用搬到带空格的独立目录，11117 个素材逐个核对 SHA256、资源根和禁用回退标记；实际窗口成功完成新局、拾取至 11500 金币、回城和全屏。DMG 校验通过；PKG 包含应用/运行素材，安装路径固定 `/Applications`、禁止迁移到旧开发副本。本轮未向系统“应用程序”目录实际安装，因为请求是提供安装包。

证据归档见 `references/validation/0.28/`，散列和平台限制见 [validation-0.28.json](../data/native/validation-0.28.json)。安装包校验码见 [SHA256SUMS](../dist/SHA256SUMS)。

## 构建入口

```sh
./scripts/build-release-macos.sh
python3 scripts/build-ios.py --preflight
```

依赖版本与源码 SHA256 固定在 `cmake/dependency-sources.json`，Mac 发行版不再加载 Homebrew SDL2-compat / SDL3。iPad 与 Mac 共用静态 SDL2、SDL_image、SDL_ttf、FreeType 和 nlohmann-json；图片使用 Apple ImageIO，中文备用字体按 SIL OFL 随包提供。源资产出处仍可追溯到原素材索引。

来源：[Apple Xcode 系统要求](https://developer.apple.com/xcode/system-requirements)、[Apple 已注册设备分发](https://developer.apple.com/documentation/xcode/distributing-your-app-to-registered-devices)、[SDL2 iOS](https://wiki.libsdl.org/SDL2/README-ios)、[SDL2 资源根](https://wiki.libsdl.org/SDL2/SDL_GetBasePath)、[SDL2 用户目录](https://wiki.libsdl.org/SDL2/SDL_GetPrefPath)。
