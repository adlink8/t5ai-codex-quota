/**
 * @file codex_mqtt.h
 * @brief MQTT 客户端接口 —— 订阅 codex/quota 主题接收额度推送
 *
 * ═══════════════════════════════════════════════════════════════
 * 【整体架构概述】
 * ═══════════════════════════════════════════════════════════════
 *
 *  本模块封装了基于 TuyaOpen libmqtt (Amazon coreMQTT v1.0.1) 的
 *  MQTT 客户端，负责：
 *    1. 连接/重连到 MQTT broker
 *    2. 订阅 "codex/quota" 主题
 *    3. 接收消息并缓存到内部队列
 *    4. 由主循环调用 process_pending_message() 解析 JSON 并更新 UI
 *
 * ═══════════════════════════════════════════════════════════════
 * 【线程安全设计约束】★ 重要 ★
 * ═══════════════════════════════════════════════════════════════
 *
 *  MQTT SDK 的 on_message / on_connected / on_disconnected 回调可能
 *  在 SDK 内部的网络线程中执行，而 LVGL（图形库）不是线程安全的。
 *  因此本模块采用"生产者-消费者"模式：
 *
 *    ┌─────────────────┐    g_pending_json[]    ┌──────────────────┐
 *    │  MQTT 回调线程   │ ──── memcpy ──────── → │  g_new_message=1 │
 *    │  (生产者)        │                        │  (原子标志)       │
 *    └─────────────────┘                        └────────┬─────────┘
 *                                                        │
 *                                          主循环周期性检查 │
 *                                                        ↓
 *                                            ┌──────────────────────┐
 *                                            │ process_pending_msg() │
 *                                            │ → codex_parse_json()  │
 *                                            │ → 更新 UI (主线程)    │
 *                                            │ (消费者)              │
 *                                            └──────────────────────┘
 *
 *  这样保证：
 *    - LVGL 操作永远在主线程执行（安全）
 *    - 回调线程只做 memcpy + 设置标志（极小临界区）
 *    - 无需 mutex/信号量（仅靠 volatile 标志即可）
 *
 * ═══════════════════════════════════════════════════════════════
 * 【消息队列机制】
 * ═══════════════════════════════════════════════════════════════
 *
 *  当前实现为"单槽缓冲区"（latest-wins），而非 FIFO 队列：
 *    - 缓冲区 g_pending_json[2048] 只存最新一条消息
 *    - 如果新消息在旧消息被消费前到达，旧消息会被覆盖
 *    - 设计理由：额度数据是"最新覆盖旧"语义，中间值无意义
 *
 * ═══════════════════════════════════════════════════════════════
 * 【重连逻辑】
 * ═══════════════════════════════════════════════════════════════
 *
 *  检测到断连后的重连流程（由主循环的退避逻辑控制）：
 *    1. codex_mqtt_yield() 检测到 g_mqtt_connected == 0
 *    2. 主循环调用 codex_mqtt_reconnect()
 *    3. reconnect 内部：销毁旧客户端 → 创建新客户端 → 连接 + 订阅
 *    4. 失败时主循环实现指数退避 + 抖动（jitter），避免雷群效应
 *
 *  on_disconnected 回调有 2 秒去抖：
 *    - 防止短时间内多次断连回调导致状态抖动
 *    - 使用 tkl_system_get_millisecond() 做时间差判断
 *
 * ═══════════════════════════════════════════════════════════════
 * 【外部依赖】
 * ═══════════════════════════════════════════════════════════════
 *
 *  - extern char g_mqtt_host[48]: 运行时 broker 地址（来自 tuya_main.c）
 *  - extern int  g_mqtt_port:     运行时 broker 端口
 *  - codex_http.h: codex_quota_t 结构体、codex_parse_json() 函数
 *  - mqtt_client_interface.h: TuyaOpen MQTT SDK API
 *
 * ═══════════════════════════════════════════════════════════════
 * 【DEVICE_ID 配置】
 * ═══════════════════════════════════════════════════════════════
 *
 *  DEVICE_ID 默认为 "t5ai-001"，可通过以下方式覆盖：
 *    - Kconfig 配置
 *    - 编译选项: -DDEVICE_ID=\"my-device-002\"
 */
#ifndef CODEX_MQTT_H
#define CODEX_MQTT_H

#include <stdint.h>
#include "codex_http.h"  /* 引入 codex_quota_t 结构体定义 */

/* ═══════════════════════════════════════════════════════════════
 * 生命周期管理函数
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief 初始化 MQTT 客户端并连接到 broker（阻塞）
 *
 * 创建 MQTT 客户端实例，配置连接参数和回调函数，发起 TCP 连接，
 * 并通过多次 yield 等待 CONNACK + SUBACK 完成。
 *
 * @param[in] host  broker 地址（如 "10.13.220.28"），不能为 NULL 或空串
 * @param[in] port  broker 端口（如 1883），不能为 0
 *
 * @return  0  连接 + 订阅成功
 * @return -1  参数无效 / 内存分配失败 / 连接超时
 *
 * @note 此函数会阻塞最多 ~2 秒（10 次 yield × 200ms sleep）
 * @note 内部配置：keepalive=60s, timeout=5s, QoS=0, clientid="codex_t5ai"
 * @note 连接成功后自动订阅 "codex/quota" 主题（QoS 0）
 *
 * @see codex_mqtt_disconnect()  对应的清理函数
 * @see codex_mqtt_reconnect()   断连后的重连函数
 */
int codex_mqtt_init_and_connect(const char *host, uint16_t port);

/**
 * @brief 处理 MQTT 网络事件（非阻塞）
 *
 * 必须在主循环中周期性调用（建议每 100-500ms 一次）。
 * 内部调用 mqtt_client_yield() 处理：
 *   - PINGREQ/PINGRESP 心跳保活
 *   - 接收消息并触发 on_message 回调
 *   - 检测连接状态变化并触发 on_disconnected 回调
 *
 * @note 仅在 g_mqtt_connected == 1 时才会调用 SDK yield。
 *       设计理由：断连后反复 yield 会导致 MQTTRecvFailed/MQTTSendFailed
 *       无限错误循环，浪费 CPU 且产生大量无意义日志。
 *
 * @note 调用后会检查 g_mqtt_connected 状态，如果 yield 导致了断连，
 *       会打印诊断日志。调用者应随后检查 codex_mqtt_is_connected()。
 *
 * @see codex_mqtt_is_connected()  检查连接状态
 * @see codex_mqtt_reconnect()     断连后重连
 */
void codex_mqtt_yield(void);

/**
 * @brief 重新连接 MQTT broker（阻塞）
 *
 * 完整的重连流程：
 *   1. 安全销毁旧客户端（disconnect → deinit → free）
 *   2. 创建新客户端实例
 *   3. 使用 g_mqtt_host/g_mqtt_port（extern 运行时配置）重新连接
 *   4. 通过多次 yield 等待连接 + 订阅完成
 *
 * @return  0  重连成功
 * @return -1  内存分配失败 / 连接超时
 *
 * @note 此函数阻塞最多 ~2 秒
 * @note 调用者（主循环）应自行实现指数退避 + 抖动，避免频繁调用
 *
 * 【设计决策：完全销毁重建 vs 断开重连】
 *   选择完全销毁重建是因为：
 *   - TuyaOpen MQTT SDK 的状态机在异常断连后可能处于不确定状态
 *   - 销毁重建可以确保干净的初始状态
 *   - 避免残留的未确认消息或订阅状态导致问题
 *   - 代价是每次重连需要重新分配内存，但嵌入式场景下重连不频繁
 *
 * @see codex_mqtt_init_and_connect()  首次连接
 */
int codex_mqtt_reconnect(void);

/**
 * @brief 断开 MQTT 连接并释放所有资源
 *
 * 执行完整的清理序列：disconnect → deinit → free
 * 将 g_mqtt_client 置 NULL，g_mqtt_connected 置 0。
 *
 * @note 此后调用 codex_mqtt_yield() 不会有任何效果
 * @note 如果需要重新连接，应调用 codex_mqtt_reconnect() 而非本函数
 *
 * @see codex_mqtt_init_and_connect()
 */
void codex_mqtt_disconnect(void);

/* ═══════════════════════════════════════════════════════════════
 * 状态查询函数（线程安全）
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief 查询 MQTT 连接状态
 *
 * 返回最近一次 on_connected/on_disconnected 回调设置的状态。
 *
 * @return  1  已连接（broker 已确认连接 + 已订阅）
 * @return  0  未连接（初始状态 / 已断连）
 *
 * @note 线程安全：g_mqtt_connected 是 int 类型，在 ARM Cortex-M 上
 *       读写 int 是原子操作。虽然没有 volatile 修饰，但在单核 MCU 上
 *       回调线程写入后，主线程在下次 yield/sleep 后总能看到最新值。
 */
int codex_mqtt_is_connected(void);

/**
 * @brief 检查是否有待处理的 MQTT 消息
 *
 * @return  1  有消息待处理（回调线程已写入 g_pending_json）
 * @return  0  无待处理消息
 *
 * @note 线程安全：g_new_message 声明为 volatile int，
 *       确保编译器不会将其缓存到寄存器，每次读取都从内存加载。
 * @note 典型用法：主循环中先检查此函数，再调用 process_pending_message()
 */
int codex_mqtt_has_pending_message(void);

/**
 * @brief 获取已接收的 MQTT 消息总数（诊断用）
 *
 * 计数器在 on_message 回调中递增，包括后续可能解析失败的消息。
 *
 * @return 自模块初始化以来收到的消息总条数
 *
 * @note 纯诊断用途，不影响业务逻辑
 */
int codex_mqtt_get_msg_count(void);

/* ═══════════════════════════════════════════════════════════════
 * 消息处理函数（必须在主循环中调用）
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief 处理待处理的 MQTT 消息（在主循环中调用）
 *
 * 从共享缓冲区 g_pending_json 读取 JSON 字符串，调用 codex_parse_json()
 * 解析为 codex_quota_t 结构体。调用者负责在解析成功后更新 UI。
 *
 * 【消息队列处理流程】
 *   1. 检查 g_new_message 标志
 *   2. 清除标志（先清除再处理，避免竞争窗口导致消息丢失）
 *   3. 调用 codex_parse_json() 解析 JSON
 *   4. 成功时填充 quota 输出参数，返回 1
 *
 * 【线程安全说明】
 *   - g_new_message 标志先清除再处理，这是"无锁单生产者单消费者"模式
 *   - 竞争窗口分析：
 *     如果清除标志后、处理前，回调线程写入了新消息：
 *       → g_new_message 会被重新设为 1
 *       → 本次处理的是旧消息内容（已被新消息覆盖的 g_pending_json）
 *       → 但额度数据是"最新覆盖旧"语义，处理旧值无害
 *       → 下次主循环会再次检测到 g_new_message=1 并处理最新值
 *   - 结论：不会丢失消息，只是可能多处理一次（幂等，无副作用）
 *
 * @param[out] quota  输出参数，解析成功时填充额度信息
 *                    调用者应传入有效的 codex_quota_t 指针
 *
 * @return  1   解析成功，quota 已填充
 * @return  0   无待处理消息（不需要更新 UI）
 * @return -1   JSON 解析失败（格式错误或字段缺失）
 *
 * @note 此函数必须在主循环（LVGL 线程）中调用，不能在 MQTT 回调中调用
 * @note 解析成功后，调用者应使用 lv_vendor_disp_lock() 保护 UI 更新
 *
 * @see codex_mqtt_has_pending_message()  先检查是否有消息
 * @see codex_parse_json()                 JSON 解析实现（在 codex_http.c 中）
 */
int codex_mqtt_process_pending_message(codex_quota_t *quota);

#endif /* CODEX_MQTT_H */
