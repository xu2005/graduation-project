# Serial_Read.vi 搭建

**接线板**：输入 `VISA session`、`error in`；输出同一 `VISA session`、`Line` (string)、`Has Line` (Boolean)、`Fatal Error` (Boolean)、`error out`。byte count 设 256；若未来帧加长再集中修改。

调用 VISA Read。成功且读到 LF 时，去掉 CR/LF，Has Line=true。没有数据且 error code=`-1073807339` 时，用 Clear Errors 清除该 timeout，Has Line=false、Fatal Error=false；其他错误 Fatal Error=true，交 Main 关闭串口。若返回 256 字节仍无 LF，标记为过长坏帧并丢弃至下一次 LF，不能把半帧当 SCAN。这个恢复策略可先在 Main 的 Parse Error 状态表示，生产级应用再用接收缓冲细化。测试时先接模拟器，STOP 状态应周期性 timeout 但 UI 不报严重错误。
