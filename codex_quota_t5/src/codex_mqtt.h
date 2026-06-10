/**
 * @file codex_mqtt.h
 * @brief MQTT 客户端接口 - 订阅 codex/quota 主题接收额度推送
 *
 * DEVICE_ID 默认为 "t5ai-001"，可在 Kconfig 或编译选项中覆盖。
 *
 * 重要设计：on_message 回调不直接调用 LVGL，而是通过消息队列
 * 延迟到主循环处理，避免线程安全和栈溢出问题。
 *
 * 依赖 TuyaOpen libmqtt (Amazon coreMQTT v1.0.1)
 */
#ifndef CODEX_MQTT_H
#define CODEX_MQTT_H

#include <stdint.h>
#include "codex_http.h"  /* codex_quota_t */

/**
 * 初始化 MQTT 客户端并连接 broker
 * @param host  broker 地址（如 "10.13.220.28"）
 * @param port  broker 端口（如 1883）
 * @return 0 成功, -1 失败
 */
int codex_mqtt_init_and_connect(const char *host, uint16_t port);

/**
 * 处理 MQTT 网络事件（非阻塞）
 * 必须在主循环中周期性调用，处理 PING/PONG 和消息接收
 */
void codex_mqtt_yield(void);

/**
 * 重新连接 MQTT broker（阻塞）
 * 完全销毁旧客户端后重新创建并连接。
 * 调用者应自行实现指数退避，避免频繁调用。
 * @return 0 成功, -1 失败
 */
int codex_mqtt_reconnect(void);

/**
 * 断开 MQTT 连接并释放资源
 */
void codex_mqtt_disconnect(void);

/**
 * 查询 MQTT 连接状态
 * @return 1 已连接, 0 未连接
 */
int codex_mqtt_is_connected(void);

/**
 * 检查是否有待处理的 MQTT 消息
 * @return 1 有消息, 0 无消息
 */
int codex_mqtt_has_pending_message(void);

/**
 * 获取已接收 MQTT 消息总数（诊断用）
 * @return 消息计数
 */
int codex_mqtt_get_msg_count(void);

/**
 * 处理待处理的 MQTT 消息（在主循环中调用）
 *
 * 此函数从共享缓冲区读取消息，解析 JSON，填充 quota 结构体。
 * 调用者负责在解析成功后更新 UI（使用 lv_vendor_disp_lock）。
 *
 * @param quota 输出参数，解析成功时填充
 * @return 1 解析成功, 0 无消息, -1 解析失败
 */
int codex_mqtt_process_pending_message(codex_quota_t *quota);

#endif /* CODEX_MQTT_H */
