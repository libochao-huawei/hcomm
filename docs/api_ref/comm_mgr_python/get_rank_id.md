# get_rank_id

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
- Atlas 推理系列产品：支持
- Atlas 训练系列产品：支持

> [!NOTE]说明
> 针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，仅支持Atlas 800T A2 训练服务器、Atlas 900 A2 PoD 集群基础单元、Atlas 200T A2 Box16 异构子框。
> 针对Atlas 推理系列产品，仅支持Atlas 300I Duo 推理卡。

## 功能说明

获取device在group中对应的rank序号。

## 函数原型

```python
def get_rank_id(group="hccl_world_group")
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| group | 输入 | String类型，最大长度为128字节，含结束符。<br>group名称，如果用户不配置此参数，默认为“hccl_world_group”。 |

## 返回值

int类型，返回device所在group的rank id。

## 约束说明

- 必须在集合通信初始化完成之后调用。
- 调用该接口的rank必须在当前接口入参group定义的范围内，不在此范围内的rank调用该接口会失败。
- [create_group](create_group.md)完成之后，调用此API获取进程在group中的rank id。
- 如果传入"hccl_world_group"，返回进程在hccl_world_group的rank id。

## 调用示例

```python
from hccl.manage.api import create_group
from hccl.manage.api import get_rank_id
create_group("myGroup", 4, [0, 1, 2, 3])
rankId = get_rank_id("myGroup")
```
