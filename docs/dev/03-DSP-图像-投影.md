# 03 DSP / 图像 / 投影 / 公共库

## 1. ndsp 流式 DSP 块体系(src-core/dsp)

命名空间 `satdump::ndsp`。三要素:Block(处理单元)、DSPStream(块间队列)、DSPBuffer(样本缓冲,volk 对齐)。

### 1.1 核心类

- `BlockIO`(block.h:45):块的输入/输出"接线柱"——`{ name, type(样本类型), fifo(shared_ptr<DSPStream>), blkdata(块私有数据,如 DDC 的 VFO), samplerate/frequency 元数据 }`。
- `Block`(block.h:102):纯虚 `work()` 是 DSP 主钩子;内部线程循环 `while (blk_should_run) if (work()) break;`。生命周期:`start()` →(自动先调一次 `init()`)→ 线程跑 work;`stop(stop_now, force)` 设退出标志并 join。析构前必须 stop,否则抛异常。
- `DSPStream`(base/stream.h):两个 moodycamel 无锁环形队列——`fifo`(数据)与 `ufifo`(空闲 buffer 池);`alloc(size)` 从池取 buffer、不够则 `volk_malloc` 对齐分配;`free()` 归还复用;`newBufferSamples()`/`newBufferTerminator()` 工厂方法。
- `DSPBuffer`(base/dsp_buffer.h):`type(SAMPLES/TERMINATOR_PROPAGATING/TERMINATOR_NON_PROPAGATING/INVALID)+ptr+size`,`getSamples<T>()`。
- 停止传播:源结束时往链里放终结符 buffer,下游收到终结符后按 propagate 与否决定是否继续往下游传递——整条链自动收尾。

### 1.2 傻瓜封装(写新块的首选)

- `BlockSimple<Ti,To>`(block_simple.h):单入单出同步块,只需实现 `uint32_t process(Ti* in, uint32_t n, To* out)` 返回输出样本数;样板自动处理终结符/buffer 归还。
- `BlockSimpleMulti<Ti,To,Ni,No>`(block_simple_multi.h):多入多出;实现 `process(Ti**, uint32_t*, To**, uint32_t*)`。
- `block_helpers.h`:`getTypeSampleType<T>()` / `getShortTypeName<T>()` 做类型映射(complex_t→c、float→f、int16→s、int8→h、uint8→b)。

### 1.3 配置协议(flowgraph/UI 动态表单的基础)

每个 Block 实现:

- `get_cfg_list()`:参数描述(ordered_json);
- `get_cfg(key)` / `set_cfg(key, value) → cfg_res_t{RES_OK, RES_LISTUPD(参数表变了), RES_IOUPD(IO 变了), RES_ERR}`;
- 批量便捷版 `get_cfg()/set_cfg(json)`。

`src-core/dsp/flowgraph/dsp_flowgraph_register.h` 的 `registerNodeSimple<T>(fg, "My/Gain")` 按块 ID 把新块挂进可视化 DSP Flowgraph。

### 1.4 IO 与样本格式

- `file_source.h`:`FileSourceBlock<T>` 读原始二进制样本(去格式识别);
- `iq_source.h`:`IQSourceBlock`,基于 `dsp::BasebandReader`,支持 cf32/cs16/cs8/cu8/wav16 + `iq_swap`;
- `iq_sink.h`:`IQSinkBlock`,写元数据(marker + samplerate 等)的基带文件,`prepareBasebandFileName()` 生成规范文件名;
- `udp_source.h`、`nng_sink.h`(NNG 网络)、`file_sink.h`、`waveform.h`;
- 格式枚举 `iq_types.h`:`CF32, CS32, CS16, CS8, CU8, WAV16`(无 w8);字符串互转在 iq_types.cpp。

### 1.5 典型解调链积木

| 环节 | 位置 | 类 |
|---|---|---|
| 下变频(时域多 VFO/FFT) | dsp/ddc/ | `DDC_Block`、`FFTDDCBlock` |
| FIR / FFT 滤波 / RRC | dsp/filter/ | `FIRBlock`、`FFTFilterBlock`、`RRC…` |
| 频谱/瀑布 | dsp/fft/fft_pan.h | `FFTPanBlock`(on_fft 回调) |
| 载波恢复 | dsp/pll/costas.h | `CostasBlock`(BlockSimple) |
| PSK 完整解调(复合块) | dsp/hier/psk_demod.h | `PSKDemodHierBlock`:RRC→AGC→MM 时钟恢复→Costas→Splitter→SNR |
| GFSK 调制 | dsp/hier/gfsk_mod.h | `GFSKModHierBlock` |
| 工具块 | dsp/utils/,path/ | AGC、blanker、hilbert、vco、multiply、add、splitter、switch… |

管线里的 `pm_demod/psk_demod/fsk_demod` 等模块(pipeline/modules/demod/)内部就是把这些 ndsp 块按参数串起来,并加进度/星座图 UI(继承 `BaseDemodModule`)。

## 2. 图像库(src-core/image)

分层:

1. **像素缓冲**:`Image`(image.h)——`d_depth/d_width/d_height/d_channels`(1 灰/2 GA/3 RGB/4 RGBA),`init()` 分配;`set/get(x,y,c)` 按 8/16bit 分派,`setf/getf` 浮点 0..1;几何:`draw_*`、`crop/mirror/resize(双线性)/fill`;位深转换 `to_rgb/to_rgba/to8bits/to16bits`;`raw_data()` 谨慎使用。自由函数 `imemcpy`、`image_to_rgba`(转 OpenGL 纹理)。
2. **编解码**:`io.h/.cpp` 统一入口 `load_img`(按文件魔数分派)/`save_img`(按扩展名)/`save_img_safe`(防覆盖自动加 _N)/`append_ext`(按配置 image_format 补后缀);实现于 `io/{pngio,jpegio,j2kio,tiffio,pbmio,qoiio}.cpp`。
3. **处理算法**:`processing.h/.cpp` 自由函数——`white_balance / median_blur / kuwahara_filter / equalize / normalize / linear_invert / simple_despeckle / rotate`。
4. **接入**:UI 开关在 `handlers/image/image_handler.cpp`(勾选→统一 `image_needs_processing`→顺序调用);Angelscript 绑定在 `src-core/angelscript/scriptsatdump/bind_image.cpp`,调 LUT/通道运算还看 `image/image_expression.h`(通道表达式系统,支持多普段 RGB 合成)。

## 3. 投影库(src-core/projection)

- 门面 `Projection`(projection.h):`forward(latlon→x/y)`、`inverse(x/y→latlon)`,JSON 序列化;`proj_type_t`:`PROJ_STANDARD / PROJ_RAYTRACER / PROJ_THINPLATESPLINE`。
- `init()` 选择链(projection.cpp):先尝试 standard → 失败降级 raytracer(逆投影 + TPS 辅助前向)→ `normal_gcps`。
- `standard/proj.h`:**投影类型** Equirectangular/Stereographic/UTM/Geos/Tpers/WebMerc;实现分文件(equirect.cpp、stereo.cpp、tmerc.cpp、geos.cpp、tpers.cpp、webmerc.cpp)。
- **图像重投影**:`reprojector.h`:`ReprojectionOperation{img, output_width/height, target_prj_info, use_old_algorithm}` + `reproject(op, &progress)`;支持地理参考元数据(挂在产品 proj_cfg 上)。
- 扩展:`RequestSatelliteRaytracerEvent`(projection/raytrace/satellite_raytracer.h)让插件注册自定义 raytracer(卫星轨道专用投影)。

## 4. 常用公共组件(src-core/common)

| 组件 | 头文件 | 用途 |
|---|---|---|
| CCSDS 空间包 | ccsds/ccsds.h | `CCSDSHeader`(apid 等)、`CCSDSPacket`、`parseCCSDSHeader`、CRC 校验(CCITT/HDLC32/垂直奇偶) |
| AOS/TM 解复用 | ccsds/ccsds_aos/(demuxer.h, vcdu.h)、ccsds/ccsds_tm/ | CADU→VCDU→按 VCID/APID 分包(JPSS/GOES 等仪器的标准前端) |
| 扰码 | codings/lfsr.h | GNU Radio 风格 Fibonacci LFSR:`next_bit_scramble/descramble`(volk_64u_popcnt 加速) |
| TLE 星历 | tracking/tle.h | TLE 解析、`get_from_norad*`、spacetrack 在线取 |
| 网络 | net/tcp.h、net/udp.h | 跨平台 TCP 客户端 / UDP 服务 |
| 瓦片地图 | tile_map/map.h | map 瓦片下载/叠加 |
| 校准 | common/calibration.h | 通用校准元数据类型 |
| 其它 DLL 层 | cli_utils / colormaps / repack(解交织)/ wav / ziq(ziq.h/ziq2.h) | 命令行工具、色表、样本重排、WAV 读写、ZIQ 压缩基带格式 |

## 5. DSP 与 pipeline 层之间的桥

`pipeline::Pipeline::run` 驱动的解调模块(DATA_FILE→DATA_FILE)内部使用 ndsp 自定义块,而 live 模式直接把 ndsp 的 `DSPStream` 从 SDR 源连进模块的 `DATA_DSP_STREAM/NDSP_STREAM` 输入。模块侧通过 `processing_stream` 等成员对接;旧的可视化流图代码在 `src-core/dsp/flowgraph/`。写新 DSP 能力时优先以 ndsp 块 + 模块外壳的形式交付(见 05 章模板)。