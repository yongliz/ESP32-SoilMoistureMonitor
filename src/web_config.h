#pragma once
#include <stdint.h>

namespace webcfg {

// 进入 AP 配置门户，阻塞执行直到：
//   - 配置成功且验证连接通过 → 返回 true（调用方应 ESP.restart()）
//   - 超时无操作 → 返回 false（调用方进入深度睡眠）
bool runPortal(uint32_t timeoutSec);

} // namespace webcfg
