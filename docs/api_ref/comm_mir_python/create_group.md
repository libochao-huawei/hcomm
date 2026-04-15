# create_group

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
- Atlas 推理系列产品：支持
- Atlas 训练系列产品：支持

> [!CAUTION]注意
> 针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，仅支持Atlas 800T A2 训练服务器、Atlas 900 A2 PoD 集群基础单元、Atlas 200T A2 Box16 异构子框。
> 针对Atlas 推理系列产品，仅支持Atlas 300I Duo 推理卡。

## 功能说明

创建集合通信用户自定义group。

如果开发者不调用此接口创建用户自定义group，则默认将所有参与集群训练的设备创建为全局的hccl_world_group。

group为参与集合通信的进程组，其中：

- hccl_world_group：默认的全局group，包含所有参与集合通信的rank，由HCCL自动创建。
- 自定义group：hccl_world_group包含的进程组的子集。

## 函数原型

```python
def create_group(group, rank_num, rank_ids)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| group | 输入 | String类型，最大长度为128字节，含结束符。<br>group名称，集合通信group的标识，不能为默认全局group名字“hccl_world_group”，如果用户传入的group名字是“hccl_world_group”，会创建失败。 |
| rank_num | 输入 | int类型。<br>组成该group的rank数量。<br>最大值为32768。 |
| rank_ids | 输入 | list类型。<br>组成该group的world_rank_id列表。<br>在不同单板类型上，有不同的限制。<br>针对Atlas 训练系列产品：<br>  - 对于Server单机场景，rank_ids需满足如下条件：rank数量必须为1/2/4/8，0-3卡与4-7卡各为一个组网，rank数量为2/4时要求选取的AI处理器同属一个cluster。<br>  - 对于Server集群场景，rank_ids满足如下条件：各Server要选取相同数量的rank（且数量要求为1/2/4/8）。各Server选取rank数量为2/4时要求选取的AI处理器同属一个cluster（即rank id按8取模余数都小于4或都大于等于4）。<br>举例：<br>假设对三台Server创建group，三台Server的rank id分别为：<br>{0,1,2,3,4,5,6,7}<br>{8,9,10,11,12,13,14,15}<br>{16,17,18,19,20,21,22,23}<br>则满足要求的rank_ids列表可以是：<br>rank_ids=[1,9,17]<br>rank_ids=[1,2,9,10,17,18]<br>rank_ids=[4,5,6,7,12,13,14,15,20,21,22,23]<br>  - 各Server要选取相同数量的rank（且数量要求为1/2/4/8）。<br>  - 各Server选取rank数量为2/4时要求选取的AI处理器同属一个cluster（即rank id按8取模余数都小于4或都大于等于4）。<br><br><br>针对Atlas A2 训练系列产品/Atlas A2 推理系列产品：<br>  - 对于Server单机场景，rank_ids无限制条件。<br>  - 对于Server集群场景，rank_ids需满足如下条件：建议各Server要选取相同数量的rank（数量大小无要求），且各Server选取的rank对应位置要相等（即rank id按8取模相等）。若各Server选取的rank数量不同，会造成性能劣化。<br>举例：<br>假设对三台Server创建group，三台Server的rank id分别为：<br>{0,1,2,3,4,5,6,7}<br>{8,9,10,11,12,13,14,15}<br>{16,17,18,19,20,21,22,23}<br>则满足要求的rank_ids列表可以是：<br>rank_ids=[1,9,17]<br>rank_ids=[1,2,9,10,17,18]<br>rank_ids=[4,5,6,7,12,13,14,15,20,21,22,23]<br><br><br>针对Atlas A3 训练系列产品/Atlas A3 推理系列产品：建议每个超节点中的Server数量一致，每个Server中的rank数量一致，若不一致，会造成性能劣化。<br>针对Atlas 300I Duo 推理卡：仅支持Server单机场景，rank_ids无限制条件。<br>补充说明：<br>建议rank_ids按照Device物理连接顺序进行排序，即将物理连接上较近的device编排在一起。例如，若device_ip按照物理连接从小到大设置，则rank_ids也建议按照从小到大的顺序设置。 |

## 返回值

无。

## 约束说明

- 必须在集合通信初始化完成之后调用。
- 调用该接口的rank必须在当前接口入参group定义的范围内，不在此范围内的rank调用该接口会失败。

## 调用示例

```python
from hccl.manage.api import create_group
create_group("myGroup", 4, [0, 1, 2, 3])
```
