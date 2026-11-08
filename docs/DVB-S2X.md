# DVB-S2X receiver

本分支在原 `dvb_support` 插件的 `dvbs2_demod` 上增加常规 DVB-S2X 扩展 PLS 接收路径，输出经过 LDPC、BCH 和 BB 解扰后的 `.bbframe`。原 DVB-S2 MODCOD 1–28 的配置方式保留。

## 使用

在 pipeline 列表选择 **DVB-S2X (BBFRAME)**（ID：`dvbs2x_bbframe`），设置符号率、扩展 PLS 值、pilots、Gold code 和实际发射端的 RRC roll-off。默认配置是 normal frame、QPSK 13/45、PLS 132、无 pilots、Gold code 0。默认符号率和环路带宽是起点，应按实际信号调整。

现有 pipeline 也可继续使用模块 ID `dvbs2_demod`，例如模块参数：

```json
{
    "standard": "dvb-s2x",
    "modcod": 132,
    "pilots": true,
    "gold_code": 0,
    "symbolrate": 1000000,
    "rrc_alpha": 0.20,
    "pll_bw": 0.005,
    "agc_rate": 0.0001,
    "clock_alpha": 0.00017,
    "ldpc_trials": 30
}
```

`standard` 接受 `dvb-s2x` / `s2x` 和 `dvb-s2` / `s2`，忽略大小写；也可用 `s2x: true`。两个选择器均未设置时，`modcod >= 128` 自动选择 S2X。S2X 的 `modcod` 是完整的扩展 PLS 数值，不能使用旧 S2 的 1–28 编号解释它。位 0 是 pilot 标志：132 无 pilots、133 有 pilots。显式设置 `pilots` 会覆盖该位；新 pipeline 提供独立的 pilots 开关，因此其 PLS 输入建议使用下面表中的偶数值。

S2X 的 normal / short frame 由 PLS 值决定，`shortframes` 不覆盖扩展 PLS。`standard: "dvb-s2x"` 配合旧 MODCOD 1–28 时仍使用传统 S2 PLS/FEC 路径。

`gold_code` 为 Gold 序列编号，范围 0–262141，默认 0。若接收参数来自使用 root 表示法的设备，可改用 `root_code`：它是 18 位 x-register 初态，范围 1–262143；0 为默认初态 1 的兼容别名。两者不能同时指定。Gold 编号和 root 初态并非相同数值，内部按 `x = (((x ^ (x >> 7)) & 1) << 17) | (x >> 1)` 从初态 1 前进 Gold 编号次完成转换。

`dvbs2_ts_extractor` 同样接受 `standard`、`s2x` 和扩展 PLS `modcod`，会据此计算 BB-frame 长度；也保留显式 `bb_size`（单位：bit）。TS 提取适用于 BB payload 实际承载 MPEG-TS 的信号。

## 支持的常规 S2X 配置

下表的 PLS 值均不含 pilot 位，每项同时支持相邻的奇数 PLS。有 25 种 normal-frame 和 17 种 short-frame 配置，共 42 种；LDPC frame 长度分别为 64800 和 16200 bit。

| 调制 | Normal-frame PLS（码率） | Short-frame PLS（码率） |
| --- | --- | --- |
| QPSK | 132 (13/45), 134 (9/20), 136 (11/20) | 216 (11/45), 218 (4/15), 220 (14/45), 222 (7/15), 224 (8/15), 226 (32/45) |
| 8PSK | 142 (23/36), 144 (25/36), 146 (13/18) | 228 (7/15), 230 (8/15), 232 (26/45), 234 (32/45) |
| 8APSK | 138 (100/180), 140 (104/180) | — |
| 8+8APSK | 148 (90/180), 150 (96/180), 152 (100/180), 158 (18/30), 164 (20/30) | — |
| 16APSK | 154 (26/45), 156 (3/5), 160 (28/45), 162 (23/36), 166 (25/36), 168 (13/18), 170 (140/180), 172 (154/180) | 236 (7/15), 238 (8/15), 240 (26/45), 242 (3/5), 244 (32/45) |
| 4+12+16APSK | 174 (2/3) | 246 (2/3), 248 (32/45) |
| 4+8+4+16APSK | 178 (128/180), 180 (132/180), 182 (140/180) | — |

该接收链按配置的单一 MODCOD 和 frame 长度处理信号。PLS 检测用于显示接收到的配置，不会逐帧切换 FEC、星座或长度，因此目前应接收 CCM 信号；完整 VCM/ACM 自动切换尚未实现。

VLSNR 的 PLS 128/130、额外 900-symbol header、相关 puncturing/spreading、medium-frame 接收、64/128/256APSK、S2X superframe 和 channel bonding 尚未接入接收链。不支持的扩展 PLS 在配置阶段明确抛出异常。代码中保留部分额外 FEC 表不表示相应物理层模式已支持。

## 移植来源

- [drmpeg/gr-dvbs2](https://github.com/drmpeg/gr-dvbs2)，参考提交 `99a2ac5592fa41e9c0f8f757995f3451ab8184a6`：PLS 编码和物理层符号、星座映射、码率对应的交织规则、Gold-to-root 转换。
- [drmpeg/gr-dvbs2rx](https://github.com/drmpeg/gr-dvbs2rx)，参考提交 `74df1dadbd20b02ce0cdfd3244f9c97022981235`：ETSI S2X LDPC 地址表与 FEC 参数。新增 `dvb_s2x_tables.hh` 与该版本原表逐字节一致，保留原作者和 GPL-3.0-or-later 声明。
- 接收端沿用 SatDump 的 LeanDVB 衍生 PL 同步、物理层解扰，以及原有 Ahmet Inan / Ron Economos 的 LDPC/BCH 引擎。

移植中适配了 SatDump 的软比特正负极性和 AGC 幅度约定，并修复 pilot 数量、pilot 跳过时的序列推进、PLL 的 pilot 判决，以及 8APSK 必须先解除四分之一周加扰再做相位判决的处理。

## 回归验证

`src-testing/dvbs2x` 是可独立构建的 CMake/CTest 项目，需要 C++17、Threads 和真实 VOLK 开发库，不依赖 GNU Radio 安装。参考发送端星座单独保存在 `reference_constellations.h`，来自上述 `gr-dvbs2` 提交，测试不用接收端 `constellation_t::mod()` 生成发送符号。

```sh
cmake -S src-testing/dvbs2x -B build-s2x-tests -DCMAKE_BUILD_TYPE=Release
cmake --build build-s2x-tests --parallel
ctest --test-dir build-s2x-tests --output-on-failure
```

VOLK 未安装在默认搜索路径时，可设置 `VOLK_INCLUDE_DIR` 和 `VOLK_LIBRARY`；使用未安装的 VOLK 构建目录时，通过 `VOLK_EXTRA_INCLUDE_DIR` 添加其源码 `include` 目录。Windows 上运行测试时，VOLK DLL 及其编译器运行库需能从 `PATH` 找到。Visual Studio 等多配置生成器应在构建和 CTest 时指定 `--config Release` / `-C Release`。

在支持 SSE4.1 的 x86 CPU 上另建测试目录并设置 `-DS2X_TEST_SSE41=ON`，检查与插件相同的 16 帧 LDPC 路径。默认构建检查标量路径。

覆盖内容：

- 全部 256 个 S2/S2X PLS 码字和对应 PL-header symbols；Gold 编号 0、1、12345、262141 的解扰序列。
- 10 组同步捕获，含 normal / short frame、带 / 不带 pilots、31-symbol 起始偏移。
- 42 种 S2X 配置各有 / 无 pilots，共 84 组；每组连续两帧，带初相 0.08 rad 和频偏 0.00002 rad/symbol。
- 独立发送端星座、物理层加扰和交织，经实际 PLL / soft demod / deinterleave 后逐 bit 对比，再经 LDPC、BCH 和 BB 解扰恢复原始数据。
- BCH 注入 8 个错误 bit；LDPC 每个 SIMD lane 注入 3 个低置信度错误 bit，验证纠错和各帧 payload。
- 原 DVB-S2 的 QPSK、8PSK、16APSK、32APSK 各 normal / short frame、带 / 不带 pilots，共 16 组兼容测试。

当前已在 Windows / MinGW GCC 15.2、VOLK 3.2.0 上通过标量和 SSE4.1 两种构建，demod / TS extractor 源码通过语法检查。完整 SatDump GUI/插件打包构建尚未验证成功；合成测试从时钟恢复后的符号开始，尚无真实 S2X IQ 录波经过完整 RRC、AGC、时钟恢复链路的接收验证，也未据此测定 AWGN 门限或吞吐性能。
