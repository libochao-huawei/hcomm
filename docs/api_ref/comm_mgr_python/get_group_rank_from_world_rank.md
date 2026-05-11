# get_group_rank_from_world_rank

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<cann-filter npu-type="310p">
- Atlas 推理系列产品：支持</cann-filter>
<cann-filter npu-type="910">
- Atlas 训练系列产品：支持</cann-filter>

> [!NOTE]说明
>
> - 针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，仅支持Atlas 800T A2 训练服务器、Atlas 900 A2 PoD 集群基础单元、Atlas 200T A2 Box16 异构子框。
<cann-filter npu-type="310p">
> - 针对Atlas 推理系列产品，仅支持Atlas 300I Duo 推理卡。</cann-filter>

## 功能说明

从world rank id，获取该进程在group中的group rank id。

## 函数原型

```python
def get_group_rank_from_world_rank(world_rank_id, group)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| world_rank_id | 输入 | int类型。<br>进程在"hccl_world_group"中的rank id。 |
| group | 输入 | String类型，最大长度为128字节，含结束符。<br>group名称，可以为用户自定义group或者"hccl_world_group"。 |

## 返回值

int类型，正常返回进程在group中的rank id。

## 约束说明

- 必须在集合通信初始化完成之后调用。
- 调用该接口的rank必须在当前接口入参group定义的范围内，不在此范围内的rank调用该接口会失败。
- [create_group](create_group.md)完成之后，调用此API转换world rank id到group rank id。

## 调用示例

```python
from hccl.manage.api import create_group
from hccl.manage.api import get_group_rank_from_world_rank
create_group("myGroup", 4, [0, 1, 2, 3])
groupRankId = get_group_rank_from_world_rank(8, "myGroup")
```
