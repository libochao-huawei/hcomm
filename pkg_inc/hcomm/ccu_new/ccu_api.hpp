#ifndef CCU_API_HPP
#define CCU_API_HPP

#include "ccu_data_api.h"
#include "ccu_data_api_impl.h"
#include "ccu_loop_macro.h"
#include "ccu_control_flow_macro.h"
#include "ccu_log.h"
#include "hcom_common.h"

namespace ccu {

// ==================== 类型别名 ====================
using Variable  = ::CcuVariable;
using Address   = ::CcuAddress;
using Event     = ::CcuEvent;
using Buffer    = ::CcuBuffer;
using LocalAddr = ::CcuLocalAddr;
using RemoteAddr = ::CcuRemoteAddr;
using CondExpr  = ::CcuCondExpr;


// ==================== 资源创建 ====================

inline CcuResult Alloc(CcuVariable* v)       { return CcuVariableAlloc(&(v->handle)); }
inline CcuResult Alloc(CcuAddress* a)         { return CcuAddressAlloc(&(a->handle)); }
inline CcuResult Alloc(CcuEvent* e)           { return CcuEventAlloc(&(e->handle)); }
inline CcuResult Alloc(CcuBuffer* b)          { return CcuBufferAlloc(&(b->handle)); }
inline CcuResult Alloc(CcuLocalAddr* la)      { return CcuLocalAddrAlloc(&(la->handle),&(la->addr.handle),&(la->token.handle)); }
inline CcuResult Alloc(CcuRemoteAddr* ra)     { return CcuRemoteAddrAlloc(&(ra->handle),&(ra->addr.handle),&(ra->token.handle)); }

inline CcuResult BlockAlloc(CcuBuffer* bufs, uint32_t count) {
    // return CcuBlockBufferAlloc(bufs, count);
    if (bufs == nullptr || count == 0) {
        return CcuResult::CCU_E_PARA;
    }
    CcuBufferHandle bufHandles[count];
    CCU_CHK_RET(CcuBlockBufferAlloc(bufHandles, count));
    for (uint32_t i = 0; i < count; i++) {
        bufs[i].handle = bufHandles[i];
    }
    return CcuResult::CCU_SUCCESS;
}
inline CcuResult BlockAlloc(CcuVariable* vars, uint32_t count) {
    if (vars == nullptr || count == 0) {
        return CcuResult::CCU_E_PARA;
    }
    CcuVariableHandle varHandles[count]; 
    CCU_CHK_RET(CcuBlockVariableAlloc(varHandles, count));
    for (uint32_t i = 0; i < count; i++) {
        vars[i].handle = varHandles[i];
    }
    return CcuResult::CCU_SUCCESS;
}
inline CcuResult BlockAlloc(CcuEvent* events, uint32_t count) {
    if (events == nullptr || count == 0) {
        return CcuResult::CCU_E_PARA;
    }
    CcuEventHandle eventHandles[count];
    CCU_CHK_RET(CcuBlockEventAlloc(eventHandles, count));
    for (uint32_t i = 0; i < count; i++) {
        events[i].handle = eventHandles[i];
    }
    return CcuResult::CCU_SUCCESS;
}
inline CcuResult CreateByChannel(ChannelHandle channel, uint32_t varIndex, CcuVariable* var) { return CcuVariableCreateByChannel(channel, varIndex, &(var->handle)); }


// ==================== 事件 ====================
inline CcuResult RecordEvent(CcuEvent e)  { return CcuRecordEvent(e.handle); }
inline CcuResult WaitEvent(CcuEvent e)    { return CcuWaitEvent(e.handle); }
inline CcuResult SetMask(CcuEvent e, uint32_t mask=1) { return CcuSetMask(e.handle, mask); }
inline CcuResult NotifyRecord(ChannelHandle channel, uint32_t remoteNotifyIdx, uint32_t mask=1){ return CcuNotifyRecord(channel, remoteNotifyIdx, mask); }
inline CcuResult NotifyWait(ChannelHandle channel, uint32_t localNotifyIdx, uint32_t mask=1){ return CcuNotifyWait(channel, localNotifyIdx, mask); }
inline CcuResult WriteVariableWithNotify(ChannelHandle channel, CcuVariable var,uint32_t remoteVarIdx, uint32_t remoteNotifyIdx, uint32_t mask=1){ return CcuWriteVariableWithNotify(channel, var.handle, remoteVarIdx, remoteNotifyIdx, mask); }

// ==================== 加载 ====================
inline CcuResult LoadArg(CcuVariable v) {
    return CcuLoadArg(v.handle);
}
inline CcuResult LoadVar(uint64_t addr, CcuVariable* v, uint32_t num) {
    return CcuLoadVar(addr, v[0].handle, num);
}

// ==================== 本地拷贝（3 种重载） ====================

// LocalAddr → LocalAddr,LocalAddr → Buffer,Buffer → LocalAddr
inline CcuResult LocalCopyNb(CcuLocalAddr dst, CcuLocalAddr src,CcuVariable len, CcuEvent event) { return CcuLocalCopyMemToMem(dst.handle, src.handle, len.handle, event.handle); }
inline CcuResult LocalCopyNb(CcuBuffer dst, CcuLocalAddr src, CcuVariable len, CcuEvent event) { return CcuLocalCopyMemToBuffer(dst.handle, src.handle, len.handle, event.handle); }
inline CcuResult LocalCopyNb(CcuLocalAddr dst, CcuBuffer src,CcuVariable len, CcuEvent event) { return CcuLocalCopyBufferToMem(dst.handle, src.handle, len.handle, event.handle); }

// ==================== 本地 Reduce ====================
inline CcuResult LocalReduceNb(CcuLocalAddr dst, CcuLocalAddr src,CcuVariable len, HcclDataType dataType, HcclReduceOp opType, CcuEvent event) { return CcuLocalMemReduce(dst.handle, src.handle, len.handle, dataType, opType, event.handle); }
inline CcuResult LocalReduceNb(CcuBuffer* buffers, uint32_t count, HcclDataType dataType, HcclDataType outputDataType, HcclReduceOp opType, CcuVariable len, CcuEvent event) 
{ 
    if (buffers == nullptr || count == 0) {
        return CcuResult::CCU_E_PARA;
    }
    CcuBufferHandle bufHandles[count];
    for (uint32_t i = 0; i < count; i++) {
        bufHandles[i] = buffers[i].handle;
    }
    return CcuLocalBufferReduce(bufHandles, count, dataType, outputDataType, opType,len.handle, event.handle); 
}

// ==================== 远端读====================

// 远端读 LocalAddr ← RemoteAddr
inline CcuResult ReadNb(ChannelHandle ch, CcuLocalAddr local, CcuRemoteAddr remote, CcuVariable len, CcuEvent event) { return CcuReadMemToMem(ch, local.handle, remote.handle, len.handle, event.handle); }
// 远端读 Buffer ← RemoteAddr
inline CcuResult ReadNb(ChannelHandle ch, CcuBuffer local, CcuRemoteAddr remote, CcuVariable len, CcuEvent event) { return CcuReadMemToBuffer(ch, local.handle, remote.handle, len.handle, event.handle); }
// 远端读 LocalAddr ← RemoteAddr Reduce (Reduce)
inline CcuResult ReadReduceNb(ChannelHandle ch, CcuLocalAddr local, CcuRemoteAddr remote, CcuVariable len, HcclDataType dataType, HcclReduceOp opType, CcuEvent event) { return CcuReadMemToMemReduce(ch, local.handle, remote.handle, len.handle, dataType, opType, event.handle); }

// ==================== 远端写 ====================

// LocalAddr → RemoteAddr
inline CcuResult WriteNb(ChannelHandle ch, CcuRemoteAddr remote, CcuLocalAddr local,  CcuVariable len, CcuEvent event){ return CcuWriteMemToMem(ch, remote.handle, local.handle, len.handle, event.handle); }
// Buffer → RemoteAddr
inline CcuResult WriteNb(ChannelHandle ch, CcuRemoteAddr remote, CcuBuffer local, CcuVariable len, CcuEvent event) { return CcuWriteBufferToMem(ch, remote.handle, local.handle, len.handle, event.handle); }
// LocalAddr → RemoteAddr Reduce (Reduce)
inline CcuResult WriteReduceNb(ChannelHandle ch, CcuRemoteAddr remote, CcuLocalAddr local, CcuVariable len, HcclDataType dataType, HcclReduceOp opType, CcuEvent event){ return CcuWriteMemToMemReduce(ch, remote.handle, local.handle, len.handle, dataType, opType, event.handle);}

// ==================== Loop ====================

inline CcuResult CreateBlockExecutor(CcuLoopExecutors *pool, uint32_t count) {
    return CcuCreateBlockExecutor(pool, count);
}

inline CcuResult LoopSetParam(CcuLoop loop, CcuVariable *formalParam, CcuVariable *actualParam) {
    if (formalParam == nullptr || actualParam == nullptr) {
        return CcuResult::CCU_E_PTR;
    }
    return CcuLoopSetParam(loop, formalParam->handle, actualParam->handle);
}

inline CcuResult LoopGroupCreate(CcuLoopGroup *group,
    const CcuLoopGroupConfig *config, CcuLoopExecutors enginePool)
{
    return CcuLoopGroupCreate(group, config, enginePool);
}

inline CcuResult LoopGroupCreateFromVar(CcuLoopGroup *group,
    CcuVariable *parallelVar, CcuVariable *offsetVar, CcuLoopExecutors enginePool)
{
    if (parallelVar == nullptr || offsetVar == nullptr) {
        return CcuResult::CCU_E_PTR;
    }
    return CcuLoopGroupCreateFromVar(group, parallelVar->handle, offsetVar->handle, enginePool);
}

inline CcuResult LoopGroupAddLoop(CcuLoopGroup group,
    CcuLoop loop, const CcuLoopConfig *config)
{
    return CcuLoopGroupAddLoop(group, loop, config);
}

inline CcuResult LoopGroupAddLoopFromVar(CcuLoopGroup group,
    CcuLoop loop, CcuVariable *loopParamVar)
{
    if (loopParamVar == nullptr) {
        return CcuResult::CCU_E_PTR;
    }
    return CcuLoopGroupAddLoopFromVar(group, loop, loopParamVar->handle);
}


} // namespace ccu

#endif // CCU_API_HPP