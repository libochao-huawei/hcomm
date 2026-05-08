#ifndef CCU_API_HPP
#define CCU_API_HPP

#include "ccu_data_api_impl.h"
#include "ccu_loop_macro.h"
#include "ccu_control_flow_macro.h"
#include "ccu_log.h"
#include "hcom_common.h"

#include "ccu_variable.hpp"
#include "ccu_address.hpp"
#include "ccu_event.hpp"
#include "ccu_buffer.hpp"
#include "ccu_local_addr.hpp"
#include "ccu_remote_addr.hpp"

namespace ccu {

// ==================== 类型别名 ====================

using Loop           = ::CcuLoop;
using LoopGroup      = ::CcuLoopGroup;
using LoopExecutors  = ::CcuLoopExecutors;
using LoopConfig     = ::CcuLoopConfig;
using LoopGroupConfig = ::CcuLoopGroupConfig;

// ==================== 资源创建 ====================

inline CcuResult Alloc(Variable* v)       { return CcuVariableAlloc(&(v->handle)); }
inline CcuResult Alloc(Address* a)         { return CcuAddressAlloc(&(a->handle)); }
inline CcuResult Alloc(Event* e)           { return CcuEventAlloc(&(e->handle)); }
inline CcuResult Alloc(Buffer* b)          { return CcuBufferAlloc(&(b->handle)); }
inline CcuResult Alloc(LocalAddr* la)      { return CcuLocalAddrAlloc(&(la->handle),&(la->addr.handle),&(la->token.handle)); }
inline CcuResult Alloc(RemoteAddr* ra)     { return CcuRemoteAddrAlloc(&(ra->handle),&(ra->addr.handle),&(ra->token.handle)); }

inline CcuResult BlockAlloc(Buffer* bufs, uint32_t count) {
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
inline CcuResult BlockAlloc(Variable* vars, uint32_t count) {
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
inline CcuResult BlockAlloc(Event* events, uint32_t count) {
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
inline CcuResult CreateByChannel(ChannelHandle channel, uint32_t varIndex, Variable* var) { return CcuVariableCreateByChannel(channel, varIndex, &(var->handle)); }


// ==================== 事件 ====================
inline CcuResult EventRecord(Event e)  { return CcuEventRecord(e.handle); }
inline CcuResult EventWait(Event e)    { return CcuEventWait(e.handle); }
inline CcuResult SetMask(Event e, uint32_t mask=1) { return CcuSetMask(e.handle, mask); }
inline CcuResult NotifyRecord(ChannelHandle channel, uint32_t remoteNotifyIdx, uint32_t mask=1){ return CcuNotifyRecord(channel, remoteNotifyIdx, mask); }
inline CcuResult NotifyWait(ChannelHandle channel, uint32_t localNotifyIdx, uint32_t mask=1){ return CcuNotifyWait(channel, localNotifyIdx, mask); }
inline CcuResult WriteVariableWithNotify(ChannelHandle channel, Variable var,uint32_t remoteVarIdx, uint32_t remoteNotifyIdx, uint32_t mask=1){ return CcuWriteVariableWithNotify(channel, var.handle, remoteVarIdx, remoteNotifyIdx, mask); }

// ==================== 加载 ====================
inline CcuResult LoadArg(Variable v, uint32_t argId) {
    return CcuLoadArg(v.handle, argId);
}
inline CcuResult LoadVar(uint64_t addr, Variable* v, uint32_t num) {
    return CcuLoadVar(addr, v[0].handle, num);
}
inline CcuResult StoreVar(uint64_t addr, Variable* v, uint32_t num) {
    return CcuStoreVar(addr, v[0].handle, num);
}
// ==================== 本地拷贝（3 种重载） ====================

// LocalAddr → LocalAddr,LocalAddr → Buffer,Buffer → LocalAddr
inline CcuResult LocalCopy(LocalAddr dst, LocalAddr src,Variable len, Event event) { return CcuLocalCopyMemToMem(dst.handle, src.handle, len.handle, event.handle); }
inline CcuResult LocalCopy(Buffer dst, LocalAddr src, Variable len, Event event) { return CcuLocalCopyMemToBuffer(dst.handle, src.handle, len.handle, event.handle); }
inline CcuResult LocalCopy(LocalAddr dst, Buffer src,Variable len, Event event) { return CcuLocalCopyBufferToMem(dst.handle, src.handle, len.handle, event.handle); }

// ==================== 本地 Reduce ====================
inline CcuResult LocalReduce(LocalAddr dst, LocalAddr src,Variable len, HcclDataType dataType, HcclReduceOp opType, Event event) { return CcuLocalMemReduce(dst.handle, src.handle, len.handle, dataType, opType, event.handle); }
inline CcuResult LocalReduce(Buffer* buffers, uint32_t count, HcclDataType dataType, HcclDataType outputDataType, HcclReduceOp opType, Variable len, Event event) 
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
inline CcuResult Read(ChannelHandle ch, LocalAddr local, RemoteAddr remote, Variable len, Event event) { return CcuReadMemToMem(ch, local.handle, remote.handle, len.handle, event.handle); }
// 远端读 Buffer ← RemoteAddr
inline CcuResult Read(ChannelHandle ch, Buffer local, RemoteAddr remote, Variable len, Event event) { return CcuReadMemToBuffer(ch, local.handle, remote.handle, len.handle, event.handle); }
// 远端读 LocalAddr ← RemoteAddr Reduce (Reduce)
inline CcuResult ReadReduce(ChannelHandle ch, LocalAddr local, RemoteAddr remote, Variable len, HcclDataType dataType, HcclReduceOp opType, Event event) { return CcuReadMemToMemReduce(ch, local.handle, remote.handle, len.handle, dataType, opType, event.handle); }

// ==================== 远端写 ====================

// LocalAddr → RemoteAddr
inline CcuResult Write(ChannelHandle ch, RemoteAddr remote, LocalAddr local,  Variable len, Event event){ return CcuWriteMemToMem(ch, remote.handle, local.handle, len.handle, event.handle); }
// Buffer → RemoteAddr
inline CcuResult Write(ChannelHandle ch, RemoteAddr remote, Buffer local, Variable len, Event event) { return CcuWriteBufferToMem(ch, remote.handle, local.handle, len.handle, event.handle); }
// LocalAddr → RemoteAddr Reduce (Reduce)
inline CcuResult WriteReduce(ChannelHandle ch, RemoteAddr remote, LocalAddr local, Variable len, HcclDataType dataType, HcclReduceOp opType, Event event){ return CcuWriteMemToMemReduce(ch, remote.handle, local.handle, len.handle, dataType, opType, event.handle);}

// ==================== Loop ====================

inline CcuResult CreateLoopExecutor(LoopExecutors *pool, uint32_t count) {
    return CcuCreateBlockExecutor(pool, count);
}

inline CcuResult LoopSetParam(Loop loop, Variable *formalParam, Variable *actualParam) {
    if (formalParam == nullptr || actualParam == nullptr) {
        return CcuResult::CCU_E_PTR;
    }
    return CcuLoopSetParam(loop, formalParam->handle, actualParam->handle);
}

inline CcuResult CreateLoopGroup(LoopGroup *group,
    const LoopGroupConfig *config, LoopExecutors enginePool)
{
    return CcuLoopGroupCreate(group, config, enginePool);
}

inline CcuResult CreateLoopGroup(LoopGroup *group,
    Variable *parallelVar, Variable *offsetVar, LoopExecutors enginePool)
{
    if (parallelVar == nullptr || offsetVar == nullptr) {
        return CcuResult::CCU_E_PTR;
    }
    return CcuLoopGroupCreateFromVar(group, parallelVar->handle, offsetVar->handle, enginePool);
}

inline CcuResult AddLoop(LoopGroup group,
    Loop loop, const LoopConfig *config)
{
    return CcuLoopGroupAddLoop(group, loop, config);
}

inline CcuResult AddLoop(LoopGroup group,
    Loop loop, Variable *loopParamVar)
{
    if (loopParamVar == nullptr) {
        return CcuResult::CCU_E_PTR;
    }
    return CcuLoopGroupAddLoopFromVar(group, loop, loopParamVar->handle);
}

} // namespace ccu

#endif // CCU_API_HPP