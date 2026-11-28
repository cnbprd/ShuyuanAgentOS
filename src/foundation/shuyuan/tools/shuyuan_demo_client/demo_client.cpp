/*
 * Copyright (c) 2026 枢元智能体操作系统 (Shuyuan AgentOS)
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * 枢元智能体操作系统 · 跨 SA 串通演示客户端（测试可执行）
 *
 * 用途：在模拟器 / 设备上验证五层 SA 是否均可被 samgr 发现并贯通调用。
 * 调用链对应架构：
 *   监观(9902) 取快照 → 灵台(9901) 注册 Agent、提交意图 →
 *   敬止(9905) 授权并审计 → 放行后和鸣(9903) 下发规划 →
 *   令终(9904) 挂接并收口 → 回读各层状态。
 *
 * 说明：本进程为测试工具，不承担业务逻辑；错误处理与日志为占位实现，
 * 正式验证时按目标源码树的日志与异常规范补齐。
 */

#include "i_lingtai_service.h"
#include "i_jingzhi_service.h"
#include "i_jianguan_service.h"
#include "i_heming_service.h"
#include "i_lingzhong_service.h"
// Proxy 头必须显式包含：BrokerDelegator<Proxy> 的静态注册发生在这些头文件里，
// 只包含 i_*_service.h 时 iface_cast 在客户端进程里找不到 creator。
#include "lingtai_service_proxy.h"
#include "jingzhi_service_proxy.h"
#include "jianguan_service_proxy.h"
#include "heming_service_proxy.h"
#include "lingzhong_service_proxy.h"
#include "iservice_registry.h"
#include "iremote_object.h"

#include <cstdio>
#include <string>

using namespace OHOS;
using namespace OHOS::Shuyuan;

namespace {

sptr<IRemoteObject> GetRemote(int32_t saId)
{
    auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (samgr == nullptr) {
        std::printf("[shuyuan-demo] samgr unavailable, saId=%d\n", saId);
        return nullptr;
    }
    return samgr->GetSystemAbility(saId);
}

} // namespace

int main()
{
    // 1. 监观：读取环境与任务状态快照（只读）。
    auto jianguanProxy = iface_cast<IJianguanService>(GetRemote(JIANGUAN_SA_ID));
    if (jianguanProxy != nullptr) {
        std::string snapshot;
        if (jianguanProxy->GetSnapshot(snapshot) == SHUYUAN_OK) {
            std::printf("[shuyuan-demo] snapshot: %s\n", snapshot.c_str());
        }
    } else {
        std::printf("[shuyuan-demo] jianguan proxy null\n");
    }

    // 2. 灵台：注册一个可被调度的 Agent。
    auto lingtaiProxy = iface_cast<ILingtaiService>(GetRemote(LINGTAI_SA_ID));
    if (lingtaiProxy == nullptr) {
        std::printf("[shuyuan-demo] lingtai proxy null, abort\n");
        return 1;
    }

    std::string agentId;
    if (lingtaiProxy->RegisterAgent("demo-agent", "query,summary", agentId) != SHUYUAN_OK) {
        std::printf("[shuyuan-demo] RegisterAgent failed\n");
        return 1;
    }
    std::printf("[shuyuan-demo] agent registered: %s\n", agentId.c_str());

    // 3. 灵台：提交任务意图，创建任务实例。
    std::string taskId;
    if (lingtaiProxy->SubmitIntent("demo intent", "demo-context", taskId) != SHUYUAN_OK) {
        std::printf("[shuyuan-demo] SubmitIntent failed\n");
        return 1;
    }
    std::printf("[shuyuan-demo] task created: %s\n", taskId.c_str());

    // 4. 敬止：动作出口前授权校验，并写审计。
    auto jingzhiProxy = iface_cast<IJingzhiService>(GetRemote(JINGZHI_SA_ID));
    int32_t verdict = VERDICT_DENY;
    std::string reason;
    if (jingzhiProxy != nullptr) {
        if (jingzhiProxy->PreAuthorize(agentId, "allow:query:demo", verdict, reason) == SHUYUAN_OK) {
            std::printf("[shuyuan-demo] authorize verdict=%d reason=%s\n", verdict, reason.c_str());
            jingzhiProxy->Audit(agentId + " allow:query:demo verdict=" + std::to_string(verdict));
        }
    } else {
        std::printf("[shuyuan-demo] jingzhi proxy null\n");
    }

    // 5. 和鸣：仅在授权放行后下发协作规划。
    std::string collabId;
    sptr<IHemingService> hemingProxy;
    if (verdict == VERDICT_ALLOW) {
        hemingProxy = iface_cast<IHemingService>(GetRemote(HEMING_SA_ID));
        if (hemingProxy != nullptr) {
            if (hemingProxy->DispatchPlan(taskId, "step1,step2", collabId) == SHUYUAN_OK) {
                std::printf("[shuyuan-demo] collab dispatched: %s\n", collabId.c_str());
            }
        } else {
            std::printf("[shuyuan-demo] heming proxy null\n");
        }
    }

    // 6. 令终：挂接任务并收口，写入结果。
    auto lingzhongProxy = iface_cast<ILingzhongService>(GetRemote(LINGZHONG_SA_ID));
    std::string finalState;
    if (lingzhongProxy != nullptr) {
        lingzhongProxy->AttachTask(taskId);
        if (lingzhongProxy->FinishTask(taskId, "demo result", finalState) == SHUYUAN_OK) {
            std::printf("[shuyuan-demo] task closed, final=%s\n", finalState.c_str());
            std::string result;
            if (lingzhongProxy->GetTaskResult(taskId, result) == SHUYUAN_OK) {
                std::printf("[shuyuan-demo] task result=%s\n", result.c_str());
            }
        }
    } else {
        std::printf("[shuyuan-demo] lingzhong proxy null\n");
    }

    // 7. 回读和鸣协作状态与灵台任务状态，作为贯通验证终点。
    if (hemingProxy != nullptr && !collabId.empty()) {
        std::string collabStatus;
        if (hemingProxy->GetCollabStatus(collabId, collabStatus) == SHUYUAN_OK) {
            std::printf("[shuyuan-demo] collab %s status=%s\n", collabId.c_str(), collabStatus.c_str());
        }
    }

    std::string status;
    if (lingtaiProxy->GetTaskStatus(taskId, status) == SHUYUAN_OK) {
        std::printf("[shuyuan-demo] task %s status=%s final=%s\n",
            taskId.c_str(), status.c_str(), finalState.c_str());
    }

    // 8. 就绪查询：各层 IsReady；灵台另取启动汇总状态。
    bool ready = false;
    if (lingtaiProxy->IsReady(ready) == SHUYUAN_OK) {
        std::printf("[shuyuan-demo] lingtai ready=%d\n", ready);
    }
    std::string bootStatus;
    if (lingtaiProxy->GetBootStatus(bootStatus) == SHUYUAN_OK) {
        std::printf("[shuyuan-demo] boot status: %s\n", bootStatus.c_str());
    }
    if (jianguanProxy != nullptr && jianguanProxy->IsReady(ready) == SHUYUAN_OK) {
        std::printf("[shuyuan-demo] jianguan ready=%d\n", ready);
    }
    if (jingzhiProxy != nullptr && jingzhiProxy->IsReady(ready) == SHUYUAN_OK) {
        std::printf("[shuyuan-demo] jingzhi ready=%d\n", ready);
    }
    if (hemingProxy != nullptr && hemingProxy->IsReady(ready) == SHUYUAN_OK) {
        std::printf("[shuyuan-demo] heming ready=%d\n", ready);
    }
    if (lingzhongProxy != nullptr && lingzhongProxy->IsReady(ready) == SHUYUAN_OK) {
        std::printf("[shuyuan-demo] lingzhong ready=%d\n", ready);
    }

    return 0;
}
