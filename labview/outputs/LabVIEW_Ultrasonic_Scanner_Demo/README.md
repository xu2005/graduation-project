# STM32 二维超声波扫描测距系统 LabVIEW 上位机 Demo

本项目把 STM32F103C8T6 的舵机角度、HC-SR04 距离和 DHT11 温度显示在 LabVIEW，并用 XY Graph 画二维扫描点、用 CSV 保存数据。目标是让初学者理解每根关键连线，并能逐步接上自己的板子。

## 最快开始

1. 第一次动手先打开 `docs/13_第一次动手_30分钟跑通LabVIEW串口Demo.md`，只搭最小串口接收与三项数值显示；之后再看 `docs/02_LabVIEW零基础操作.md`、`docs/04_程序框图逐模块讲解.md` 和 `labview/` 的完整说明。**目前没有自动生成的 `.vi`；不要把这些 Markdown 当作可运行 VI。**
2. 有 Python 时，在本目录运行 `py tools/stm32_serial_simulator.py --mode stdout --count 5`，先观察协议。当前机器 `py` 启动器存在，但它未注册可用的 Python；本次测试使用 Codex 自带 Python 解释器。日常使用建议安装 Python 3.10+。stdout 模式无需第三方包。
3. 要让 LabVIEW 的 VISA Read 收到模拟帧，安装并配置一对虚拟串口（例如 COM7↔COM8），`py -m pip install -r tools/requirements.txt`，运行 `py tools/stm32_serial_simulator.py --mode serial --port COM8`。LabVIEW 选 COM7，115200，点 Connect 再点 START。**stdout 仅供观察，不会自动进入 VISA。**
4. 接真实 STM32 时，用 USB 串口接 USART1，共地，确认电平兼容；LabVIEW 打开 PC 上对应 COM，步骤与模拟器相同。详见 `docs/09_如何接入STM32.md`。

当前机器可直接用本次验证过的解释器运行 stdout 模式（从本工程目录执行）：

```powershell
& 'C:\Users\xuhengyi\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' .\tools\stm32_serial_simulator.py --mode stdout --count 5
```

这个 Codex 运行时路径可能随软件更新改变，长期使用请安装正式 Python 3.10+。

## 环境调查（2026-09-27）

Windows 64 位；`D:\multisim\LabVIEW 2025\LabVIEW.exe` 是 LabVIEW 25.1f0 Development System，配置文件表明为 32 位；还发现 2022–2024 的资源目录。NI-VISA 注册表版本 25.0.0；LabVIEW CLI 在 `D:\multisim\Shared\LabVIEW CLI\LabVIEWCLI.exe`；发现 `vi.lib/_script`。G-CLI 未发现。VI Scripting 文件存在，但 COM 自动化连接返回 `0x80080005`，未能通过创建、保存和重新打开 VI 的验证，因此交付**搭建说明而非伪造 VI**。LabVIEW CLI 可用于调用已有 VI，不能凭空生成本工程。

## 默认参数

115200 8N1、LF 终止、VISA timeout 200 ms；角度 30–150°、步进 2°、间隔 100 ms；距离有效范围 `(0,400]` cm；图缓存最多 500 点。CSV 路径由 Save Data 文件对话框选择，建议放在 `data/`（自己创建）。参数集中在 Main 前面板的“配置”区域，细节见 `docs/08_参数修改指南.md`。

## 建议学习顺序

1. 打开 Front Panel；2. 看串口控件；3. 用 Python 模拟器产生数据；4. 看 VISA Read；5. 看字符串解析；6. 看角度/距离显示；7. 看 Polar_To_XY；8. 看 XY Graph；9. 测 START/STOP；10. 修改扫描参数；11. 接真实 STM32。

从 `docs/01_项目结构.md` 看全貌，按 `docs/12_测试步骤.md` 验收，遇到错误查 `docs/10_常见错误排查.md`。本版不做 CRC、数据库或复杂动画。
