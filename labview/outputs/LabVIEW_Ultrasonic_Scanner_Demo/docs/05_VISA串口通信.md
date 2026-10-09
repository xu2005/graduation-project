# 05 VISA 串口通信

使用 NI-VISA。Windows 设备管理器确认 COM 编号；前面板 COM Port 选 `ASRLx::INSTR`。Connect 事件中 VISA Configure Serial Port：baud=115200、8 data bits、no parity、1 stop bit、flow control none、timeout=200 ms；开启终止字符、值 10 (LF)。如果配置节点版本不暴露终止字符，用 VISA Property Node 的 Serial Settings → Termination Char / Termination Char Enable 设置。具体 Palette 路径因版本会变，可搜索节点名。

写命令时用字符串常量或 Format Into String，确保**真的有 CR 和 LF 字节**。LabVIEW 字符串显示方式切到 `\ Codes Display`，输入 `\r\n`；普通显示模式输入反斜杠字母不会产生换行。每个 VISA Write 看 return count 和 error cluster，SET 完成后才 START。

VISA Read 的 byte count 设 256；终止字符启用时到 LF 即返回。把尾部 CR/LF 去掉后交解析。空闲可能报超时 `-1073807339`，仅将它作为无数据处理，其他错误要显示并 Close。若设备不断发送而 UI 卡住，检查 Read timeout 是否太大、是否未设 LF、以及事件 Timeout 是否被按钮事件长期占用。

一个 COM 只能由一个进程占用：虚拟串口成对时 Python 打开 COM8，LabVIEW 打开 COM7。不要让 Python 和 LabVIEW 打开同一端。串口模式下模拟器启动停止发帧，LabVIEW START 后才发；可先发 PING 看 PONG。
