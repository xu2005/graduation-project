# Serial_Init.vi 搭建

**接线板**：输入 `COM Port` (VISA Resource Name)、`Baud Rate` (I32)、`VISA Timeout ms` (I32)、`error in`；输出 `VISA session`、`error out`。不在此 VI 写死 COM 号。

放 `VISA Configure Serial Port`，接入 COM、baud、timeout，data bits=8、parity none、stop bits=1、flow control none。启用 termination character 并设置 LF=10；若当前节点没有对应输入，串接 VISA Property Node 设置 Serial Settings。error wire 从入口穿至输出。Main Connect 事件只在未连接时调用它；error status=true 时 Main 不使用该 session 并显示 Serial Error。单测可用成对虚拟串口，不要求另一端正在发数据。
