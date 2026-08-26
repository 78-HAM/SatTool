# SatDump / SatTool 开发文档(内部)

本目录是面向 **SatTool(基于 SatDump 的并行分支软件)** 开发的内部技术文档,目的是让开发者无需反复翻代码即可理解整个代码库的架构、扩展点与工作流。所有描述基于当前仓库(版本线 2.0.0-alpha,`d:\code\SatTool\SatTool`)的实际代码核对。

## 文档索引

| 文档 | 内容 |
|---|---|
| [01-架构总览.md](01-架构总览.md) | 分层架构、数据流、启动生命周期、目录职责地图 |
| [02-核心子系统.md](02-核心子系统.md) | 插件系统、事件总线、流水线、产品体系、配置、CLI |
| [03-DSP-图像-投影.md](03-DSP-图像-投影.md) | ndsp 流式 DSP、图像库、投影库、公共工具 |
| [04-UI与CLI架构.md](04-UI与CLI架构.md) | Explorer/Handler 体系、backend 抽象、命令行、Android |
| [05-扩展开发指南.md](05-扩展开发指南.md) | 新插件/新模块/新产品/新 UI 窗口/新 CLI 命令的完整模板 |
| [06-SatTool分支规划.md](06-SatTool分支规划.md) | 品牌替换点清单、分支策略、差异化方向、外部资料整理 |

## 仓库地图(顶层)

```
SatTool/                      # 仓库根(原 SatDump)
├── CMakeLists.txt            # 顶层构建:定义 BUILD_GUI/BUILD_ZIQ 等 option,CPack 打包
├── satdump_cfg.json          # 默认配置(用户自定义存 config/settings.json)
├── resources/                # ★ 数据资源:pipelines/*.json(流水线定义)、校准、LUT、地图、字体…
├── src-core/                 # ★ 核心库 satdump_core(全部逻辑,UI 也在内用 ImGui 绘制)
│   ├── init.cpp              # 初始化编排
│   ├── core/                 # plugin/config/params/resources/backend/cli/opencl
│   ├── pipeline/             # 流水线模型 + 内置处理模块(pipeline/modules/)
│   ├── products/             # 产品体系(product.cbor / dataset.json)
│   ├── handlers/             # UI Handler 体系(浏览/处理/图像/投影)
│   ├── explorer/             # 主窗口容器 ExplorerApplication
│   ├── dsp/                  # ★ ndsp 流式 DSP 块体系(block/DSPStream/io)
│   ├── image/                # 图像库(Image/编解码/处理)
│   ├── projection/           # 投影库(standard/raytracer/TPS + 重投影)
│   ├── common/               # ccsds、tle、net、codings、tile_map 等公共代码
│   ├── db/                   # SQLite:用户设置 + Kepler(TLE)/IERS 星历缓存
│   └── utils/                # 事件总线、任务调度、文件、HTTP、统计等
├── src-interface/            # ★ 库 satdump_interface:主菜单/设置/离线处理/Recorder
├── src-ui/                   # 桌面 GUI 可执行 satdump-ui(GLFW + OpenGL + NFD,绑定 backend)
├── src-cli/                  # 命令行可执行 satdump(CLI11 子命令 + legacy live/record)
├── android/                  # Android NDK 前端(EGL/GLES3 + ImGui_ImplAndroid)
├── plugins/                  # ★ 动态插件:sdr_sources/、audio_sinks/、simd_extensions/、
│                             #   各卫星 *_support、bitview_app/tools_app/webhook_app 等
├── tools/                    # 独立小工具(需 -DBUILD_TOOLS=ON)
├── src-testing/              # 测试程序(-DBUILD_TESTING=ON)
├── docs/                     # 用户手册(QuickStart/Pipelines/SDR_Options/ZIQ…)
├── windows/, macOS/, cmake/  # 平台打包与 CMake 模块
└── std_filesystem/           # 旧编译器 <filesystem> 兜底
```

## 构建体系要点

- CMake ≥ 3.12,C++17。顶层按 `src-core → src-cli → (src-interface, src-ui) → plugins` 的顺序 add_subdirectory。
- 主要 option:`BUILD_GUI`(默认 ON)、`BUILD_ZIQ`、`BUILD_ZIQ2`(WIP)、`BUILD_OPENCL`、`BUILD_OPENMP`、`BUILD_TOOLS`、`BUILD_TESTING`、`ENABLE_INSTALL`、`ENABLE_I18N`、`BUILD_MSVC`。
- 产物:核心库 `satdump_core`、接口库 `satdump_interface`、可执行 `satdump-ui`(GUI)与 `satdump`(CLI);所有插件 `.so/.dll` 输出到 `<build>/plugins/`。
- 插件开关约定:`PLUGINS_ALL` 全开,或 `option(PLUGIN_XXX ...)` 逐个控制;SDR 源类插件独立于 `PLUGINS_ALL`(各自 ON/OFF)。
- 版本号通过 git tag/branch/hash 注入(`satdump_vars.h` 中的 `SATDUMP_VERSION`),编译期写入。
- Windows 用 MSVC + vcpkg(不推荐 MinGW);Linux 依赖 fftw/volk/nng/png/tiff/jemalloc/sqlite3 等;Android 走 gradle + NDK。

## 关键外部资料

- 官方用户文档:<https://docs.satdump.org>(构建、流水线、SDR 选项等)
- 仓库内用户手册:`docs/pages/`(QuickStart、Pipelines、ZIQ、LutGenerator…)
- a-centauri.com 档案站(见 [06-SatTool分支规划.md](06-SatTool分支规划.md) 中整理的可用资料清单)
- 上游仓库:github.com/SatDump/SatDump