/*
 * Copyright (c) 2026 枢元智能体操作系统 (Shuyuan AgentOS)
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jianguan_service.h"
#include "hilog/log.h"
#include <sstream>

namespace OHOS {
using HiviewDFX::HiLog;
namespace Shuyuan {

namespace {
constexpr OHOS::HiviewDFX::HiLogLabel LABEL = {LOG_CORE, 0xD001802, "ShuyuanJianguan"};
}

REGISTER_SYSTEM_ABILITY_BY_ID(JianguanService, JIANGUAN_SA_ID, true);

JianguanService::JianguanService(int32_t saId, bool runOnCreate)
    : SystemAbility(saId, runOnCreate)
{
}

void JianguanService::OnStart()
{
    HiLog::Info(LABEL, "JianguanService::OnStart enter.");
    bool ret = Publish(this);
    if (!ret) {
        HiLog::Error(LABEL, "JianguanService Publish to samgr failed.");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ready_ = true;
    }
    HiLog::Info(LABEL, "JianguanService published as SA %{public}d.", JIANGUAN_SA_ID);
}

void JianguanService::OnStop()
{
    HiLog::Info(LABEL, "JianguanService::OnStop enter.");
    std::lock_guard<std::mutex> lock(mutex_);
    ready_ = false;
}

int32_t JianguanService::GetSnapshot(std::string& snapshot)
{
    std::lock_guard<std::mutex> lock(mutex_);
    sampleCount_++;
    std::ostringstream oss;
    oss << "{\"layer\":\"jianguan\",\"samples\":" << sampleCount_ << "}";
    snapshot = oss.str();
    return SHUYUAN_OK;
}

int32_t JianguanService::IsReady(bool& ready)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ready = ready_;
    return SHUYUAN_OK;
}

} // namespace Shuyuan
} // namespace OHOS
