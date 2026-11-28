/*
 * Copyright (c) 2026 枢元智能体操作系统 (Shuyuan AgentOS)
 * SPDX-License-Identifier: Apache-2.0
 */
#include "heming_service.h"
#include "hilog/log.h"

namespace OHOS {
using HiviewDFX::HiLog;
namespace Shuyuan {

namespace {
constexpr OHOS::HiviewDFX::HiLogLabel LABEL = {LOG_CORE, 0xD001803, "ShuyuanHeming"};
}

REGISTER_SYSTEM_ABILITY_BY_ID(HemingService, HEMING_SA_ID, true);

HemingService::HemingService(int32_t saId, bool runOnCreate)
    : SystemAbility(saId, runOnCreate)
{
}

void HemingService::OnStart()
{
    HiLog::Info(LABEL, "HemingService::OnStart enter.");
    bool ret = Publish(this);
    if (!ret) {
        HiLog::Error(LABEL, "HemingService Publish to samgr failed.");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ready_ = true;
    }
    HiLog::Info(LABEL, "HemingService published as SA %{public}d.", HEMING_SA_ID);
}

void HemingService::OnStop()
{
    HiLog::Info(LABEL, "HemingService::OnStop enter.");
    std::lock_guard<std::mutex> lock(mutex_);
    ready_ = false;
    collabStatus_.clear();
}

int32_t HemingService::DispatchPlan(const std::string& planId, const std::string& steps,
    std::string& collabId)
{
    if (planId.empty()) {
        return SHUYUAN_ERR_INVALID_PARAM;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    collabSeq_++;
    collabId = "C-" + std::to_string(3000 + collabSeq_);
    collabStatus_[collabId] = "dispatched";
    HiLog::Info(LABEL, "Plan %{public}s accepted as collab %{public}s.",
        planId.c_str(), collabId.c_str());
    return SHUYUAN_OK;
}

int32_t HemingService::GetCollabStatus(const std::string& collabId, std::string& status)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = collabStatus_.find(collabId);
    if (it == collabStatus_.end()) {
        return SHUYUAN_ERR_NOT_FOUND;
    }
    status = it->second;
    return SHUYUAN_OK;
}

int32_t HemingService::IsReady(bool& ready)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ready = ready_;
    return SHUYUAN_OK;
}

} // namespace Shuyuan
} // namespace OHOS
