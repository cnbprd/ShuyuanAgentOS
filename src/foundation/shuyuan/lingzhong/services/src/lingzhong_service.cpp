/*
 * Copyright (c) 2026 枢元智能体操作系统 (Shuyuan AgentOS)
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lingzhong_service.h"
#include "lingtai_service_proxy.h"
#include "iservice_registry.h"
#include "iremote_object.h"
#include "hilog/log.h"

namespace OHOS {
using HiviewDFX::HiLog;
namespace Shuyuan {

namespace {
constexpr OHOS::HiviewDFX::HiLogLabel LABEL = {LOG_CORE, 0xD001804, "ShuyuanLingzhong"};
}

REGISTER_SYSTEM_ABILITY_BY_ID(LingzhongService, LINGZHONG_SA_ID, true);

LingzhongService::LingzhongService(int32_t saId, bool runOnCreate)
    : SystemAbility(saId, runOnCreate)
{
}

void LingzhongService::OnStart()
{
    HiLog::Info(LABEL, "LingzhongService::OnStart enter.");
    bool ret = Publish(this);
    if (!ret) {
        HiLog::Error(LABEL, "LingzhongService Publish to samgr failed.");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ready_ = true;
    }
    HiLog::Info(LABEL, "LingzhongService published as SA %{public}d.", LINGZHONG_SA_ID);
}

void LingzhongService::OnStop()
{
    HiLog::Info(LABEL, "LingzhongService::OnStop enter.");
    std::lock_guard<std::mutex> lock(mutex_);
    ready_ = false;
    taskState_.clear();
    taskResult_.clear();
}

int32_t LingzhongService::AttachTask(const std::string& taskId)
{
    if (taskId.empty()) {
        return SHUYUAN_ERR_INVALID_PARAM;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (taskState_.find(taskId) == taskState_.end()) {
        taskState_[taskId] = "attached";
    }
    return SHUYUAN_OK;
}

int32_t LingzhongService::FinishTask(const std::string& taskId, const std::string& result,
    std::string& finalState)
{
    if (taskId.empty()) {
        return SHUYUAN_ERR_INVALID_PARAM;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = taskState_.find(taskId);
        if (it == taskState_.end()) {
            return SHUYUAN_ERR_NOT_FOUND;
        }
        it->second = "done";
        taskResult_[taskId] = result;
        finalState = "done";
    }

    // 不持锁发 IPC：先把本地终态与结果落内存，再通知灵台更新调度视图。
    // 通知失败不回滚终态，但会在日志中留下不一致告警。
    int32_t notifyRet = NotifyLingtaiStatus(taskId, finalState);
    HiLog::Info(LABEL, "Task %{public}s finished, result length=%{public}zu, notify=%{public}d.",
        taskId.c_str(), result.size(), notifyRet);
    return SHUYUAN_OK;
}

int32_t LingzhongService::GetTaskResult(const std::string& taskId, std::string& result)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = taskResult_.find(taskId);
    if (it == taskResult_.end()) {
        return SHUYUAN_ERR_NOT_FOUND;
    }
    result = it->second;
    return SHUYUAN_OK;
}

int32_t LingzhongService::NotifyLingtaiStatus(const std::string& taskId, const std::string& state)
{
    auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (samgr == nullptr) {
        HiLog::Error(LABEL, "NotifyLingtaiStatus: samgr unavailable.");
        return SHUYUAN_ERR_IPC;
    }
    auto remote = samgr->GetSystemAbility(LINGTAI_SA_ID);
    if (remote == nullptr) {
        HiLog::Error(LABEL, "NotifyLingtaiStatus: lingtai SA %{public}d not found.", LINGTAI_SA_ID);
        return SHUYUAN_ERR_NOT_FOUND;
    }
    auto proxy = iface_cast<ILingtaiService>(remote);
    if (proxy == nullptr) {
        HiLog::Error(LABEL, "NotifyLingtaiStatus: iface_cast to lingtai failed.");
        return SHUYUAN_ERR_IPC;
    }
    return proxy->UpdateTaskStatus(taskId, state);
}

int32_t LingzhongService::GetTaskFinalState(const std::string& taskId, std::string& state)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = taskState_.find(taskId);
    if (it == taskState_.end()) {
        return SHUYUAN_ERR_NOT_FOUND;
    }
    state = it->second;
    return SHUYUAN_OK;
}

int32_t LingzhongService::IsReady(bool& ready)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ready = ready_;
    return SHUYUAN_OK;
}

} // namespace Shuyuan
} // namespace OHOS
