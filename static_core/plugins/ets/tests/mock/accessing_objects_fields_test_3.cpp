/**
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License"
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

#include <gtest/gtest.h>

#include "plugins/ets/tests/mock/accessing_objects_fields_test_helper.h"

// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg, readability-magic-numbers)

namespace panda::ets::test {

static const char *TEST_BIN_FILE_NAME = "AccessingObjectsFieldsTest.abc";

class AccessingObjectsFieldsTestGeneral : public AccessingObjectsFieldsTestBase {
public:
    AccessingObjectsFieldsTestGeneral() : AccessingObjectsFieldsTestBase(TEST_BIN_FILE_NAME) {}
};

class AccessingObjectsFieldsTest : public AccessingObjectsFieldsTestGeneral {};
class AccessingObjectsFieldsTestDeath : public AccessingObjectsFieldsTestGeneral {};

TEST_F(AccessingObjectsFieldsTestDeath, GetFieldIDDeathTests)
{
    testing::FLAGS_gtest_death_test_style = "threadsafe";

    {
        EXPECT_DEATH(env_->Getp_field(nullptr, "some text", "some text"), "");
        EXPECT_DEATH(env_->Getp_field(nullptr, nullptr, "some text"), "");
        EXPECT_DEATH(env_->Getp_field(nullptr, "some text", nullptr), "");
        EXPECT_DEATH(env_->Getp_field(nullptr, nullptr, nullptr), "");
    }

    {
        ets_class cls = env_->FindClass("A");
        EXPECT_DEATH(env_->Getp_field(cls, nullptr, "some text"), "");
        EXPECT_DEATH(env_->Getp_field(cls, "some text", nullptr), "");
    }
}

TEST_F(AccessingObjectsFieldsTest, GetFieldID2)
{
    ets_class cls = env_->FindClass("F");
    ASSERT_NE(cls, nullptr);

    ets_field member_id = env_->Getp_field(cls, "member4", "I");
    ASSERT_NE(member_id, nullptr);
}

// NOTE(m.morozov): uncomment this test when inheritance will be implemented
// TEST_F(AccessingObjectsFieldsTest, GetBaseFieldID2)
// {
//     ets_class cls = env->FindClass("F_sub");
//     ASSERT_NE(cls, nullptr);

//     ets_field member_id = env->Getp_field(cls, "member4", "I");
//     ASSERT_NE(member_id, nullptr);
// }

TEST_F(AccessingObjectsFieldsTest, GetTypeField)
{
    ets_class cls = env_->FindClass("F");
    ASSERT_NE(cls, nullptr);
    ets_method ctor = env_->Getp_method(cls, "<ctor>", ":V");
    ASSERT_NE(ctor, nullptr);
    ets_object obj = env_->NewObject(cls, ctor);
    ASSERT_NE(obj, nullptr);

    ets_field member0_id = env_->Getp_field(cls, "member0", "Z");
    ASSERT_NE(member0_id, nullptr);
    ets_field member1_id = env_->Getp_field(cls, "member1", "B");
    ASSERT_NE(member1_id, nullptr);
    ets_field member2_id = env_->Getp_field(cls, "member2", "C");
    ASSERT_NE(member2_id, nullptr);
    ets_field member3_id = env_->Getp_field(cls, "member3", "S");
    ASSERT_NE(member3_id, nullptr);
    ets_field member4_id = env_->Getp_field(cls, "member4", "I");
    ASSERT_NE(member4_id, nullptr);
    ets_field member5_id = env_->Getp_field(cls, "member5", "J");
    ASSERT_NE(member5_id, nullptr);
    ets_field member6_id = env_->Getp_field(cls, "member6", "F");
    ASSERT_NE(member6_id, nullptr);
    ets_field member7_id = env_->Getp_field(cls, "member7", "D");
    ASSERT_NE(member7_id, nullptr);
    ets_field member8_id = env_->Getp_field(cls, "member8", "LA;");
    ASSERT_NE(member8_id, nullptr);

    EXPECT_EQ(env_->GetBooleanField(obj, member0_id), static_cast<ets_boolean>(1));
    EXPECT_EQ(env_->GetByteField(obj, member1_id), static_cast<ets_byte>(2));
    EXPECT_EQ(env_->GetCharField(obj, member2_id), static_cast<ets_char>(3));
    EXPECT_EQ(env_->GetShortField(obj, member3_id), static_cast<ets_short>(4));
    EXPECT_EQ(env_->GetIntField(obj, member4_id), static_cast<ets_int>(5));
    EXPECT_EQ(env_->GetLongField(obj, member5_id), static_cast<ets_long>(6));
    EXPECT_FLOAT_EQ(env_->GetFloatField(obj, member6_id), static_cast<ets_float>(7.0F));
    EXPECT_DOUBLE_EQ(env_->GetDoubleField(obj, member7_id), static_cast<ets_double>(8.0));

    ets_class a_cls = env_->FindClass("A");
    ASSERT_NE(a_cls, nullptr);
    ets_field a_member_id = env_->Getp_field(a_cls, "member", "I");
    ASSERT_NE(a_member_id, nullptr);
    ets_object a_obj = env_->GetObjectField(obj, member8_id);
    ASSERT_NE(a_obj, nullptr);
    EXPECT_EQ(env_->GetIntField(a_obj, a_member_id), static_cast<ets_int>(1));
}

TEST_F(AccessingObjectsFieldsTest, SetTypeField)
{
    ets_class cls = env_->FindClass("F");
    ASSERT_NE(cls, nullptr);
    ets_method ctor = env_->Getp_method(cls, "<ctor>", ":V");
    ASSERT_NE(ctor, nullptr);
    ets_object obj = env_->NewObject(cls, ctor);
    ASSERT_NE(obj, nullptr);

    ets_field member0_id = env_->Getp_field(cls, "member0", "Z");
    ASSERT_NE(member0_id, nullptr);
    ets_field member1_id = env_->Getp_field(cls, "member1", "B");
    ASSERT_NE(member1_id, nullptr);
    ets_field member2_id = env_->Getp_field(cls, "member2", "C");
    ASSERT_NE(member2_id, nullptr);
    ets_field member3_id = env_->Getp_field(cls, "member3", "S");
    ASSERT_NE(member3_id, nullptr);
    ets_field member4_id = env_->Getp_field(cls, "member4", "I");
    ASSERT_NE(member4_id, nullptr);
    ets_field member5_id = env_->Getp_field(cls, "member5", "J");
    ASSERT_NE(member5_id, nullptr);
    ets_field member6_id = env_->Getp_field(cls, "member6", "F");
    ASSERT_NE(member6_id, nullptr);
    ets_field member7_id = env_->Getp_field(cls, "member7", "D");
    ASSERT_NE(member7_id, nullptr);
    ets_field member8_id = env_->Getp_field(cls, "member8", "LA;");
    ASSERT_NE(member8_id, nullptr);

    ets_class a_cls = env_->FindClass("A");
    ASSERT_NE(a_cls, nullptr);
    ets_method a_ctor = env_->Getp_method(a_cls, "<ctor>", ":V");
    ASSERT_NE(a_ctor, nullptr);
    ets_object a_obj = env_->NewObject(a_cls, a_ctor);
    ASSERT_NE(a_obj, nullptr);
    ets_field a_member_id = env_->Getp_field(a_cls, "member", "I");
    ASSERT_NE(a_member_id, nullptr);

    env_->SetBooleanField(obj, member0_id, static_cast<ets_boolean>(1));
    env_->SetByteField(obj, member1_id, static_cast<ets_byte>(1));
    env_->SetCharField(obj, member2_id, static_cast<ets_char>(1));
    env_->SetShortField(obj, member3_id, static_cast<ets_short>(1));
    env_->SetIntField(obj, member4_id, static_cast<ets_int>(1));
    env_->SetLongField(obj, member5_id, static_cast<ets_long>(1));
    env_->SetFloatField(obj, member6_id, static_cast<ets_float>(1.0F));
    env_->SetDoubleField(obj, member7_id, static_cast<ets_double>(1.0));

    env_->SetIntField(a_obj, a_member_id, static_cast<ets_int>(5));
    env_->SetObjectField(obj, member8_id, a_obj);

    EXPECT_EQ(env_->GetBooleanField(obj, member0_id), static_cast<ets_boolean>(1));
    EXPECT_EQ(env_->GetByteField(obj, member1_id), static_cast<ets_byte>(1));
    EXPECT_EQ(env_->GetCharField(obj, member2_id), static_cast<ets_char>(1));
    EXPECT_EQ(env_->GetShortField(obj, member3_id), static_cast<ets_short>(1));
    EXPECT_EQ(env_->GetIntField(obj, member4_id), static_cast<ets_int>(1));
    EXPECT_EQ(env_->GetLongField(obj, member5_id), static_cast<ets_long>(1));
    EXPECT_FLOAT_EQ(env_->GetFloatField(obj, member6_id), static_cast<ets_float>(1.0F));
    EXPECT_DOUBLE_EQ(env_->GetDoubleField(obj, member7_id), static_cast<ets_double>(1.0));

    ets_object set_a_obj = env_->GetObjectField(obj, member8_id);
    ASSERT_NE(set_a_obj, nullptr);
    EXPECT_EQ(env_->GetIntField(set_a_obj, a_member_id), static_cast<ets_int>(5));
}

TEST_F(AccessingObjectsFieldsTestDeath, GetStaticFieldIDDeathTests)
{
    testing::FLAGS_gtest_death_test_style = "threadsafe";

    {
        EXPECT_DEATH(env_->GetStaticp_field(nullptr, "some text", "some text"), "");
        EXPECT_DEATH(env_->GetStaticp_field(nullptr, nullptr, "some text"), "");
        EXPECT_DEATH(env_->GetStaticp_field(nullptr, "some text", nullptr), "");
        EXPECT_DEATH(env_->GetStaticp_field(nullptr, nullptr, nullptr), "");
    }

    {
        ets_class cls = env_->FindClass("A");
        EXPECT_DEATH(env_->GetStaticp_field(cls, nullptr, "some text"), "");
        EXPECT_DEATH(env_->GetStaticp_field(cls, "some text", nullptr), "");
    }
}

TEST_F(AccessingObjectsFieldsTest, GetStaticFieldID2)
{
    ets_class cls = env_->FindClass("F_static");
    ASSERT_NE(cls, nullptr);

    ets_field member_id = env_->GetStaticp_field(cls, "member4", "I");
    ASSERT_NE(member_id, nullptr);
}

TEST_F(AccessingObjectsFieldsTest, GetStaticTypeField)
{
    ets_class cls = env_->FindClass("F_static");
    ASSERT_NE(cls, nullptr);

    ets_field member0_id = env_->GetStaticp_field(cls, "member0", "Z");
    ASSERT_NE(member0_id, nullptr);
    ets_field member1_id = env_->GetStaticp_field(cls, "member1", "B");
    ASSERT_NE(member1_id, nullptr);
    ets_field member2_id = env_->GetStaticp_field(cls, "member2", "C");
    ASSERT_NE(member2_id, nullptr);
    ets_field member3_id = env_->GetStaticp_field(cls, "member3", "S");
    ASSERT_NE(member3_id, nullptr);
    ets_field member4_id = env_->GetStaticp_field(cls, "member4", "I");
    ASSERT_NE(member4_id, nullptr);
    ets_field member5_id = env_->GetStaticp_field(cls, "member5", "J");
    ASSERT_NE(member5_id, nullptr);
    ets_field member6_id = env_->GetStaticp_field(cls, "member6", "F");
    ASSERT_NE(member6_id, nullptr);
    ets_field member7_id = env_->GetStaticp_field(cls, "member7", "D");
    ASSERT_NE(member7_id, nullptr);
    ets_field member8_id = env_->GetStaticp_field(cls, "member8", "LA;");
    ASSERT_NE(member8_id, nullptr);

    EXPECT_EQ(env_->GetStaticBooleanField(cls, member0_id), static_cast<ets_boolean>(1));
    EXPECT_EQ(env_->GetStaticByteField(cls, member1_id), static_cast<ets_byte>(2));
    EXPECT_EQ(env_->GetStaticCharField(cls, member2_id), static_cast<ets_char>(3));
    EXPECT_EQ(env_->GetStaticShortField(cls, member3_id), static_cast<ets_short>(4));
    EXPECT_EQ(env_->GetStaticIntField(cls, member4_id), static_cast<ets_int>(5));
    EXPECT_EQ(env_->GetStaticLongField(cls, member5_id), static_cast<ets_long>(6));
    EXPECT_FLOAT_EQ(env_->GetStaticFloatField(cls, member6_id), static_cast<ets_float>(7.0F));
    EXPECT_DOUBLE_EQ(env_->GetStaticDoubleField(cls, member7_id), static_cast<ets_double>(8.0));

    ets_class a_cls = env_->FindClass("A");
    ASSERT_NE(a_cls, nullptr);
    ets_field a_member_id = env_->Getp_field(a_cls, "member", "I");
    ASSERT_NE(a_member_id, nullptr);
    ets_object a_obj = env_->GetStaticObjectField(cls, member8_id);
    ASSERT_NE(a_obj, nullptr);
    EXPECT_EQ(env_->GetIntField(a_obj, a_member_id), static_cast<ets_int>(1));
}

TEST_F(AccessingObjectsFieldsTest, SetStaticField)
{
    ets_class cls = env_->FindClass("F_static");
    ASSERT_NE(cls, nullptr);

    ets_field member0_id = env_->GetStaticp_field(cls, "member0", "Z");
    ASSERT_NE(member0_id, nullptr);
    ets_field member1_id = env_->GetStaticp_field(cls, "member1", "B");
    ASSERT_NE(member1_id, nullptr);
    ets_field member2_id = env_->GetStaticp_field(cls, "member2", "C");
    ASSERT_NE(member2_id, nullptr);
    ets_field member3_id = env_->GetStaticp_field(cls, "member3", "S");
    ASSERT_NE(member3_id, nullptr);
    ets_field member4_id = env_->GetStaticp_field(cls, "member4", "I");
    ASSERT_NE(member4_id, nullptr);
    ets_field member5_id = env_->GetStaticp_field(cls, "member5", "J");
    ASSERT_NE(member5_id, nullptr);
    ets_field member6_id = env_->GetStaticp_field(cls, "member6", "F");
    ASSERT_NE(member6_id, nullptr);
    ets_field member7_id = env_->GetStaticp_field(cls, "member7", "D");
    ASSERT_NE(member7_id, nullptr);
    ets_field member8_id = env_->GetStaticp_field(cls, "member8", "LA;");
    ASSERT_NE(member8_id, nullptr);

    ets_class a_cls = env_->FindClass("A");
    ASSERT_NE(a_cls, nullptr);
    ets_field a_member_id = env_->Getp_field(a_cls, "member", "I");
    ASSERT_NE(a_member_id, nullptr);
    ets_object a_obj = env_->GetStaticObjectField(cls, member8_id);
    ASSERT_NE(a_obj, nullptr);

    env_->SetIntField(a_obj, a_member_id, static_cast<ets_int>(5));

    env_->SetStaticBooleanField(cls, member0_id, static_cast<ets_boolean>(1));
    env_->SetStaticByteField(cls, member1_id, static_cast<ets_byte>(10));
    env_->SetStaticCharField(cls, member2_id, static_cast<ets_char>(20));
    env_->SetStaticShortField(cls, member3_id, static_cast<ets_short>(30));
    env_->SetStaticIntField(cls, member4_id, static_cast<ets_int>(40));
    env_->SetStaticLongField(cls, member5_id, static_cast<ets_long>(50));
    env_->SetStaticFloatField(cls, member6_id, static_cast<ets_float>(60.0F));
    env_->SetStaticDoubleField(cls, member7_id, static_cast<ets_double>(70.0));
    env_->SetStaticObjectField(cls, member8_id, a_obj);

    EXPECT_EQ(env_->GetStaticBooleanField(cls, member0_id), static_cast<ets_boolean>(1));
    EXPECT_EQ(env_->GetStaticByteField(cls, member1_id), static_cast<ets_byte>(10));
    EXPECT_EQ(env_->GetStaticCharField(cls, member2_id), static_cast<ets_char>(20));
    EXPECT_EQ(env_->GetStaticShortField(cls, member3_id), static_cast<ets_short>(30));
    EXPECT_EQ(env_->GetStaticIntField(cls, member4_id), static_cast<ets_int>(40));
    EXPECT_EQ(env_->GetStaticLongField(cls, member5_id), static_cast<ets_long>(50));
    EXPECT_FLOAT_EQ(env_->GetStaticFloatField(cls, member6_id), static_cast<ets_float>(60.0F));
    EXPECT_DOUBLE_EQ(env_->GetStaticDoubleField(cls, member7_id), static_cast<ets_double>(70.0));

    env_->SetStaticBooleanField(nullptr, member0_id, static_cast<ets_boolean>(1));
    env_->SetStaticByteField(nullptr, member1_id, static_cast<ets_byte>(10));
    env_->SetStaticCharField(nullptr, member2_id, static_cast<ets_char>(20));
    env_->SetStaticShortField(nullptr, member3_id, static_cast<ets_short>(30));
    env_->SetStaticIntField(nullptr, member4_id, static_cast<ets_int>(40));
    env_->SetStaticLongField(nullptr, member5_id, static_cast<ets_long>(50));
    env_->SetStaticFloatField(nullptr, member6_id, static_cast<ets_float>(60.0F));
    env_->SetStaticDoubleField(nullptr, member7_id, static_cast<ets_double>(70.0));
    env_->SetStaticObjectField(nullptr, member8_id, a_obj);

    EXPECT_EQ(env_->GetStaticBooleanField(nullptr, member0_id), static_cast<ets_boolean>(1));
    EXPECT_EQ(env_->GetStaticByteField(nullptr, member1_id), static_cast<ets_byte>(10));
    EXPECT_EQ(env_->GetStaticCharField(nullptr, member2_id), static_cast<ets_char>(20));
    EXPECT_EQ(env_->GetStaticShortField(nullptr, member3_id), static_cast<ets_short>(30));
    EXPECT_EQ(env_->GetStaticIntField(nullptr, member4_id), static_cast<ets_int>(40));
    EXPECT_EQ(env_->GetStaticLongField(nullptr, member5_id), static_cast<ets_long>(50));
    EXPECT_FLOAT_EQ(env_->GetStaticFloatField(nullptr, member6_id), static_cast<ets_float>(60.0F));
    EXPECT_DOUBLE_EQ(env_->GetStaticDoubleField(nullptr, member7_id), static_cast<ets_double>(70.0));

    ets_object set_a_obj = env_->GetStaticObjectField(cls, member8_id);
    ASSERT_NE(set_a_obj, nullptr);
    EXPECT_EQ(env_->IsInstanceOf(set_a_obj, a_cls), ETS_TRUE);
    EXPECT_EQ(env_->GetIntField(set_a_obj, a_member_id), static_cast<ets_int>(5));
}

TEST_F(AccessingObjectsFieldsTestDeath, GetStaticTypeFieldDeathTests)
{
    testing::FLAGS_gtest_death_test_style = "threadsafe";

    {
        EXPECT_DEATH(env_->GetStaticObjectField(nullptr, nullptr), "");
        EXPECT_DEATH(env_->GetStaticBooleanField(nullptr, nullptr), "");
        EXPECT_DEATH(env_->GetStaticByteField(nullptr, nullptr), "");
        EXPECT_DEATH(env_->GetStaticCharField(nullptr, nullptr), "");
        EXPECT_DEATH(env_->GetStaticShortField(nullptr, nullptr), "");
        EXPECT_DEATH(env_->GetStaticIntField(nullptr, nullptr), "");
        EXPECT_DEATH(env_->GetStaticLongField(nullptr, nullptr), "");
        EXPECT_DEATH(env_->GetStaticFloatField(nullptr, nullptr), "");
        EXPECT_DEATH(env_->GetStaticDoubleField(nullptr, nullptr), "");
    }

    {
        ets_class cls = env_->FindClass("F_static");
        ASSERT_NE(cls, nullptr);

        EXPECT_DEATH(env_->GetStaticObjectField(cls, nullptr), "");
        EXPECT_DEATH(env_->GetStaticBooleanField(cls, nullptr), "");
        EXPECT_DEATH(env_->GetStaticByteField(cls, nullptr), "");
        EXPECT_DEATH(env_->GetStaticCharField(cls, nullptr), "");
        EXPECT_DEATH(env_->GetStaticShortField(cls, nullptr), "");
        EXPECT_DEATH(env_->GetStaticIntField(cls, nullptr), "");
        EXPECT_DEATH(env_->GetStaticLongField(cls, nullptr), "");
        EXPECT_DEATH(env_->GetStaticFloatField(cls, nullptr), "");
        EXPECT_DEATH(env_->GetStaticDoubleField(cls, nullptr), "");
    }
}

// NOTE(m.morozov): uncomment this test when inheritance will be implemented
// TEST_F(AccessingObjectsFieldsTest, GetStaticTypeFieldBase)
// {
//     ets_class cls = env->FindClass("F_static_sub");
//     ASSERT_NE(cls, nullptr);

//     ets_field member0_id = env->GetStaticp_field(cls, "member0", "Z");
//     ASSERT_NE(member0_id, nullptr);
//     ets_field member1_id = env->GetStaticp_field(cls, "member1", "B");
//     ASSERT_NE(member1_id, nullptr);
//     ets_field member2_id = env->GetStaticp_field(cls, "member2", "C");
//     ASSERT_NE(member2_id, nullptr);
//     ets_field member3_id = env->GetStaticp_field(cls, "member3", "S");
//     ASSERT_NE(member3_id, nullptr);
//     ets_field member4_id = env->GetStaticp_field(cls, "member4", "I");
//     ASSERT_NE(member4_id, nullptr);
//     ets_field member5_id = env->GetStaticp_field(cls, "member5", "J");
//     ASSERT_NE(member5_id, nullptr);
//     ets_field member6_id = env->GetStaticp_field(cls, "member6", "F");
//     ASSERT_NE(member6_id, nullptr);
//     ets_field member7_id = env->GetStaticp_field(cls, "member7", "D");
//     ASSERT_NE(member7_id, nullptr);
//     ets_field member8_id = env->GetStaticp_field(cls, "member8", "LA;");
//     ASSERT_NE(member8_id, nullptr);

//     EXPECT_EQ(env->GetStaticBooleanField(cls, member0_id), static_cast<ets_boolean>(1));
//     EXPECT_EQ(env->GetStaticByteField(cls, member1_id), static_cast<ets_byte>(2));
//     EXPECT_EQ(env->GetStaticCharField(cls, member2_id), static_cast<ets_char>(3));
//     EXPECT_EQ(env->GetStaticShortField(cls, member3_id), static_cast<ets_short>(4));
//     EXPECT_EQ(env->GetStaticIntField(cls, member4_id), static_cast<ets_int>(5));
//     EXPECT_EQ(env->GetStaticLongField(cls, member5_id), static_cast<ets_long>(6));
//     EXPECT_FLOAT_EQ(env->GetStaticFloatField(cls, member6_id), static_cast<ets_float>(7.0F));
//     EXPECT_DOUBLE_EQ(env->GetStaticDoubleField(cls, member7_id), static_cast<ets_double>(8.0));

//     ets_class a_cls = env->FindClass("A");
//     ASSERT_NE(a_cls, nullptr);
//     ets_field a_member_id = env->Getp_field(a_cls, "member", "I");
//     ASSERT_NE(a_member_id, nullptr);
//     ets_object a_obj = env->GetStaticObjectField(cls, member8_id);
//     ASSERT_NE(a_obj, nullptr);
//     EXPECT_EQ(env->GetIntField(a_obj, a_member_id), static_cast<ets_int>(1));
// }

TEST_F(AccessingObjectsFieldsTestDeath, SetStaticTypeFieldDeathTests)
{
    testing::FLAGS_gtest_death_test_style = "threadsafe";

    ets_class a_cls = env_->FindClass("A");
    ASSERT_NE(a_cls, nullptr);
    ets_object a_obj = env_->AllocObject(a_cls);
    ASSERT_NE(a_obj, nullptr);

    {
        EXPECT_DEATH(env_->SetStaticObjectField(nullptr, nullptr, nullptr), "");
        EXPECT_DEATH(env_->SetStaticObjectField(nullptr, nullptr, a_obj), "");

        EXPECT_DEATH(env_->SetStaticBooleanField(nullptr, nullptr, static_cast<ets_boolean>(0)), "");
        EXPECT_DEATH(env_->SetStaticByteField(nullptr, nullptr, static_cast<ets_byte>(0)), "");
        EXPECT_DEATH(env_->SetStaticCharField(nullptr, nullptr, static_cast<ets_char>(0)), "");
        EXPECT_DEATH(env_->SetStaticShortField(nullptr, nullptr, static_cast<ets_short>(0)), "");
        EXPECT_DEATH(env_->SetStaticIntField(nullptr, nullptr, static_cast<ets_int>(0)), "");
        EXPECT_DEATH(env_->SetStaticLongField(nullptr, nullptr, static_cast<ets_long>(0)), "");
        EXPECT_DEATH(env_->SetStaticFloatField(nullptr, nullptr, static_cast<ets_float>(0.0F)), "");
        EXPECT_DEATH(env_->SetStaticDoubleField(nullptr, nullptr, static_cast<ets_double>(0.0)), "");
    }

    {
        ets_class cls = env_->FindClass("F_static");
        ASSERT_NE(cls, nullptr);

        EXPECT_DEATH(env_->SetStaticObjectField(cls, nullptr, nullptr), "");
        EXPECT_DEATH(env_->SetStaticObjectField(cls, nullptr, a_obj), "");

        EXPECT_DEATH(env_->SetStaticBooleanField(cls, nullptr, static_cast<ets_boolean>(0)), "");
        EXPECT_DEATH(env_->SetStaticByteField(cls, nullptr, static_cast<ets_byte>(0)), "");
        EXPECT_DEATH(env_->SetStaticCharField(cls, nullptr, static_cast<ets_char>(0)), "");
        EXPECT_DEATH(env_->SetStaticShortField(cls, nullptr, static_cast<ets_short>(0)), "");
        EXPECT_DEATH(env_->SetStaticIntField(cls, nullptr, static_cast<ets_int>(0)), "");
        EXPECT_DEATH(env_->SetStaticLongField(cls, nullptr, static_cast<ets_long>(0)), "");
        EXPECT_DEATH(env_->SetStaticFloatField(cls, nullptr, static_cast<ets_float>(0.0F)), "");
        EXPECT_DEATH(env_->SetStaticDoubleField(cls, nullptr, static_cast<ets_double>(0.0)), "");
    }
}

// NOTE(m.morozov): uncomment this test when inheritance will be implemented
// TEST_F(AccessingObjectsFieldsTest, SetStaticFieldBase)
// {
//     ets_class cls = env->FindClass("F_static_sub");
//     ASSERT_NE(cls, nullptr);

//     ets_field member0_id = env->GetStaticp_field(cls, "member0", "Z");
//     ASSERT_NE(member0_id, nullptr);
//     ets_field member1_id = env->GetStaticp_field(cls, "member1", "B");
//     ASSERT_NE(member1_id, nullptr);
//     ets_field member2_id = env->GetStaticp_field(cls, "member2", "C");
//     ASSERT_NE(member2_id, nullptr);
//     ets_field member3_id = env->GetStaticp_field(cls, "member3", "S");
//     ASSERT_NE(member3_id, nullptr);
//     ets_field member4_id = env->GetStaticp_field(cls, "member4", "I");
//     ASSERT_NE(member4_id, nullptr);
//     ets_field member5_id = env->GetStaticp_field(cls, "member5", "J");
//     ASSERT_NE(member5_id, nullptr);
//     ets_field member6_id = env->GetStaticp_field(cls, "member6", "F");
//     ASSERT_NE(member6_id, nullptr);
//     ets_field member7_id = env->GetStaticp_field(cls, "member7", "D");
//     ASSERT_NE(member7_id, nullptr);
//     ets_field member8_id = env->GetStaticp_field(cls, "member8", "LA;");
//     ASSERT_NE(member8_id, nullptr);

//     ets_class a_cls = env->FindClass("A");
//     ASSERT_NE(a_cls, nullptr);
//     ets_field a_member_id = env->Getp_field(a_cls, "member", "I");
//     ASSERT_NE(a_member_id, nullptr);
//     ets_object a_obj = env->GetStaticObjectField(cls, member8_id);
//     ASSERT_NE(a_obj, nullptr);

//     env->SetIntField(a_obj, a_member_id, static_cast<ets_int>(5));

//     env->SetStaticBooleanField(cls, member0_id, static_cast<ets_boolean>(1));
//     env->SetStaticByteField(cls, member1_id, static_cast<ets_byte>(10));
//     env->SetStaticCharField(cls, member2_id, static_cast<ets_char>(20));
//     env->SetStaticShortField(cls, member3_id, static_cast<ets_short>(30));
//     env->SetStaticIntField(cls, member4_id, static_cast<ets_int>(40));
//     env->SetStaticLongField(cls, member5_id, static_cast<ets_long>(50));
//     env->SetStaticFloatField(cls, member6_id, static_cast<ets_float>(60.0F));
//     env->SetStaticDoubleField(cls, member7_id, static_cast<ets_double>(70.0));
//     env->SetStaticObjectField(cls, member8_id, a_obj);

//     EXPECT_EQ(env->GetStaticBooleanField(cls, member0_id), static_cast<ets_boolean>(1));
//     EXPECT_EQ(env->GetStaticByteField(cls, member1_id), static_cast<ets_byte>(10));
//     EXPECT_EQ(env->GetStaticCharField(cls, member2_id), static_cast<ets_char>(20));
//     EXPECT_EQ(env->GetStaticShortField(cls, member3_id), static_cast<ets_short>(30));
//     EXPECT_EQ(env->GetStaticIntField(cls, member4_id), static_cast<ets_int>(40));
//     EXPECT_EQ(env->GetStaticLongField(cls, member5_id), static_cast<ets_long>(50));
//     EXPECT_FLOAT_EQ(env->GetStaticFloatField(cls, member6_id), static_cast<ets_float>(60.0F));
//     EXPECT_DOUBLE_EQ(env->GetStaticDoubleField(cls, member7_id), static_cast<ets_double>(70.0));

//     ets_object set_a_obj = env->GetStaticObjectField(cls, member8_id);
//     ASSERT_NE(set_a_obj, nullptr);
//     EXPECT_EQ(env->IsInstanceOf(set_a_obj, a_cls), ETS_TRUE);
//     EXPECT_EQ(env->GetIntField(set_a_obj, a_member_id), static_cast<ets_int>(5));
// }

}  // namespace panda::ets::test

// NOLINTEND(cppcoreguidelines-pro-type-vararg, readability-magic-numbers)