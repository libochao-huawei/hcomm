/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef VECTOR_QUEUE_H
#define VECTOR_QUEUE_H

#include "queue.h"
#include <vector>
#include <algorithm>

namespace Hccl {

template <typename T> class VectorQueue : public Queue<T> {
private:
    std::vector<T> elems_;

public:
    VectorQueue() {
        elems_.reserve(2048);
    }
    class Iterator : public Queue<T>::Iterator {
    private:
        VectorQueue *queue_{nullptr};

    protected:
        void check() override
        {
            if ((this->it_) < queue_->elems_.begin() || (this->it_) > queue_->elems_.end()) {
                THROW<InternalException>(StringFormat("VectorQueue<T>::Iterator out of range"));
            }
        }

    public:
        Iterator(typename std::vector<T>::iterator it, VectorQueue *queue) : Queue<T>::Iterator(it), queue_(queue)
        {
            check();
        }

        ~Iterator() override = default;

        typename Queue<T>::Iterator &operator++() override
        {
            (this->it_)++;
            check();
            return *this;
        }

        typename Queue<T>::Iterator operator++(int) override
        {
            Iterator temp = *this;
            (this->it_)++;
            check();
            return temp;
        }

        typename Queue<T>::Iterator &operator--() override
        {
            (this->it_)--;
            check();
            return *this;
        }

        typename Queue<T>::Iterator operator--(int) override
        {
            Iterator temp = *this;
            (this->it_)--;
            check();
            return temp;
        }
    };

    void Append(const T &value) override
    {
        elems_.push_back(value);
    }

    void Traverse(std::function<void(const T &)> action) override
    {
        for (const auto &elem : elems_) {
            action(elem);
        }
    }

    size_t Size() const override
    {
        return elems_.size();
    }

    bool IsEmpty() const override
    {
        return elems_.empty();
    }

    bool IsFull() const override
    {
        return false;
    }

    size_t Capacity() const override
    {
        return elems_.capacity();
    }

    std::shared_ptr<typename Queue<T>::Iterator> Find(std::function<bool(const T &)> cond) override
    {
        auto it = std::find_if(elems_.begin(), elems_.end(), cond);
        if (it != elems_.end()) {
            return std::make_shared<Iterator>(it, this);
        }
        return std::make_shared<Iterator>(elems_.end(), this);
    }

    std::shared_ptr<typename Queue<T>::Iterator> Begin() override
    {
        return std::make_shared<Iterator>(elems_.begin(), this);
    }

    std::shared_ptr<typename Queue<T>::Iterator> Tail() override
    {
        return std::make_shared<Iterator>(elems_.begin() + elems_.size() - 1, this);
    }

    std::shared_ptr<typename Queue<T>::Iterator> End() override
    {
        return std::make_shared<Iterator>(elems_.end(), this);
    }
};

} // namespace Hccl
#endif // VECTOR_QUEUE_H