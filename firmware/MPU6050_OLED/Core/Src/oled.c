/**
  ******************************************************************************
  * @file    oled.c
  * @brief   SSD1306 OLED 驱动（软件模拟 I2C）
  ******************************************************************************
  * @note    PB8/PB9 是开漏输出，所以：
  *            写 0 -> 开漏 MOS 导通，总线被拉低；
  *            写 1 -> 输出级释放，靠模块上的外部上拉电阻把总线拉高。
  *          这也意味着读取 SDA 时必须先把 SDA 写成 1（释放总线），
  *          再读 IDR，才能读到从机给出的电平，ACD 应答就是这么读的。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "oled.h"
#include "oled_font_cn.h"

/* Private constants ---------------------------------------------------------*/
/* SSD1306 控制字节：
   Co = 0 表示后续字节沿用本次的 D/C#，不再插入控制字节，直到 STOP；
   Co = 1 表示每个数据字节前都要再跟一个控制字节。
   D/C# = 0 表示命令，D/C# = 1 表示显存数据。
   下面两个值都取 Co = 0，所以 OLED_WriteData() 可以一次连续发送整页数据。 */
#define OLED_CTRL_CMD          0x00U
#define OLED_CTRL_DATA         0x40U

/* Private functions prototypes ----------------------------------------------*/
static void OLED_I2C_Delay(void);
static void OLED_I2C_Start(void);
static void OLED_I2C_Stop(void);
static uint8_t OLED_I2C_SendByte(uint8_t byte);
static void OLED_WriteCmd(uint8_t cmd);
static void OLED_WriteData(const uint8_t *data, uint16_t len);
static void OLED_SetCursor(uint8_t x, uint8_t page);
static void OLED_WriteByte(uint8_t byte, uint8_t is_data);
static void OLED_DrawGlyph(uint8_t x, uint8_t page, const uint8_t *glyph, uint8_t width);
static const uint8_t *OLED_FindCJK(uint16_t code);
static uint8_t OLED_Utf8Next(const char **text, uint16_t *code);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  软件 I2C 的位延时
  * @note   这只是给总线速度留裕量的粗略延时，不是精确标定值：循环次数在
  *         -O0 和 -Os 下对应的实际时间不同，而且一个数据位要经过多次本函数
  *         和多次 HAL 调用，最终 SCL 周期必须在实机上用示波器确认
  *         （SSD1306 要求 SCL 周期不小于 2.5 us，SDA 保持时间不小于 300 ns）。
  *         当前取值只保证速度远低于 400 kHz 上限。
  */
static void OLED_I2C_Delay(void)
{
  for (volatile uint32_t i = 0U; i < 40U; i++)
  {
  }
}

static void OLED_SCL_High(void)
{
  HAL_GPIO_WritePin(OLED_SCL_GPIO_PORT, OLED_SCL_PIN, GPIO_PIN_SET);
}

static void OLED_SCL_Low(void)
{
  HAL_GPIO_WritePin(OLED_SCL_GPIO_PORT, OLED_SCL_PIN, GPIO_PIN_RESET);
}

static void OLED_SDA_High(void)
{
  HAL_GPIO_WritePin(OLED_SDA_GPIO_PORT, OLED_SDA_PIN, GPIO_PIN_SET);
}

static void OLED_SDA_Low(void)
{
  HAL_GPIO_WritePin(OLED_SDA_GPIO_PORT, OLED_SDA_PIN, GPIO_PIN_RESET);
}

static uint8_t OLED_SDA_Read(void)
{
  return (HAL_GPIO_ReadPin(OLED_SDA_GPIO_PORT, OLED_SDA_PIN) == GPIO_PIN_SET) ? 1U : 0U;
}

/**
  * @brief  I2C 起始条件：SCL 为高时 SDA 由高变低
  */
static void OLED_I2C_Start(void)
{
  OLED_SDA_High();
  OLED_SCL_High();
  OLED_I2C_Delay();
  OLED_SDA_Low();
  OLED_I2C_Delay();
  OLED_SCL_Low();
  OLED_I2C_Delay();
}

/**
  * @brief  I2C 停止条件：SCL 为高时 SDA 由低变高
  */
static void OLED_I2C_Stop(void)
{
  OLED_SDA_Low();
  OLED_SCL_High();
  OLED_I2C_Delay();
  OLED_SDA_High();
  OLED_I2C_Delay();
}

/**
  * @brief  发送一个字节，并在第 9 个时钟读取从机应答
  * @param  byte 待发送字节，高位先发
  * @retval 1 表示收到从机应答（SDA 被拉低），0 表示无应答
  * @note   SSD1306 的写操作没有可读取的数据，一次写失败最直观的现象就是
  *         屏幕没反应，所以本阶段调用者不根据应答做错误处理。
  */
static uint8_t OLED_I2C_SendByte(uint8_t byte)
{
  uint8_t ack_received = 0U;

  for (uint8_t i = 0U; i < 8U; i++)
  {
    /* SDA 只能在 SCL 为低电平时改变 */
    OLED_SCL_Low();
    OLED_I2C_Delay();

    if ((byte & 0x80U) != 0U)
    {
      OLED_SDA_High();
    }
    else
    {
      OLED_SDA_Low();
    }
    byte = (uint8_t)(byte << 1);

    OLED_I2C_Delay();
    OLED_SCL_High();
    OLED_I2C_Delay();
  }

  /* 第 9 个时钟：主机释放 SDA，由从机拉低表示应答 */
  OLED_SCL_Low();
  OLED_SDA_High();
  OLED_I2C_Delay();
  OLED_SCL_High();
  OLED_I2C_Delay();

  ack_received = (OLED_SDA_Read() == 0U) ? 1U : 0U;

  OLED_SCL_Low();
  OLED_I2C_Delay();

  return ack_received;
}

/**
  * @brief  向 SSD1306 写入一个控制字节 + 一个数据字节
  * @param  byte    待写入的字节
  * @param  is_data 0 表示命令，1 表示显存数据
  */
static void OLED_WriteByte(uint8_t byte, uint8_t is_data)
{
  OLED_I2C_Start();
  (void)OLED_I2C_SendByte(OLED_I2C_ADDR_WRITE);
  (void)OLED_I2C_SendByte((is_data != 0U) ? OLED_CTRL_DATA : OLED_CTRL_CMD);
  (void)OLED_I2C_SendByte(byte);
  OLED_I2C_Stop();
}

static void OLED_WriteCmd(uint8_t cmd)
{
  OLED_WriteByte(cmd, 0U);
}

/**
  * @brief  连续写入一段显存数据（页寻址模式下列地址会自动加一）
  */
static void OLED_WriteData(const uint8_t *data, uint16_t len)
{
  OLED_I2C_Start();
  (void)OLED_I2C_SendByte(OLED_I2C_ADDR_WRITE);
  (void)OLED_I2C_SendByte(OLED_CTRL_DATA);
  for (uint16_t i = 0U; i < len; i++)
  {
    (void)OLED_I2C_SendByte(data[i]);
  }
  OLED_I2C_Stop();
}

/**
  * @brief  设置写入光标：页地址 + 列地址（页寻址模式）
  * @param  x    列地址，0 ~ 127
  * @param  page 页地址，0 ~ 7
  */
static void OLED_SetCursor(uint8_t x, uint8_t page)
{
  OLED_WriteCmd((uint8_t)(0xB0U | page));               /* 页起始地址 */
  OLED_WriteCmd((uint8_t)(0x00U | (x & 0x0FU)));        /* 列低 4 位 */
  OLED_WriteCmd((uint8_t)(0x10U | ((x >> 4) & 0x0FU))); /* 列高 4 位 */
}

/* Exported functions --------------------------------------------------------*/

void OLED_Init(void)
{
  /* SSD1306 上电后需要一点时间完成内部复位，等待期间不做任何总线操作 */
  HAL_Delay(100U);

  OLED_WriteCmd(0xAEU);        /* 关闭显示，以下配置在关显示状态下进行 */

  OLED_WriteCmd(0x20U);        /* 设置内存寻址模式 */
  OLED_WriteCmd(0x02U);        /* 页寻址模式，便于按页刷新 */

  OLED_WriteCmd(0xB0U);        /* 页起始地址 = 0 */
  OLED_WriteCmd(0xC8U);        /* COM 扫描方向：从下往上，配合模块的安装方向 */
  OLED_WriteCmd(0x00U);        /* 列低 4 位 = 0 */
  OLED_WriteCmd(0x10U);        /* 列高 4 位 = 0 */
  OLED_WriteCmd(0x40U);        /* 显示起始行 = 0 */

  OLED_WriteCmd(0x81U);        /* 对比度 */
  OLED_WriteCmd(0xCFU);

  OLED_WriteCmd(0xA1U);        /* 段重映射：左右翻转，配合 COM 扫描方向用 */
  OLED_WriteCmd(0xA6U);        /* 正常显示（0xA7 为反色） */
  OLED_WriteCmd(0xA8U);        /* 多路复用比 */
  OLED_WriteCmd(0x3FU);        /* 1/64 duty */
  OLED_WriteCmd(0xA4U);        /* 显示内容跟随显存，而非强制全亮 */

  OLED_WriteCmd(0xD3U);        /* 显示偏移 */
  OLED_WriteCmd(0x00U);

  OLED_WriteCmd(0xD5U);        /* 时钟分频 / 振荡频率 */
  OLED_WriteCmd(0x80U);

  OLED_WriteCmd(0xD9U);        /* 预充电周期 */
  OLED_WriteCmd(0xF1U);

  OLED_WriteCmd(0xDAU);        /* COM 引脚硬件配置 */
  OLED_WriteCmd(0x12U);

  OLED_WriteCmd(0xDBU);        /* VCOMH 电压：手册只定义了 0x00/0x20/0x30，
                                  取复位默认值 0x20，行为有规格保证 */
  OLED_WriteCmd(0x20U);

  OLED_WriteCmd(0x8DU);        /* 电荷泵：SSD1306 内部升压，必须打开 */
  OLED_WriteCmd(0x14U);

  OLED_WriteCmd(0xAFU);        /* 打开显示 */

  OLED_Clear();
}

void OLED_Clear(void)
{
  /* 放在静态存储区，避免在只有 1 KB 的栈上临时开辟 128 字节 */
  static const uint8_t blank[OLED_WIDTH] = {0};

  for (uint8_t page = 0U; page < OLED_PAGE_COUNT; page++)
  {
    OLED_SetCursor(0U, page);
    OLED_WriteData(blank, OLED_WIDTH);
  }
}

/**
  * @brief  画一个字形：先写上半页，再写下半页
  * @param  glyph 页格式字形数据：前 width 字节是上半页各列，后 width 字节是下半页各列
  * @param  width 字形宽度（汉字 16，ASCII 8），高度固定 16
  */
static void OLED_DrawGlyph(uint8_t x, uint8_t page, const uint8_t *glyph, uint8_t width)
{
  OLED_SetCursor(x, page);
  OLED_WriteData(glyph, width);
  OLED_SetCursor(x, (uint8_t)(page + 1U));
  OLED_WriteData(&glyph[width], width);
}

/**
  * @brief  按 Unicode 码点在字库里查找汉字字形
  * @retval 字形指针；未收录时返回 NULL
  * @note   字库只有几十项，线性查找足够，不需要哈希或二分
  */
static const uint8_t *OLED_FindCJK(uint16_t code)
{
  for (uint8_t i = 0U; i < OLED_FONT_CN_COUNT; i++)
  {
    if (OLED_FontCN16_Code[i] == code)
    {
      return OLED_FontCN16[i];
    }
  }

  return NULL;
}

/**
  * @brief  从 UTF-8 字符串中取出一个字符
  * @param  text 输入输出参数，进入时指向当前字符，返回时指向下一个字符
  * @param  code 输出参数，字符的 Unicode 码点；无法识别时为 0
  * @retval 实际消费的字节数，恒大于 0，调用者不会陷入死循环
  * @note   源文件里的汉字是以 UTF-8 存进字符串常量的，显示时要按 UTF-8 解析。
  *         判断后续字节是否为合法续字节（0x80~0xBF）可以顺带挡住字符串结尾，
  *         因为 '\0' 不满足该条件，短路后不会再读 p[2]，不会越界。
  */
static uint8_t OLED_Utf8Next(const char **text, uint16_t *code)
{
  const uint8_t *p = (const uint8_t *)(*text);

  if ((p[0] & 0x80U) == 0U)
  {
    *code = (uint16_t)p[0];
    *text += 1;
    return 1U;
  }

  /* 常用汉字都在 U+4E00 ~ U+9FFF 之间，UTF-8 是三字节编码 */
  if (((p[0] & 0xF0U) == 0xE0U) &&
      ((p[1] & 0xC0U) == 0x80U) &&
      ((p[2] & 0xC0U) == 0x80U))
  {
    *code = (uint16_t)((((uint16_t)p[0] & 0x0FU) << 12) |
                       (((uint16_t)p[1] & 0x3FU) << 6) |
                       ((uint16_t)p[2] & 0x3FU));
    *text += 3;
    return 3U;
  }

  *code = 0U;
  *text += 1;
  return 1U;
}

void OLED_ShowText(uint8_t x, uint8_t page, const char *text)
{
  /* 一行占两页，所以最后一行的起始页只能是 OLED_PAGE_COUNT - 2 */
  if ((text == NULL) || (page >= (OLED_PAGE_COUNT - 1U)) || (x >= OLED_WIDTH))
  {
    return;
  }

  while (*text != '\0')
  {
    uint16_t code = 0U;

    (void)OLED_Utf8Next(&text, &code);

    if (code == 0U)
    {
      continue;   /* 非法字节，跳过它继续，不要丢掉整行剩余内容 */
    }

    if ((code >= OLED_FONT_ASCII_FIRST) && (code <= OLED_FONT_ASCII_LAST))
    {
      /* 超出右边界的字符直接丢弃，避免跨页回绕写花后面的内容 */
      if (((uint16_t)x + OLED_FONT_ASCII_WIDTH) > OLED_WIDTH)
      {
        break;
      }
      OLED_DrawGlyph(x, page, OLED_FontASCII8x16[code - OLED_FONT_ASCII_FIRST],
                     OLED_FONT_ASCII_WIDTH);
      x = (uint8_t)(x + OLED_FONT_ASCII_WIDTH);
    }
    else
    {
      const uint8_t *glyph = NULL;

      if (((uint16_t)x + OLED_FONT_CN_WIDTH) > OLED_WIDTH)
      {
        break;
      }

      glyph = OLED_FindCJK(code);
      if (glyph != NULL)
      {
        OLED_DrawGlyph(x, page, glyph, OLED_FONT_CN_WIDTH);
      }
      /* 未收录的汉字只占位不绘制，后面的字符位置才不会乱 */
      x = (uint8_t)(x + OLED_FONT_CN_WIDTH);
    }
  }
}

void OLED_ShowTextHex(uint8_t x, uint8_t page, uint8_t value)
{
  static const char hex_digits[] = "0123456789ABCDEF";
  char text[3];

  text[0] = hex_digits[(value >> 4) & 0x0FU];
  text[1] = hex_digits[value & 0x0FU];
  text[2] = '\0';

  OLED_ShowText(x, page, text);
}

void OLED_ShowTextNum(uint8_t x, uint8_t page, int32_t value, uint8_t width)
{
  char text[12];
  uint32_t magnitude;
  uint8_t pos;
  uint8_t negative = 0U;

  /* 定宽显示器：右对齐、左侧补空格，数值变化时字符位置不变 */
  if ((width == 0U) || (width > 11U))
  {
    return;
  }

  /* 先转成无符号再取负，避免 -2147483648 直接取绝对值时溢出 */
  if (value < 0)
  {
    negative = 1U;
    magnitude = (uint32_t)(-(value + 1)) + 1U;
  }
  else
  {
    magnitude = (uint32_t)value;
  }

  /* 从右往左填数字，位数不够时剩下的位置留作补空格 */
  text[width] = '\0';
  pos = width;
  do
  {
    pos--;
    text[pos] = (char)('0' + (magnitude % 10U));
    magnitude /= 10U;
  } while ((magnitude != 0U) && (pos != 0U));

  if ((negative != 0U) && (pos != 0U))
  {
    pos--;
    text[pos] = '-';
  }

  while (pos != 0U)
  {
    pos--;
    text[pos] = ' ';
  }

  OLED_ShowText(x, page, text);
}