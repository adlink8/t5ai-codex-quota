/**
 * @file codex_mqtt.c
 * @brief MQTT 客户端实现 —— 订阅 codex/quota 主题接收额度推送
 *
 * ═══════════════════════════════════════════════════════════════
 * 【模块概述】
 * ═══════════════════════════════════════════════════════════════
 *
 *  使用 TuyaOpen libmqtt API (mqtt_client_interface.h) 实现 MQTT 客户端。
 *  连接成功后自动订阅 "codex/quota" 主题，收到消息后通过"回调缓存 →
 *  主循环消费"模式解析 JSON 并更新 UI。
 *
 * ═══════════════════════════════════════════════════════════════
 * 【线程模型与安全约束】★ 核心设计 ★
 * ═══════════════════════════════════════════════════════════════
 *
 *  线程模型：
 *    - 主线程：运行 LVGL 事件循环 + 本模块的 yield/process 函数
 *    - MQTT SDK 线程：触发 on_message / on_connected / on_disconnected 回调
 *
 *  安全约束：
 *    1. LVGL 不是线程安全的 —— 只能在主线程调用 LVGL API
 *    2. MQTT 回调在 SDK 内部线程执行 —— 不能调用 LVGL
 *    3. MQTT 回调的栈空间通常只有 2-4KB —— 不能做重操作
 *
 *  解决方案：生产者-消费者模式
 *    回调线程（生产者）：memcpy 到共享缓冲区 + 设置 volatile 标志
 *    主线程（消费者）：检查标志 → 读取缓冲区 → 解析 JSON → 更新 UI
 *
 *  为什么不用 mutex？
 *    - 在单核 MCU（ARM Cortex-M）上，volatile 标志已足够
 *    - int 类型的读写在 ARM 上是原子的
 *    - 引入 mutex 会增加代码复杂度，且可能在回调线程中造成死锁
 *
 * ═══════════════════════════════════════════════════════════════
 * 【消息队列机制】
 * ═══════════════════════════════════════════════════════════════
 *
 *  当前采用"单槽 latest-wins"缓冲区，而非 FIFO 队列：
 *
 *    回调线程                    主循环
 *    ─────────                   ──────
 *    memcpy(payload → buf)
 *    g_new_message = 1
 *                                if g_new_message:
 *                                  g_new_message = 0
 *                                  parse(buf)
 *                                  → update UI
 *
 *  设计理由：
 *    - 额度数据是"最新值覆盖旧值"语义，中间值无意义
 *    - 单槽缓冲区无需管理队列指针、溢出、内存分配
 *    - 适合嵌入式场景（内存受限、消息频率低）
 *
 *  如果未来需要 FIFO 队列（如消息不可丢失），可改为环形缓冲区：
 *    - 定义 MQTT_MSG_QUEUE_SIZE（如 8）
 *    - 回调写入 g_msg_queue[g_write_idx++]
 *    - 主循环从 g_read_idx 读取
 *    - 需要额外处理队列满时的丢弃策略
 *
 * ═══════════════════════════════════════════════════════════════
 * 【重连逻辑详解】
 * ═══════════════════════════════════════════════════════════════
 *
 *  重连触发条件：
 *    codex_mqtt_yield() 检测到 g_mqtt_connected == 0
 *    → 主循环的退避逻辑决定何时调用 codex_mqtt_reconnect()
 *
 *  重连流程（在 reconnect 函数内）：
 *    ①  安全销毁旧客户端: disconnect → deinit → free → 置 NULL
 *    ②  创建新客户端: mqtt_client_new()
 *    ③  配置连接参数（使用 extern g_mqtt_host / g_mqtt_port）
 *    ④  初始化: mqtt_client_init()
 *    ⑤  连接: mqtt_client_connect()
 *    ⑥  yield 循环等待: 最多 10 次 × 200ms = 2 秒
 *    ⑦  检查 g_mqtt_connected 标志（由 on_connected 回调设置）
 *
 *  为什么销毁重建而不是 disconnect + reconnect？
 *    - SDK 的内部状态机在异常断连后可能处于不确定状态
 *    - 销毁重建保证干净的初始状态
 *    - 避免残留的未 ACK 消息或订阅状态
 *
 *  退避策略（由调用者实现）：
 *    - 指数退避：1s → 2s → 4s → 8s → ... → 上限 60s
 *    - 随机抖动（jitter）：±50%，避免多设备同时重连的"雷群效应"
 *    - 成功后重置退避计时器
 *
 *  on_disconnected 回调去抖：
 *    - 使用 g_last_disc_ms 记录上次断连时间戳
 *    - 2 秒内的重复断连回调只标记状态，不执行重连逻辑
 *    - 防止短时间内大量断连回调导致状态抖动
 *
 * ═══════════════════════════════════════════════════════════════
 * 【配置参数说明】
 * ═══════════════════════════════════════════════════════════════
 *
 *  DEVICE_ID         设备标识符，默认 "t5ai-001"，可通过编译选项覆盖
 *  MQTT_TOPIC        订阅主题，固定为 "codex/quota"
 *  MQTT_QOS          服务质量等级 0（At most once，即 Fire and Forget）
 *                    选择 QoS 0 的理由：额度数据频繁更新，丢失一条无影响
 *  MQTT_KEEPALIVE    心跳间隔 60 秒
 *  MQTT_TIMEOUT_MS   连接超时 5000 毫秒
 *  MQTT_CLIENT_ID    客户端标识 "codex_t5ai"
 *  MQTT_MSG_BUF_SIZE 消息缓冲区大小 2048 字节
 *
 * ═══════════════════════════════════════════════════════════════
 * 【诊断计数器说明】
 * ═══════════════════════════════════════════════════════════════
 *
 *  g_disc_cb_count   on_disconnected 回调触发总次数
 *  g_conn_cb_count   on_connected 回调触发总次数
 *  g_msg_rx_count    on_message 回调触发总次数（含解析失败的消息）
 *  g_last_disc_ms    上次断连回调的毫秒时间戳（用于去抖判断）
 *
 *  这些计数器可通过 PR_NOTICE 日志查看，用于排查连接稳定性问题。
 */

#include "codex_mqtt.h"
#include "codex_http.h"     /* codex_parse_json(), codex_quota_t */
#include "codex_ui.h"       /* codex_ui_update */

#include "mqtt_client_interface.h"  /* TuyaOpen MQTT SDK API */
#include "tal_log.h"                /* PR_NOTICE / PR_ERR 日志宏 */
#include "tal_memory.h"             /* 内存管理 */
#include "tkl_system.h"             /* tkl_system_sleep(), tkl_system_get_millisecond() */
#include "lv_vendor.h"              /* lv_vendor_disp_lock/unlock */

#include <string.h>                 /* memcpy, strlen */

/* ═══════════════════════════════════════════════════════════════
 * 外部运行时配置（来自 tuya_main.c）
 *
 * 这两个变量在 tuya_main.c 中定义，允许在运行时（如从 NVS/Flash）
 * 动态配置 MQTT broker 地址，无需重新编译固件。
 * ═══════════════════════════════════════════════════════════════ */
extern char g_mqtt_host[48];   /**< broker 地址，如 "10.13.220.28" */
extern int  g_mqtt_port;       /**< broker 端口，如 1883 */

/* ═══════════════════════════════════════════════════════════════
 * 编译期配置常量
 * ═══════════════════════════════════════════════════════════════ */

/** 设备标识符，可通过 Kconfig 或 -DDEVICE_ID=\"xxx\" 覆盖 */
#ifndef DEVICE_ID
#define DEVICE_ID           "t5ai-001"
#endif

#define MQTT_TOPIC          "codex/quota"   /**< 订阅的 MQTT 主题 */
#define MQTT_QOS            0       /**< QoS 0: At most once（Fire and Forget） */
#define MQTT_KEEPALIVE      60      /**< 心跳保活间隔（秒） */
#define MQTT_TIMEOUT_MS     5000    /**< TCP + MQTT 连接超时（毫秒） */
#define MQTT_CLIENT_ID      "codex_t5ai"    /**< MQTT 客户端标识符 */

/* ═══════════════════════════════════════════════════════════════
 * 内部状态变量
 *
 * 【线程安全说明】
 *   g_mqtt_client:    void* 指针，仅在主线程读写（connect/reconnect/yield）
 *   g_mqtt_connected: int 标志，由回调线程写入，主线程读取
 *                     ARM Cortex-M 上 int 读写是原子的
 * ═══════════════════════════════════════════════════════════════ */

/** MQTT 客户端实例句柄，NULL 表示未创建/已释放 */
static void *g_mqtt_client = NULL;

/**
 * 连接状态标志
 * - 由 on_connected 回调设为 1（回调线程写入）
 * - 由 on_disconnected 回调设为 0（回调线程写入）
 * - 由主线程读取（yield/is_connected/process 函数中）
 * - 非 volatile：单核 MCU 上主线程 yield/sleep 后总能看到最新值
 */
static int g_mqtt_connected = 0;

/* ═══════════════════════════════════════════════════════════════
 * 消息队列（回调线程 → 主循环）
 *
 * 【设计：单槽 latest-wins 缓冲区】
 *   - g_pending_json[]: 存储最新一条消息的 JSON 字符串
 *   - g_new_message:    volatile 原子标志，1=有新消息，0=已消费
 *
 * 【写入方】on_message 回调（MQTT SDK 线程）
 *   memcpy(g_pending_json, payload, length);
 *   g_pending_json[length] = '\0';
 *   g_new_message = 1;
 *
 * 【读取方】codex_mqtt_process_pending_message()（主线程）
 *   if (g_new_message) {
 *       g_new_message = 0;        // 先清除标志
 *       parse(g_pending_json);    // 再处理数据
 *   }
 *
 * 【为什么先清除标志再处理？】
 *   如果先处理再清除，在处理期间回调写入的新消息会被丢失。
 *   先清除标志虽然可能导致处理"旧数据"（被新 memcpy 覆盖），
 *   但额度数据是幂等的，且下次循环会再次检测到新标志。
 * ═══════════════════════════════════════════════════════════════ */

#define MQTT_MSG_BUF_SIZE 2048  /**< 消息缓冲区大小（字节），需 > 最大 JSON 长度 */

/** 消息缓冲区，存储最新一条 MQTT 消息的 JSON 内容 */
static char g_pending_json[MQTT_MSG_BUF_SIZE];

/**
 * 新消息标志（volatile 保证跨线程可见性）
 *
 * 为什么用 volatile？
 *   编译器优化可能将 g_new_message 缓存到寄存器，导致主线程
 *   永远看不到回调线程的写入。volatile 强制每次读取都从内存加载。
 *
 * 为什么不用 atomic_int？
 *   TuyaOpen SDK 目标平台是 ARM Cortex-M，int 读写天然原子，
 *   且该 SDK 可能不支持 C11 <stdatomic.h>。
 */
static volatile int g_new_message = 0;

/* ═══════════════════════════════════════════════════════════════
 * 诊断计数器
 *
 * 用于排查连接稳定性问题，可通过 UART 日志查看。
 * 仅在回调中递增，主线程只读，无竞争问题。
 * ═══════════════════════════════════════════════════════════════ */

static int g_disc_cb_count = 0;     /**< on_disconnected 回调总次数 */
static int g_conn_cb_count  = 0;    /**< on_connected 回调总次数 */
static int g_msg_rx_count   = 0;    /**< on_message 回调总次数（含解析失败） */

/**
 * 上次断连回调的毫秒时间戳
 * 用于 on_disconnected 回调的 2 秒去抖判断
 * @see on_disconnected() 中的去抖逻辑
 */
static uint32_t g_last_disc_ms = 0;

/* ═══════════════════════════════════════════════════════════════
 * MQTT SDK 回调函数
 *
 * 【重要】以下回调可能在 MQTT SDK 内部线程中执行！
 *   - 不能调用 LVGL API（非线程安全）
 *   - 不能调用 malloc/free（可能与主线程竞争）
 *   - 不能做耗时操作（回调栈空间有限，通常 2-4KB）
 *   - 只能做：修改全局变量、memcpy、设置标志
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief 连接成功回调
 *
 * MQTT 连接成功（收到 CONNACK）后由 SDK 调用。
 * 设置连接标志并自动订阅 "codex/quota" 主题。
 *
 * @param[in] client   MQTT 客户端句柄（由 SDK 传入）
 * @param[in] userdata 用户数据（未使用，传入时为 NULL）
 *
 * @note 【线程安全】此回调可能在 SDK 线程执行
 * @note 设置 g_mqtt_connected = 1 后，主线程的 yield 函数会检测到
 * @note 自动订阅 QoS 0，如果订阅失败只打印错误日志，不影响连接状态
 */
static void on_connected(void *client, void *userdata)
{
    (void)userdata;     /* 未使用，避免编译器警告 */
    g_conn_cb_count++;
    g_mqtt_connected = 1;
    PR_NOTICE("[mqtt] on_connected #%d: 已连接 broker，订阅 %s",
              g_conn_cb_count, MQTT_TOPIC);

    /* 订阅主题，QoS 0（At most once） */
    uint16_t msgid = mqtt_client_subscribe(client, MQTT_TOPIC, MQTT_QOS);
    if (msgid > 0) {
        PR_NOTICE("[mqtt] 订阅成功 msgid=%u", msgid);
    } else {
        /* 订阅失败不视为致命错误，broker 可能稍后重试 */
        PR_ERR("[mqtt] 订阅失败 msgid=0");
    }
}

/**
 * @brief 断连回调（带去抖 + 幂等检查）
 *
 * 连接断开后由 SDK 调用。实现两层保护：
 *   1. 幂等检查：已断连状态直接返回，避免重复处理
 *   2. 硬性去抖：2 秒内的重复回调只标记状态，不执行重连逻辑
 *
 * @param[in] client   MQTT 客户端句柄（未使用）
 * @param[in] userdata 用户数据（未使用）
 *
 * @note 【线程安全】此回调可能在 SDK 线程执行
 * @note 【关键设计】不在回调中销毁客户端！
 *       此回调可能从 SDK yield() 内部触发（即 yield → SDK 内部 →
 *       检测到断连 → 调用此回调）。如果在这里调用 mqtt_client_free()，
 *       会导致 yield 返回后访问已释放的内存（use-after-free）。
 *       因此只标记状态，由主线程的 codex_mqtt_reconnect() 负责销毁。
 *
 * @note 去抖理由：网络抖动可能在短时间内触发多次 disconnect 事件，
 *       每次都执行重连逻辑会导致资源浪费和日志洪泛。
 *
 * 【诊断日志策略】
 *   - 首次断连：完整日志
 *   - 去抖命中：打印 DEBOUNCE 标记
 *   - 已断连状态下的重复回调：每 50 次打印一次诊断
 */
static void on_disconnected(void *client, void *userdata)
{
    (void)client;       /* 未使用，避免编译器警告 */
    (void)userdata;

    g_disc_cb_count++;
    uint32_t now_ms = tkl_system_get_millisecond();

    /* ── 第一层保护：幂等检查 ── */
    /* 已经处于断连状态，只做静默计数，避免日志洪泛 */
    if (g_mqtt_connected == 0) {
        /* 每 50 次静默回调打印一次诊断，方便排查网络问题 */
        if ((g_disc_cb_count % 50) == 0) {
            PR_NOTICE("[mqtt] disc_cb #%d silent (conn=0, dt=%u ms)",
                      g_disc_cb_count,
                      (unsigned)(now_ms - g_last_disc_ms));
        }
        return;
    }

    /* ── 第二层保护：硬性去抖（2 秒窗口）── */
    /* 短时间内多次断连回调只标记状态，不重复执行重连逻辑 */
    if (g_last_disc_ms != 0 && (now_ms - g_last_disc_ms) < 2000) {
        PR_NOTICE("[mqtt] disc_cb #%d DEBOUNCE (dt=%u ms, conn was %d)",
                  g_disc_cb_count,
                  (unsigned)(now_ms - g_last_disc_ms), g_mqtt_connected);
        /* 仍然标记断连（确保主线程能检测到），但不触发额外逻辑 */
        g_mqtt_connected = 0;
        g_last_disc_ms = now_ms;
        return;
    }

    /* ── 正常断连处理 ── */
    g_last_disc_ms = now_ms;
    g_mqtt_connected = 0;
    PR_NOTICE("[mqtt] on_disconnect #%d: 标记断连 (conn_cb=%d)",
              g_disc_cb_count, g_conn_cb_count);

    /* 【关键】不在这里销毁客户端！
     * 此回调可能从 SDK yield() 内部触发，销毁会导致 use-after-free。
     * 由主循环的 codex_mqtt_reconnect() 负责完整销毁和重建。 */
}

/**
 * @brief 消息接收回调 —— 仅复制数据，不处理
 *
 * 收到 MQTT 消息后由 SDK 调用。将 payload 复制到共享缓冲区，
 * 设置 g_new_message 标志通知主循环有新消息待处理。
 *
 * @param[in] client   MQTT 客户端句柄（未使用）
 * @param[in] msgid    消息 ID（用于日志诊断）
 * @param[in] msg      消息结构体，包含 topic、payload、length
 * @param[in] userdata 用户数据（未使用）
 *
 * @note 【线程安全】此回调在 MQTT SDK 内部线程执行
 * @note 【栈空间限制】MQTT 任务栈通常只有 2-4KB，只做 memcpy
 * @note 【不调用 LVGL】LVGL 不是线程安全的，只缓存数据到共享缓冲区
 *
 * 【丢弃条件】
 *   - payload 为 NULL 或 length == 0 → 打印错误，丢弃
 *   - payload >= 2048 字节 → 打印错误，丢弃（缓冲区溢出保护）
 *
 * 【竞争分析】
 *   如果主线程正在处理旧消息时回调写入新消息：
 *   - memcpy 会覆盖 g_pending_json 的内容
 *   - g_new_message 会被重新设为 1
 *   - 主线程下次循环会检测到新标志并处理最新数据
 *   - 由于额度数据是幂等的，中间覆盖不会导致错误
 */
static void on_message(void *client, uint16_t msgid,
                       const mqtt_client_message_t *msg, void *userdata)
{
    (void)client;
    (void)userdata;

    g_msg_rx_count++;

    PR_NOTICE("[mqtt] 收到消息 #%d: topic=%s len=%u msgid=%u",
              g_msg_rx_count, msg->topic, (unsigned)msg->length, msgid);

    /* 基本有效性检查：payload 不能为 NULL 或空 */
    if (msg->payload == NULL || msg->length == 0) {
        PR_ERR("[mqtt] 空 payload");
        return;
    }

    /* 安全检查：payload 长度不能超过缓冲区（预留 '\0' 位置） */
    if (msg->length >= MQTT_MSG_BUF_SIZE) {
        PR_ERR("[mqtt] payload 过大: %u >= %u, 丢弃消息",
               (unsigned)msg->length, (unsigned)MQTT_MSG_BUF_SIZE);
        return;
    }

    /* ── 核心操作：仅 memcpy + 设置标志 ── */
    /* 这是整个回调中唯一的"写"操作，临界区极小 */
    memcpy(g_pending_json, msg->payload, msg->length);
    g_pending_json[msg->length] = '\0';     /* 确保 C 字符串终止 */
    g_new_message = 1;  /* 原子标志通知主循环：有新消息待处理 */

    PR_NOTICE("[mqtt] 消息已缓存，等待主循环处理 (len=%u)", (unsigned)msg->length);
}

/**
 * @brief 订阅确认回调
 *
 * broker 确认订阅后由 SDK 调用。仅打印日志，无业务逻辑。
 *
 * @param[in] client   MQTT 客户端句柄（未使用）
 * @param[in] msgid    订阅消息 ID（与 mqtt_client_subscribe 返回值对应）
 * @param[in] userdata 用户数据（未使用）
 *
 * @note 【线程安全】此回调可能在 SDK 线程执行，但只做日志输出
 */
static void on_subscribed(void *client, uint16_t msgid, void *userdata)
{
    (void)client;
    (void)userdata;
    PR_NOTICE("[mqtt] 订阅确认 msgid=%u", msgid);
}

/* ═══════════════════════════════════════════════════════════════
 * 公开接口实现
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief 初始化 MQTT 客户端并连接到 broker（阻塞）
 *
 * 完整流程：
 *   ① 参数校验（host 非空、port 非零）
 *   ② mqtt_client_new()  —— 分配客户端内存
 *   ③ mqtt_client_init() —— 配置连接参数和回调
 *   ④ mqtt_client_connect() —— 发起 TCP + MQTT 连接
 *   ⑤ yield 循环等待 —— 等待 CONNACK + 自动订阅完成
 *   ⑥ 检查 g_mqtt_connected 标志
 *
 * @param[in] host  broker 地址（如 "10.13.220.28"），不能为 NULL 或空串
 * @param[in] port  broker 端口（如 1883），不能为 0
 *
 * @return  0  连接 + 订阅成功
 * @return -1  失败（参数无效 / 内存不足 / 连接超时）
 *
 * @note 阻塞最多 ~2 秒（10 次 yield × 200ms sleep）
 * @note 失败时自动释放已分配的资源（无内存泄漏）
 *
 * 【错误恢复】
 *   任何步骤失败都会执行完整的清理序列：
 *   - mqtt_client_init 失败 → free
 *   - mqtt_client_connect 失败 → deinit → free
 *   - yield 超时 → deinit → free
 *   确保不泄漏内存。
 */
int codex_mqtt_init_and_connect(const char *host, uint16_t port)
{
    /* ── ① 参数校验 ── */
    if (host == NULL || host[0] == '\0' || port == 0) {
        PR_ERR("[mqtt] 无效参数: host=%s port=%u", host ? host : "NULL", port);
        return -1;
    }

    PR_NOTICE("[mqtt] 创建客户端 broker=%s:%u", host, port);

    /* ── ② 分配客户端实例 ── */
    g_mqtt_client = mqtt_client_new();
    if (g_mqtt_client == NULL) {
        PR_ERR("[mqtt] mqtt_client_new() 返回 NULL");
        return -1;
    }

    /* ── ③ 配置连接参数 ── */
    mqtt_client_config_t config = {
        .cacert = NULL,             /* 无 TLS（使用明文 MQTT） */
        .cacert_len = 0,
        .host = host,               /* broker 地址 */
        .port = port,               /* broker 端口 */
        .keepalive = MQTT_KEEPALIVE,    /* 心跳间隔 60 秒 */
        .timeout_ms = MQTT_TIMEOUT_MS,  /* 连接超时 5 秒 */
        .clientid = MQTT_CLIENT_ID,     /* 客户端标识 "codex_t5ai" */
        .username = NULL,           /* 无认证 */
        .password = NULL,
        .userdata = NULL,           /* 不传递用户数据到回调 */
        /* ── 回调函数注册 ── */
        .on_connected = on_connected,       /* 连接成功 → 订阅主题 */
        .on_disconnected = on_disconnected, /* 断连 → 标记状态 */
        .on_message = on_message,           /* 收消息 → 缓存到队列 */
        .on_published = NULL,               /* 不关心发布确认 */
        .on_subscribed = on_subscribed,     /* 订阅确认 → 日志 */
        .on_unsubscribed = NULL,            /* 不使用取消订阅 */
    };

    /* ── ③ 初始化 SDK ── */
    mqtt_client_status_t status = mqtt_client_init(g_mqtt_client, &config);
    if (status != MQTT_STATUS_SUCCESS) {
        PR_ERR("[mqtt] mqtt_client_init() 失败 status=%d", status);
        mqtt_client_free(g_mqtt_client);
        g_mqtt_client = NULL;
        return -1;
    }

    /* ── ④ 发起连接 ── */
    PR_NOTICE("[mqtt] 正在连接...");
    status = mqtt_client_connect(g_mqtt_client);
    if (status != MQTT_STATUS_SUCCESS) {
        PR_ERR("[mqtt] mqtt_client_connect() 失败 status=%d", status);
        mqtt_client_deinit(g_mqtt_client);
        mqtt_client_free(g_mqtt_client);
        g_mqtt_client = NULL;
        return -1;
    }

    /* ── ⑤ yield 循环等待连接 + 订阅完成 ── */
    /* 首次 yield 让 SDK 处理 CONNACK */
    mqtt_client_yield(g_mqtt_client);

    if (g_mqtt_connected) {
        PR_NOTICE("[mqtt] 连接 + 订阅完成");
        return 0;
    }

    /* 多次重试 yield，每次间隔 200ms，最多 10 次（共 ~2 秒） */
    for (int i = 0; i < 10; i++) {
        mqtt_client_yield(g_mqtt_client);
        if (g_mqtt_connected) {
            PR_NOTICE("[mqtt] 连接 + 订阅完成 (yield %d)", i + 1);
            return 0;
        }
        tkl_system_sleep(200);  /* 等待 200ms，让 SDK 处理网络数据 */
    }

    /* ── ⑥ 超时处理 ── */
    PR_ERR("[mqtt] 连接超时");
    mqtt_client_deinit(g_mqtt_client);
    mqtt_client_free(g_mqtt_client);
    g_mqtt_client = NULL;
    return -1;
}

/**
 * @brief 处理 MQTT 网络事件（非阻塞）
 *
 * 在主循环中周期性调用，驱动 MQTT SDK 处理网络 I/O。
 * 内部调用 mqtt_client_yield() 处理心跳、消息接收等。
 *
 * @note 【设计决策】仅在已连接状态下调用 yield
 *       断连后继续 yield 会触发 MQTTRecvFailed/MQTTSendFailed 错误，
 *       形成无限错误循环，浪费 CPU 且产生大量无意义日志。
 *       断连后应由主循环的退避逻辑调用 codex_mqtt_reconnect()。
 *
 * @note yield 返回后检查 g_mqtt_connected，如果变成了 0，
 *       说明 yield 期间检测到了断连（如 keepalive 超时）。
 */
void codex_mqtt_yield(void)
{
    /* 仅在客户端存在且已连接时调用 yield */
    if (g_mqtt_client != NULL && g_mqtt_connected) {
        mqtt_client_yield(g_mqtt_client);

        /* yield 可能触发 on_disconnected 回调，检查状态 */
        if (!g_mqtt_connected) {
            PR_NOTICE("[mqtt] yield 返回后检测到断连");
        }
    }
}

/**
 * @brief 重新连接 MQTT broker（阻塞）
 *
 * 完整的重连流程：销毁旧客户端 → 创建新客户端 → 连接 + 订阅。
 * 使用 extern g_mqtt_host / g_mqtt_port 作为连接参数。
 *
 * @return  0  重连成功
 * @return -1  失败（内存不足 / 连接超时）
 *
 * @note 阻塞最多 ~2 秒
 * @note 调用者应实现指数退避 + 抖动，避免频繁调用
 *
 * 【为什么销毁重建？】
 *   TuyaOpen MQTT SDK 内部维护了连接状态机、未确认消息队列等。
 *   异常断连后这些状态可能不一致（如半开连接、未清理的订阅）。
 *   销毁重建是确保干净状态的最可靠方法。
 *   代价：每次重连需要重新分配内存，但嵌入式场景下重连不频繁。
 *
 * 【yield 等待策略】
 *   与 init_and_connect 相同：先 yield 一次，再最多重试 10 次 × 200ms。
 *   这给了 SDK 足够的时间处理 TCP 三次握手 + MQTT CONNECT/CONNACK。
 */
int codex_mqtt_reconnect(void)
{
    PR_NOTICE("[mqtt] reconnect: entry (disc_cb=%d conn_cb=%d was_conn=%d)",
              g_disc_cb_count, g_conn_cb_count, g_mqtt_connected);

    /* ── ① 安全销毁旧客户端 ── */
    /* disconnect → deinit → free 三步确保完全释放 */
    if (g_mqtt_client != NULL) {
        mqtt_client_disconnect(g_mqtt_client);  /* 发送 MQTT DISCONNECT 包 */
        mqtt_client_deinit(g_mqtt_client);      /* 清理 SDK 内部状态 */
        mqtt_client_free(g_mqtt_client);         /* 释放客户端内存 */
        g_mqtt_client = NULL;
        g_mqtt_connected = 0;
    }

    /* ── ② 创建新客户端 ── */
    g_mqtt_client = mqtt_client_new();
    if (g_mqtt_client == NULL) {
        PR_ERR("[mqtt] mqtt_client_new() 返回 NULL");
        return -1;
    }

    /* ── ③ 配置连接参数（使用运行时 extern 变量）── */
    mqtt_client_config_t config = {
        .cacert = NULL,
        .cacert_len = 0,
        .host = g_mqtt_host,        /* 运行时配置的 broker 地址 */
        .port = g_mqtt_port,        /* 运行时配置的 broker 端口 */
        .keepalive = MQTT_KEEPALIVE,
        .timeout_ms = MQTT_TIMEOUT_MS,
        .clientid = MQTT_CLIENT_ID,
        .username = NULL,
        .password = NULL,
        .userdata = NULL,
        .on_connected = on_connected,
        .on_disconnected = on_disconnected,
        .on_message = on_message,
        .on_published = NULL,
        .on_subscribed = on_subscribed,
        .on_unsubscribed = NULL,
    };

    /* ── ④ 初始化 ── */
    mqtt_client_status_t status = mqtt_client_init(g_mqtt_client, &config);
    if (status != MQTT_STATUS_SUCCESS) {
        PR_ERR("[mqtt] init 失败 status=%d", status);
        mqtt_client_free(g_mqtt_client);
        g_mqtt_client = NULL;
        return -1;
    }

    /* ── ⑤ 发起连接 ── */
    status = mqtt_client_connect(g_mqtt_client);
    if (status != MQTT_STATUS_SUCCESS) {
        PR_ERR("[mqtt] reconnect connect() failed status=%d (disc_cb=%d)",
               status, g_disc_cb_count);
        mqtt_client_deinit(g_mqtt_client);
        mqtt_client_free(g_mqtt_client);
        g_mqtt_client = NULL;
        return -1;
    }

    PR_NOTICE("[mqtt] reconnect: TCP+MQTT connected, yielding... (disc_cb=%d)",
              g_disc_cb_count);

    /* ── ⑥ yield 等待连接 + 订阅完成 ── */
    mqtt_client_yield(g_mqtt_client);
    if (g_mqtt_connected) {
        PR_NOTICE("[mqtt] 重连成功");
        return 0;
    }

    /* 多次重试，每次间隔 200ms */
    for (int i = 0; i < 10; i++) {
        mqtt_client_yield(g_mqtt_client);
        if (g_mqtt_connected) {
            PR_NOTICE("[mqtt] 重连成功 (yield %d)", i + 1);
            return 0;
        }
        tkl_system_sleep(200);
    }

    /* ── ⑦ 超时处理 ── */
    PR_ERR("[mqtt] reconnect: timeout (disc_cb=%d, conn=%d)",
            g_disc_cb_count, g_mqtt_connected);
    mqtt_client_deinit(g_mqtt_client);
    mqtt_client_free(g_mqtt_client);
    g_mqtt_client = NULL;
    return -1;
}

/**
 * @brief 断开 MQTT 连接并释放所有资源
 *
 * 执行完整的清理序列：
 *   ① mqtt_client_disconnect() —— 发送 MQTT DISCONNECT 包（优雅断开）
 *   ② mqtt_client_deinit()    —— 清理 SDK 内部状态
 *   ③ mqtt_client_free()      —— 释放客户端内存
 *   ④ 重置状态变量
 *
 * @note 此后 codex_mqtt_yield() 不会有任何效果（client == NULL 检查）
 * @note 如果需要重新连接，应调用 codex_mqtt_reconnect()
 * @note 如果 g_mqtt_client 已经是 NULL，此函数不执行任何操作（幂等）
 */
void codex_mqtt_disconnect(void)
{
    if (g_mqtt_client != NULL) {
        mqtt_client_disconnect(g_mqtt_client);  /* 优雅断开 */
        mqtt_client_deinit(g_mqtt_client);       /* 清理 SDK 状态 */
        mqtt_client_free(g_mqtt_client);          /* 释放内存 */
        g_mqtt_client = NULL;                     /* 防止悬空指针 */
        g_mqtt_connected = 0;                     /* 标记为断连 */
        PR_NOTICE("[mqtt] 已断开并释放");
    }
}

/**
 * @brief 查询 MQTT 连接状态
 *
 * @return  1  已连接
 * @return  0  未连接
 *
 * @note 线程安全：int 读写在 ARM Cortex-M 上是原子操作
 * @note 返回值反映最近一次回调设置的状态
 */
int codex_mqtt_is_connected(void)
{
    return g_mqtt_connected;
}

/**
 * @brief 检查是否有待处理的 MQTT 消息
 *
 * @return  1  有消息（on_message 已写入 g_pending_json）
 * @return  0  无消息
 *
 * @note 线程安全：g_new_message 是 volatile int，保证跨线程可见性
 * @note 典型用法：主循环中先检查此函数，有消息再调用 process_pending_message()
 */
int codex_mqtt_has_pending_message(void)
{
    return g_new_message;
}

/**
 * @brief 获取已接收的 MQTT 消息总数（诊断用）
 *
 * @return 自模块初始化以来 on_message 回调触发的总次数
 *
 * @note 包括后续解析失败的消息
 * @note 纯诊断用途，不影响业务逻辑
 */
int codex_mqtt_get_msg_count(void)
{
    return g_msg_rx_count;
}

/**
 * @brief 处理待处理的 MQTT 消息（在主循环中调用）
 *
 * 从共享缓冲区 g_pending_json 读取 JSON 字符串并解析为 codex_quota_t。
 *
 * 【处理流程】
 *   1. 检查 g_new_message 标志（无消息则直接返回 0）
 *   2. 清除标志（先清除再处理，避免竞争）
 *   3. 调用 codex_parse_json() 解析 JSON
 *   4. 返回解析结果
 *
 * 【竞争窗口分析】
 *   "先清除标志"的策略存在一个理论竞争窗口：
 *     主线程: g_new_message = 0
 *     回调线程: memcpy(新数据) + g_new_message = 1
 *     主线程: parse(被覆盖的数据)  ← 处理的是新数据，但标志已清除
 *
 *   这种情况的后果：
 *     - 本次处理的是最新数据（被覆盖后的内容）→ 结果正确
 *     - g_new_message 被回调重新设为 1 → 下次循环会再次处理
 *     - 最多多处理一次，但因为幂等，无副作用
 *
 *   如果反过来"先处理再清除"：
 *     主线程: parse(旧数据)  ← 处理过程中回调写入新数据
 *     回调线程: memcpy(新数据) + g_new_message = 1
 *     主线程: g_new_message = 0  ← 新消息标志被误清！消息丢失！
 *
 *   结论：先清除再处理是正确的选择，不会丢失消息。
 *
 * @param[out] quota  输出参数，解析成功时填充额度信息
 *                    调用者必须传入有效的 codex_quota_t* 指针
 *
 * @return  1   解析成功，quota 已填充有效数据
 * @return  0   无待处理消息，quota 未被修改
 * @return -1   JSON 解析失败（格式错误 / 字段缺失），quota 未被修改
 *
 * @note 必须在主循环（LVGL 线程）中调用
 * @note 解析成功后，调用者应使用 lv_vendor_disp_lock() 保护 UI 更新
 * @note 本函数不直接调用 LVGL，保持职责单一
 */
int codex_mqtt_process_pending_message(codex_quota_t *quota)
{
    /* ── 检查是否有待处理消息 ── */
    if (!g_new_message) {
        return 0;  /* 无消息，调用者无需操作 */
    }

    /* ── 先清除标志，再处理数据 ── */
    /* 【线程安全】先清除是关键：避免"先处理再清除"导致的消息丢失 */
    g_new_message = 0;

    PR_NOTICE("[mqtt] 主循环处理缓存消息 (len=%u)",
              (unsigned)strlen(g_pending_json));

    /* ── 解析 JSON ── */
    if (codex_parse_json(g_pending_json, quota) == 0) {
        PR_NOTICE("[mqtt] 解析成功: %s | 主剩余 %.1f%% | 副剩余 %.1f%%",
                  quota->plan_type,
                  quota->primary.remaining,
                  quota->has_secondary ? quota->secondary.remaining : 0.0);
        return 1;   /* 解析成功 */
    } else {
        PR_ERR("[mqtt] JSON 解析失败");
        return -1;  /* 解析失败 */
    }
}
