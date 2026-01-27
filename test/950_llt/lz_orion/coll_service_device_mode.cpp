void CollServiceDeviceMode::LoadWithOpBasedMode(CollOperator &op, std::unique_ptr<Stream> stream)
{
    HCCL_INFO("[CollServiceDeviceMode::%s] start.", __func__);
    // AIV aclgrah 流程
    if (comm->GetOpExecuteConfig().accState == AcceleratorState::AIV) {
        HandleAclGraphFirstOpAivBuff(stream->GetPtr());
    }
 
    // 入参buffer和stream注册
    RegisterOpBufToBufMgr(op);
 
    RegisterOpbasedStream(std::move(stream));
 
    if(comm->GetOpExecuteConfig().accState == AcceleratorState::AIV){
        auto  insQueue = make_shared<InsQueue>();
 
        AivOpCacheArgs opCacheParam{comm->GetCurAlgName(), op.dataCount, op.dataType, op.opType, op.reduceOp, op.root, op.blockDimLimit, op.outputDataType,{},{}};
        if(op.opType == OpType::ALLTOALL){
            opCacheParam.all2allDataDes = {op.all2AllDataDes.sendType, op.all2AllDataDes.recvType, op.all2AllDataDes.sendCount, op.all2AllDataDes.recvCount};
        } 
        if(op.opType == OpType::ALLTOALLV){
            opCacheParam.all2allVDataDes = {op.all2AllVDataDes.sendType, op.all2AllVDataDes.recvType, op.all2AllVDataDes.sendCounts, op.all2AllVDataDes.recvCounts,
                                              op.all2AllVDataDes.sdispls,op.all2AllVDataDes.rdispls};
        }
        auto it = comm->hcclCacheMap_.find(opCacheParam);
        bool isCache = false;
        if (it != comm->hcclCacheMap_.end()) {
            isCache = true;
            insQueue = it->second;
        }  else{
            // 算法编排返回insQueue, 包含ccu扩展指令和aicpu扩展指令
            insQueue = Orchestrate(op);
        }
        AllocQueueNotify(*insQueue);
        // 日志打印    
        if (comm->GetAivTag() == 1) {
            std::vector<LinkData> uniqueLinks = GetFullMeshLinks();
            comm->SetCommStatus(CommStatus::COMM_BUILDING);
            // Socket建链
            comm->GetSocketManager().BatchCreateSockets(uniqueLinks);
            aivInsPreprocessor.Preprocess(insQueue);
        }
        // translate
        SaveMirrorDfxOpInfo();
        Interpreter interpreter(*comm);
        interpreter.Submit(*insQueue);
        if(!isCache){
            comm->GetCacheMap(opCacheParam, insQueue);
        }
    } else {
        // 用于aicpu专用流
        comm->GetAicpuStreamManager().AllocFreeStream();
        // 算法编排返回insQueue, 包含ccu扩展指令和aicpu扩展指令
        shared_ptr<InsQueue> insQueue = Orchestrate(op);
        AllocQueueNotify(*insQueue);
        // 日志打印
        auto info
            = StringFormat("Entry-Hccl(opType[%s]_opBaseOpIndex[%u]): group[%s], AlgName[%s]", op.opType.Describe().c_str(),
                        comm->GetOpBaseOpIndex(), comm->GetId().c_str(), comm->GetCurAlgName().c_str());
        comm->GetTrace().Save(info);
        // 获取insQueue中所有Ins的linkDats
        std::vector<LinkData> uniqueLinks = GetUniqueLinks(insQueue);
        // 将通讯域设置为transport建链中状态
        comm->SetCommStatus(CommStatus::COMM_BUILDING);
 
        // Socket建链
        comm->GetSocketManager().BatchCreateSockets(uniqueLinks);
 
        // 对insQueue中ccuIns进行预处理(transport建链和交换, 资源申请、注册等)
        ccuInsPreprocessor.Preprocess(insQueue);
 
        if (ccuInsPreprocessor.IsRollback()) { // 如果是回退，流程退出
            return;
        } 
        SaveMirrorDfxOpInfo();
            // translate
        Interpreter interpreter(*comm);
        interpreter.Submit(*insQueue);
    } 
    HCCL_INFO("[CollServiceDeviceMode::%s] end.", __func__);
}