# 第一次动手：30 分钟跑通 LabVIEW 串口 Demo

**本次只做一件事：**在 LabVIEW 2025 的一个 `Main.vi` 中打开 COM，循环读取以 `\r\n` 结束的 `$SCAN,角度,距离,温度`，显示原始行和三个数值。示例帧 `$SCAN,45.0,123.4,26.0\r\n` 应解析为 45.0°、123.4 cm、26.0 °C。本次不做图表、保存、发送控制命令、状态机或 Event Structure。

**计时起点：**LabVIEW 2025 和 NI-VISA 已安装，已有一对可用的虚拟串口（下文举例 COM7↔COM8），且 Python 3.10+ 可运行。安装虚拟串口驱动不计入 30 分钟；没有虚拟串口时，也可用已在持续发送同格式帧的真实 STM32。现有工程尚无 `.vi`，你将亲手创建它。

## 0–4 分钟：让现有模拟器持续发帧

在工程根目录打开两个 PowerShell 窗口。**窗口 A**（只需首次安装依赖）：

```powershell
py -m pip install -r tools\requirements.txt
py tools\stm32_serial_simulator.py --mode serial --port COM8
```

看到“等待 `$START`”是正常的。模拟器已打开 COM8；由于本次 VI 不发送命令，先在**窗口 B**临时打开配对端 COM7，发一次 START 后立即关闭：

```powershell
py -c 'import serial; s=serial.Serial("COM7",115200); s.write(b"$START\r\n"); s.close()'
```

窗口 B 命令结束后 COM7 才交给 LabVIEW。若 `py` 提示没有 Python，先看工程 README 的解释器说明；若端口号不是 COM7/COM8，三处都改为实际配对号。**验收：**窗口 A 持续运行且没有串口打开错误。模拟器的默认第一帧角度是 30°，随后 32°、34°……，不会恰好输出上面的 45° 示例帧。**常见错误：**两个程序抢同一个 COM；虚拟端口并未真正配对；没先发 START，模拟器就一直安静。

## 4–18 分钟：建立 Front Panel 和串口读取框图

1. **Front Panel（前面板）：**LabVIEW 2025 → `File → New VI`。空白处右键，搜索 `Stop Button`，放一个 **Boolean Control**，改名 `停止（Stop）`。再右键搜索 `String Indicator`，放一个 **String Indicator**，改名 `原始接收（Raw RX）`。在 Raw RX 上右键选 `\ Codes Display`，以后能直接看到末尾的 `\r\n`。`Ctrl+S`，保存为本工程 `labview/Main.vi`。
2. 按 `Ctrl+E` 到 **Block Diagram（程序框图）**。空白处右键搜索 `VISA Configure Serial Port`，放在左侧、循环外。右键它的 `VISA resource name` **输入端** → `Create → Control`，自动得到 COM 的 **Control**，回前面板改名 `串口（COM Port）`，选 COM7。对 `baud rate` 输入端也做 `Create → Control`，命名 `波特率（Baud Rate）`，前面板填 `115200`。
3. 仍在框图：对 Configure 的 `timeout (ms)` 输入端右键 → `Create → Constant`，填 `200`；`enable termination char` 输入端建布尔常量并设 `True`；`termination char` 输入端建数值常量并填 `10`（LF）。其余串口设置采用 8 数据位、无校验、1 停止位的默认值。右键 Configure 的 `error out` → `Create → Indicator`，前面板命名 `打开串口错误`；这是 **Indicator**，方便发现端口被占用。鼠标停在节点端子上按 `Ctrl+H` 可看端子名称，不要凭位置猜。
4. 框图空白处右键搜索 `While Loop`，在 Configure 右侧画一个较大的循环框。把 `停止（Stop）` 的绿色终端拖到循环**内部**，连到循环右下角红色停止端子；若端子不是 `Stop if True`，右键该端子切换成 `Stop if True`。按钮终端必须在循环内，点击才能被反复读取。
5. 在循环内部右键搜索 `VISA Read`。把 Configure 的 `VISA resource name` **输出**接到 VISA Read 同名**输入**；线穿过循环左边框时自动形成 tunnel。把 Configure 的 `error out` 再分一支，穿过循环边框接 VISA Read 的 `error in`。对 VISA Read 的 `byte count` 输入右键 `Create → Constant`，填 `256`。LF 终止已启用，所以 256 是**上限**，收到 LF 会提前返回一整行。
6. 把 VISA Read 的 `read buffer`（字符串输出）接到 `原始接收（Raw RX）` 粉色终端；确保这个 Indicator 终端在循环**内**。在循环右侧、循环外右键搜索 `VISA Close`。把 VISA Read 的 `VISA resource name` 输出穿过循环右边框，接到 VISA Close 的同名输入；**先从循环出来再 Close**，保证按 Stop 后释放 COM。VISA Close 的 `error in` 此时留空；不要把读取超时错误接到下一轮读取或 Close。

此时框图主线应是：

```text
COM、115200 → VISA Configure → [While Loop: VISA Read → Raw RX；Stop] → VISA Close
                        error out → VISA Read error in（只从循环外进入，不反馈）
```

**验收：**点工具栏白色运行箭头；`Raw RX` 每约 100 ms 更新为 `$SCAN,30.0,...\r\n`、`$SCAN,32.0,...\r\n` 等。点 `停止（Stop）` 后 VI 结束，再运行时 COM7 能重新打开。**常见错误：**运行箭头破碎时点击它看断线清单；Raw RX 空白时核对 COM7/COM8、START、115200、LF=10；`打开串口错误.status=True` 多为 COM 不存在或被占用。这里**不要**把 VISA Read 的 `error out` 送回下一次 Read：空闲超时会使后续读取被跳过。[NI 说明 LF 可以结束 VISA Read](https://www.ni.com/en/support/documentation/supplemental/06/termination-characters-in-ni-visa.html)。

## 18–27 分钟：解析 Angle / Distance / Temperature

1. 先按 Stop，回 `Block Diagram` 修改。右键搜索 `Trim Whitespace`，放在 VISA Read 右侧。把 `read buffer` 的线分支接到它的 `string` 输入；`Raw RX` 原有连线保留。该节点去掉读到的末尾 CR/LF，只把干净文本交给解析。
2. 右键搜索 `Scan From String`，放在 Trim 右侧。`trimmed string` → `input string`。右键 Scan 节点的 `format string` 输入 → `Create → Constant`，常量**原样输入** `$SCAN,%f,%f,%f`；这要求开头是 `$SCAN,`，再读三个浮点数。将 Scan 节点的**下边缘向下拖**，直到右侧出现 `output 1`、`output 2`、`output 3` 三个数值输出；用 `Ctrl+H` 核对名称。
3. VISA Read 的 `error out` → Scan 的 `error in`。右键搜索 `Unbundle By Name`，放在 Scan 右侧；Scan 的 `error out` → 它的输入，点击其字段名选择 `status`。再右键搜索 `Case Structure`，在三项数值输出的右边画一个小 Case；将 `status` 布尔线接到 Case 左侧的 `?` 选择端。`False` 表示读取和解析均成功，`True` 表示超时或格式错误。
4. 切到 Case 的 **False** 分支。在三个 Scan 数值输出端依次右键 `Create → Indicator`，把新建的三个橙色 Indicator 终端拖到 False 分支**内部**；如果原连线断开，就重新从各 Scan 输出穿过 Case 左边框接到对应终端。到 Front Panel 分别命名 `当前角度（Angle）`、`距离 cm（Distance）`、`温度 °C（Temperature）`。它们都是 **Numeric Indicator**，不是可编辑的 Control。切到 **True** 分支，保持为空；这样坏帧或空闲超时不会覆盖上一组数值。Case Structure 只是成功/失败分支，不是状态机。
5. `Ctrl+S`，运行。若数字在 False 分支里出现断线，确认 Scan 节点已经展开到 3 个输出，并且从 Case 外的 Scan 输出连到了 Case 内的各 Indicator。若节点自动创建的 Indicator 终端落在 Case 外，停止运行后把**终端**拖进 False 分支，不要只移动前面板外观。

**验收：**当 Raw RX 收到例如 `$SCAN,45.0,123.4,26.0\r\n`，三项分别显示 `45.0`、`123.4`、`26.0`；现有模拟器的数据会随角度变化。按 Stop 后 COM 被关闭。**常见错误：**三个数字一直为 0，检查格式常量中的 `$SCAN,`、三个 `%f` 和 Scan `error out.status`；错误 85 常见于帧格式不匹配；Raw RX 有 `\r\n` 但数字不动，检查 `Trim Whitespace` 输出是否真正接入 Scan。此 30 分钟版本仅解析固定四字段格式；更严格的坏帧检查以后按 `06_字符串协议解析.md` 扩展。[NI 的 Scan From String 说明](https://knowledge.ni.com/KnowledgeArticleDetails?id=kA00Z0000019N4XSAU)可用来核对格式字符串和错误 85。

## 27–30 分钟：保存并复查

按 Stop → `Ctrl+S` → 再运行一次。确认 `Raw RX` 在刷新、三项数字有值、停止后可以重新打开 COM。把这个 `Main.vi` 留作工程的第一版；以后再按原工程文档加入控制按钮、图和 CSV。本教程的 LabVIEW 操作步骤是搭建指导，当前工程交付时尚无已验证运行的真实 VI。
