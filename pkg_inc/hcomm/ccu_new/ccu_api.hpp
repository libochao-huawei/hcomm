#ifndef CCU_API_HPP
#define CCU_API_HPP

#include "ccu_data_api.h"
#include "ccu_data_api_impl.h"
#include "ccu_loop_macro.h"

namespace ccu {

// ==================== 资源创建 ====================

inline CcuResult Create(CcuVariable* v)       { return CcuVariableCreate(v); }
inline CcuResult Create(CcuAddress* a)         { return CcuAddressCreate(a); }
inline CcuResult Create(CcuEvent* e)           { return CcuCompletedEventCreate(e); }
inline CcuResult Create(CcuBuffer* b)          { return CcuBufferCreate(b); }
inline CcuResult Create(CcuLocalAddr* la)      { return CcuLocalAddrCreate(la); }
inline CcuResult Create(CcuRemoteAddr* ra)     { return CcuRemoteAddrCreate(ra); }

inline CcuResult CreateFromChannel(ChannelHandle channel, uint32_t varIndex, CcuVariable *var) {
    return CcuVariableCreateFromChannel(channel, varIndex, var);
}

inline CcuResult BlockCreate(CcuBuffer* bufs, uint32_t count) {
    return CcuBlockBufferCreate(bufs, count);
}
inline CcuResult BlockCreate(CcuVariable* vars, uint32_t count) {
    return CcuContinuousVariableCreate(vars, count);
}
inline CcuResult BlockCreate(CcuEvent* events, uint32_t count) {
    return CcuBlockCompletedEventCreate(events, count);
}
// ==================== 事件 ====================

inline CcuResult Record(CcuEvent e)  { return CcuRecordEvent(e); }
inline CcuResult Wait(CcuEvent e)    { return CcuWaitEvent(e); }

// ==================== 加载 ====================
inline CcuResult LoadArg(CcuVariable v) {
    return CcuLoadArg(v);
}
inline CcuResult Load(uint64_t addr, CcuVariable v, uint32_t num) {
    return CcuLoad(addr, v, num);
}

// ==================== 本地拷贝（3 种重载） ====================

// LocalAddr → LocalAddr
inline CcuResult Copy(CcuLocalAddr dst, CcuLocalAddr src,
    CcuVariable len, CcuEvent event)
{
    return CcuLocalCopyHBMToHBM(dst, src, len, event);
}

// LocalAddr → Buffer
inline CcuResult Copy(CcuBuffer dst, CcuLocalAddr src,
    CcuVariable len, CcuEvent event)
{
    return CcuLocalCopyHBMToBuffer(dst, src, len, event);
}

// Buffer → LocalAddr
inline CcuResult Copy(CcuLocalAddr dst, CcuBuffer src,
    CcuVariable len, CcuEvent event)
{
    return CcuLocalCopyBufferToHBM(dst, src, len, event);
}

// ==================== 本地 Reduce ====================

inline CcuResult Reduce(CcuLocalAddr dst, CcuLocalAddr src,
    CcuVariable len, HcclDataType dataType,
    HcclReduceOp opType, CcuEvent event)
{
    return CcuLocalHBMReduce(dst, src, len, dataType, opType, event);
}

inline CcuResult Reduce(CcuBuffer* bufs, uint32_t count,
    HcclDataType dataType, HcclDataType outputDataType,
    HcclReduceOp opType, CcuVariable len, CcuEvent event)
{
    return CcuLocalBufferReduce(bufs, count, dataType, outputDataType, opType, len, event);
}

// ==================== 远端读（2 种重载） ====================

// LocalAddr ← RemoteAddr
inline CcuResult Read(ChannelHandle ch, CcuLocalAddr local, CcuRemoteAddr remote,
    CcuVariable len, CcuEvent event)
{
    return CcuReadHBMToHBM(ch, local, remote, len, event);
}

// Buffer ← RemoteAddr
inline CcuResult Read(ChannelHandle ch, CcuBuffer local, CcuRemoteAddr remote,
    CcuVariable len, CcuEvent event)
{
    return CcuReadHBMToBuffer(ch, local, remote, len, event);
}
// LocalAddr ← RemoteAddr Reduce (Reduce)
inline CcuResult ReadReduce(ChannelHandle ch, CcuLocalAddr local, CcuRemoteAddr remote,
    CcuVariable len, HcclDataType dataType,
    HcclReduceOp opType, CcuEvent event)
{
    return CcuReadHBMToHBMReduce(ch, local, remote, len, dataType, opType, event);
}

// ==================== 远端写（2 种重载） ====================

// LocalAddr → RemoteAddr
inline CcuResult Write(ChannelHandle ch, CcuRemoteAddr remote, CcuLocalAddr local, 
    CcuVariable len, CcuEvent event)
{
    return CcuWriteHBMToHBM(ch, remote, local, len, event);
}

// Buffer → RemoteAddr
inline CcuResult Write(ChannelHandle ch, CcuRemoteAddr remote, CcuBuffer local, 
    CcuVariable len, CcuEvent event)
{
    return CcuWriteBufferToHBM(ch, remote, local, len, event);
}
// LocalAddr → RemoteAddr Reduce (Reduce)
inline CcuResult WriteReduce(ChannelHandle ch, CcuRemoteAddr remote, CcuLocalAddr local,
    CcuVariable len, HcclDataType dataType,
    HcclReduceOp opType, CcuEvent event)
{
    return CcuWriteHBMToHBMReduce(ch, remote, local, len, dataType, opType, event);
}
inline CcuResult WriteVariableWithNotify(ChannelHandle channel, CcuVariable var,uint32_t remoteVarIdx, uint32_t remoteNotifyIdx, uint32_t mask)
{
    return CcuWriteVariableWithNotify(channel, var, remoteVarIdx, remoteNotifyIdx, mask);
}
inline CcuResult WriteNotify(ChannelHandle channel, uint32_t remoteNotifyIdx, uint32_t mask)
{
    return CcuWriteNotify(channel, remoteNotifyIdx, mask);
}
inline CcuResult NotifyWait(ChannelHandle channel, uint32_t localNotifyIdx, uint32_t mask)
{
    return CcuNotifyWait(channel, localNotifyIdx, mask);
}
// ==================== Loop ====================

inline CcuResult CreateBlockExecutor(CcuLoopExecutors *pool, uint32_t count) {
    return CcuCreateBlockExecutorImpl(pool, count);
}

inline CcuResult LoopSetParam(CcuLoop loop, CcuVariable *formalParam, CcuVariable *actualParam) {
    if (formalParam == nullptr || actualParam == nullptr) {
        return CcuResult::CCU_E_PTR;
    }
    return CcuLoopSetParamImpl(loop, formalParam->handle, actualParam->handle);
}

inline CcuResult LoopGroupCreate(CcuLoopGroup *group,
    const CcuLoopGroupConfig *config, CcuLoopExecutors enginePool)
{
    return CcuLoopGroupCreateImpl(group, config, enginePool);
}

inline CcuResult LoopGroupCreateFromVar(CcuLoopGroup *group,
    CcuVariable *parallelVar, CcuVariable *offsetVar, CcuLoopExecutors enginePool)
{
    if (parallelVar == nullptr || offsetVar == nullptr) {
        return CcuResult::CCU_E_PTR;
    }
    return CcuLoopGroupCreateFromVarImpl(group, parallelVar->handle, offsetVar->handle, enginePool);
}

inline CcuResult LoopGroupAddLoop(CcuLoopGroup group,
    CcuLoop loop, const CcuLoopConfig *config)
{
    return CcuLoopGroupAddLoopImpl(group, loop, config);
}

inline CcuResult LoopGroupAddLoopFromVar(CcuLoopGroup group,
    CcuLoop loop, CcuVariable *loopParamVar)
{
    if (loopParamVar == nullptr) {
        return CcuResult::CCU_E_PTR;
    }
    return CcuLoopGroupAddLoopFromVarImpl(group, loop, loopParamVar->handle);
}

} // namespace ccu

#endif // CCU_API_HPP