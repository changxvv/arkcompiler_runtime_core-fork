/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef LIBLLVMBACKEND_OBJECT_CODE_CREATED_OBJECT_FILE_H
#define LIBLLVMBACKEND_OBJECT_CODE_CREATED_OBJECT_FILE_H

#include <llvm/Object/ObjectFile.h>

#include <map>
#include <unordered_map>

namespace panda::llvmbackend {

class SectionReference {
public:
    SectionReference() = default;

    explicit SectionReference(const uint8_t *memory, uintptr_t size, std::string name, size_t alignment)
        : memory_(memory), size_(size), name_(std::move(name)), alignment_(alignment)
    {
    }

    const uint8_t *GetMemory() const
    {
        return memory_;
    }
    uintptr_t GetSize() const
    {
        return size_;
    }
    const std::string &GetName() const
    {
        return name_;
    }

    size_t GetAlignment() const
    {
        return alignment_;
    }

    std::vector<uint8_t> ContentToVector();

private:
    const uint8_t *memory_ {nullptr};
    uintptr_t size_ {0};
    std::string name_;
    size_t alignment_ {0};
};

class CreatedObjectFile {
public:
    using ObjectFilePostProcessor = std::function<void(llvm::object::ObjectFile *)>;

    struct StackMapSymbol {
        uint64_t idx;
        uint64_t section_offset;
    };

    static llvm::Expected<std::unique_ptr<CreatedObjectFile>> CopyOf(
        llvm::MemoryBufferRef object_file_buffer,
        const ObjectFilePostProcessor &object_file_post_processor = [](llvm::object::ObjectFile *) -> void {});

    explicit CreatedObjectFile(std::unique_ptr<llvm::MemoryBuffer> object_file_buffer,
                               std::unique_ptr<llvm::object::ObjectFile> object_file);

    llvm::object::ObjectFile *GetObjectFile() const;

    bool HasSection(const std::string &name) const;

    SectionReference GetSection(const std::string &name) const;

    SectionReference GetSectionByFunctionName(const std::string &full_function_name) const;

    void WriteTo(std::string_view output) const;

    size_t Size() const;

private:
    std::unique_ptr<llvm::MemoryBuffer> buffer_;
    std::unique_ptr<llvm::object::ObjectFile> object_file_;
    std::map<llvm::StringRef, llvm::object::SectionRef> section_index_;
};

}  // namespace panda::llvmbackend
#endif  // LIBLLVMBACKEND_OBJECT_CODE_CREATED_OBJECT_FILE_H