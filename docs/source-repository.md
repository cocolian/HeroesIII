# 源码仓库与构建说明

公开仓库为 <https://github.com/cocolian/HeroesIII>，当前源码版本 0.48.1。

仓库包含 `native/src`、`native/include`、`native/generated`、原生测试、macOS／iPadOS 工程资源、CMake 固定版本依赖配置、资源采集与生成脚本，以及早期网页原型。生成的 C++ 表随源码保存，编译规则引擎不要求重新生成这些表。

`assets/`、下载地图、`references/`、`build/`、`dist/`、截图和存档不提交到 Git。完整开发目录的验证记录与安装包保留在本机；README 和历史开发文档中指向这些目录的链接，在 GitHub 上可能没有对应文件。App 图标属于平台工程资源，随工程提交。仓库中的来源链接与资源采集脚本不改变原资源的许可。

## 编译不依赖运行素材的规则引擎

需要 CMake 3.21 以上、支持 C++20 的编译器、nlohmann-json 3.9 以上与 Python 3。

macOS 可先安装编译依赖：

```sh
brew install cmake nlohmann-json
```

在仓库根目录执行：

```sh
cmake -S . -B build/source-core \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DH3_BUILD_GUI=OFF \
  -DBUILD_TESTING=ON
cmake --build build/source-core -j 4
ctest --test-dir build/source-core --output-on-failure \
  -R '^(native_async_save|resource_formats)$'
```

这两组测试分别验证后台保存的快照隔离／最终写入／错误处理，以及资源格式解码边界，不依赖被排除的游戏素材。其余规则、地图、汉化、地形及界面测试需要完整本地运行数据；直接在纯源码克隆中执行全部 CTest 会缺少这些测试夹具。

## 编译并运行图形应用

将已有完整开发目录中的 `assets/`、`maps/community/` 以及所需 `references/` 复制到对应路径后，执行：

```sh
./scripts/build-native.sh
./scripts/run-native.sh
```

构建脚本通过 CMake 下载并校验固定版本的 SDL2、SDL_image、SDL_ttf、FreeType 和 nlohmann-json 源码。资源相关脚本位于 `scripts/collect-*.py` 与 `scripts/generate-*.py`，其输入、输出和来源各有不同，不能把单个下载命令视为完整素材恢复流程。需要重建资源时，可结合 README 的本地开发步骤与 `docs/` 中对应版本说明执行。

macOS 应用默认全屏，F4 切换窗口。iPadOS 构建说明见 `native/ios/README.md`，真机安装仍需要 Xcode、苹果签名和设备配置。

## 本机文件保护

`.gitignore` 排除个人进度、签名文件、环境配置、下载包及构建产物。提交源代码不会移动或删除这些本机文件。
