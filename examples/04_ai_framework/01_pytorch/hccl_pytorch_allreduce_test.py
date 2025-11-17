import torch
import torch_npu
import torch.distributed as dist
import torch.multiprocessing as mp


def run_hccl(rank: int, world_size: int, master_ip: str, master_port: int):
    # 指定当前进程使用的 NPU 设备
    torch_npu.npu.set_device(rank)

    # 初始化进程组，后端使用 HCCL
    init_method = f"tcp://{master_ip}:{master_port}"
    print("[Rank %d] Listening on %s" % (rank, init_method))
    dist.init_process_group(
        backend="hccl", rank=rank, world_size=world_size, init_method=init_method
    )

    # 构造输入数据，1行8列，值为当前rank
    torch_tensor = torch.full((1, world_size), rank, dtype=torch.float32, device="npu")
    print("[Rank %d] Input tensor: %s" % (rank, torch_tensor))

    try:
        # 调用 HCCL 接口，下发 AllReduce 集合通信算子
        dist.all_reduce(torch_tensor, op=dist.ReduceOp.SUM)
    except Exception as e:
        print("[Rank %d] Error occurred: %s", e)
    else:
        print("[Rank %d] Output tensor: %s" % (rank, torch_tensor))


def main():
    print("Executing AllReduce collective operation via HCCL backend")
    rank_size = torch_npu.npu.device_count()
    print("Available NPU count: %d" % rank_size)

    # 启动多进程
    mp.spawn(
        run_hccl, args=(rank_size, "127.0.0.1", 50001), nprocs=rank_size, join=True
    )


if __name__ == "__main__":
    main()
