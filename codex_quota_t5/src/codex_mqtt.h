/**
 * @file codex_mqtt.h
 * @brief MQTT 客户端接口 - 订阅 codex/quota 主题接收额度推送
 *
 * DEVICE_ID 默认为 "t5ai-001"，可在 Kconfig 或编译选项中覆盖。
 *
 * 依赖 TuyaOpen libmqtt (Amazon coreMQTT v1.0.1)
 */
#ifndef CODEX_MQTT_H
#define CODEX_MQTT_H

#include <stdint.h>

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

#endif /* CODEX_MQTT_H */
