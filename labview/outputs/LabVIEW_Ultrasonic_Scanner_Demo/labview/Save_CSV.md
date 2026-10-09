# Save_CSV.vi 搭建

**接线板**：输入 `File Path` (path)、`Records`（一维记录簇数组：timestamp string、angle/distance/temperature/x/y 五个 DBL）、`error in`；输出 `Rows Written` (I32)、`error out`。

1. 先构造表头 `timestamp,angle_deg,distance_cm,temperature_c,x_cm,y_cm\r\n`。
2. For Loop 遍历 Records，用 Format Into String 把每条记录转为一行，数值建议固定三位小数；日期时间字符串不得含逗号或换行。每行追加 CRLF。选用固定英文数字区域设置，检查 CSV 小数点是 `.`。记录创建时用 Get Date/Time in Seconds + Format Date/Time String 生成时间戳；毫秒可按当前 LabVIEW 版本可用格式添加，最少保留秒。
3. 使用 Write to Text File 一次写完整 UTF-8 文本，覆盖用户选定的新文件或先确认覆盖；error wire 串接。文件对话框取消时 Main 不调用此 VI。
4. 用文本编辑器检查标题、行数和列数。Save Data 导出的是本次运行累计的有效记录；Clear Graph 只清图。长时间连续记录可在后续版本改成逐行追加，以减少内存。
