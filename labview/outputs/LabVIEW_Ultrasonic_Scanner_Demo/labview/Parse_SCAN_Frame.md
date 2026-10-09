# Parse_SCAN_Frame.vi 搭建

**接线板**：输入 `RX Line` (string)、`Max Distance cm` (DBL，默认 400)；输出 `Valid` (Boolean)、`Angle deg`/`Distance cm`/`Temperature C` (DBL)、`Reason` (string)。错误帧输出 Valid=false，数字输出 0，Reason 为明确原因，不抛出导致 Main 停止的错误。

1. Trim Whitespace 去 CR/LF；空串 Case 直接失败。
2. 用 Spreadsheet String To Array 配字符串类型及逗号分隔，或用 Match Pattern 循环拆分；数组大小必须为 4，第 0 项必须为 `$SCAN`。若使用 Spreadsheet String To Array，先单独测试连续逗号空字段是否保留；不能保留就改用 Match Pattern 精确拆分。
3. 后三项分别用 Scan From String `%f` 转 DBL，同时验证 error out 和剩余字符串是否仅为空白；任何字母尾缀均失败。LabVIEW `%f` 在不同区域设置下的小数点行为要用示例帧验证。
4. 比较 `0≤angle≤180`、`0<distance≤Max Distance`、`-40≤temperature≤80`，并拒绝 NaN/Inf。用 Boolean AND 汇总为 Valid；选择第一个失败原因写 Reason。
5. 把接线板的所有输入输出接好。测试六条坏帧见 `docs/06_字符串协议解析.md`，尤其 `12abc`、字段过多、距离 0 和 NaN。

类型判断与错误分支在这里留出将来加 CRC 的位置；Main 收到 PONG 时绕过本 VI。
