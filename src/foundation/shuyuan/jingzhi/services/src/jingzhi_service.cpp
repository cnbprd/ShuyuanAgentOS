/*
 * Copyright (c) 2026 枢元智能体操作系统 (Shuyuan AgentOS)
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jingzhi_service.h"
#include "hilog/log.h"
#include <algorithm>
#include <cctype>
#include <cstdint>

namespace OHOS {
using HiviewDFX::HiLog;
namespace Shuyuan {

namespace {
constexpr OHOS::HiviewDFX::HiLogLabel LABEL = {LOG_CORE, 0xD001801, "ShuyuanJingzhi"};
constexpr size_t MAX_AUDIT_QUEUE_SIZE = 1024;
constexpr size_t MAX_AUDIT_RECORD_SIZE = 512;

std::string NormalizeAction(const std::string& action)
{
    size_t begin = 0;
    size_t end = action.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(action[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(action[end - 1])) != 0) {
        --end;
    }
    std::string normalized = action.substr(begin, end - begin);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return normalized;
}
}

REGISTER_SYSTEM_ABILITY_BY_ID(JingzhiService, JINGZHI_SA_ID, true);

JingzhiService::JingzhiService(int32_t saId, bool runOnCreate)
    : SystemAbility(saId, runOnCreate)
{
}

void JingzhiService::OnStart()
{
    HiLog::Info(LABEL, "JingzhiService::OnStart enter.");
    bool ret = Publish(this);
    if (!ret) {
        HiLog::Error(LABEL, "JingzhiService Publish to samgr failed.");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ready_ = true;
    }
    HiLog::Info(LABEL, "JingzhiService published as SA %{public}d.", JINGZHI_SA_ID);
}

void JingzhiService::OnStop()
{
    HiLog::Info(LABEL, "JingzhiService::OnStop enter.");
    std::lock_guard<std::mutex> lock(mutex_);
    ready_ = false;
    auditQueue_.clear();
    auditCount_ = 0;
}

int32_t JingzhiService::PreAuthorize(const std::string& subject, const std::string& action,
    int32_t& verdict, std::string& reason)
{
    if (action.empty()) {
        return SHUYUAN_ERR_INVALID_PARAM;
    }

    // 第一版占位静态策略：显式前缀 + 默认拒绝。
    // 不是安全边界：没有调用方身份、授权令牌或下游强制校验；正式版必须补这三项。
    const std::string normalized = NormalizeAction(action);
    if (subject.empty()) {
        verdict = VERDICT_DENY;
        reason = "empty subject";
    } else if (normalized.rfind("allow:", 0) == 0) {
        verdict = VERDICT_ALLOW;
        reason = "explicit allow rule";
    } else if (normalized.rfind("deny:", 0) == 0) {
        verdict = VERDICT_DENY;
        reason = "explicit deny rule";
    } else if (normalized.rfind("pending:", 0) == 0) {
        verdict = VERDICT_PENDING;
        reason = "requires approval";
    } else {
        verdict = VERDICT_DENY;
        reason = "unknown action: default deny";
    }

    HiLog::Info(LABEL, "PreAuthorize subject=%{public}s action=%{public}s verdict=%{public}d.",
        subject.c_str(), normalized.c_str(), verdict);
    return SHUYUAN_OK;
}

int32_t JingzhiService::Audit(const std::string& record)
{
    if (record.empty()) {
        return SHUYUAN_ERR_INVALID_PARAM;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (auditQueue_.size() >= MAX_AUDIT_QUEUE_SIZE) {
        auditQueue_.pop_front();
    }
    const std::string recordToStore =
        record.size() > MAX_AUDIT_RECORD_SIZE ? record.substr(0, MAX_AUDIT_RECORD_SIZE) : record;
    auditQueue_.push_back(recordToStore);
    auditCount_++;
    if ((auditCount_ % 100) == 0) {
        HiLog::Info(LABEL, "Audit records accepted=%{public}d, buffered=%{public}zu.",
            auditCount_, auditQueue_.size());
    }
    return SHUYUAN_OK;
}

int32_t JingzhiService::IsReady(bool& ready)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ready = ready_;
    return SHUYUAN_OK;
}

} // namespace Shuyuan
} // namespace OHOS
