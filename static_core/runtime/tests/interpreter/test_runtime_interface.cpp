/**
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "runtime/tests/interpreter/test_runtime_interface.h"

namespace panda::interpreter::test {

RuntimeInterface::NullPointerExceptionData RuntimeInterface::npe_data_;

RuntimeInterface::ArrayIndexOutOfBoundsExceptionData RuntimeInterface::array_oob_exception_data_;

RuntimeInterface::NegativeArraySizeExceptionData RuntimeInterface::array_neg_size_exception_data_;

RuntimeInterface::ArithmeticException RuntimeInterface::arithmetic_exception_data_;

RuntimeInterface::ClassCastExceptionData RuntimeInterface::class_cast_exception_data_;

RuntimeInterface::AbstractMethodError RuntimeInterface::abstract_method_error_data_;

RuntimeInterface::ArrayStoreExceptionData RuntimeInterface::array_store_exception_data_;

coretypes::Array *RuntimeInterface::array_object_;

Class *RuntimeInterface::array_class_;

uint32_t RuntimeInterface::array_length_;

Class *RuntimeInterface::resolved_class_;

ObjectHeader *RuntimeInterface::object_;

Class *RuntimeInterface::object_class_;

uint32_t RuntimeInterface::catch_block_pc_offset_;

// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
RuntimeInterface::InvokeMethodHandler RuntimeInterface::invoke_handler_;

DummyGC::DummyGC(panda::mem::ObjectAllocatorBase *object_allocator, const panda::mem::GCSettings &settings)
    : GC(object_allocator, settings)
{
}

// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
DummyGC RuntimeInterface::dummy_gc_(nullptr, panda::mem::GCSettings());

Method *RuntimeInterface::resolved_method_;

Field *RuntimeInterface::resolved_field_;

const void *RuntimeInterface::entry_point_;

uint32_t RuntimeInterface::jit_threshold_;

}  // namespace panda::interpreter::test
