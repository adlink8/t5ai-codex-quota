#!/usr/bin/env python3
"""
Codex 额度桥接服务 v2（实时版）
================================
通过 ChatGPT 内部 API (wham/usage) 实时查询 Codex 额度，
供小米手环快应用通过局域网请求。

数据源: https://chatgpt.com/backend-api/wham/usage
认证:   ~/.codex/auth.json 中的 access_token (自动刷新)

使用方式:
    python codex_bridge_server.py              # 默认端口 5678
    python codex_bridge_server.py --port 8080  # 自定义端口

启动后手环访问: http://<PC的局域网IP>:5678/quota
"""

import json
import os
import sys
import time
import base64
import argparse
import threading
import urllib.request
import urllib.error
import urllib.parse
from datetime import datetime, timezone, timedelta
from pathlib import Path
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse

try:
    import paho.mqtt.client as mqtt
    HAS_MQTT = True
except ImportError:
    HAS_MQTT = False

# ── 配置 ──────────────────────────────────────────────
CODEX_AUTH_FILE = Path.home() / ".codex" / "auth.json"
WHAM_USAGE_URL = "https://chatgpt.com/backend-api/wham/usage"
OAUTH_TOKEN_URL = "https://auth.openai.com/api/accounts/oauth/token"
TZ_CST = timezone(timedelta(hours=8))
CACHE_TTL = 60          # API 缓存 60 秒
TOKEN_CHECK_INTERVAL = 3600  # 每小时检查一次 token 有效期
TOKEN_REFRESH_BUFFER = 86400  # token 剩余 < 1天时自动刷新

# ── MQTT 配置 ──────────────────────────────────────────
MQTT_HOST = "127.0.0.1"        # Mosquitto broker 在同一台 PC
MQTT_PORT = 1883
MQTT_TOPIC = "codex/quota"
MQTT_QOS = 0
MQTT_RETAIN = True
MQTT_PUBLISH_INTERVAL = 60     # 每 60 秒 publish 一次（与 API 缓存一致）

# ── 全局状态 ──────────────────────────────────────────
_cache = {"data": None, "updated_at": 0}
_cache_lock = threading.Lock()
_token_lock = threading.Lock()


def format_resets_in(resets_at) -> str:
    """将重置时间戳转换为人类可读的倒计时"""
    if not resets_at or resets_at <= 0:
        return ""
    diff = resets_at - time.time()
    if diff <= 0:
        return "已重置"
    days = int(diff // 86400)
    hours = int((diff % 86400) // 3600)
    minutes = int((diff % 3600) // 60)
    parts = []
    if days > 0:
        parts.append(f"{days}天")
    if hours > 0:
        parts.append(f"{hours}小时")
    if minutes > 0 or not parts:
        parts.append(f"{minutes}分钟")
    return "".join(parts)


def format_window_label(window_seconds) -> str:
    """根据窗口秒数生成标签"""
    if window_seconds <= 0:
        return "额度"
    hours = window_seconds / 3600
    if hours <= 6:
        return f"{int(hours)}小时"
    days = hours / 24
    if days <= 8:
        return f"{int(days)}天"
    return f"{int(days)}天"


def get_local_ip():
    """获取本机局域网 IP 地址"""
    import socket
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


def read_auth_file() -> dict:
    """读取 ~/.codex/auth.json"""
    if not CODEX_AUTH_FILE.exists():
        raise FileNotFoundError(f"认证文件不存在: {CODEX_AUTH_FILE}\n请先运行 codex 并登录")
    with open(CODEX_AUTH_FILE, "r", encoding="utf-8") as f:
        return json.load(f)


def decode_jwt_payload(token: str) -> dict:
    """解码 JWT payload（不验证签名）"""
    parts = token.split(".")
    if len(parts) != 3:
        return {}
    # Base64url → Base64, add padding
    payload = parts[1]
    payload += "=" * (4 - len(payload) % 4)
    try:
        return json.loads(base64.b64decode(payload))
    except Exception:
        return {}


def check_and_refresh_token():
    """检查 access_token 是否即将过期，如是则刷新"""
    with _token_lock:
        try:
            auth = read_auth_file()
        except Exception as e:
            print(f"  [token] 读取认证文件失败: {e}")
            return

        access_token = auth.get("tokens", {}).get("access_token", "")
        if not access_token:
            print("  [token] access_token 为空")
            return

        payload = decode_jwt_payload(access_token)
        exp = payload.get("exp", 0)
        remaining = exp - time.time()

        if remaining > TOKEN_REFRESH_BUFFER:
            return  # token 还早，不需要刷新

        print(f"  [token] access_token 剩余 {remaining/3600:.1f} 小时，尝试刷新...")
        refresh_token = auth.get("tokens", {}).get("refresh_token", "")
        if not refresh_token:
            print("  [token] 无 refresh_token，请重新运行 codex 登录")
            return

        # 尝试 OAuth 刷新
        try:
            data = urllib.parse.urlencode({
                "grant_type": "refresh_token",
                "refresh_token": refresh_token,
                "client_id": "app_EMoamEEZ73f0CkXaXp7hrann",
                "redirect_uri": "http://localhost:1455/auth/callback",
            }).encode()
            req = urllib.request.Request(
                OAUTH_TOKEN_URL, data=data,
                headers={"Content-Type": "application/x-www-form-urlencoded"}
            )
            resp = urllib.request.urlopen(req, timeout=15)
            result = json.loads(resp.read().decode())

            new_access = result.get("access_token", "")
            new_refresh = result.get("refresh_token", refresh_token)

            if new_access:
                auth["tokens"]["access_token"] = new_access
                auth["tokens"]["refresh_token"] = new_refresh
                auth["last_refresh"] = datetime.now(timezone.utc).isoformat()
                with open(CODEX_AUTH_FILE, "w", encoding="utf-8") as f:
                    json.dump(auth, f, indent=2, ensure_ascii=False)
                print("  [token] 刷新成功 [OK]")
            else:
                print("  [token] 刷新响应中无 access_token")
        except urllib.error.HTTPError as e:
            body = e.read().decode()[:200] if hasattr(e, 'read') else ""
            print(f"  [token] 刷新失败 (HTTP {e.code}): {body}")
        except Exception as e:
            print(f"  [token] 刷新失败: {e}")

        # 刷新后尝试重新读取（Codex CLI 可能已自动更新）
        try:
            fresh = read_auth_file()
            fresh_token = fresh.get("tokens", {}).get("access_token", "")
            if fresh_token != access_token:
                print("  [token] 从文件读取到新 token [OK]")
        except Exception:
            pass


def fetch_wham_usage() -> dict:
    """
    调用 ChatGPT wham/usage API 获取实时候额度数据。
    返回与旧版兼容的标准化格式。
    """
    check_and_refresh_token()

    auth = read_auth_file()
    access_token = auth.get("tokens", {}).get("access_token", "")
    if not access_token:
        return {"error": "no_token", "message": f"未找到 access_token，请检查 {CODEX_AUTH_FILE}"}

    req = urllib.request.Request(WHAM_USAGE_URL, headers={
        "Authorization": f"Bearer {access_token}",
        "Content-Type": "application/json",
        "User-Agent": "Mozilla/5.0",
    })

    try:
        resp = urllib.request.urlopen(req, timeout=15)
        data = json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        code = e.code
        body = e.read().decode()[:300] if hasattr(e, "read") else ""
        if code == 401:
            return {"error": "auth_failed", "message": "Token 已过期，请运行 codex 重新登录", "detail": body}
        return {"error": "api_error", "message": f"API 返回 HTTP {code}", "detail": body}
    except Exception as e:
        return {"error": "network_error", "message": f"请求失败: {e}"}

    return _normalize_usage(data)


def _normalize_usage(raw: dict) -> dict:
    """将 wham/usage 原始响应标准化为手环应用期望的格式"""
    plan_type = raw.get("plan_type", "unknown")
    rate_limit = raw.get("rate_limit", {})
    primary_raw = rate_limit.get("primary_window") or {}
    secondary_raw = rate_limit.get("secondary_window") or {}

    now_str = datetime.now(TZ_CST).isoformat()

    # 主窗口
    p_reset_at = primary_raw.get("reset_at", 0)
    p_window_sec = primary_raw.get("limit_window_seconds", 0)
    p_used = primary_raw.get("used_percent", 0)
    result = {
        "plan_type": plan_type,
        "primary": {
            "label": format_window_label(p_window_sec),
            "used_percent": p_used,
            "remaining_percent": round(100 - p_used, 1),
            "window_minutes": int(p_window_sec / 60) if p_window_sec else 0,
            "resets_at": p_reset_at,
            "resets_in": format_resets_in(p_reset_at),
        },
        "secondary": None,
        "updated_at": now_str,
        "live": True,  # 标记为实时数据
    }

    # 副窗口（Plus/Pro 用户才有）
    if secondary_raw:
        s_reset_at = secondary_raw.get("reset_at", 0)
        s_window_sec = secondary_raw.get("limit_window_seconds", 0)
        s_used = secondary_raw.get("used_percent", 0)
        result["secondary"] = {
            "label": format_window_label(s_window_sec),
            "used_percent": s_used,
            "remaining_percent": round(100 - s_used, 1),
            "window_minutes": int(s_window_sec / 60) if s_window_sec else 0,
            "resets_at": s_reset_at,
            "resets_in": format_resets_in(s_reset_at),
        }

    # 额外信息
    credits_info = raw.get("credits", {})
    result["credits"] = {
        "has_credits": credits_info.get("has_credits", False),
        "balance": credits_info.get("balance"),
        "unlimited": credits_info.get("unlimited", False),
    }

    return result


def get_quota_data() -> dict:
    """获取额度数据（带缓存）"""
    now = time.time()

    with _cache_lock:
        if _cache["data"] is not None and (now - _cache["updated_at"]) < CACHE_TTL:
            return _cache["data"]

    result = fetch_wham_usage()

    with _cache_lock:
        _cache["data"] = result
        _cache["updated_at"] = now

    return result


# ── MQTT 发布器 ────────────────────────────────────────
_mqtt_client = None
_mqtt_connected = False


def _on_mqtt_connect(client, userdata, flags, rc, properties=None):
    """MQTT 连接成功回调"""
    global _mqtt_connected
    if rc == 0:
        _mqtt_connected = True
        print(f"  [mqtt] 已连接 broker {MQTT_HOST}:{MQTT_PORT}")
        # 连接后立即发布一次当前数据
        data = get_quota_data()
        if "error" not in data:
            payload = json.dumps(data, ensure_ascii=False)
            client.publish(MQTT_TOPIC, payload, qos=MQTT_QOS, retain=MQTT_RETAIN)
            print(f"  [mqtt] 已发布初始数据到 {MQTT_TOPIC}")
    else:
        print(f"  [mqtt] 连接失败, rc={rc}")


def _on_mqtt_disconnect(client, userdata, flags, rc, properties=None):
    """MQTT 断线回调"""
    global _mqtt_connected
    _mqtt_connected = False
    if rc != 0:
        print(f"  [mqtt] 意外断线 (rc={rc}), 将自动重连...")


def mqtt_publisher_loop():
    """后台线程：定期发布额度数据到 MQTT"""
    global _mqtt_client

    if not HAS_MQTT:
        print("  [mqtt] paho-mqtt 未安装，跳过 MQTT 发布")
        return

    try:
        _mqtt_client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id="codex_bridge",
        )
        _mqtt_client.on_connect = _on_mqtt_connect
        _mqtt_client.on_disconnect = _on_mqtt_disconnect

        _mqtt_client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
        _mqtt_client.loop_start()  # 后台网络线程
    except Exception as e:
        print(f"  [mqtt] 初始化失败: {e}")
        return

    # 定期发布循环
    while True:
        try:
            time.sleep(MQTT_PUBLISH_INTERVAL)
            if not _mqtt_connected:
                continue

            data = get_quota_data()
            if "error" in data:
                print(f"  [mqtt] 数据获取失败: {data.get('message', 'unknown')}")
                continue

            payload = json.dumps(data, ensure_ascii=False)
            info = _mqtt_client.publish(
                MQTT_TOPIC, payload, qos=MQTT_QOS, retain=MQTT_RETAIN
            )
            if info.rc == mqtt.MQTT_ERR_SUCCESS:
                p = data.get("primary", {})
                s = data.get("secondary")
                parts = [f"primary={p.get('remaining_percent', '?')}%"]
                if s:
                    parts.append(f"secondary={s.get('remaining_percent', '?')}%")
                print(f"  [mqtt] 已发布 {' | '.join(parts)}")
            else:
                print(f"  [mqtt] 发布失败: rc={info.rc}")

        except Exception as e:
            print(f"  [mqtt] 发布异常: {e}")


class QuotaHandler(BaseHTTPRequestHandler):
    """HTTP 请求处理器"""

    def do_GET(self):
        parsed = urlparse(self.path)

        if parsed.path == "/quota":
            data = get_quota_data()
            self._send_json(200, data)
        elif parsed.path == "/health":
            self._send_json(200, {"status": "ok", "timestamp": datetime.now(TZ_CST).isoformat()})
        elif parsed.path == "/":
            self._send_html()
        else:
            self._send_json(404, {"error": "not_found"})

    def _send_json(self, code: int, data: dict):
        body = json.dumps(data, ensure_ascii=False, indent=2).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_html(self):
        """状态页面"""
        data = get_quota_data()
        if "error" in data:
            status_text = f"<p style='color:#e74c3c'>{data.get('message', 'Unknown error')}</p>"
        else:
            p = data.get("primary", {})
            s = data.get("secondary")
            cards = f"""
            <div class="card">
                <h3>{p.get('label', '额度')} 窗口</h3>
                <div class="bar"><div class="fill p" style="width:{p.get('used_percent',0)}%"></div></div>
                <p>已用 {p.get('used_percent',0)}% · 剩余 {p.get('remaining_percent',100)}% · {p.get('resets_in','')}</p>
            </div>
            """
            if s:
                cards += f"""
                <div class="card">
                    <h3>{s.get('label', '副')} 窗口</h3>
                    <div class="bar"><div class="fill s" style="width:{s.get('used_percent',0)}%"></div></div>
                    <p>已用 {s.get('used_percent',0)}% · 剩余 {s.get('remaining_percent',100)}% · {s.get('resets_in','')}</p>
                </div>
                """
            status_text = cards

        live_badge = '<span style="color:#3fb950">● 实时</span>' if data.get("live") else ""

        html = f"""<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Codex Quota Bridge</title>
<style>
body{{font-family:system-ui;max-width:480px;margin:40px auto;padding:0 20px;background:#0d1117;color:#c9d1d9}}
h1{{color:#58a6ff}}
.card{{background:#161b22;border-radius:12px;padding:20px;margin:16px 0}}
.bar{{background:#21262d;border-radius:6px;height:12px;overflow:hidden;margin:8px 0}}
.fill{{height:100%;border-radius:6px;transition:width .3s}}
.fill.p{{background:linear-gradient(90deg,#3fb950,#d29922,#f85149)}}
.fill.s{{background:linear-gradient(90deg,#58a6ff,#d29922,#f85149)}}
p{{color:#8b949e;font-size:14px}}
.endpoint{{background:#161b22;border-radius:8px;padding:12px;font-family:monospace;font-size:13px;color:#58a6ff;word-break:break-all}}
</style></head><body>
<h1>Codex Quota Bridge</h1>
<p>套餐: {data.get('plan_type', 'N/A').upper()} {live_badge}</p>
{status_text}
<div class="card"><h3>API Endpoint</h3>
<div class="endpoint">GET /quota</div>
<p>数据每 {CACHE_TTL} 秒刷新 · 数据源: chatgpt.com/wham/usage (实时)</p></div>
<script>setTimeout(()=>location.reload(),60000)</script>
</body></html>"""
        body = html.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        """自定义日志格式"""
        now = datetime.now(TZ_CST).strftime("%H:%M:%S")
        print(f"  [{now}] {args[0]}")


def main():
    parser = argparse.ArgumentParser(description="Codex 额度桥接服务 v2 (实时)")
    parser.add_argument("--port", "-p", type=int, default=5678, help="监听端口，默认 5678")
    parser.add_argument("--host", default="0.0.0.0", help="监听地址，默认 0.0.0.0")
    args = parser.parse_args()

    local_ip = get_local_ip()

    # 启动时检查认证
    try:
        auth = read_auth_file()
        at = auth.get("tokens", {}).get("access_token", "")
        payload = decode_jwt_payload(at)
        exp = payload.get("exp", 0)
        remaining_h = (exp - time.time()) / 3600
        plan = "unknown"
        # 先做一次查询来获取 plan_type
        print()
        print("  ==================================================")
        print("    Codex Quota Bridge v2 (realtime)  Ctrl+C quit")
        print("  ==================================================")
        print()
        print(f"  Auth file: {CODEX_AUTH_FILE}")
        print(f"  Token 剩余: {remaining_h:.1f} 小时", end="")
        if remaining_h < 24:
            print(" [!] 即将过期!")
        else:
            print(" [OK]")
    except FileNotFoundError as e:
        print(f"\n  [!] {e}")
        print("  请先运行 codex CLI 并登录 ChatGPT 账号\n")
        sys.exit(1)

    print(f"  数据源:   {WHAM_USAGE_URL}")
    print(f"  缓存:     {CACHE_TTL}秒")
    if HAS_MQTT:
        print(f"  MQTT:     {MQTT_HOST}:{MQTT_PORT} topic={MQTT_TOPIC}")
    else:
        print(f"  MQTT:     未安装 paho-mqtt，已禁用")
    print()
    print(f"  本机地址:  http://{local_ip}:{args.port}")
    print(f"  额度接口:  http://{local_ip}:{args.port}/quota")
    print(f"  状态页面:  http://{local_ip}:{args.port}/")
    print(f"  健康检查:  http://{local_ip}:{args.port}/health")
    print()

    # 启动 MQTT 发布线程
    if HAS_MQTT:
        mqtt_thread = threading.Thread(
            target=mqtt_publisher_loop,
            daemon=True,
            name="mqtt-publisher"
        )
        mqtt_thread.start()

    server = HTTPServer((args.host, args.port), QuotaHandler)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n  已停止服务。")
        server.server_close()
        sys.exit(0)


if __name__ == "__main__":
    main()
