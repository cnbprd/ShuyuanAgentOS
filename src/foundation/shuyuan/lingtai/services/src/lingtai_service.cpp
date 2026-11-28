/*
 * Copyright (c) 2026 枢元智能体操作系统 (Shuyuan AgentOS)
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lingtai_service.h"
#include "hilog/log.h"

namespace OHOS {
using HiviewDFX::HiLog;
namespace Shuyuan {

namespace {
constexpr OHOS::HiviewDFX::HiLogLabel LABEL = {LOG_CORE, 0xD001800, "ShuyuanLingtai"};
}

REGISTER_SYSTEM_ABILITY_BY_ID(LingtaiService, LINGTAI_SA_ID, true);

LingtaiService::LingtaiService(int32_t saId, bool runOnCreate)
    : SystemAbility(saId, runOnCreate)
{
}

void LingtaiService::OnStart()
{
    HiLog::Info(LABEL, "LingtaiService::OnStart enter.");

    // 向 samgr 发布本 SA；发布成功后外部方可经 GetSystemAbility 获取代理。
    bool ret = Publish(this);
    if (!ret) {
        HiLog::Error(LABEL, "LingtaiService Publish to samgr failed.");
        return;
    }

    // IPC 接口已可响应，置内存就绪标志；对外就绪参数置位待系统属性 API 接入。
    // 加锁写入：IsReady 可能由其他 IPC 线程并发读取，避免数据竞争。
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ready_ = true;
    }
    // 就绪里程碑：此处置位 sys.shuyuan.lingtai.ready=1。
    // 系统属性写入 API（SetParameter 等）以目标源码树 headers 为准，阶段二接入。
    HiLog::Info(LABEL, "LingtaiService published as SA %{public}d.", LINGTAI_SA_ID);
}

void LingtaiService::OnStop()
{
    HiLog::Info(LABEL, "LingtaiService::OnStop enter.");
    std::lock_guard<std::mutex> lock(mutex_);
    ready_ = false;
    agentRegistry_.clear();
    taskStatus_.clear();
}

int32_t LingtaiService::RegisterAgent(const std::string& name, const std::string& capabilities,
    std::string& agentId)
{
    if (name.empty()) {
        return SHUYUAN_ERR_INVALID_PARAM;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    agentSeq_++;
    agentId = "A-" + std::to_string(1000 + agentSeq_);
    agentRegistry_[agentId] = name + ":" + capabilities;
    HiLog::Info(LABEL, "Agent registered: %{public}s.", agentId.c_str());
    return SHUYUAN_OK;
}

int32_t LingtaiService::UnregisterAgent(const std::string& agentId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = agentRegistry_.find(agentId);
    if (it == agentRegistry_.end()) {
        return SHUYUAN_ERR_NOT_FOUND;
    }
    agentRegistry_.erase(it);
    return SHUYUAN_OK;
}

int32_t LingtaiService::SubmitIntent(const std::string& intent, const std::string& contextRef,
    std::string& taskId)
{
    if (intent.empty()) {
        return SHUYUAN_ERR_INVALID_PARAM;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    taskSeq_++;
    taskId = "T-" + std::to_string(2000 + taskSeq_);
    // 第一版仅登记为 pending；编排下发由后续接入的和鸣与外部运行时完成。
    taskStatus_[taskId] = "pending";
    HiLog::Info(LABEL, "Intent accepted as task %{public}s.", taskId.c_str());
    return SHUYUAN_OK;
}

int32_t LingtaiService::GetTaskStatus(const std::string& taskId, std::string& status)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = taskStatus_.find(taskId);
    if (it == taskStatus_.end()) {
        return SHUYUAN_ERR_NOT_FOUND;
    }
    status = it->second;
    return SHUYUAN_OK;
}

int32_t LingtaiService::UpdateTaskStatus(const std::string& taskId, const std::string& status)
{
    if (taskId.empty() || status.empty()) {
        return SHUYUAN_ERR_INVALID_PARAM;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = taskStatus_.find(taskId);
    if (it == taskStatus_.end()) {
        return SHUYUAN_ERR_NOT_FOUND;
    }
    // 第一版只做状态字符串回写；合法状态机与越权校验在编排层接入后收紧。
    it->second = status;
    HiLog::Info(LABEL, "Task %{public}s status updated to %{public}s.",
        taskId.c_str(), status.c_str());
    return SHUYUAN_OK;
}

int32_t LingtaiService::IsReady(bool& ready)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ready = ready_;
    return SHUYUAN_OK;
}

int32_t LingtaiService::GetBootStatus(std::string& status)
{
    std::lock_guard<std::mutex> lock(mutex_);
    status = ready_ ? "lingtai:ready" : "lingtai:not-ready";
    return SHUYUAN_OK;
}

} // namespace Shuyuan
} // namespace OHOS
