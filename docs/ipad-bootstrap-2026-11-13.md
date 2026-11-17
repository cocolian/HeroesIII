# Xcode 与 iPad 模拟器验证 · 2026-11-13

Xcode 26.6（17F113）已从 Apple 官方下载，安装于 `/Applications/Xcode.app`。XIP 的 Apple Software 签名与安装应用的深度严格签名检查通过。用户亲自完成 Xcode / Apple SDKs 许可及首次启动授权；`xcodebuild -checkFirstLaunchStatus` 成功，开发目录已指向完整 Xcode。iOS / Simulator 26.5 SDK、iOS 26.5 模拟器运行时可用。

当前源码的 arm64 Release 模拟器应用与未签名 iPhoneOS 应用已编译通过。首次真实构建修复了 SDL_image 的 iOS ImageIO / MobileCoreServices 链接依赖；GameData 改用 Xcode 原生资源阶段，避免平台目录占位符被当作字面路径，并让资源变化触发重新签名。构建脚本在完成后逐个校验 11,117 个运行文件及图标目录，并检查签名。

iPad 窗口默认使用原生全屏。地图按设备窗口扩展可见区域，城镇等传统界面保留 800×600 的构图比例。模拟器旋转到横屏后，已通过界面实际点击完成：新游戏 → 开始游戏 → 宝箱路线规划 → 再次点击行军 → 拾取 1500 金币。底栏金币从 10000 增至 11500。返回 iPad 主屏幕、点击游戏图标恢复后，地图、英雄和金币保持正确。

运行时诊断确认素材来自模拟器安装后的 `HeroesIII.app/GameData`，关闭了源工程路径回退，用户数据使用应用沙盒目录。桌面主屏幕显示本游戏图标。截图与可核对的构建/环境记录位于 `references/validation/ipad-bootstrap-2026-11-13/`。

用户的 iPad mini 7 暂不在身边，已按用户要求推迟真机签名和安装。未创建证书、配置发行权限、上传应用或生成可安装 IPA。未签名的 iPhoneOS `.app` 与模拟器 `.app` 均不能作为真机安装包。本轮只验证上述模拟器流程，未验证 iPad 真机或 iOS 完整战斗、施法、整图通关及音频硬件表现。竖屏冷启动仍可保持竖屏视口，游玩需将设备旋转到横屏。

Mac 0.28 安装包和原验证归档保持原样；旧 iPad 构建资料 ZIP 未包含本轮后续修复。当前 iPad 构建须使用工作区源码与 CMake 4.1 或更新版本。

来源：[Apple Xcode 系统要求](https://developer.apple.com/xcode/system-requirements)、[Apple Developer 下载](https://developer.apple.com/download/all/?q=Xcode%2026.6)。
