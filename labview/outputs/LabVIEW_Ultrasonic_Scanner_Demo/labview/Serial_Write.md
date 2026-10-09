# Serial_Write.vi 搭建

**接线板**：输入 `VISA session`、`Command` (string，已带 CRLF)、`error in`；输出同一 session、`Bytes Written` (I32)、`error out`。放 VISA Write，直接连四个端口。调用方负责组帧，VI 不自动追加第二个 CRLF，避免双换行。

Main 的 START 事件先传 `$SET,...\r\n`，把第一次 error out 接第二次 `$START\r\n` 的 error in，这样保证 SET 先完成。发送后把完整 Command 写 TX Frame。STOP 与 PING 也走此 VI。若 Bytes Written 小于字符串字节数，按 Serial Error 处理。
