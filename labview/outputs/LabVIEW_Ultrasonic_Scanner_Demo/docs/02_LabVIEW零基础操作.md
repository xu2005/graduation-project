# 02 LabVIEW 零基础操作

打开 LabVIEW，File → New VI。`Ctrl+E` 在 **Front Panel 前面板**（给用户操作/看结果）与 **Block Diagram 程序框图**（放计算和流程）之间切换；`Ctrl+S` 保存。前面板右键打开 Controls Palette，程序框图右键打开 Functions Palette；搜索框可直接输入节点英文名。

| 概念 | 在本项目怎么用 |
|---|---|
| Control 控件 | 用户输入，如 COM、Start Angle、START 按钮。箭头从终端输出值。 |
| Indicator 显示器 | 程序输出，如 Distance、RX Frame、XY Graph。终端接收值。 |
| Terminal 终端 | 放置前面板对象后在框图出现的对应小方块；给它接线才会更新。 |
| Wire 连线 | 传递有类型的数据；橙色通常是浮点，蓝色通常是整数，粉色是字符串，绿色是布尔。断线会使运行箭头变碎。 |
| Dataflow 数据流 | 节点等输入数据齐全才运行；用 error wire 约束 VISA Configure → Write/Read → Close 的顺序。 |
| While Loop | 程序的主循环。右下停止条件接 Exit 布尔量；不要让串口读取阻塞太久。 |
| Shift Register 移位寄存器 | 循环边界上的一对端子，左边取上轮值、右边写下轮值；这里保存状态、VISA 引用、X/Y 数组和记录数组。右击循环边框 Add Shift Register。 |
| Case Structure | 按布尔或状态值选择一段代码；这里按 Connected/Scanning 判断是否读取。 |
| Event Structure | 等待按钮 Value Change 或 Timeout；这里 UI 按钮触发控制，Timeout 每 50 ms 检查串口。不要把耗时任务放进按钮事件。 |
| Cluster 簇 | 把相关值打包；XY Graph 单条曲线需要一个包含 X 数组和 Y 数组的簇，可用 Bundle。 |
| Array 数组 | 一串同类型值；X Array 和 Y Array 位置一一对应。Build Array 的 Concatenate Inputs 用于追加单点。 |
| Error Cluster 错误簇 | 包含 status/code/source；VISA 函数间串接它。Timeout 要单独辨认并清除，其他串口错误要提示。 |

建议先用一条模拟字符串常量 `$SCAN,45.0,123.4,26.0` 单独搭 Parse VI，再搭 Polar VI，再接入串口。每完成一个 VI 都点运行箭头并在 Probe（连线上右键 Probe）看中间值。按钮使用 Latch When Released；它的终端必须放在对应事件分支内，才能自动复位。状态栏用字符串 Indicator，先不用复杂 typedef。首次保存 VI 时选择 `labview/` 目录。
