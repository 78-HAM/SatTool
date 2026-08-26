# 04 UI 与 CLI 架构

## 1. 总体模型(重要:本版已重构)

当前代码库**没有** `SATDUMP_REGISTER_UI` 之类的注册宏,也没有运行期 UI backend 切换。架构是:

```
核心库(satdump_core)负责一切 UI 逻辑(ImGui 绘制不依赖前端细节)
   ↑ backend:: 函数指针(帧循环/文件对话框/图标)由前端填充
前端(satdump-ui = GLFW+OpenGL3+NFD / Android = EGL+GLES3)链接期决定
窗口 = handlers::Handler 子类,挂到 explorer::ExplorerApplication 上
扩展 = 插件通过事件总线注入 Handler / 菜单项 / 文件打开器
```

- `backend::` 函数指针定义:`src-core/core/backend.h`(`beginFrame/endFrame/device_scale/rebuildFonts/setIcon`、`selectFolderDialog/selectFileDialog/saveFileDialog` 等)。
- 桌面填充:`src-ui/backend.cpp::bindBackendFunctions()`(NFD 对话框、GLFW 循环);Android 填充:`android/backend.cpp`(EGL、JNI 对话框)。`src-interface` 与 `src-core` 只用这些指针,不感知前端。

## 2. Handler 体系(窗口对象)

`src-core/handlers/handler.h`:

- `class Handler`:纯虚 `drawMenu()`(该窗口在系统菜单栏的菜单)、`drawContents(ImVec2 win_size)`(内容区)、`getID()`;子窗口树:`addSubHandler()/delSubHandler()/getAllSubHandlers()/drawTreeMenu()`。
- `handlers/processing_handler.h::ProcessingHandler`:多线程处理封装——`process()` 纯虚 + `asyncProcess()`(后台线程,status 查询),用于离线处理等长任务(参考 `handlers/processing/OffProcessingHandler {getID()->"processing_handler"}`)。
- 内置 Handler:`handlers/image/`(图像查看)、`handlers/projection/`(投影)、`handlers/product/`(产品)、`src-interface/recorder/recorder.h::RecorderApplication` 等。

## 3. Explorer 主窗口容器

`src-core/explorer/explorer.h/.cpp`:`ExplorerApplication`

- 布局:`draw()` = 菜单栏(`drawMenuBar`) + 左侧面板树(`drawPanel`,分组见 `group_definitions`,如 Recorders/Products) + 内容区(`drawContents`,欢迎页含 "Start Processing / Add Recorder" 按钮)。
- `addHandler(h, open, is_processing)`:把 Handler 挂进面板树并可选立即打开;
- 关键事件:`ExplorerAddHandlerEvent`(动态加窗口)、`RenderLoadMenuElementsEvent`(注入"Add"等菜单条目)、`ExplorerRequestFileLoad/ExplorerLoadFileEvent`、`FileDropEvent`(按扩展名选 loader:`tryOpenFileInExplorer` 支持 .cbor/.json/.shp/图像/基带…)。
- 构造时即注册了 `ExplorerAddHandlerEvent` 等事件,所以**任何线程任何模块**都能 `fire_event<ExplorerAddHandlerEvent>` 开新窗口。

## 4. satdump_interface 应用壳

`src-interface/main_ui.cpp`:

- `initMainUI()`:创建 `explorer_app`(ExplorerApplication),注册 `ShowProcesingEvent/AddRecorderEvent/SetIsProcessingEvent` 等接口事件的 handler;
- `renderMainUI()`:菜单栏(File→Processing/Settings,Add→Recorder…)+ `explorer_app->draw()` 循环;
- 离线向导 `offline.cpp`:`PipelineUISelector`(widgets/pipeline_selector.h)选流水线 → 参数表单 → 构造 `OffProcessingHandler` 并 fire `ExplorerAddHandlerEvent`;
- 设置页 `settings.cpp`:把 `satdump_cfg.main_cfg` 转成 `EditableParameter` 表单渲染,保存时 `saveUser()`。

桌面入口 `src-ui/main.cpp`:GLFW/ImGui 初始化(OpenGL3,失败回退 2.1)→ 绑定 backend → `initSatDump(true)` → 注册 `recstart/open` 子命令 → `initMainUI()` → 主循环 `renderMainUI()`。gl3w 直接编译进工程(src-ui/CMakeLists.txt),NFD 走子目录。

## 5. CLI 分层

```
satdump(可执行)                     子命令由 CommandHandler 注册
 ├── pipeline <id> <level> <in> <out> [--参数]    离线处理单条流水线
 ├── module <模块id> ...                          对任意已注册模块跑一遍(自动生成)
 ├── process <product> <dir>                     对产品跑处理器
 ├── probe / dsp_bench / script / hserver ...
 └── legacy live|record|autotrack|sdr_probe ...  旧式命令行(src-cli/legacy/*)
```

- 典型用法(README 可见):`satdump pipeline metop_ahrpt baseband x.cs16 out --samplerate 6e6 --baseband_format cs16`;
- live:`satdump legacy live metop_ahrpt out --source airspy --samplerate 6e6 --frequency 1701.3e6 --timeout 780`;
- 所有 pipeline/模块参数自动成为 CLI flag(ModuleCmdHandler + EditableParameter 体系)。

## 6. Android

- 入口 `android/main.cpp::android_main`:EGL init → ImGui_ImplAndroid/OpenGL3 → `initSatDump()` → `initMainUI()` → `ALooper_pollAll` 循环 tick;`handleAppCmd` 处理 INIT_WINDOW/TERM_WINDOW/SAVE_STATE(保存配置)。
- UI 逻辑 100% 复用 `satdump_interface`;差异只在 backend 实现与 gradle 资源打包。受手机限制,仅 RTL-SDR/Airspy/AirspyHF/LimeSDR Mini/HackRF 可用。

## 7. 如何新增 UI:两分钟决策

- 临时/内置:继承 `Handler`,在 `explorer.cpp::drawMenuBar()`("Add"菜单)或 `main_ui.cpp` 菜单栏加 `MenuItem`,点击后 `fire_event<ExplorerAddHandlerEvent>`(或直接 `addHandler`)。
- 永久/可插拔:做成插件,注册 `RenderLoadMenuElementsEvent` 注入菜单项(参考 `plugins/bitview_app/main.cpp`、`plugins/tools_app/main.cpp`)。
- 需要后台任务:再继承 `ProcessingHandler` 并用 `asyncProcess()`。

详见 [05-扩展开发指南.md](05-扩展开发指南.md) 模板 D。

## 8. 常见 UI 相关小知识

- 主题:`resources/themes/*.json`(Dark/Light/Win98),生效于 `src-core/core/style.*`。
- 字体:`resources/fonts/font.ttf`;国际化资源 `resources/i18n/po/`(gettext,ENABLE_I18N)。
- 状态栏/日志见 `src-ui` 主循环与 `logger.h`(slog 后端,控制台+文件双 sink)。
- 图像到屏幕:ImGui 纹理封装 `core/imgui/imgui_image.h`(OpenGL 纹理更新走 `bindImageTextureFunctions`)。