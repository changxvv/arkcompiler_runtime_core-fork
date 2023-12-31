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

#include <gtest/gtest.h>

#include "base_options.h"
#include "plugins/ets/runtime/ets_vm.h"
#include "plugins/ets/runtime/napi/ets_napi.h"

namespace panda::ets::test {

// NOLINTBEGIN(google-runtime-int)
extern "C" ets_long SkoalaCreateRedrawerPeer([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                             [[maybe_unused]] ets_object object /*any*/)
{
    return 1;
}
extern "C" ets_long SkoalaGetFrame([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                   [[maybe_unused]] ets_long peer /*KNativePointer*/, [[maybe_unused]] ets_int a,
                                   [[maybe_unused]] ets_int b)
{
    return 1;
}
extern "C" ets_long SkoalaInitRedrawer([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                       [[maybe_unused]] ets_int width, [[maybe_unused]] ets_int height,
                                       [[maybe_unused]] ets_float scale,
                                       [[maybe_unused]] ets_long peer /*KNativePointer*/,
                                       [[maybe_unused]] ets_long frame /*KNativePointer*/)
{
    return 1;
}
extern "C" ets_long SkoalaPaint1nMake([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass)
{
    return 1;
}
extern "C" ets_long SkoalaPictureRecorder1nBeginRecording(
    [[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass, [[maybe_unused]] ets_long ptr /*KNativePointer*/,
    [[maybe_unused]] ets_float left, [[maybe_unused]] ets_float top, [[maybe_unused]] ets_float right,
    [[maybe_unused]] ets_float bottom)
{
    return 1;
}
extern "C" ets_long SkoalaPictureRecorder1nFinishRecordingAsDrawable([[maybe_unused]] EtsEnv *env,
                                                                     [[maybe_unused]] ets_class klass,
                                                                     [[maybe_unused]] ets_long ptr /*KNativePointer*/)
{
    return 1;
}
extern "C" ets_long SkoalaPictureRecorder1nFinishRecordingAsPictureWithCull(
    [[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass, [[maybe_unused]] ets_long ptr /*KNativePointer*/,
    [[maybe_unused]] ets_float left, [[maybe_unused]] ets_float top, [[maybe_unused]] ets_float right,
    [[maybe_unused]] ets_float bottom)
{
    return 1;
}
extern "C" ets_long SkoalaPictureRecorder1nMake([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass)
{
    return 1;
}
extern "C" ets_long SkoalaParagraphParagraphBuilder1nMake(
    [[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
    [[maybe_unused]] ets_long paragraph_style_ptr /*KNativePointer*/,
    [[maybe_unused]] ets_long font_collection_ptr /*KNativePointer*/)
{
    return 1;
}
extern "C" ets_long SkoalaParagraphParagraphBuilder1nGetFinalizer([[maybe_unused]] EtsEnv *env,
                                                                  [[maybe_unused]] ets_class klass)
{
    return 1;
}
extern "C" ets_long SkoalaParagraphParagraphBuilder1nBuild([[maybe_unused]] EtsEnv *env,
                                                           [[maybe_unused]] ets_class klass,
                                                           [[maybe_unused]] ets_long ptr /*KNativePointer*/)
{
    return 1;
}
extern "C" ets_long SkoalaParagraphFontCollection1nMake([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass)
{
    return 1;
}
extern "C" ets_long SkoalaFontMgr1nDefault([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass)
{
    return 1;
}
extern "C" ets_long SkoalaParagraphParagraphStyle1nMake([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass)
{
    return 1;
}
extern "C" ets_long SkoalaParagraphTextStyle1nMake([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass)
{
    return 1;
}
extern "C" ets_long SkoalaManagedString1nMake([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                              [[maybe_unused]] ets_byteArray text_str /*KStringPtr*/)
{
    return 1;
}
extern "C" ets_long SkoalaPaint1nGetFinalizer([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass)
{
    return 1;
}
extern "C" ets_long SkoalaImplRefCntGetFinalizer([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass)
{
    return 1;
}
extern "C" ets_long SkoalaParagraphParagraphStyle1nGetFinalizer([[maybe_unused]] EtsEnv *env,
                                                                [[maybe_unused]] ets_class klass)
{
    return 1;
}
extern "C" ets_long SkoalaParagraphTextStyle1nGetFinalizer([[maybe_unused]] EtsEnv *env,
                                                           [[maybe_unused]] ets_class klass)
{
    return 1;
}
extern "C" ets_long SkoalaParagraphParagraph1nGetFinalizer([[maybe_unused]] EtsEnv *env,
                                                           [[maybe_unused]] ets_class klass)
{
    return 1;
}
extern "C" ets_long SkoalaManagedString1nGetFinalizer([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass)
{
    return 1;
}
extern "C" ets_long GetPeerFactory([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass)
{
    return 1;
}
extern "C" ets_long GetEngine([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass)
{
    return 1;
}
extern "C" ets_int SkoalaGetFrameWidth([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                       [[maybe_unused]] ets_long peer /*KNativePointer*/,
                                       [[maybe_unused]] ets_long frame /*KNativePointer*/)
{
    return 1;
}
extern "C" ets_int SkoalaGetFrameHeight([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                        [[maybe_unused]] ets_long peer /*KNativePointer*/,
                                        [[maybe_unused]] ets_long frame /*KNativePointer*/)
{
    return 1;
}
extern "C" ets_int SkoalaCanvas1nSave([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                      [[maybe_unused]] ets_long ptr /*KNativePointer*/)
{
    return 1;
}
extern "C" ets_void SkoalaDrawPicture([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                      [[maybe_unused]] ets_long picture /*KNativePointer*/,
                                      [[maybe_unused]] ets_long data /*KNativePointer*/,
                                      [[maybe_unused]] ets_object cb /*any*/, [[maybe_unused]] ets_boolean sync)
{
    return ets_void();
}
extern "C" ets_void SkoalaProvidePeerFactory([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                             [[maybe_unused]] ets_long func /*KNativePointer*/,
                                             [[maybe_unused]] ets_long arg /*KNativePointer*/)
{
    return ets_void();
}
extern "C" ets_void SkoalaSetPlatformApi([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                         [[maybe_unused]] ets_object api /*any*/)
{
    return ets_void();
}
extern "C" ets_void SkoalaCanvas1nDrawDrawable([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                               [[maybe_unused]] ets_long ptr /*KNativePointer*/,
                                               [[maybe_unused]] ets_long drawable_ptr /*KNativePointer*/,
                                               [[maybe_unused]] ets_long matrix_arr /*KFloatPtr*/)
{
    return ets_void();
}
extern "C" ets_void SkoalaCanvas1nRestore([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                          [[maybe_unused]] ets_long ptr /*KNativePointer*/)
{
    return ets_void();
}
extern "C" ets_void SkoalaPaint1nSetColor([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                          [[maybe_unused]] ets_long ptr /*KNativePointer*/,
                                          [[maybe_unused]] ets_int color)
{
    return ets_void();
}
extern "C" ets_void SkoalaCanvas1nDrawOval([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                           [[maybe_unused]] ets_long canvas_ptr /*KNativePointer*/,
                                           [[maybe_unused]] ets_float left, [[maybe_unused]] ets_float top,
                                           [[maybe_unused]] ets_float right, [[maybe_unused]] ets_float bottom,
                                           [[maybe_unused]] ets_long paint_ptr /*KNativePointer*/)
{
    return ets_void();
}
extern "C" ets_void SkoalaParagraphParagraphBuilder1nPushStyle(
    [[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass, [[maybe_unused]] ets_long ptr /*KNativePointer*/,
    [[maybe_unused]] ets_long text_style_ptr /*KNativePointer*/)
{
    return ets_void();
}
extern "C" ets_void SkoalaParagraphParagraphBuilder1nAddText([[maybe_unused]] EtsEnv *env,
                                                             [[maybe_unused]] ets_class klass,
                                                             [[maybe_unused]] ets_long ptr /*KNativePointer*/,
                                                             [[maybe_unused]] ets_long text_string /*KStringPtr*/)
{
    return ets_void();
}
extern "C" ets_void SkoalaParagraphFontCollection1nSetDefaultFontManager(
    [[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass, [[maybe_unused]] ets_long ptr /*KNativePointer*/,
    [[maybe_unused]] ets_long font_manager_ptr /*KNativePointer*/,
    [[maybe_unused]] ets_long default_family_name_str /*KStringPtr*/)
{
    return ets_void();
}
extern "C" ets_void SkoalaParagraphParagraph1nLayout([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                                     [[maybe_unused]] ets_long ptr /*KNativePointer*/,
                                                     [[maybe_unused]] ets_float width)
{
    return ets_void();
}
extern "C" ets_void SkoalaParagraphParagraph1nPaint([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                                    [[maybe_unused]] ets_long ptr /*KNativePointer*/,
                                                    [[maybe_unused]] ets_long canvas_ptr /*KNativePointer*/,
                                                    [[maybe_unused]] ets_float x, [[maybe_unused]] ets_float y)
{
    return ets_void();
}
extern "C" ets_void SkoalaParagraphTextStyle1nSetColor([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                                       [[maybe_unused]] ets_long ptr /*KNativePointer*/,
                                                       [[maybe_unused]] ets_int color)
{
    return ets_void();
}
extern "C" ets_void SkoalaParagraphTextStyle1nSetFontSize([[maybe_unused]] EtsEnv *env,
                                                          [[maybe_unused]] ets_class klass,
                                                          [[maybe_unused]] ets_long ptr /*KNativePointer*/,
                                                          [[maybe_unused]] ets_float size)
{
    return ets_void();
}
extern "C" ets_void SkoalaImplManagedInvokeFinalizer([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                                     [[maybe_unused]] ets_long finalizer /*KNativePointer*/,
                                                     [[maybe_unused]] ets_long obj /*KNativePointer*/)
{
    return ets_void();
}
extern "C" ets_void SkoalaEnqueueRun([[maybe_unused]] EtsEnv *env, [[maybe_unused]] ets_class klass,
                                     [[maybe_unused]] ets_long redrawer_peer_ptr /*KNativePointer*/)
{
    return ets_void();
}
// NOLINTEND(google-runtime-int)

class EtsVMConfingNapi : public testing::Test {
public:
    bool CreateRuntime(const std::string &stdlib_abc, const std::string &path_abc, bool enable_jit,
                       const std::string &path_an = "")
    {
        std::vector<EtsVMOption> options = {{EtsOptionType::EtsLogLevel, "info"},
                                            {EtsOptionType::EtsBootFile, stdlib_abc.c_str()},
                                            {EtsOptionType::EtsBootFile, path_abc.c_str()},
                                            {EtsOptionType::EtsArkFile, path_abc.c_str()},
                                            {EtsOptionType::EtsGcTriggerType, "heap-trigger"},
                                            {enable_jit ? EtsOptionType::EtsJit : EtsOptionType::EtsNoJit, nullptr}};
        if (!path_an.empty()) {
            options.emplace_back(EtsVMOption {EtsOptionType::EtsAot, nullptr});
            options.emplace_back(EtsVMOption {EtsOptionType::EtsAotFile, path_an.c_str()});
        }

        EtsVMInitArgs args = {ETS_NAPI_VERSION_1_0, static_cast<ets_int>(options.size()), options.data()};
        return ETS_CreateVM(&vm_, &env_, &args) == ETS_OK;
    }

    bool InitExports()
    {
        const std::array<EtsNativeMethod, 43> impls = {
            {{"_skoala_createRedrawerPeer", nullptr, reinterpret_cast<void *>(SkoalaCreateRedrawerPeer)},
             {"_skoala_drawPicture", nullptr, reinterpret_cast<void *>(SkoalaDrawPicture)},
             {"_skoala_getFrame", nullptr, reinterpret_cast<void *>(SkoalaGetFrame)},
             {"_skoala_getFrameWidth", nullptr, reinterpret_cast<void *>(SkoalaGetFrameWidth)},
             {"_skoala_getFrameHeight", nullptr, reinterpret_cast<void *>(SkoalaGetFrameHeight)},
             {"_skoala_initRedrawer", nullptr, reinterpret_cast<void *>(SkoalaInitRedrawer)},
             {"_skoala_providePeerFactory", nullptr, reinterpret_cast<void *>(SkoalaProvidePeerFactory)},
             {"_skoala_setPlatformAPI", nullptr, reinterpret_cast<void *>(SkoalaSetPlatformApi)},
             {"_skoala_Canvas__1nDrawDrawable", nullptr, reinterpret_cast<void *>(SkoalaCanvas1nDrawDrawable)},
             {"_skoala_Canvas__1nRestore", nullptr, reinterpret_cast<void *>(SkoalaCanvas1nRestore)},
             {"_skoala_Canvas__1nSave", nullptr, reinterpret_cast<void *>(SkoalaCanvas1nSave)},
             {"_skoala_Paint__1nMake", nullptr, reinterpret_cast<void *>(SkoalaPaint1nMake)},
             {"_skoala_Paint__1nSetColor", nullptr, reinterpret_cast<void *>(SkoalaPaint1nSetColor)},
             {"_skoala_PictureRecorder__1nBeginRecording", nullptr,
              reinterpret_cast<void *>(SkoalaPictureRecorder1nBeginRecording)},
             {"_skoala_PictureRecorder__1nFinishRecordingAsDrawable", nullptr,
              reinterpret_cast<void *>(SkoalaPictureRecorder1nFinishRecordingAsDrawable)},
             {"_skoala_PictureRecorder__1nFinishRecordingAsPictureWithCull", nullptr,
              reinterpret_cast<void *>(SkoalaPictureRecorder1nFinishRecordingAsPictureWithCull)},
             {"_skoala_PictureRecorder__1nMake", nullptr, reinterpret_cast<void *>(SkoalaPictureRecorder1nMake)},
             {"_skoala_Canvas__1nDrawOval", nullptr, reinterpret_cast<void *>(SkoalaCanvas1nDrawOval)},
             {"_skoala_paragraph_ParagraphBuilder__1nMake", nullptr,
              reinterpret_cast<void *>(SkoalaParagraphParagraphBuilder1nMake)},
             {"_skoala_paragraph_ParagraphBuilder__1nGetFinalizer", nullptr,
              reinterpret_cast<void *>(SkoalaParagraphParagraphBuilder1nGetFinalizer)},
             {"_skoala_paragraph_ParagraphBuilder__1nPushStyle", nullptr,
              reinterpret_cast<void *>(SkoalaParagraphParagraphBuilder1nPushStyle)},
             {"_skoala_paragraph_ParagraphBuilder__1nAddText", nullptr,
              reinterpret_cast<void *>(SkoalaParagraphParagraphBuilder1nAddText)},
             {"_skoala_paragraph_ParagraphBuilder__1nBuild", nullptr,
              reinterpret_cast<void *>(SkoalaParagraphParagraphBuilder1nBuild)},
             {"_skoala_paragraph_FontCollection__1nMake", nullptr,
              reinterpret_cast<void *>(SkoalaParagraphFontCollection1nMake)},
             {"_skoala_paragraph_FontCollection__1nSetDefaultFontManager", nullptr,
              reinterpret_cast<void *>(SkoalaParagraphFontCollection1nSetDefaultFontManager)},
             {"_skoala_FontMgr__1nDefault", nullptr, reinterpret_cast<void *>(SkoalaFontMgr1nDefault)},
             {"_skoala_paragraph_ParagraphStyle__1nMake", nullptr,
              reinterpret_cast<void *>(SkoalaParagraphParagraphStyle1nMake)},
             {"_skoala_paragraph_Paragraph__1nLayout", nullptr,
              reinterpret_cast<void *>(SkoalaParagraphParagraph1nLayout)},
             {"_skoala_paragraph_Paragraph__1nPaint", nullptr,
              reinterpret_cast<void *>(SkoalaParagraphParagraph1nPaint)},
             {"_skoala_paragraph_TextStyle__1nMake", nullptr, reinterpret_cast<void *>(SkoalaParagraphTextStyle1nMake)},
             {"_skoala_paragraph_TextStyle__1nSetColor", nullptr,
              reinterpret_cast<void *>(SkoalaParagraphTextStyle1nSetColor)},
             {"_skoala_paragraph_TextStyle__1nSetFontSize", nullptr,
              reinterpret_cast<void *>(SkoalaParagraphTextStyle1nSetFontSize)},
             {"_skoala_ManagedString__1nMake", nullptr, reinterpret_cast<void *>(SkoalaManagedString1nMake)},
             {"_skoala_Paint__1nGetFinalizer", nullptr, reinterpret_cast<void *>(SkoalaPaint1nGetFinalizer)},
             {"_skoala_impl_Managed__invokeFinalizer", nullptr,
              reinterpret_cast<void *>(SkoalaImplManagedInvokeFinalizer)},
             {"_skoala_impl_RefCnt__getFinalizer", nullptr, reinterpret_cast<void *>(SkoalaImplRefCntGetFinalizer)},
             {"_skoala_paragraph_ParagraphStyle__1nGetFinalizer", nullptr,
              reinterpret_cast<void *>(SkoalaParagraphParagraphStyle1nGetFinalizer)},
             {"_skoala_paragraph_TextStyle__1nGetFinalizer", nullptr,
              reinterpret_cast<void *>(SkoalaParagraphTextStyle1nGetFinalizer)},
             {"_skoala_paragraph_Paragraph__1nGetFinalizer", nullptr,
              reinterpret_cast<void *>(SkoalaParagraphParagraph1nGetFinalizer)},
             {"_skoala_ManagedString__1nGetFinalizer", nullptr,
              reinterpret_cast<void *>(SkoalaManagedString1nGetFinalizer)},
             {"_getPeerFactory", nullptr, reinterpret_cast<void *>(GetPeerFactory)},
             {"_getEngine", nullptr, reinterpret_cast<void *>(GetEngine)},
             {"_skoala_enqueue_run", nullptr, reinterpret_cast<void *>(SkoalaEnqueueRun)}}};

        auto global_class = env_->FindClass("ETSGLOBAL");
        if (global_class == nullptr) {
            return false;
        }

        return env_->RegisterNatives(global_class, impls.data(), impls.size()) == ETS_OK;
    }

    bool DestroyRuntime() const
    {
        return vm_->DestroyEtsVM() == ETS_OK;
    }

    int ExecuteMain() const
    {
        auto klass = env_->FindClass("ETSGLOBAL");
        auto method = env_->GetStaticp_method(klass, "main", ":I");
        return env_->CallStaticIntMethod(klass, method);  // NOLINT(cppcoreguidelines-pro-type-vararg)
    }

private:
    EtsVM *vm_ {};
    EtsEnv *env_ {};
};

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
TEST_F(EtsVMConfingNapi, PeasINT)
{
    std::string stdlib_abc;
    std::string path_abc;

#if defined(STDLIB_ABC) && defined(PATH_ABC)
#define ETS_UNIT_STRING_STEP(s) #s
#define ETS_UNIT_STRING(s) ETS_UNIT_STRING_STEP(s)
    stdlib_abc = ETS_UNIT_STRING(STDLIB_ABC);
    path_abc = ETS_UNIT_STRING(PATH_ABC);
#undef ETS_UNIT_STRING
#undef ETS_UNIT_STRING_STEP
#endif

    ASSERT_FALSE(stdlib_abc.empty());
    ASSERT_FALSE(path_abc.empty());

    ASSERT_TRUE(CreateRuntime(stdlib_abc, path_abc, false));
    EXPECT_TRUE(InitExports());
    EXPECT_TRUE(ExecuteMain() == 0);
    ASSERT_TRUE(DestroyRuntime());
}

TEST_F(EtsVMConfingNapi, PeasJIT)
{
    std::string stdlib_abc;
    std::string path_abc;

#if defined(STDLIB_ABC) && defined(PATH_ABC)
#define ETS_UNIT_STRING_STEP(s) #s
#define ETS_UNIT_STRING(s) ETS_UNIT_STRING_STEP(s)
    stdlib_abc = ETS_UNIT_STRING(STDLIB_ABC);
    path_abc = ETS_UNIT_STRING(PATH_ABC);
#undef ETS_UNIT_STRING
#undef ETS_UNIT_STRING_STEP
#endif

    ASSERT_FALSE(stdlib_abc.empty());
    ASSERT_FALSE(path_abc.empty());

    ASSERT_TRUE(CreateRuntime(stdlib_abc, path_abc, true));
    EXPECT_TRUE(InitExports());
    EXPECT_TRUE(ExecuteMain() == 0);
    ASSERT_TRUE(DestroyRuntime());
}

TEST_F(EtsVMConfingNapi, PeasAOT)
{
#ifdef HOST_CROSSCOMPILING
    GTEST_SKIP();
#endif

    std::string stdlib_abc;
    std::string path_abc;
    std::string path_an;

#if defined(STDLIB_ABC) && defined(PATH_ABC)
#define ETS_UNIT_STRING_STEP(s) #s
#define ETS_UNIT_STRING(s) ETS_UNIT_STRING_STEP(s)
    stdlib_abc = ETS_UNIT_STRING(STDLIB_ABC);
    path_abc = ETS_UNIT_STRING(PATH_ABC);
    path_an = ETS_UNIT_STRING(PATH_AN);
#undef ETS_UNIT_STRING
#undef ETS_UNIT_STRING_STEP
#endif

    ASSERT_FALSE(stdlib_abc.empty());
    ASSERT_FALSE(path_abc.empty());
    ASSERT_FALSE(path_an.empty());

    ASSERT_TRUE(CreateRuntime(stdlib_abc, path_abc, false, path_an));
    EXPECT_TRUE(InitExports());
    EXPECT_TRUE(ExecuteMain() == 0);
    ASSERT_TRUE(DestroyRuntime());
}
// NOLINTEND(cppcoreguidelines-macro-usage)

}  // namespace panda::ets::test
