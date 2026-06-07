/**
 * @file codex_ui.h
 * @brief Codex 额度显示 UI 接口
 */
#ifndef CODEX_UI_H
#define CODEX_UI_H

#include "codex_http.h"

/** 创建完整 UI 界面（启动时调用一次） */
void codex_ui_create(void);

/** 用最新数据刷新 UI */
void codex_ui_update(const codex_quota_t *quota);

/** 设置启动/连接/拉取中的状态文本 */
void codex_ui_set_status(const char *message);

/** 设置具体错误状态 */
void codex_ui_set_error(const char *message);

/** 设置离线状态 */
void codex_ui_set_offline(void);

#endif /* CODEX_UI_H */
