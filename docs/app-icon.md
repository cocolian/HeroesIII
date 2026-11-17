# 应用图标

应用沿用原创蓝金城堡与“III”图标。源图为 `native/assets/app-icon.png`，由原有 `assets/app-icon/app-icon-v2.png` 原样复制，随源码提交。

- macOS Finder 使用 `native/macos/Assets/HeroesIII.icns`，含 16–1024 像素表示。
- 应用启动后通过 SDL 设置同一图标，覆盖直接运行可执行文件的开发启动方式；发行包内置源 PNG，无需依赖工程目录。
- iPadOS 使用 `native/ios/Assets.xcassets/AppIcon.appiconset` 的不透明图标。
- 网页原型使用同款页签图标及 Apple Touch 图标。

重新导出平台图标：

```sh
./scripts/build-app-icon.sh
python3 scripts/build-ios-icons.py
```

源图与全部平台图标均已纳入 Git；上述命令不依赖被排除的运行素材目录。
