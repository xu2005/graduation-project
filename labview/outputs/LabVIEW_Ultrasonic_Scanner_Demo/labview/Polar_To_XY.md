# Polar_To_XY.vi 搭建

**接线板**：输入 `Angle deg`、`Distance cm`（DBL）；输出 `X cm`、`Y cm`（DBL）。

框图依次连接 Multiply (`angle * pi`) → Divide (`/180`) 得 theta_rad。theta 同时接 Sine 与 Cosine；各自与 distance 相乘，Cosine 支路输出 X，Sine 支路输出 Y。可在 Formula Node 写相同三行式，但基础数学节点更利于初学者看懂。调用前由 Parse VI 保证输入有效。

测试：0°/100 cm → (100,0)，90°/100 cm → (0,100)，45°/123.4 cm → 约 (87.26,87.26)；浮点误差使 90° 的 X 可能是极小数，显示格式可设两位小数。
