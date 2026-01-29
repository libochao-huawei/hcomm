/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: data slice header file
 * Author: zhangqingwei
 * Create: 2023-09-14
 */

#ifndef HCCLV2_DATA_SLICE_H
#define HCCLV2_DATA_SLICE_H

#include <sstream>
#include <memory>
#include "buffer_type.h"
#include "types/types.h"
#include "string_util.h"

namespace Hccl {

class DataSlice {
public:
    explicit DataSlice() : type(BufferType::INPUT), offset(0), size(0)
    {
    }

    explicit DataSlice(u64 size) : type(BufferType::INPUT), offset(0), size(size)
    {
    }

    DataSlice(BufferType bufferType, u64 offset, u64 size) : type(bufferType), offset(offset), size(size)
    {
    }

    bool operator==(const DataSlice &other) const
    {
        return (type == other.type && offset == other.offset && size == other.size);
    }

    bool operator!=(const DataSlice &other) const
    {
        return (type != other.type || offset != other.offset || size != other.size);
    }

    std::string Describe() const
    {
        return StringFormat("DataSlice[%s, offset=%s, size=%s]", type.Describe().c_str(), Dec2hex(offset).c_str(),
                            Dec2hex(size).c_str());
    }

    inline BufferType GetType() const
    {
        return type;
    }

    inline u64 GetOffset() const
    {
        return offset;
    }

    inline u64 GetSize() const
    {
        return size;
    }

    void SetBufferType(const BufferType bufferType)
    {
        type = bufferType;
    }

    void SetOffset(u64 off)
    {
        offset = off;
    }

private:
    BufferType type;
    u64        offset;
    u64        size;
};

inline bool DataSliceSizeIsEqual(std::unique_ptr<DataSlice> &a, std::unique_ptr<DataSlice> &b)
{
    return a->GetSize() == b->GetSize();
}

inline bool DataSliceSizeIsEqual(std::unique_ptr<DataSlice> &a, std::unique_ptr<DataSlice> &b,
                                 std::unique_ptr<DataSlice> &c)
{
    return (a->GetSize() == b->GetSize()) && (b->GetSize() == c->GetSize());
}

} // namespace Hccl
#endif