# 06 SatTool 分支规划(从 SatDump 派生)

## 1. 目标与原则

- SatTool 是与 SatDump **并行**的分支软件:共享上游大部分能力,发展自己的特色(自定义流程/UI/协议/自动化)。
- 推荐原则:**先低侵入,后替换**——能做成插件(卫星支持、SDR、工具、UI 窗口、CLI 命令)的绝不改核心;只有品牌/路径/默认行为层面才改核心代码。这样可持续合并上游。

## 2. 品牌与标识替换点清单(必须改)

| 位置 | 现状 | 动作 |
|---|---|---|
| 顶层 `CMakeLists.txt` | `project(SatDump)`,CPACK 名称/厂商/NSIS 文案 | `project(SatTool)`,改名 `CPACK_PACKAGE_*`、`CPACK_PACKAGE_INSTALL_DIRECTORY`、NSIS brand/header |
| `src-core/satdump_vars.*` | 版本名/资源搜索路径宏 | 按需改版本名与文案 |
| `src-core/init.cpp` | `user_path`:`%APPDATA%/satdump`、`~/.config/satdump`、`./config`;`main.db` | 换成 sattoool 目录(注意:改名会丢弃用户旧配置,可做一次迁移逻辑) |
| `satdump_cfg.json` 文件名 + `core/config.cpp` 加载逻辑 | 默认配置名 `satdump_cfg.json` | 改名或保留兼容;`install(...)` 同步 |
| `src-core/core/resources.cpp` / init | `resources/` 子目录逻辑 | 一般保留 |
| `plugins/CMakeLists.txt` | 插件输出目录 `plugins/`、include 路径 | 保留机制,路径可不动 |
| 桌面集成 | `satdump.desktop`、`satdump.appdata.xml`、`satdump_install` | 改名称/ID/文案 |
| 可执行与库名 | `satdump` / `satdump-ui` / `satdump_core` / `satdump_interface` | CMake target + `SATDUMP_DLL` 导出宏可见性;UI 标题/src-ui 窗口标题 |
| Android | `org.satdump.SatDump`、gradle 名称 | 换包名 |
| Docker | Dockerfile/docker-compose 镜像名、`COMMAND` 默认命令 | 改名 |
| 文档/图标/主题 | README、icon.png、windows/installer-header.bmp | 重做 |
| 日志 banner | `init.cpp` 的 ASCII 大字 + "Starting …" | 换 SatTool |
| gettext 域 | `bindtextdomain("satdump",…)`,`resources/i18n/` | 可保留域或改名(改名需同步 .pot) |

## 3. 建议的分支策略

1. **上游追踪**:保持 `origin/upstream` 远端,定期 `git merge`/`rebase`;差异化修改尽量集中在独立 commit 前缀(如 `sattool:`)便于冲突处理。
2. **功能路线**:
   - 阶段 1(搭建):完成第 2 节全部改名,构建/打包/CI 通过;
   - 阶段 2(差异化):把 SatTool 新能力全部做成 plugins/<sattool_*> 插件 + resources 配置,不动 src-core;能实现 80% 需求;
   - 阶段 3(必要时):仅当确需改核心行为(如新 pipeline 引擎、新 UI 范式)才 fork 核心模块,并在本 dev 文档系列记录偏离点。
3. **配置兼容**:`satdump_cfg.json` 结构不变的话,用户配置可直接沿用;若改 user_path,第一次启动做一次旧目录检测/复制。

## 4. 可用的差异化扩展点(基于 01-05 章)

- 新卫星/协议/解调:模板 A;新图像处理:模板 C;新 UI 窗口:模板 D;新 CLI:模板 E;新 SDR/网络源:`plugins/sdr_sources/`;自动化/事件钩子:利用 `Daniking` 链——监听 `PipelineDoneProcessingEvent`、`AutoUpdateKeplersEvent`、`RegisterSubcommandEvent` 等事件。
- 集成第三方工具:仿 `webhook_app`(配置页+事件驱动通知)。

## 5. 仓库健康度待办(当前仓库的小瑕疵)

- `.gitignore` 第 1 行 `，z.vscode` 是损坏行(中文逗号误入),应为 `.vscode`;
- `src-testing/main copy.cpp` 是误入的副本文件;
- `resources/satdump_is_this_old`、`resources/tod.txt`、`resources/todd.txt` 疑似垃圾/占位文件;
- `src-core/satdump_varst.cxx` 疑似生成残留;
- 大量 `TODOREWORK` 注释标记着上游进行中的重构(UI 向 explorer/Handler 模型迁移、dsp 向 ndsp 迁移),**追踪上游进展**,勿在此类代码上做深度定制,以免与上游冲突。

## 6. a-centauri.com/archivio 资料整理(外部资料)

该站是 h5ai 文件服务器,绝大部分目录(Minecraft Forge 1.4.7、VPN、Arduino、EUMETSAT 教学 PPT 等)与 SatDump 无直接关系。**与 SatDump/卫星接收直接相关**的有:

| 资源 | 说明 |
|---|---|
| `Radio/autotrack.json` | 站主本人的 SatDump 自动跟踪配置:NOAA-15/18/19(apt+dsb)、Meteor M2-x(lrpt)多目标,带 `multi_mode`、`autotrack_min_elevation`、rotctld 云台(az/el 回中)、`http_server 0.0.0.0:8081`、QTH(44N,9E)。是研究 track/autotrack 参数与写自己的跟踪配置的**绝佳真实样例** |
| `Radio/autotrack_multi.json` | 上者的多星变体 |
| `Radio/sat-tools.tar.gz`(及 .deb) | 站主自打包的卫星小工具(待解包确认内容) |
| `Radio/itos-decoder/` | ITOS(老一代 NOAA TIROS 卫星)解码器 |
| `Radio/MTG_FCI_Ita.pdf` | MTG-I(FCI)意大利语讲义——MTG 支持插件的背景资料 |
| `Radio/satdump_1.1.4-9cac497_armhf.deb` | 旧版 SatDump(ARM)安装包,可作低版本对照 |
| `Radio/ORBleaflet.pdf`、`modifica_mmds.pdf` | 天线/馈源改造资料 |
| `EUMETSAT/`、`manuali/` | 卫星气象学教学 PPT(通道、RGB 合成概念,对理解 products 的 RGB 合成/校准有参考价值,但非代码) |
| `SATELLITE CHANNEL MAPPINGS.ods` | 卫星通道映射表(标定参考) |

结论:该站最大价值是 **autotrack 配置样例与卫星知识文档**,可在 SatTool 文档/测试数据中引用;代码层面不必依赖。

## 7. 里程碑建议(供讨论)

1. SatTool 品牌替换 + 三平台构建通过;
2. 首个 SatTool 特有插件(演示模板 A/D 全流程);
3. 更新本 dev 文档至与代码同步;CI 挂 CMake 全局构建 + 插件模板单测。