import numpy as np
import tensorflow as tf
from hccl.split.api import *
from hccl.manage.api import *
from npu_bridge.estimator.npu.util import *
from npu_bridge.hccl import hccl_ops
from npu_bridge.estimator import npu_ops
from npu_bridge.estimator.npu import util


def hccl_operator(rank_id, group, data):
    tensors = {}
    send_data = np.full(data, rank_id + 2, dtype=np.float32)
    allreduce_sum_output = hccl_ops.allreduce(send_data, "sum", group=group)
    tensors["allreduce_sum_output"] = allreduce_sum_output

    return tensors


def main():
    # session参数配置
    hccl_session_config = tf.ConfigProto()
    custom_op = (
        hccl_session_config.graph_options.rewrite_options.custom_optimizers.add()
    )
    custom_op.name = "NpuOptimizer"
    custom_op.parameter_map["use_off_line"].b = True
    custom_op.parameter_map["iterations_per_loop"].i = 1

    npu_init = npu_ops.initialize_system()
    npu_shutdown = npu_ops.shutdown_system()

    # 创建session
    with tf.Session(config=hccl_session_config) as sess:
        # 进行集合通信初始化
        sess.run(npu_init)

        # 获取device在group中对应的rank序号
        rank_id = get_rank_id()
        try:
            # 调用HCCL接口,下发集合通信算子
            tensors = hccl_operator(rank_id, "hccl_world_group", 8)

            # tf框架全局变量初始化
            init_var = tf.compat.v1.global_variables_initializer()
            sess.run(init_var)

            # 设置迭代次数,执行训练
            train_op = tf.group(tensors)
            iter_op = util.set_iteration_per_loop(sess, train_op, 1)
            _, v = sess.run([iter_op, tensors])

            # 开启日志记录
            tf.compat.v1.logging.info(v)

            # 关闭session
            sess.run(npu_shutdown)

        except Exception as e:
            print("ERROR : %s" % e)
            print("device:%s tensorflow hccl test fail" % rank_id)
        else:
            print("device:%s tensorflow hccl test success" % rank_id)


if __name__ == "__main__":
    tf.compat.v1.logging.set_verbosity(tf.logging.INFO)
    main()
