CURRENT_DIR=$(dirname $(readlink -f ${BASH_SOURCE[0]}))
BUILD_DIR=${CURRENT_DIR}/../build/
for dir in ${CURRENT_DIR}/*/*/;do
    # 检查是否是需要跳过的目录
    if [ "$dir" = "${CURRENT_DIR}/03_collectives/09_scatter/" ]; then
        echo "Skipping directory: $dir" | tee -a ${BUILD_DIR}/build.log
        continue
    fi

    # 进入子目录
    if ! cd "$dir"; then
        echo "Failed to enter directory: $dir" | tee -a ${BUILD_DIR}/build.log
        continue
    fi

    # 检查是否存在Makefile
    if [ -f Makefile ]; then
        echo "Processing directory: $dir" | tee -a ${BUILD_DIR}/build.log
        # 执行make和make test，并将输出记录到build.log
        make ${JOB_NUM} | tee -a ${BUILD_DIR}/build.log
        make test | tee -a ${BUILD_DIR}/build.log
    else
        echo "No Makefile found in directory: $dir" | tee -a ${BUILD_DIR}/build.log
    fi
    
    # 返回上级目录
    cd ..
done