# TuyaOpen 系统层 API 参考手册

> **版本**: 1.0  
> **适用平台**: Tuya IoT Development Platform  
> **头文件路径**: `src/tal_system/include/`  
> **版权**: Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.

---

## 目录

1. [公共类型说明](#1-公共类型说明)
2. [tal_system.h — 系统级工具与操作](#2-tal_systemh--系统级工具与操作)
3. [tal_log.h — 日志管理](#3-tal_logh--日志管理)
4. [tal_memory.h — 内存管理](#4-tal_memoryh--内存管理)
5. [tal_thread.h — 线程管理](#5-tal_threadh--线程管理)
6. [tal_sw_timer.h — 软件定时器管理](#6-tal_sw_timerh--软件定时器管理)

---

## 1. 公共类型说明

所有 API 均依赖 `tuya_cloud_types.h` 中定义的公共类型：

| 类型 | 说明 |
|------|------|
| `OPERATE_RET` | 操作返回值，`OPRT_OK` 表示成功，其他错误码参见 `tuya_error_code.h` |
| `BOOL_T` | 布尔类型，`TRUE` / `FALSE` |
| `uint32_t` / `int32_t` | 标准整型 |
| `size_t` | 大小类型 |
| `SYS_TICK_T` | 系统 tick 计数类型 |
| `SYS_TIME_T` | 系统毫秒时间类型 |
| `TIME_MS` | 毫秒时间类型 |
| `TUYA_RESET_REASON_E` | 系统复位原因枚举 |
| `TUYA_CPU_INFO_T` | CPU 信息结构体 |

---

## 2. tal_system.h — 系统级工具与操作

提供系统级操作接口，包括临界区管理、系统休眠与复位、tick 计数与毫秒计时、随机数生成、复位原因查询、延时操作和 CPU 信息获取。

### 2.1 便捷宏

#### `TAL_ENTER_CRITICAL()`

进入临界区。声明局部变量 `__irq_mask` 并保存当前中断掩码。

```c
TAL_ENTER_CRITICAL();
// 临界区代码
```

#### `TAL_EXIT_CRITICAL()`

退出临界区。恢复由 `TAL_ENTER_CRITICAL()` 保存的中断掩码。

```c
TAL_ENTER_CRITICAL();
// 临界区代码
TAL_EXIT_CRITICAL();
```

> **注意**: `TAL_ENTER_CRITICAL` 和 `TAL_EXIT_CRITICAL` 必须在同一作用域内成对使用。

---

### 2.2 临界区管理

#### `tal_system_enter_critical`

```c
uint32_t tal_system_enter_critical(void);
```

**功能**: 进入系统临界区（禁用中断）。

**参数**: 无

**返回值**: 当前中断掩码（`uint32_t`），需保存用于退出临界区时恢复。

---

#### `tal_system_exit_critical`

```c
void tal_system_exit_critical(uint32_t irq_mask);
```

**功能**: 退出系统临界区（恢复中断）。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `irq_mask` | IN | 由 `tal_system_enter_critical()` 返回的中断掩码 |

**返回值**: 无

---

### 2.3 系统休眠与复位

#### `tal_system_sleep`

```c
void tal_system_sleep(uint32_t time_ms);
```

**功能**: 系统休眠指定毫秒数。休眠期间系统可进入低功耗状态。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `time_ms` | IN | 休眠时间，单位：毫秒 |

**返回值**: 无

---

#### `tal_system_reset`

```c
void tal_system_reset(void);
```

**功能**: 系统复位（重启设备）。

**参数**: 无

**返回值**: 无（调用后系统将重启）

---

#### `tal_system_get_reset_reason`

```c
TUYA_RESET_REASON_E tal_system_get_reset_reason(char **describe);
```

**功能**: 获取系统复位原因。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `describe` | OUT | 指向复位原因描述字符串的指针 |

**返回值**: 复位原因枚举值（`TUYA_RESET_REASON_E`）。

---

### 2.4 时间与计时

#### `tal_system_get_tick_count`

```c
SYS_TICK_T tal_system_get_tick_count(void);
```

**功能**: 获取系统当前 tick 计数值。

**参数**: 无

**返回值**: 当前系统 tick 计数（`SYS_TICK_T`）。

---

#### `tal_system_get_millisecond`

```c
SYS_TIME_T tal_system_get_millisecond(void);
```

**功能**: 获取系统当前毫秒时间戳。

**参数**: 无

**返回值**: 当前系统毫秒时间（`SYS_TIME_T`）。

---

#### `tal_system_delay`

```c
void tal_system_delay(uint32_t time_ms);
```

**功能**: 系统延时（忙等待），不进入低功耗状态。适用于需要精确短延时的场景。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `time_ms` | IN | 延时时间，单位：毫秒 |

**返回值**: 无

> **与 `tal_system_sleep` 的区别**: `delay` 为忙等待，精度高但阻塞 CPU；`sleep` 可进入低功耗，适合较长等待。

---

### 2.5 随机数

#### `tal_system_get_random`

```c
int tal_system_get_random(uint32_t range);
```

**功能**: 获取范围 `[0, range)` 内的随机整数。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `range` | IN | 随机数上限（不含） |

**返回值**: `[0, range)` 范围内的随机整数。

---

### 2.6 CPU 信息

#### `tal_system_get_cpu_info`

```c
OPERATE_RET tal_system_get_cpu_info(TUYA_CPU_INFO_T **cpu_ary, int32_t *cpu_cnt);
```

**功能**: 获取系统 CPU 信息。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `cpu_ary` | OUT | 指向 CPU 信息数组的指针 |
| `cpu_cnt` | OUT | CPU 数量 |

**返回值**: `OPRT_OK` 成功，其他错误码参见 `tuya_error_code.h`。

---

### 2.7 PSRAM 操作（条件编译）

> 以下函数仅在定义了 `ENABLE_EXT_RAM` 且值为 `1` 时可用。PSRAM（伪静态随机存储器）用于大数据分配。

#### `tal_psram_malloc`

```c
void *tal_psram_malloc(size_t size);
```

**功能**: 从 PSRAM 分配指定大小的内存块。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `size` | IN | 要分配的内存大小（字节） |

**返回值**: 成功返回内存指针，失败返回 `NULL`。

---

#### `tal_psram_free`

```c
void tal_psram_free(void *ptr);
```

**功能**: 释放从 PSRAM 分配的内存。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `ptr` | IN | 要释放的内存指针。若为 `NULL`，则不执行任何操作 |

**返回值**: 无

---

#### `tal_psram_calloc`

```c
void *tal_psram_calloc(size_t nitems, size_t size);
```

**功能**: 从 PSRAM 分配并零初始化内存（分配 `nitems × size` 字节）。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `nitems` | IN | 元素数量 |
| `size` | IN | 每个元素的大小（字节） |

**返回值**: 成功返回已清零的内存指针，失败返回 `NULL`。

---

#### `tal_psram_realloc`

```c
void *tal_psram_realloc(void *ptr, size_t size);
```

**功能**: 重新分配 PSRAM 中的内存块。保留原有内容（取新旧大小的较小值）。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `ptr` | IN | 原内存指针。若为 `NULL`，等效于 `tal_psram_malloc(size)` |
| `size` | IN | 新的内存大小（字节） |

**返回值**: 成功返回新内存指针，失败返回 `NULL`。原指针在调用后可能失效。

---

#### `tal_psram_get_free_heap_size`

```c
int tal_psram_get_free_heap_size(void);
```

**功能**: 获取 PSRAM 堆的空闲内存大小。

**参数**: 无

**返回值**: PSRAM 空闲堆内存大小（字节）。

---

## 3. tal_log.h — 日志管理

提供分级日志、模块日志、Hex 转储、彩色输出等功能。支持自定义输出终端和日志级别控制。

### 3.1 日志显示模式枚举

`TAL_LOG_DISPLAY_MODE_E`（`uint8_t` 类型）:

| 宏 | 值 | 说明 |
|----|----|------|
| `TAL_LOG_DISPLAY_MODE_DEFAULT` | 0 | 默认显示 |
| `TAL_LOG_DISPLAY_MODE_HIGH_LIGHT` | 1 | 高亮显示 |
| `TAL_LOG_DISPLAY_MODE_UNDER_LINE` | 4 | 下划线 |
| `TAL_LOG_DISPLAY_MODE_FLASH` | 5 | 闪烁 |
| `TAL_LOG_DISPLAY_MODE_REVERSE` | 7 | 反色 |

### 3.2 字体颜色枚举

`TAL_LOG_FONT_COLOR_E`（`uint8_t` 类型）:

| 宏 | 值 | 颜色 |
|----|----|------|
| `TAL_LOG_FONT_COLOR_BLACK` | 30 | 黑色 |
| `TAL_LOG_FONT_COLOR_RED` | 31 | 红色 |
| `TAL_LOG_FONT_COLOR_GREEN` | 32 | 绿色 |
| `TAL_LOG_FONT_COLOR_YELLOW` | 33 | 黄色 |
| `TAL_LOG_FONT_COLOR_BLUE` | 34 | 蓝色 |
| `TAL_LOG_FONT_COLOR_PURPLE` | 35 | 紫色 |
| `TAL_LOG_FONT_COLOR_CYAN` | 36 | 青色 |
| `TAL_LOG_FONT_COLOR_WHITE` | 37 | 白色 |
| `TAL_LOG_FONT_COLOR_DEFAULT` | 39 | 默认 |

### 3.3 背景颜色枚举

`TAL_LOG_BACKGROUND_COLOR_E`（`uint8_t` 类型）:

| 宏 | 值 | 颜色 |
|----|----|------|
| `TAL_LOG_BACKGROUND_COLOR_BLACK` | 40 | 黑色 |
| `TAL_LOG_BACKGROUND_COLOR_RED` | 41 | 红色 |
| `TAL_LOG_BACKGROUND_COLOR_GREEN` | 42 | 绿色 |
| `TAL_LOG_BACKGROUND_COLOR_YELLOW` | 43 | 黄色 |
| `TAL_LOG_BACKGROUND_COLOR_BLUE` | 44 | 蓝色 |
| `TAL_LOG_BACKGROUND_COLOR_PURPLE` | 45 | 紫色 |
| `TAL_LOG_BACKGROUND_COLOR_CYAN` | 46 | 青色 |
| `TAL_LOG_BACKGROUND_COLOR_WHITE` | 47 | 白色 |
| `TAL_LOG_BACKGROUND_COLOR_DEFAULT` | 49 | 默认 |

### 3.4 日志级别枚举

`TAL_LOG_LEVEL_E`:

| 枚举值 | 说明 |
|--------|------|
| `TAL_LOG_LEVEL_ERR` | 错误 |
| `TAL_LOG_LEVEL_WARN` | 警告 |
| `TAL_LOG_LEVEL_NOTICE` | 通知 |
| `TAL_LOG_LEVEL_INFO` | 信息 |
| `TAL_LOG_LEVEL_DEBUG` | 调试 |
| `TAL_LOG_LEVEL_TRACE` | 跟踪 |

### 3.5 常量

| 宏 | 值 | 说明 |
|----|----|------|
| `DEF_LOG_BUF_LEN` | 4096（或 `MAX_SIZE_OF_DEBUG_BUF`） | 默认日志缓冲区大小 |

### 3.6 回调类型

```c
typedef void (*TAL_LOG_OUTPUT_CB)(const char *str);
```

日志输出回调函数原型，接收格式化后的日志字符串。

### 3.7 日志打印宏

以下宏自动携带文件名和行号，支持 printf 风格格式化：

#### 基础日志宏

| 宏 | 级别 | 说明 |
|----|------|------|
| `PR_ERR(fmt, ...)` | ERR | 错误日志 |
| `PR_WARN(fmt, ...)` | WARN | 警告日志 |
| `PR_NOTICE(fmt, ...)` | NOTICE | 通知日志 |
| `PR_INFO(fmt, ...)` | INFO | 信息日志 |
| `PR_DEBUG(fmt, ...)` | DEBUG | 调试日志 |
| `PR_TRACE(fmt, ...)` | TRACE | 跟踪日志 |

#### TAL 前缀日志宏（功能同上，推荐在 TAL 层使用）

| 宏 | 级别 |
|----|------|
| `TAL_PR_ERR(fmt, ...)` | ERR |
| `TAL_PR_WARN(fmt, ...)` | WARN |
| `TAL_PR_NOTICE(fmt, ...)` | NOTICE |
| `TAL_PR_INFO(fmt, ...)` | INFO |
| `TAL_PR_DEBUG(fmt, ...)` | DEBUG |
| `TAL_PR_TRACE(fmt, ...)` | TRACE |

#### Hex 转储日志宏

| 宏 | 级别 | 说明 |
|----|------|------|
| `PR_HEXDUMP_ERR(title, buf, size)` | ERR | 每行 8 字节 |
| `PR_HEXDUMP_WARN(title, buf, size)` | WARN | 每行 8 字节 |
| `PR_HEXDUMP_NOTICE(title, buf, size)` | NOTICE | 每行 8 字节 |
| `PR_HEXDUMP_INFO(title, buf, size)` | INFO | 每行 8 字节 |
| `PR_HEXDUMP_DEBUG(title, buf, size)` | DEBUG | 每行 8 字节 |
| `PR_HEXDUMP_TRACE(title, buf, size)` | TRACE | 每行 8 字节 |
| `PR_HEX_DUMP(title, width, buf, size)` | NOTICE | 自定义每行宽度 |

#### 其他日志宏

| 宏 | 说明 |
|----|------|
| `PR_DEBUG_RAW(fmt, ...)` | 原始打印，不带日志前缀 |
| `PR_TRACE_ENTER()` | 打印函数进入跟踪 |
| `PR_TRACE_LEAVE()` | 打印函数离开跟踪 |

---

### 3.8 API 函数

#### `tal_log_init`

```c
OPERATE_RET tal_log_init(const TAL_LOG_LEVEL_E level, const int buf_len, const TAL_LOG_OUTPUT_CB output);
```

**功能**: 初始化日志管理系统。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `level` | IN | 初始日志级别 |
| `buf_len` | IN | 日志缓冲区大小 |
| `output` | IN | 日志输出回调函数 |

**返回值**: `OPRT_OK` 成功，其他错误码参见 `tuya_error_code.h`。

---

#### `tal_log_add_output_term`

```c
OPERATE_RET tal_log_add_output_term(const char *name, const TAL_LOG_OUTPUT_CB term);
```

**功能**: 添加一个日志输出终端。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `name` | IN | 终端名称（唯一标识） |
| `term` | IN | 输出回调函数 |

**返回值**: `OPRT_OK` 成功，其他错误码参见 `tuya_error_code.h`。

---

#### `tal_log_del_output_term`

```c
void tal_log_del_output_term(const char *name);
```

**功能**: 删除一个日志输出终端。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `name` | IN | 要删除的终端名称 |

**返回值**: 无

---

#### `tal_log_set_level`

```c
OPERATE_RET tal_log_set_level(const TAL_LOG_LEVEL_E level);
```

**功能**: 设置全局日志级别。低于该级别的日志将被过滤。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `level` | IN | 日志级别 |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_log_get_level`

```c
OPERATE_RET tal_log_get_level(TAL_LOG_LEVEL_E *level);
```

**功能**: 获取当前全局日志级别。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `level` | OUT | 返回当前全局日志级别 |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_log_set_ms_info`

```c
OPERATE_RET tal_log_set_ms_info(BOOL_T if_ms_level);
```

**功能**: 设置日志时间戳是否包含毫秒信息。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `if_ms_level` | IN | `TRUE` 显示毫秒，`FALSE` 不显示 |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_log_add_module_level`

```c
OPERATE_RET tal_log_add_module_level(const char *module_name, const TAL_LOG_LEVEL_E level);
```

**功能**: 为指定模块添加独立的日志级别。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `module_name` | IN | 模块名称 |
| `level` | IN | 该模块的日志级别 |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_log_set_module_level`

```c
OPERATE_RET tal_log_set_module_level(const char *module_name, TAL_LOG_LEVEL_E level);
```

**功能**: 设置指定模块的日志级别。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `module_name` | IN | 模块名称 |
| `level` | IN | 日志级别 |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_log_get_module_level`

```c
OPERATE_RET tal_log_get_module_level(const char *module_name, TAL_LOG_LEVEL_E *level);
```

**功能**: 获取指定模块的日志级别。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `module_name` | IN | 模块名称 |
| `level` | OUT | 返回该模块的日志级别 |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_log_delete_module_level`

```c
OPERATE_RET tal_log_delete_module_level(const char *module_name);
```

**功能**: 删除指定模块的独立日志级别设置（恢复使用全局级别）。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `module_name` | IN | 模块名称 |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_log_print`

```c
OPERATE_RET tal_log_print(const TAL_LOG_LEVEL_E level, const char *file, const int line, const char *fmt, ...);
```

**功能**: 打印日志（底层函数，通常通过宏调用）。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `level` | IN | 日志级别 |
| `file` | IN | 源文件名 |
| `line` | IN | 行号 |
| `fmt` | IN | 格式化字符串 |
| `...` | IN | 可变参数 |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_log_print_secure`

```c
OPERATE_RET tal_log_print_secure(BOOL_T is_const_fmt, const TAL_LOG_LEVEL_E level, const char *file, const int line, const char *fmt, ...);
```

**功能**: 安全打印日志（支持格式字符串检查，底层函数，通常通过宏调用）。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `is_const_fmt` | IN | 格式字符串是否为常量 |
| `level` | IN | 日志级别 |
| `file` | IN | 源文件名 |
| `line` | IN | 行号 |
| `fmt` | IN | 格式化字符串 |
| `...` | IN | 可变参数 |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_log_print_raw`

```c
OPERATE_RET tal_log_print_raw(const char *pFmt, ...);
```

**功能**: 原始打印（不带日志前缀、级别、文件名等信息）。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `pFmt` | IN | 格式化字符串 |
| `...` | IN | 可变参数 |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_log_vprint_raw`

```c
OPERATE_RET tal_log_vprint_raw(const char *pFmt, va_list ap);
```

**功能**: `tal_log_print_raw` 的 `va_list` 版本。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `pFmt` | IN | 格式化字符串 |
| `ap` | IN | `va_list` 参数列表 |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_log_print_escape`

```c
OPERATE_RET tal_log_print_escape(const TAL_LOG_LEVEL_E level, const char *file, const int line, const char *prefix, const char *user_str);
```

**功能**: 安全打印用户字符串，内部将 `%` 转义为 `%%` 以避免格式化解析。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `level` | IN | 日志级别 |
| `file` | IN | 源文件名 |
| `line` | IN | 行号 |
| `prefix` | IN | 固定前缀，可为 `NULL` 或空字符串 |
| `user_str` | IN | 用户字符串 |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_log_hex_dump`

```c
void tal_log_hex_dump(const TAL_LOG_LEVEL_E level, const char *file, const int line, const char *title, uint8_t width, uint8_t *buf, uint16_t size);
```

**功能**: 以十六进制格式打印缓冲区内容。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `level` | IN | 日志级别 |
| `file` | IN | 源文件名 |
| `line` | IN | 行号 |
| `title` | IN | 缓冲区标题 |
| `width` | IN | 每行显示的字节数 |
| `buf` | IN | 缓冲区地址 |
| `size` | IN | 缓冲区大小 |

**返回值**: 无

---

#### `tal_log_color_enable_set`

```c
void tal_log_color_enable_set(BOOL_T enable);
```

**功能**: 启用或禁用日志彩色输出。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `enable` | IN | `TRUE` 启用彩色，`FALSE` 禁用 |

**返回值**: 无

---

#### `tal_log_color_set`

```c
void tal_log_color_set(const TAL_LOG_LEVEL_E level, TAL_LOG_DISPLAY_MODE_E display_mode, TAL_LOG_FONT_COLOR_E font_color, TAL_LOG_BACKGROUND_COLOR_E background_color);
```

**功能**: 为指定日志级别设置颜色配置。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `level` | IN | 日志级别 |
| `display_mode` | IN | 显示模式（默认/高亮/下划线等） |
| `font_color` | IN | 字体颜色 |
| `background_color` | IN | 背景颜色 |

**返回值**: 无

---

#### `tal_log_color_print_raw`

```c
OPERATE_RET tal_log_color_print_raw(TAL_LOG_DISPLAY_MODE_E display_mode, TAL_LOG_FONT_COLOR_E font_color, TAL_LOG_BACKGROUND_COLOR_E background_color, const char *pFmt, ...);
```

**功能**: 打印带指定颜色的消息。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `display_mode` | IN | 显示模式 |
| `font_color` | IN | 字体颜色 |
| `background_color` | IN | 背景颜色 |
| `pFmt` | IN | 格式化字符串 |
| `...` | IN | 可变参数 |

**返回值**: `OPRT_OK` 成功，或写入字符数。

---

#### `tal_log_release`

```c
void tal_log_release(void);
```

**功能**: 销毁日志管理系统，释放所有资源。

**参数**: 无

**返回值**: 无

---

## 4. tal_memory.h — 内存管理

提供动态内存分配、释放、重分配及堆信息查询功能。

### 4.1 便捷宏

当定义了 `ENABLE_EXT_RAM` 且值为 `1` 时，以下宏映射到 PSRAM 版本；否则映射到内部 RAM 版本：

| 宏 | 内部 RAM 版本 | PSRAM 版本 |
|----|---------------|------------|
| `Malloc(req_size)` | `tal_malloc(req_size)` | `tal_psram_malloc(req_size)` |
| `Calloc(req_count, req_size)` | `tal_calloc(req_count, req_size)` | `tal_psram_calloc(req_count, req_size)` |
| `Free(ptr)` | `tal_free(ptr)` | `tal_psram_free(ptr)` |

---

### 4.2 API 函数

#### `tal_malloc`

```c
void *tal_malloc(size_t size);
```

**功能**: 从系统堆分配指定大小的内存。内存未初始化。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `size` | IN | 要分配的内存大小（字节） |

**返回值**: 成功返回内存指针，失败返回 `NULL`。

---

#### `tal_free`

```c
void tal_free(void *ptr);
```

**功能**: 释放由 `tal_malloc`/`tal_calloc`/`tal_realloc` 分配的内存。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `ptr` | IN | 要释放的内存指针。若为 `NULL`，则不执行操作 |

**返回值**: 无

---

#### `tal_calloc`

```c
void *tal_calloc(size_t nitems, size_t size);
```

**功能**: 分配并零初始化内存（分配 `nitems × size` 字节）。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `nitems` | IN | 元素数量 |
| `size` | IN | 每个元素的大小（字节） |

**返回值**: 成功返回已清零的内存指针，失败返回 `NULL`。

---

#### `tal_realloc`

```c
void *tal_realloc(void *ptr, size_t size);
```

**功能**: 重新分配内存块大小。保留原有内容。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `ptr` | IN | 原内存指针。若为 `NULL`，等效于 `tal_malloc(size)` |
| `size` | IN | 新的内存大小（字节） |

**返回值**: 成功返回新内存指针，失败返回 `NULL`。原指针可能失效。

---

#### `tal_system_get_free_heap_size`

```c
int tal_system_get_free_heap_size(void);
```

**功能**: 获取系统堆的空闲内存大小。

**参数**: 无

**返回值**: 空闲堆内存大小（字节）。

---

## 5. tal_thread.h — 线程管理

提供线程创建、启动、删除、状态查询和诊断功能。

### 5.1 常量

| 宏 | 值 | 说明 |
|----|----|------|
| `TAL_THREAD_MAX_NAME_LEN` | 16 | 线程名称最大长度 |

### 5.2 类型定义

#### 线程句柄

```c
typedef void *THREAD_HANDLE;
```

#### 回调函数类型

```c
typedef void (*THREAD_FUNC_CB)(void *args);   // 线程主函数
typedef void (*THREAD_ENTER_CB)(void);         // 线程进入回调（线程主函数执行前调用）
typedef void (*THREAD_EXIT_CB)(void);          // 线程退出回调（线程主函数执行后调用）
```

### 5.3 线程状态枚举

`THREAD_STATE_E`:

| 枚举值 | 值 | 说明 |
|--------|----|------|
| `THREAD_STATE_EMPTY` | 0 | 空闲/未使用 |
| `THREAD_STATE_RUNNING` | — | 运行中 |
| `THREAD_STATE_STOP` | — | 已停止 |
| `THREAD_STATE_DELETE` | — | 已删除 |

### 5.4 线程优先级枚举

`THREAD_PRIO_E`:

| 枚举值 | 值 | 说明 |
|--------|----|------|
| `THREAD_PRIO_0` | 5 | 最高优先级 |
| `THREAD_PRIO_1` | 4 | 次高优先级 |
| `THREAD_PRIO_2` | 3 | 中高优先级 |
| `THREAD_PRIO_3` | 2 | 中等优先级 |
| `THREAD_PRIO_4` | 1 | 中低优先级 |
| `THREAD_PRIO_5` | 0 | 低优先级 |
| `THREAD_PRIO_6` | 0 | 低优先级（与 PRIO_5 相同） |

> 数值越大优先级越高。

### 5.5 线程配置结构体

```c
typedef struct {
    uint32_t stackDepth;     // 栈大小（字节）
    uint8_t  priority;       // 线程优先级（参见 THREAD_PRIO_E）
    char    *thrdname;       // 线程名称（最大 TAL_THREAD_MAX_NAME_LEN - 1 字符）
    uint8_t  psram_mode : 1; // 是否在 PSRAM 中运行（1=是，0=否）
} THREAD_CFG_T;
```

### 5.6 API 函数

#### `tal_thread_create_and_start`

```c
OPERATE_RET tal_thread_create_and_start(
    THREAD_HANDLE       *handle,
    const THREAD_ENTER_CB enter,
    const THREAD_EXIT_CB  exit,
    const THREAD_FUNC_CB  func,
    const void           *func_args,
    const THREAD_CFG_T   *cfg
);
```

**功能**: 创建并启动一个 Tuya SDK 线程。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `handle` | OUT | 返回线程句柄 |
| `enter` | IN | 线程进入回调（线程主函数执行前调用），可为 `NULL` |
| `exit` | IN | 线程退出回调（线程主函数执行后调用），可为 `NULL` |
| `func` | IN | 线程主函数 |
| `func_args` | IN | 主函数参数，可为 `NULL` |
| `cfg` | IN | 线程配置参数 |

**返回值**: `OPRT_OK` 成功，其他错误码参见 `tuya_error_code.h`。

**示例**:

```c
THREAD_HANDLE thread_handle;
THREAD_CFG_T cfg = {
    .stackDepth = 4096,
    .priority   = THREAD_PRIO_2,
    .thrdname   = "my_thread",
    .psram_mode = 0,
};

OPERATE_RET ret = tal_thread_create_and_start(
    &thread_handle,
    NULL,        // enter callback
    NULL,        // exit callback
    my_thread_func,
    NULL,        // func_args
    &cfg
);
```

---

#### `tal_thread_delete`

```c
OPERATE_RET tal_thread_delete(const THREAD_HANDLE handle);
```

**功能**: 停止并删除指定线程。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `handle` | IN | 线程句柄 |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_thread_is_self`

```c
OPERATE_RET tal_thread_is_self(const THREAD_HANDLE handle, BOOL_T *bl);
```

**功能**: 检查当前调用者是否在指定线程上下文中运行。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `handle` | IN | 线程句柄 |
| `bl` | OUT | `TRUE` 表示当前在该线程中运行，`FALSE` 表示不在 |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_thread_get_state`

```c
THREAD_STATE_E tal_thread_get_state(const THREAD_HANDLE handle);
```

**功能**: 获取线程当前运行状态。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `handle` | IN | 线程句柄 |

**返回值**: 线程状态枚举值（`THREAD_STATE_E`）。

---

#### `tal_thread_diagnose`

```c
OPERATE_RET tal_thread_diagnose(const THREAD_HANDLE handle);
```

**功能**: 诊断线程（如打印任务栈信息等），用于调试。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `handle` | IN | 线程句柄 |

**返回值**: `OPRT_OK` 成功。

---

## 6. tal_sw_timer.h — 软件定时器管理

提供软件定时器的初始化、创建、启动、停止、删除和状态查询功能。软件定时器不依赖硬件定时器资源，适用于定时操作、周期任务和超时机制。

### 6.1 定时器类型枚举

`TIMER_TYPE`:

| 枚举值 | 值 | 说明 |
|--------|----|------|
| `TAL_TIMER_ONCE` | 0 | 单次定时器（触发一次后自动停止） |
| `TAL_TIMER_CYCLE` | — | 周期定时器（自动重复触发） |

### 6.2 类型定义

#### 定时器 ID

```c
typedef void *TIMER_ID;
```

#### 定时器回调函数

```c
typedef void (*TAL_TIMER_CB)(TIMER_ID timer_id, void *arg);
```

**参数说明**:

| 参数 | 说明 |
|------|------|
| `timer_id` | 触发的定时器 ID |
| `arg` | 创建定时器时传入的用户参数 |

### 6.3 API 函数

#### `tal_sw_timer_init`

```c
OPERATE_RET tal_sw_timer_init(void);
```

**功能**: 初始化软件定时器系统。必须在使用其他定时器 API 之前调用。

**参数**: 无

**返回值**: `OPRT_OK` 成功，其他错误码参见 `tuya_error_code.h`。

---

#### `tal_sw_timer_create`

```c
OPERATE_RET tal_sw_timer_create(TAL_TIMER_CB func, void *arg, TIMER_ID *timer_id);
```

**功能**: 创建一个软件定时器。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `func` | IN | 定时器触发时的回调函数 |
| `arg` | IN | 回调函数的用户参数 |
| `timer_id` | OUT | 返回创建的定时器 ID |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_sw_timer_start`

```c
OPERATE_RET tal_sw_timer_start(TIMER_ID timer_id, TIME_MS time_ms, TIMER_TYPE timer_type);
```

**功能**: 启动软件定时器。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `timer_id` | IN | 定时器 ID |
| `time_ms` | IN | 定时周期（毫秒） |
| `timer_type` | IN | 定时器类型（单次/周期） |

**返回值**: `OPRT_OK` 成功。

**示例**:

```c
TIMER_ID my_timer;
tal_sw_timer_create(my_timer_cb, NULL, &my_timer);

// 启动一个 1000ms 周期定时器
tal_sw_timer_start(my_timer, 1000, TAL_TIMER_CYCLE);

// 启动一个 500ms 单次定时器
tal_sw_timer_start(my_timer, 500, TAL_TIMER_ONCE);
```

---

#### `tal_sw_timer_stop`

```c
OPERATE_RET tal_sw_timer_stop(TIMER_ID timer_id);
```

**功能**: 停止软件定时器。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `timer_id` | IN | 定时器 ID |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_sw_timer_delete`

```c
OPERATE_RET tal_sw_timer_delete(TIMER_ID timer_id);
```

**功能**: 删除软件定时器，释放相关资源。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `timer_id` | IN | 定时器 ID |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_sw_timer_is_running`

```c
BOOL_T tal_sw_timer_is_running(TIMER_ID timer_id);
```

**功能**: 检查软件定时器是否正在运行。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `timer_id` | IN | 定时器 ID |

**返回值**: `TRUE` 正在运行，`FALSE` 未运行。

---

#### `tal_sw_timer_remain_time_get`

```c
OPERATE_RET tal_sw_timer_remain_time_get(TIMER_ID timer_id, uint32_t *remain_time);
```

**功能**: 获取定时器剩余时间。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `timer_id` | IN | 定时器 ID |
| `remain_time` | OUT | 剩余时间（毫秒） |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_sw_timer_trigger`

```c
OPERATE_RET tal_sw_timer_trigger(TIMER_ID timer_id);
```

**功能**: 立即触发软件定时器（不等待定时周期到达）。

**参数**:

| 参数 | 方向 | 说明 |
|------|------|------|
| `timer_id` | IN | 定时器 ID |

**返回值**: `OPRT_OK` 成功。

---

#### `tal_sw_timer_release`

```c
OPERATE_RET tal_sw_timer_release(void);
```

**功能**: 释放软件定时器系统的所有资源。通常在系统关闭时调用。

**参数**: 无

**返回值**: `OPRT_OK` 成功。

---

#### `tal_sw_timer_get_num`

```c
int tal_sw_timer_get_num(void);
```

**功能**: 获取当前已创建的软件定时器节点数量。

**参数**: 无

**返回值**: 当前定时器节点数量。

---

## 附录：典型使用流程

### 系统初始化流程

```
1. tal_sw_timer_init()          -- 初始化软件定时器系统
2. tal_log_init()               -- 初始化日志系统
3. 创建应用线程                    -- tal_thread_create_and_start()
```

### 临界区使用模式

```c
TAL_ENTER_CRITICAL();
// 共享资源操作（禁止中断）
TAL_EXIT_CRITICAL();
```

### 内存管理最佳实践

```c
void *ptr = tal_malloc(256);
if (ptr == NULL) {
    PR_ERR("malloc failed");
    return;
}
// 使用内存...
tal_free(ptr);
ptr = NULL;  // 防止悬空指针
```

### 定时器典型用法

```c
// 定时器回调
void timeout_cb(TIMER_ID id, void *arg) {
    PR_INFO("timer triggered");
}

// 创建并启动
TIMER_ID timer;
tal_sw_timer_create(timeout_cb, NULL, &timer);
tal_sw_timer_start(timer, 2000, TAL_TIMER_ONCE);

// 查询状态
if (tal_sw_timer_is_running(timer)) {
    uint32_t remain;
    tal_sw_timer_remain_time_get(timer, &remain);
    PR_INFO("remaining: %u ms", remain);
}

// 不再使用时清理
tal_sw_timer_delete(timer);
```

---

*本文档基于 TuyaOpen 源码头文件自动生成，如有疑问请参考源码注释或 Tuya 官方文档。*
