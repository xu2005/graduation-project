# 07 XY Graph 二维扫描

扫描点 `(angle,distance)` 转 `theta=angle*pi/180`，`x=distance*cos(theta)`，`y=distance*sin(theta)`，单位 cm。例：45°、123.4 cm → x≈87.26、y≈87.26。只对解析有效的点计算。坐标系约定 0° 在 X 正向，90° 在 Y 正向；舵机安装方向不同，可在 Polar VI 调整角度映射。

主 While Loop 左右移位寄存器各保留 `X Array`、`Y Array`；有效点到来时各追加一个 DBL。长度超过 500 时同时从两数组头部删去相同数量，确保索引配对。Bundle 两数组为一条曲线的 XY cluster，接 XY Graph。Plot Legend 选点（不连线），X 轴建议 -400..400 cm，Y 轴 0..400 cm。清图按钮写两个空数组到寄存器并同步写 Graph。缓存上限只约束图；CSV 的记录数组另行管理。
