/*
 * Copyright (c) 2024-2025 Huawei Device Co., Ltd.
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

#include "program_dump.h"
#include "method_data_accessor-inl.h"
#include "abc2program_log.h"

namespace panda::abc2program {

void PandasmProgramDumper::Dump(std::ostream &os, pandasm::Program &program)
{
    os << "# source binary: " << abc_file_.GetFilename() << "\n";
    os << ".language ";
    if (program.lang == panda::panda_file::SourceLang::ECMASCRIPT) {
        os << "ECMAScript";
    } else {
        os << "PandaAssembly";
    }
}

void PandasmProgramDumper::Dump(std::ostream &os, pandasm::Record &record)
{
    log::Unimplemented(__PRETTY_FUNCTION__);
}

void PandasmProgramDumper::Dump(std::ostream &os, pandasm::Function &function)
{
    log::Unimplemented(__PRETTY_FUNCTION__);
}

void PandasmProgramDumper::Dump(std::ostream &os, pandasm::Field &field)
{
    log::Unimplemented(__PRETTY_FUNCTION__);
}

std::string PandasmProgramDumper::SerializeLiteralArray(const pandasm::LiteralArray &lit_array) const
{
    if (lit_array.literals_.empty()) {
        return "";
    }
    std::stringstream ss;
    ss << "{ ";
    const auto &tag = lit_array.literals_[0].tag_;
    if (IsArray(tag)) {
        ss << LiteralTagToString(tag);
    }
    ss << lit_array.literals_.size();
    ss << " [ ";
    SerializeValues(lit_array, ss);
    ss << "]}";
    return ss.str();
}

void PandasmProgramDumper::GetLiteralArrayByOffset(pandasm::LiteralArray *lit_array,
                                                   panda_file::File::EntityId offset) const
{
    panda_file::LiteralDataAccessor lit_array_accessor(abc_file_, abc_file_.GetLiteralArraysId());
    lit_array_accessor.EnumerateLiteralVals(
        offset, [this, lit_array](
                    const panda_file::LiteralDataAccessor::LiteralValue &value,
                    const panda_file::LiteralTag &tag) {
            switch (tag) {
                case panda_file::LiteralTag::ARRAY_U1:
                    FillLiteralArrayData<bool>(lit_array, tag, value);
                    break;
                case panda_file::LiteralTag::ARRAY_I8:
                case panda_file::LiteralTag::ARRAY_U8:
                    FillLiteralArrayData<uint8_t>(lit_array, tag, value);
                    break;
                case panda_file::LiteralTag::ARRAY_I16:
                case panda_file::LiteralTag::ARRAY_U16:
                    FillLiteralArrayData<uint16_t>(lit_array, tag, value);
                    break;
                case panda_file::LiteralTag::ARRAY_I32:
                case panda_file::LiteralTag::ARRAY_U32:
                    FillLiteralArrayData<uint32_t>(lit_array, tag, value);
                    break;
                case panda_file::LiteralTag::ARRAY_I64:
                case panda_file::LiteralTag::ARRAY_U64:
                    FillLiteralArrayData<uint64_t>(lit_array, tag, value);
                    break;
                case panda_file::LiteralTag::ARRAY_F32:
                    FillLiteralArrayData<float>(lit_array, tag, value);
                    break;
                case panda_file::LiteralTag::ARRAY_F64:
                    FillLiteralArrayData<double>(lit_array, tag, value);
                    break;
                case panda_file::LiteralTag::ARRAY_STRING:
                    FillLiteralArrayData<uint32_t>(lit_array, tag, value);
                    break;
                default:
                    FillLiteralData(lit_array, value, tag);
                    break;
            }
        });
}

template <typename T>
void PandasmProgramDumper::FillLiteralArrayData(pandasm::LiteralArray *lit_array, const panda_file::LiteralTag &tag,
                                                const panda_file::LiteralDataAccessor::LiteralValue &value) const
{
    panda_file::File::EntityId id(std::get<uint32_t>(value));
    auto sp = abc_file_.GetSpanFromId(id);
    auto len = panda_file::helpers::Read<sizeof(uint32_t)>(&sp);
    if (tag != panda_file::LiteralTag::ARRAY_STRING) {
        for (size_t i = 0; i < len; i++) {
            pandasm::LiteralArray::Literal lit;
            lit.tag_ = tag;
            lit.value_ = bit_cast<T>(panda_file::helpers::Read<sizeof(T)>(&sp));
            lit_array->literals_.emplace_back(lit);
        }
        return;
    }
    for (size_t i = 0; i < len; i++) {
        auto str_id = panda_file::helpers::Read<sizeof(T)>(&sp);
        pandasm::LiteralArray::Literal lit;
        lit.tag_ = tag;
        lit.value_ = abc_string_table_.GetStringById(str_id);
        lit_array->literals_.emplace_back(lit);
    }
}

void PandasmProgramDumper::FillLiteralData(pandasm::LiteralArray *lit_array,
                                           const panda_file::LiteralDataAccessor::LiteralValue &value,
                                           const panda_file::LiteralTag &tag) const
{
    pandasm::LiteralArray::Literal lit;
    lit.tag_ = tag;
    switch (tag) {
        case panda_file::LiteralTag::BOOL:
            lit.value_ = std::get<bool>(value);
            break;
        case panda_file::LiteralTag::ACCESSOR:
        case panda_file::LiteralTag::NULLVALUE:
        case panda_file::LiteralTag::BUILTINTYPEINDEX:
            lit.value_ = std::get<uint8_t>(value);
            break;
        case panda_file::LiteralTag::METHODAFFILIATE:
            lit.value_ = std::get<uint16_t>(value);
            break;
        case panda_file::LiteralTag::LITERALBUFFERINDEX:
        case panda_file::LiteralTag::INTEGER:
            lit.value_ = std::get<uint32_t>(value);
            break;
        case panda_file::LiteralTag::DOUBLE:
            lit.value_ = std::get<double>(value);
            break;
        case panda_file::LiteralTag::STRING:
            lit.value_ = std::get<uint32_t>(value);
            break;
        case panda_file::LiteralTag::METHOD:
        case panda_file::LiteralTag::GETTER:
        case panda_file::LiteralTag::SETTER:
        case panda_file::LiteralTag::GENERATORMETHOD:
            lit.value_ = std::get<uint32_t>(value);
            break;
        case panda_file::LiteralTag::LITERALARRAY:
            lit.value_ = std::get<uint32_t>(value);
            break;
        case panda_file::LiteralTag::TAGVALUE:
            return;
        default:
            UNREACHABLE();
    }
    lit_array->literals_.emplace_back(lit);
}

void PandasmProgramDumper::FillLiteralData4Method(const panda_file::LiteralDataAccessor::LiteralValue &value,
                                                  pandasm::LiteralArray::Literal &lit) const
{
    panda_file::MethodDataAccessor mda(abc_file_, panda_file::File::EntityId(std::get<uint32_t>(value)));
    lit.value_ = abc_string_table_.GetStringById(mda.GetNameId());
}

void PandasmProgramDumper::FillLiteralData4LiteralArray(const panda_file::LiteralDataAccessor::LiteralValue &value,
                                                        pandasm::LiteralArray::Literal &lit) const
{
    std::stringstream ss;
    ss << "0x" << std::hex << std::get<uint32_t>(value);
    lit.value_ = ss.str();
}

LiteralTagToStringMap PandasmProgramDumper::literal_tag_to_string_map_ = {
    {panda_file::LiteralTag::BOOL, "u1"},
    {panda_file::LiteralTag::ARRAY_U1, "u1"},
    {panda_file::LiteralTag::ARRAY_U8, "u8"},
    {panda_file::LiteralTag::ARRAY_I8, "i8"},
    {panda_file::LiteralTag::ARRAY_U16, "u16"},
    {panda_file::LiteralTag::ARRAY_I16, "i16"},
    {panda_file::LiteralTag::ARRAY_U32, "u32"},
    {panda_file::LiteralTag::INTEGER, "i32"},
    {panda_file::LiteralTag::ARRAY_I32, "i32"},
    {panda_file::LiteralTag::ARRAY_U64, "u64"},
    {panda_file::LiteralTag::ARRAY_I64, "i64"},
    {panda_file::LiteralTag::ARRAY_F32, "f32"},
    {panda_file::LiteralTag::DOUBLE, "f64"},
    {panda_file::LiteralTag::ARRAY_F64, "f64"},
    {panda_file::LiteralTag::STRING, "string"},
    {panda_file::LiteralTag::ARRAY_STRING, "string"},
    {panda_file::LiteralTag::METHOD, "method"},
    {panda_file::LiteralTag::GETTER, "getter"},
    {panda_file::LiteralTag::SETTER, "setter"},
    {panda_file::LiteralTag::GENERATORMETHOD, "generator_method"},
    {panda_file::LiteralTag::ACCESSOR, "accessor"},
    {panda_file::LiteralTag::METHODAFFILIATE, "method_affiliate"},
    {panda_file::LiteralTag::NULLVALUE, "null_value"},
    {panda_file::LiteralTag::TAGVALUE, "tagvalue"},
    {panda_file::LiteralTag::LITERALBUFFERINDEX, "lit_index"},
    {panda_file::LiteralTag::LITERALARRAY, "lit_offset"},
    {panda_file::LiteralTag::BUILTINTYPEINDEX, "builtin_type"},
};

std::string PandasmProgramDumper::LiteralTagToString(const panda_file::LiteralTag &tag) const
{
    auto it = literal_tag_to_string_map_.find(tag);
    if (it != literal_tag_to_string_map_.end()) {
        return it->second;
    }
    UNREACHABLE();
    return "";
}

bool PandasmProgramDumper::IsArray(const panda_file::LiteralTag &tag)
{
    switch (tag) {
        case panda_file::LiteralTag::ARRAY_U1:
        case panda_file::LiteralTag::ARRAY_U8:
        case panda_file::LiteralTag::ARRAY_I8:
        case panda_file::LiteralTag::ARRAY_U16:
        case panda_file::LiteralTag::ARRAY_I16:
        case panda_file::LiteralTag::ARRAY_U32:
        case panda_file::LiteralTag::ARRAY_I32:
        case panda_file::LiteralTag::ARRAY_U64:
        case panda_file::LiteralTag::ARRAY_I64:
        case panda_file::LiteralTag::ARRAY_F32:
        case panda_file::LiteralTag::ARRAY_F64:
        case panda_file::LiteralTag::ARRAY_STRING:
            return true;
        default:
            return false;
    }
}

template <typename T>
void PandasmProgramDumper::SerializeValues(const pandasm::LiteralArray &lit_array, T &os) const
{
    switch (lit_array.literals_[0].tag_) {
        case panda_file::LiteralTag::ARRAY_U1:
            SerializeValues4ArrayU1(lit_array, os);
            break;
        case panda_file::LiteralTag::ARRAY_U8:
            SerializeValues4ArrayU8(lit_array, os);
            break;
        case panda_file::LiteralTag::ARRAY_I8:
            SerializeValues4ArrayI8(lit_array, os);
            break;
        case panda_file::LiteralTag::ARRAY_U16:
            SerializeValues4ArrayU16(lit_array, os);
            break;
        case panda_file::LiteralTag::ARRAY_I16:
            SerializeValues4ArrayI16(lit_array, os);
            break;
        case panda_file::LiteralTag::ARRAY_U32:
            SerializeValues4ArrayU32(lit_array, os);
            break;
        case panda_file::LiteralTag::ARRAY_I32:
            SerializeValues4ArrayI32(lit_array, os);
            break;
        case panda_file::LiteralTag::ARRAY_U64:
            SerializeValues4ArrayU64(lit_array, os);
            break;
        case panda_file::LiteralTag::ARRAY_I64:
            SerializeValues4ArrayI64(lit_array, os);
            break;
        case panda_file::LiteralTag::ARRAY_F32:
            SerializeValues4ArrayF32(lit_array, os);
            break;
        case panda_file::LiteralTag::ARRAY_F64:
            SerializeValues4ArrayF64(lit_array, os);
            break;
        case panda_file::LiteralTag::ARRAY_STRING:
            SerializeValues4ArrayString(lit_array, os);
            break;
        default:
            SerializeLiterals(lit_array, os);
    }
}

template <typename T>
void PandasmProgramDumper::SerializeValues4ArrayU1(const pandasm::LiteralArray &lit_array, T &os) const
{
    for (size_t i = 0; i < lit_array.literals_.size(); i++) {
        os << (std::get<bool>(lit_array.literals_[i].value_)) << " ";
    }
}

template <typename T>
void PandasmProgramDumper::SerializeValues4ArrayU8(const pandasm::LiteralArray &lit_array, T &os) const
{
    for (size_t i = 0; i < lit_array.literals_.size(); i++) {
        os << (static_cast<uint16_t>(std::get<uint8_t>(lit_array.literals_[i].value_))) << " ";
    }
}

template <typename T>
void PandasmProgramDumper::SerializeValues4ArrayI8(const pandasm::LiteralArray &lit_array, T &os) const
{
    for (size_t i = 0; i < lit_array.literals_.size(); i++) {
        os << (static_cast<int16_t>(bit_cast<int8_t>(std::get<uint8_t>(lit_array.literals_[i].value_)))) << " ";
    }
}

template <typename T>
void PandasmProgramDumper::SerializeValues4ArrayU16(const pandasm::LiteralArray &lit_array, T &os) const
{
    for (size_t i = 0; i < lit_array.literals_.size(); i++) {
        os << (std::get<uint16_t>(lit_array.literals_[i].value_)) << " ";
    }
}

template <typename T>
void PandasmProgramDumper::SerializeValues4ArrayI16(const pandasm::LiteralArray &lit_array, T &os) const
{
    for (size_t i = 0; i < lit_array.literals_.size(); i++) {
        os << (bit_cast<int16_t>(std::get<uint16_t>(lit_array.literals_[i].value_))) << " ";
    }
}

template <typename T>
void PandasmProgramDumper::SerializeValues4ArrayU32(const pandasm::LiteralArray &lit_array, T &os) const
{
    for (size_t i = 0; i < lit_array.literals_.size(); i++) {
        os << (std::get<uint32_t>(lit_array.literals_[i].value_)) << " ";
    }
}

template <typename T>
void PandasmProgramDumper::SerializeValues4ArrayI32(const pandasm::LiteralArray &lit_array, T &os) const
{
    for (size_t i = 0; i < lit_array.literals_.size(); i++) {
        os << (bit_cast<int32_t>(std::get<uint32_t>(lit_array.literals_[i].value_))) << " ";
    }
}

template <typename T>
void PandasmProgramDumper::SerializeValues4ArrayU64(const pandasm::LiteralArray &lit_array, T &os) const
{
    for (size_t i = 0; i < lit_array.literals_.size(); i++) {
        os << (std::get<uint64_t>(lit_array.literals_[i].value_)) << " ";
    }
}

template <typename T>
void PandasmProgramDumper::SerializeValues4ArrayI64(const pandasm::LiteralArray &lit_array, T &os) const
{
    for (size_t i = 0; i < lit_array.literals_.size(); i++) {
        os << (bit_cast<int64_t>(std::get<uint64_t>(lit_array.literals_[i].value_))) << " ";
    }
}

template <typename T>
void PandasmProgramDumper::SerializeValues4ArrayF32(const pandasm::LiteralArray &lit_array, T &os) const
{
    for (size_t i = 0; i < lit_array.literals_.size(); i++) {
        os << (std::get<float>(lit_array.literals_[i].value_)) << " ";
    }
}

template <typename T>
void PandasmProgramDumper::SerializeValues4ArrayF64(const pandasm::LiteralArray &lit_array, T &os) const
{
    for (size_t i = 0; i < lit_array.literals_.size(); i++) {
        os << std::get<double>(lit_array.literals_[i].value_) << " ";
    }
}

template <typename T>
void PandasmProgramDumper::SerializeValues4ArrayString(const pandasm::LiteralArray &lit_array, T &os) const
{
    for (size_t i = 0; i < lit_array.literals_.size(); i++) {
        os << "\"" << std::get<std::string>(lit_array.literals_[i].value_) << "\" ";
    }
}

template <typename T>
void PandasmProgramDumper::SerializeLiterals(const pandasm::LiteralArray &lit_array, T &os) const
{
    for (size_t i = 0; i < lit_array.literals_.size(); i++) {
        SerializeLiteralsAtIndex(lit_array, os, i);
        os << ", ";
    }
}

template <typename T>
void PandasmProgramDumper::SerializeLiteralsAtIndex(const pandasm::LiteralArray &lit_array, T &os, size_t i) const
{
    const auto &tag = lit_array.literals_[i].tag_;
    os << LiteralTagToString(tag) << ":";
    const auto &val = lit_array.literals_[i].value_;
    switch (tag) {
        case panda_file::LiteralTag::BOOL:
            os << (std::get<bool>(val));
            break;
        case panda_file::LiteralTag::LITERALBUFFERINDEX:
        case panda_file::LiteralTag::INTEGER:
            os << (bit_cast<int32_t>(std::get<uint32_t>(val)));
            break;
        case panda_file::LiteralTag::DOUBLE:
            os << (std::get<double>(val));
            break;
        case panda_file::LiteralTag::STRING:
            os << "\"" << (std::get<std::string>(val)) << "\"";
            break;
        case panda_file::LiteralTag::METHOD:
        case panda_file::LiteralTag::GETTER:
        case panda_file::LiteralTag::SETTER:
        case panda_file::LiteralTag::GENERATORMETHOD:
            os << (std::get<std::string>(val));
            break;
        case panda_file::LiteralTag::NULLVALUE:
        case panda_file::LiteralTag::ACCESSOR:
            os << (static_cast<int16_t>(bit_cast<int8_t>(std::get<uint8_t>(val))));
            break;
        case panda_file::LiteralTag::METHODAFFILIATE:
            os << (std::get<uint16_t>(val));
            break;
        case panda_file::LiteralTag::LITERALARRAY:
            os << (std::get<std::string>(val));
            break;
        case panda_file::LiteralTag::BUILTINTYPEINDEX:
            os << (static_cast<int16_t>(std::get<uint8_t>(val)));
            break;
        default:
            UNREACHABLE();
            break;
    }
}

} // namespace panda::abc2program