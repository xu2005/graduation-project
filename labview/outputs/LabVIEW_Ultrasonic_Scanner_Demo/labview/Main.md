# Main.vi 搭建清单

前面板控件逐项见 `docs/03_前面板控件说明.md`；框图完整执行顺序见 `docs/04_程序框图逐模块讲解.md`。Main 是唯一顶层 VI：While Loop 中一层 Event Structure，事件为 Connect、Disconnect、START、STOP、Clear Graph、Save Data、Panel Close?、Timeout(50 ms)。按钮用 Latch When Released 并把终端放在对应事件内。

移位寄存器：`State` enum、`VISA session`、`Session Valid?` Boolean、`X Array` DBL[]、`Y Array` DBL[]、`Records` 记录簇[]，以及 `Last RX`/`Last TX`/`Status Text` 三个字符串。初始化为空数组和 DISCONNECTED。每个事件分支都连通所有寄存器；只有有效数据才追加记录和图。把 XY Graph、RX Frame、TX Frame、Status 的显示器终端放在 Event Structure **外**，每轮由相应事件输出接入；这样不同事件都能更新同一个显示器。VISA Read 的 Timeout 是空闲，不转 ERROR；坏帧显示 Parse Error 并继续。串口硬错误调用 VISA Close 并把 Session Valid? 设 false。退出事件使 While Loop 结束；循环之后再次按 Session Valid? 决定是否 VISA Close。图的 Max Graph Points 用前面板参数，在追加时截尾。

逐阶段验证：①只接 Connect/Disconnect，确认状态；②串口 Timeout 能看到 RX Frame；③ Parse VI 更新三个数字；④ Polar VI 更新 XY Graph；⑤ SET/START/STOP 控制模拟器；⑥ Save Data 输出六列 CSV；⑦窗口关闭后 COM 能重新打开。一次只增加一个阶段，断线时先修破碎运行箭头。
