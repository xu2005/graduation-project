# LabVIEW VI 搭建入口

当前目录是**逐 VI 搭建说明**，不是可运行 LabVIEW 工程。按以下顺序用 LabVIEW 2025 新建 VI，并保存为同名 `.vi`：`Parse_SCAN_Frame`、`Polar_To_XY`、`Serial_Init`、`Serial_Read`、`Serial_Write`、`Save_CSV`、`Main`。每个说明含接线端、内部步骤和单独测试方法。

最后在 LabVIEW File → New Project → Blank Project，保存 `Scanner.lvproj`，右击 My Computer → Add → File，添加七个真实 VI；设 Main 为顶层运行 VI。版本较老的 LabVIEW 可能无法打开新版本保存的 VI，建议全部在同一版本搭建。系统发现 VI Scripting 组件，但本次未能验证可重复的自动创建/保存流程，因此没有提供未经验证的脚本或伪 `.vi`。
