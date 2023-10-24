/**
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

import { cons, car, cdr, sum, TreeNode, DynObject, Empty, make_swappable, StaticClass, extract_squared_int, ObjectWithPrototype, undefined }
    from "/plugins/ets/tests/interop_js/tests/checked/js_call/js_call.js"

//! CHECKER      Several js function calls JIT (slow)
//! RUN          force_jit: true, entry: "call_sequence", options: "--compiler-regex=ETSGLOBAL::call_sequence --compiler-enable-fast-interop=false"
//! METHOD       "call_sequence"
//! PASS_AFTER   "IrBuilder"
//! INST         /CallStatic.*jscall::invoke/
//! INST_NEXT    /CallStatic.*std.interop.js.JSRuntime::getValueInt/
//! INST_NEXT    /CallStatic.*jscall::invoke/
//! INST_NEXT    "Intrinsic.JSRuntimeGetValueDouble"
//! INST_NEXT    /CallStatic.*jscall::invoke/
//! INST_NEXT    "Intrinsic.JSRuntimeGetValueString"

//! CHECKER      Several js function calls JIT (fast)
//! RUN          force_jit: true, entry: "call_sequence", options: "--compiler-regex=ETSGLOBAL::call_sequence"
//! METHOD       "call_sequence"
//! PASS_AFTER   "IrBuilder"
//! INST_NOT     /CallStatic.*jscall::invoke/
//! INST_COUNT   "Intrinsic.CompilerCreateLocalScope", 3
//! INST_COUNT   "Intrinsic.CompilerConvertJSValueToLocal", 3
//! INST_COUNT   "Intrinsic.CompilerResolveQualifiedJSCall", 3
//! INST_COUNT   /LoadImmediate.*string/, 3
//! INST_COUNT   "Intrinsic.CompilerGetJSNamedProperty", 3
//! INST_COUNT   "Intrinsic.CompilerJSCallCheck", 3
//! INST_COUNT   "Intrinsic.CompilerConvertI32ToLocal", 4
//! INST_COUNT   "Intrinsic.CompilerConvertF64ToLocal", 1
//! INST_COUNT   "Intrinsic.CompilerConvertRefTypeToLocal", 1
//! INST_COUNT   "Intrinsic.CompilerJSCallFunction", 3
//! INST_COUNT   "Intrinsic.CompilerConvertLocalToJSValue", 3
//! INST_COUNT   "Intrinsic.CompilerDestroyLocalScope", 3
//! INST         /CallStatic.*std.interop.js.JSRuntime::getValueInt/
//! INST         "Intrinsic.JSRuntimeGetValueDouble"
//! INST         "Intrinsic.JSRuntimeGetValueString"

//! CHECKER      Several js function calls AOT (slow)
//! RUN_PAOC     options: "--compiler-regex=ETSGLOBAL::call_sequence --compiler-enable-fast-interop=false"
//! METHOD       "call_sequence"
//! PASS_AFTER   "IrBuilder"
//! INST         /CallStatic.*jscall::invoke/
//! INST_NEXT    /CallStatic.*JSRuntime::getValueInt/
//! INST_NEXT    /CallStatic.*jscall::invoke/
//! INST_NEXT    "Intrinsic.JSRuntimeGetValueDouble"
//! INST_NEXT    /CallStatic.*jscall::invoke/
//! INST_NEXT    "Intrinsic.JSRuntimeGetValueString"
//! RUN          entry: "call_sequence"

//! CHECKER      Several js function calls AOT (fast)
//! RUN_PAOC     options: "--compiler-regex=ETSGLOBAL::call_sequence"
//! METHOD       "call_sequence"
//! PASS_AFTER   "IrBuilder"
//! INST_NOT     /CallStatic.*jscall::invoke/
//! INST_COUNT   "Intrinsic.CompilerCreateLocalScope", 3
//! INST_COUNT   "Intrinsic.CompilerConvertJSValueToLocal", 3
//! INST_COUNT   "Intrinsic.CompilerResolveQualifiedJSCall", 3
//! INST_COUNT   /LoadImmediate.*PandaFileOffset/, 3
//! INST_COUNT   "Intrinsic.CompilerGetJSNamedProperty", 3
//! INST_COUNT   "Intrinsic.CompilerJSCallCheck", 3
//! INST_COUNT   "Intrinsic.CompilerConvertI32ToLocal", 4
//! INST_COUNT   "Intrinsic.CompilerConvertF64ToLocal", 1
//! INST_COUNT   "Intrinsic.CompilerConvertRefTypeToLocal", 1
//! INST_COUNT   "Intrinsic.CompilerJSCallFunction", 3
//! INST_COUNT   "Intrinsic.CompilerConvertLocalToJSValue", 3
//! INST_COUNT   "Intrinsic.CompilerDestroyLocalScope", 3
//! INST         /CallStatic.*std.interop.js.JSRuntime::getValueInt/
//! INST         "Intrinsic.JSRuntimeGetValueDouble"
//! INST         "Intrinsic.JSRuntimeGetValueString"
//! RUN          entry: "call_sequence"

function call_sequence(): int {
    for (let i = 0; i < 10; i++) {
        let sum_int = sum(i, 2) as int;
        let sum_double = sum(i, 0.5) as double;
        let sum_string = sum("abc", i) as string;
        if (sum_int != i + 2) {
            return 1;
        }
        if (abs(i + 0.5 - sum_double) > 1e-6) {
            return 2;
        }
        if (sum_string != "abc" + i) {
            return 3;
        }
    }
    return 0;
}

//! CHECKER      Chain of js function calls JIT (slow)
//! RUN          force_jit: true, entry: "call_chain", options: "--compiler-regex=ETSGLOBAL::call_chain --compiler-enable-fast-interop=false"
//! METHOD       "call_chain"
//! PASS_AFTER   "IrBuilder"
//! INST_COUNT   /CallStatic.*jscall::invoke/, 6
//! INST         /CallStatic.*JSRuntime::getValueInt/

//! CHECKER      Chain of js function calls JIT (fast)
//! RUN          force_jit: true, entry: "call_chain", options: "--compiler-regex=ETSGLOBAL::call_chain"
//! METHOD       "call_chain"
//! PASS_AFTER   "IrBuilder"
//! INST_NOT     /CallStatic.*jscall::invoke/
//! INST_COUNT   "Intrinsic.CompilerCreateLocalScope", 6
//! INST_COUNT   "Intrinsic.CompilerConvertJSValueToLocal", 6
//! INST_COUNT   "Intrinsic.CompilerResolveQualifiedJSCall", 6
//! INST_COUNT   /LoadImmediate.*string/, 6
//! INST_COUNT   "Intrinsic.CompilerGetJSNamedProperty", 6
//! INST_COUNT   "Intrinsic.CompilerJSCallCheck", 6
//! INST_COUNT   "Intrinsic.CompilerConvertI32ToLocal", 3
//! INST_COUNT   "Intrinsic.CompilerConvertRefTypeToLocal", 6
//! INST_COUNT   "Intrinsic.CompilerJSCallFunction", 6
//! INST_COUNT   "Intrinsic.CompilerConvertLocalToJSValue", 6
//! INST_COUNT   "Intrinsic.CompilerDestroyLocalScope", 6
//! INST         /CallStatic.*JSRuntime::getValueInt/

//! CHECKER      Chain of js function calls AOT (slow)
//! RUN_PAOC     options: "--compiler-regex=ETSGLOBAL::call_chain --compiler-enable-fast-interop=false"
//! METHOD       "call_chain"
//! PASS_AFTER   "Codegen"
//! INST_COUNT   /CallStatic.*jscall::invoke/, 6
//! INST         /CallStatic.*JSRuntime::getValueInt/
//! RUN          entry: "call_chain"

//! CHECKER      Chain of js function calls AOT (fast)
//! RUN_PAOC     options: "--compiler-regex=ETSGLOBAL::call_chain"
//! METHOD       "call_chain"
//! PASS_AFTER   "IrBuilder"
//! INST_NOT     /CallStatic.*jscall::invoke/
//! INST_COUNT   "Intrinsic.CompilerCreateLocalScope", 6
//! INST_COUNT   "Intrinsic.CompilerConvertJSValueToLocal", 6
//! INST_COUNT   "Intrinsic.CompilerResolveQualifiedJSCall", 6
//! INST_COUNT   /LoadImmediate.*PandaFileOffset/, 6
//! INST_COUNT   "Intrinsic.CompilerGetJSNamedProperty", 6
//! INST_COUNT   "Intrinsic.CompilerJSCallCheck", 6
//! INST_COUNT   "Intrinsic.CompilerConvertI32ToLocal", 3
//! INST_COUNT   "Intrinsic.CompilerConvertRefTypeToLocal", 6
//! INST_COUNT   "Intrinsic.CompilerJSCallFunction", 6
//! INST_COUNT   "Intrinsic.CompilerConvertLocalToJSValue", 6
//! INST_COUNT   "Intrinsic.CompilerDestroyLocalScope", 6
//! INST         /CallStatic.*JSRuntime::getValueInt/
//! RUN          entry: "call_chain"

function call_chain(): int {
    for (let i = 0; i < 10; i++) {
        let list = cons(0, cons(1, cons(i, undefined)));
        let tail = car(cdr(cdr(list)));
        if (tail as int != i) {
            return 1;
        }
    }
    return 0;
}

//! CHECKER      Chain of js new calls JIT (slow)
//! RUN          force_jit: true, entry: "new_chain", options: "--compiler-regex=ETSGLOBAL::new_chain --compiler-enable-fast-interop=false"
//! METHOD       "new_chain"
//! PASS_AFTER   "IrBuilder"
//! INST_COUNT   /CallStatic.*jsnew::invoke/, 3
//! INST_COUNT   /CallStatic.*jscall::invoke/, 1
//! INST_COUNT   /CallStatic.*JSRuntime::getValueInt/, 1

//! CHECKER      Chain of js new calls JIT (fast)
//! RUN          force_jit: true, entry: "new_chain", options: "--compiler-regex=ETSGLOBAL::new_chain"
//! METHOD       "new_chain"
//! PASS_AFTER   "IrBuilder"
//! INST_NOT     /CallStatic.*jsnew::invoke/
//! INST_NOT     /CallStatic.*jscall::invoke/
//! INST_COUNT   "Intrinsic.CompilerCreateLocalScope", 4
//! INST_COUNT   "Intrinsic.CompilerConvertJSValueToLocal", 4
//! INST_COUNT   "Intrinsic.CompilerResolveQualifiedJSCall", 4
//! INST_COUNT   /LoadImmediate.*string/, 4
//! INST_COUNT   "Intrinsic.CompilerGetJSNamedProperty", 4
//! INST_COUNT   "Intrinsic.CompilerJSCallCheck", 4
//! INST_COUNT   "Intrinsic.CompilerConvertI32ToLocal", 3
//! INST_COUNT   "Intrinsic.CompilerConvertRefTypeToLocal", 2
//! INST_COUNT   "Intrinsic.CompilerJSNewInstance", 3
//! INST_COUNT   "Intrinsic.CompilerJSCallFunction", 1
//! INST_COUNT   "Intrinsic.CompilerConvertLocalToJSValue", 4
//! INST_COUNT   "Intrinsic.CompilerDestroyLocalScope", 4
//! INST_COUNT   /CallStatic.*JSRuntime::getValueInt/, 1

//! CHECKER      Chain of js new calls AOT (slow)
//! RUN_PAOC     options: "--compiler-regex=ETSGLOBAL::new_chain --compiler-enable-fast-interop=false"
//! METHOD       "new_chain"
//! PASS_AFTER   "Codegen"
//! INST_COUNT   /CallStatic.*jsnew::invoke/, 3
//! INST_COUNT   /CallStatic.*jscall::invoke/, 1
//! INST_COUNT   /CallStatic.*JSRuntime::getValueInt/, 1
//! RUN          entry: "new_chain"

//! CHECKER      Chain of js new calls AOT (fast)
//! RUN_PAOC     options: "--compiler-regex=ETSGLOBAL::new_chain"
//! METHOD       "new_chain"
//! PASS_AFTER   "IrBuilder"
//! INST_NOT     /CallStatic.*jsnew::invoke/
//! INST_NOT     /CallStatic.*jscall::invoke/
//! INST_COUNT   "Intrinsic.CompilerCreateLocalScope", 4
//! INST_COUNT   "Intrinsic.CompilerConvertJSValueToLocal", 4
//! INST_COUNT   "Intrinsic.CompilerResolveQualifiedJSCall", 4
//! INST_COUNT   /LoadImmediate.*PandaFileOffset/, 4
//! INST_COUNT   "Intrinsic.CompilerGetJSNamedProperty", 4
//! INST_COUNT   "Intrinsic.CompilerJSCallCheck", 4
//! INST_COUNT   "Intrinsic.CompilerConvertI32ToLocal", 3
//! INST_COUNT   "Intrinsic.CompilerConvertRefTypeToLocal", 2
//! INST_COUNT   "Intrinsic.CompilerJSNewInstance", 3
//! INST_COUNT   "Intrinsic.CompilerJSCallFunction", 1
//! INST_COUNT   "Intrinsic.CompilerConvertLocalToJSValue", 4
//! INST_COUNT   "Intrinsic.CompilerDestroyLocalScope", 4
//! INST_COUNT   /CallStatic.*JSRuntime::getValueInt/, 1
//! RUN          entry: "new_chain"

function new_chain(): int {
    let last = new TreeNode(0);
    for (let i = 0; i < 10; i++) {
        let right = new TreeNode(100);
        let current = new TreeNode(i, last, right);
        last = current;
    }
    let js_tree_sum = last.sum() as int;
    if (js_tree_sum != 1045) {
        return 1;
    }
    return 0;
}

//! CHECKER      JS load and store JIT (slow)
//! RUN          force_jit: true, entry: "load_store_chain", options: "--compiler-regex=ETSGLOBAL::load_store_chain --compiler-enable-fast-interop=false"
//! METHOD       "load_store_chain"
//! PASS_AFTER   "IrBuilder"
//! INST         /CallStatic.*jsnew::invoke/
//! INST_NEXT    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeSetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeSetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeStrictEqual"

//! CHECKER      JS load and store JIT (fast)
//! RUN          force_jit: true, entry: "load_store_chain", options: "--compiler-regex=ETSGLOBAL::load_store_chain"
//! METHOD       "load_store_chain"
//! PASS_AFTER   "IrBuilder"
//! INST_NOT     /CallStatic.*jsnew::invoke/
//! INST         "Intrinsic.CompilerCreateLocalScope"
//! INST_NEXT    "Intrinsic.CompilerConvertJSValueToLocal"
//! INST_NEXT    "Intrinsic.CompilerResolveQualifiedJSCall"
//! INST_NEXT    /LoadImmediate.*string/
//! INST_NEXT    "Intrinsic.CompilerGetJSNamedProperty"
//! INST_NEXT    "Intrinsic.CompilerJSCallCheck"
//! INST_NEXT    "Intrinsic.CompilerConvertI32ToLocal"
//! INST_NEXT    "Intrinsic.CompilerJSNewInstance"
//! INST_NEXT    "Intrinsic.CompilerConvertLocalToJSValue"
//! INST_NEXT    "Intrinsic.CompilerDestroyLocalScope"
//! INST_NEXT    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeSetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeSetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeStrictEqual"

//! CHECKER      JS load and store AOT (slow)
//! RUN_PAOC     options: "--compiler-regex=ETSGLOBAL::load_store_chain --compiler-enable-fast-interop=false"
//! METHOD       "load_store_chain"
//! PASS_AFTER   "Codegen"
//! INST         /CallStatic.*jsnew::invoke/
//! INST_NEXT    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeSetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeSetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeStrictEqual"
//! RUN          entry: "load_store_chain"

//! CHECKER      JS load and store AOT (fast)
//! RUN_PAOC     options: "--compiler-regex=ETSGLOBAL::load_store_chain"
//! METHOD       "load_store_chain"
//! PASS_AFTER   "IrBuilder"
//! INST_NOT     /CallStatic.*jsnew::invoke/
//! INST         "Intrinsic.CompilerCreateLocalScope"
//! INST_NEXT    "Intrinsic.CompilerConvertJSValueToLocal"
//! INST_NEXT    "Intrinsic.CompilerResolveQualifiedJSCall"
//! INST_NEXT    /LoadImmediate.*PandaFileOffset/
//! INST_NEXT    "Intrinsic.CompilerGetJSNamedProperty"
//! INST_NEXT    "Intrinsic.CompilerJSCallCheck"
//! INST_NEXT    "Intrinsic.CompilerConvertI32ToLocal"
//! INST_NEXT    "Intrinsic.CompilerJSNewInstance"
//! INST_NEXT    "Intrinsic.CompilerConvertLocalToJSValue"
//! INST_NEXT    "Intrinsic.CompilerDestroyLocalScope"
//! INST_NEXT    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeSetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeSetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    "Intrinsic.JSRuntimeStrictEqual"
//! RUN          entry: "load_store_chain"

function load_store_chain(): int {
    for (let i = 0; i < 10; i++) {
        let obj = new DynObject(i);
        obj.v1 = obj.v0;
        obj.v2 = obj.v1;
        if (obj.v2 !== obj.v0) {
            return 1;
        }
    }
    return 0;
}

//! CHECKER      Define object method in js and call it from ets, JIT (slow)
//! RUN          force_jit: true, entry: "method_call", options: "--compiler-regex=ETSGLOBAL::method_call --compiler-enable-fast-interop=false"
//! METHOD       "method_call"
//! PASS_AFTER   "IrBuilder"
//! INST_COUNT   /CallStatic.*jsnew::invoke/, 1
//! INST_COUNT   /CallStatic.*jscall::invoke/, 5 # one function call and four method calls

//! CHECKER      Define object method in js and call it from ets, JIT (fast)
//! RUN          force_jit: true, entry: "method_call", options: "--compiler-regex=ETSGLOBAL::method_call"
//! METHOD       "method_call"
//! PASS_AFTER   "IrBuilder"
//! INST_NOT     /CallStatic.*jsnew::invoke/
//! INST_NOT     /CallStatic.*jscall::invoke/
//! INST_COUNT   "Intrinsic.CompilerCreateLocalScope", 6
//! INST_COUNT   "Intrinsic.CompilerConvertJSValueToLocal", 6
//! INST_COUNT   "Intrinsic.CompilerResolveQualifiedJSCall", 6
//! INST_COUNT   /LoadImmediate.*string/, 6
//! INST_COUNT   "Intrinsic.CompilerGetJSNamedProperty", 6
//! INST_COUNT   "Intrinsic.CompilerJSCallCheck", 6
//! INST_COUNT   "Intrinsic.CompilerConvertRefTypeToLocal", 1
//! INST_COUNT   "Intrinsic.CompilerJSNewInstance", 1
//! INST_COUNT   "Intrinsic.CompilerJSCallFunction", 5
//! INST_COUNT   "Intrinsic.CompilerConvertLocalToJSValue", 6
//! INST_COUNT   "Intrinsic.CompilerDestroyLocalScope", 6
//! INST_COUNT   /CallStatic.*JSRuntime::getValueInt/, 4

//! CHECKER      Define object method in js and call it from ets, AOT (slow)
//! RUN_PAOC     options: "--compiler-regex=ETSGLOBAL::method_call --compiler-enable-fast-interop=false"
//! METHOD       "method_call"
//! PASS_AFTER   "IrBuilder"
//! INST_COUNT   /CallStatic.*jsnew::invoke/, 1
//! INST_COUNT   /CallStatic.*jscall::invoke/, 5
//! RUN          entry: "method_call"

//! CHECKER      Define object method in js and call it from ets, AOT (fast)
//! RUN_PAOC     options: "--compiler-regex=ETSGLOBAL::method_call"
//! METHOD       "method_call"
//! PASS_AFTER   "IrBuilder"
//! INST_NOT     /CallStatic.*jsnew::invoke/
//! INST_NOT     /CallStatic.*jscall::invoke/
//! INST_COUNT   "Intrinsic.CompilerCreateLocalScope", 6
//! INST_COUNT   "Intrinsic.CompilerConvertJSValueToLocal", 6
//! INST_COUNT   "Intrinsic.CompilerResolveQualifiedJSCall", 6
//! INST_COUNT   /LoadImmediate.*PandaFileOffset/, 6
//! INST_COUNT   "Intrinsic.CompilerGetJSNamedProperty", 6
//! INST_COUNT   "Intrinsic.CompilerJSCallCheck", 6
//! INST_COUNT   "Intrinsic.CompilerConvertRefTypeToLocal", 1
//! INST_COUNT   "Intrinsic.CompilerJSNewInstance", 1
//! INST_COUNT   "Intrinsic.CompilerJSCallFunction", 5
//! INST_COUNT   "Intrinsic.CompilerConvertLocalToJSValue", 6
//! INST_COUNT   "Intrinsic.CompilerDestroyLocalScope", 6
//! INST_COUNT   /CallStatic.*JSRuntime::getValueInt/, 4
//! RUN          entry: "method_call"

function method_call(): int {
    for (let i = 0; i < 5; i++) {
        let pair = new Empty();
        pair.first = -i;
        pair.second = i;
        make_swappable(pair);
        pair.swap();
        if (pair.first as int != i || pair.second as int != -i) {
            return 1;
        }
        pair.swap();
        pair.swap();
        pair.swap();
        if (pair.first as int != -i || pair.second as int != i) {
            return 2;
        }
    }
    return 0;
}

//! CHECKER       Call JS static method and access static property, JIT (slow)
//! RUN           force_jit: true, entry: "static_method_call", options: "--compiler-regex=ETSGLOBAL::static_method_call --compiler-enable-fast-interop=false"
//! METHOD        "static_method_call"
//! PASS_AFTER    "IrBuilder"
//! INST          "Intrinsic.JSRuntimeGetPropertyJSValue" # get StaticClass for lhs
//! INST_NEXT     "Intrinsic.JSRuntimeGetPropertyJSValue" # get StaticClass for rhs
//! INST_NEXT     "Intrinsic.JSRuntimeGetPropertyJSValue" # get StaticClass.staticProperty
//! INST_NEXT_NOT "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT     /CallStatic.*JSRuntime::getValueInt/
//! INST_NEXT     /CallStatic.*JSRuntime::setPropertyInt/ # set StaticClass.staticProperty
//! INST_NEXT     /CallStatic.*jscall::invoke/
//! INST_NEXT     /CallStatic.*JSRuntime::getValueInt/

//! CHECKER         Call JS static method and access static property, JIT (fast)
//! RUN             force_jit: true, entry: "static_method_call", options: "--compiler-regex=ETSGLOBAL::static_method_call"
//! METHOD          "static_method_call"
//! PASS_AFTER      "IrBuilder"
//! INST            "Intrinsic.JSRuntimeGetPropertyJSValue" # get StaticClass for lhs
//! INST_NEXT       "Intrinsic.JSRuntimeGetPropertyJSValue" # get StaticClass for rhs
//! INST_NEXT       "Intrinsic.JSRuntimeGetPropertyJSValue" # get StaticClass.staticProperty
//! INST_NEXT_NOT   "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT       /CallStatic.*JSRuntime::getValueInt/
//! INST_NEXT       /CallStatic.*JSRuntime::setPropertyInt/ # set StaticClass.staticProperty
//! INST_NEXT_NOT   /CallStatic.*jscall::invoke/
//! INST_NEXT       "Intrinsic.CompilerCreateLocalScope"
//! INST_NEXT       "Intrinsic.CompilerConvertJSValueToLocal"
//! INST_NEXT       "Intrinsic.CompilerResolveQualifiedJSCall"
//! INST_NEXT       /LoadImmediate.*string/
//! INST_NEXT       "Intrinsic.CompilerGetJSNamedProperty"
//! INST_NEXT       "Intrinsic.CompilerJSCallCheck"
//! INST_NEXT       "Intrinsic.CompilerJSCallFunction"
//! INST_NEXT       "Intrinsic.CompilerConvertLocalToJSValue"
//! INST_NEXT       "Intrinsic.CompilerDestroyLocalScope"
//! INST_NEXT       /CallStatic.*JSRuntime::getValueInt/

//! CHECKER      Call JS static method and access static property, AOT (slow)
//! RUN_PAOC     options: "--compiler-regex=ETSGLOBAL::static_method_call --compiler-enable-fast-interop=false"
//! METHOD       "static_method_call"
//! PASS_AFTER   "IrBuilder"
//! INST_COUNT   "Intrinsic.JSRuntimeGetPropertyJSValue", 3
//! INST         /CallStatic.*JSRuntime::getValueInt/
//! INST_NEXT    /CallStatic.*JSRuntime::setPropertyInt/
//! INST_NEXT    /CallStatic.*jscall::invoke/
//! INST_NEXT    /CallStatic.*JSRuntime::getValueInt/
//! RUN          entry: "static_method_call"

//! CHECKER         Call JS static method and access static property, AOT (fast)
//! RUN_PAOC        options: "--compiler-regex=ETSGLOBAL::static_method_call"
//! METHOD          "static_method_call"
//! PASS_AFTER      "IrBuilder"
//! INST            "Intrinsic.JSRuntimeGetPropertyJSValue" # get StaticClass for lhs
//! INST_NEXT       "Intrinsic.JSRuntimeGetPropertyJSValue" # get StaticClass for rhs
//! INST_NEXT       "Intrinsic.JSRuntimeGetPropertyJSValue" # get StaticClass.staticProperty
//! INST_NEXT_NOT   "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT       /CallStatic.*JSRuntime::getValueInt/
//! INST_NEXT       /CallStatic.*JSRuntime::setPropertyInt/ # set StaticClass.staticProperty
//! INST_NEXT_NOT   /CallStatic.*jscall::invoke/
//! INST_NEXT       "Intrinsic.CompilerCreateLocalScope"
//! INST_NEXT       "Intrinsic.CompilerConvertJSValueToLocal"
//! INST_NEXT       "Intrinsic.CompilerResolveQualifiedJSCall"
//! INST_NEXT       /LoadImmediate.*PandaFileOffset/
//! INST_NEXT       "Intrinsic.CompilerGetJSNamedProperty"
//! INST_NEXT       "Intrinsic.CompilerJSCallCheck"
//! INST_NEXT       "Intrinsic.CompilerJSCallFunction"
//! INST_NEXT       "Intrinsic.CompilerConvertLocalToJSValue"
//! INST_NEXT       "Intrinsic.CompilerDestroyLocalScope"
//! INST_NEXT       /CallStatic.*JSRuntime::getValueInt/
//! RUN             entry: "static_method_call"

function static_method_call(): int {
    for (let i = 1; i <= 5; i++) {
        StaticClass.staticProperty = StaticClass.staticProperty as int + 1;
        if (StaticClass.staticMethod() as int != 110 + i) {
            return 1;
        }
    }
    return 0;
}

class EtsClass {
    int_value: int;
    constructor(value: int) {
        this.int_value = value;
    }
}

//! CHECKER      Use ets object from js, JIT (slow)
//! RUN          force_jit: true, entry: "js_call_ets_object", options: "--compiler-regex=ETSGLOBAL::js_call_ets_object --compiler-enable-fast-interop=false"
//! METHOD       "js_call_ets_object"
//! PASS_AFTER   "IrBuilder"
//! INST         /CallStatic.*jscall::invoke/
//! INST_NEXT    /CallStatic.*JSRuntime::getValueInt/

//! CHECKER     Use ets object from js, JIT (fast)
//! RUN         force_jit: true, entry: "js_call_ets_object", options: "--compiler-regex=ETSGLOBAL::js_call_ets_object"
//! METHOD     "js_call_ets_object"
//! PASS_AFTER "IrBuilder"
//! INST_NOT   /CallStatic.*jscall::invoke/
//! INST       "Intrinsic.CompilerCreateLocalScope"
//! INST_NEXT  "Intrinsic.CompilerConvertJSValueToLocal"
//! INST_NEXT  "Intrinsic.CompilerResolveQualifiedJSCall"
//! INST_NEXT  /LoadImmediate.*string/
//! INST_NEXT  "Intrinsic.CompilerGetJSNamedProperty"
//! INST_NEXT  "Intrinsic.CompilerJSCallCheck"
//! INST_NEXT  "Intrinsic.CompilerConvertRefTypeToLocal"
//! INST_NEXT  "Intrinsic.CompilerJSCallFunction"
//! INST_NEXT  "Intrinsic.CompilerConvertLocalToJSValue"
//! INST_NEXT  "Intrinsic.CompilerDestroyLocalScope"
//! INST_NEXT  /CallStatic.*JSRuntime::getValueInt/

//! CHECKER      Use ets object from js, AOT (slow)
//! RUN_PAOC     options: "--compiler-regex=ETSGLOBAL::js_call_ets_object --compiler-enable-fast-interop=false"
//! METHOD       "js_call_ets_object"
//! PASS_AFTER   "IrBuilder"
//! INST         /CallStatic.*jscall::invoke/
//! INST_NEXT    /CallStatic.*JSRuntime::getValueInt/
//! RUN          entry: "js_call_ets_object"

//! CHECKER     Use ets object from js, AOT (fast)
//! RUN_PAOC    options: "--compiler-regex=ETSGLOBAL::js_call_ets_object"
//! METHOD     "js_call_ets_object"
//! PASS_AFTER "IrBuilder"
//! INST_NOT   /CallStatic.*jscall::invoke/
//! INST       "Intrinsic.CompilerCreateLocalScope"
//! INST_NEXT  "Intrinsic.CompilerConvertJSValueToLocal"
//! INST_NEXT  "Intrinsic.CompilerResolveQualifiedJSCall"
//! INST_NEXT  /LoadImmediate.*PandaFileOffset/
//! INST_NEXT  "Intrinsic.CompilerGetJSNamedProperty"
//! INST_NEXT  "Intrinsic.CompilerJSCallCheck"
//! INST_NEXT  "Intrinsic.CompilerConvertRefTypeToLocal"
//! INST_NEXT  "Intrinsic.CompilerJSCallFunction"
//! INST_NEXT  "Intrinsic.CompilerConvertLocalToJSValue"
//! INST_NEXT  "Intrinsic.CompilerDestroyLocalScope"
//! INST_NEXT  /CallStatic.*JSRuntime::getValueInt/
//! RUN        entry: "js_call_ets_object"

function js_call_ets_object(): int {
    for (let i = 1; i <= 5; i++) {
        let ets_object = new EtsClass(i);
        let res = extract_squared_int(ets_object) as int;
        if (res != i * i) {
            return 1;
        }
    }
    return 0;
}

//! CHECKER      Load value from js prototype, JIT 
//! RUN          force_jit: true, entry: "js_load_from_prototype", options: "--compiler-regex=ETSGLOBAL::js_load_from_prototype"
//! METHOD       "js_load_from_prototype"
//! PASS_AFTER   "IrBuilder"
//! INST_NOT         /CallStatic.*jsnew::invoke/
//! INST    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    /CallStatic.*JSRuntime::getValueInt/

//! CHECKER      Load value from js prototype, AOT
//! RUN_PAOC     options: "--compiler-regex=ETSGLOBAL::js_load_from_prototype"
//! METHOD       "js_load_from_prototype"
//! PASS_AFTER   "IrBuilder"
//! INST_NOT         /CallStatic.*jsnew::invoke/
//! INST    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    /CallStatic.*JSRuntime::getValueInt/
//! RUN          entry: "js_load_from_prototype"

function js_load_from_prototype(): int {
    for (let i = 1; i <= 5; i++) {
        let obj = new ObjectWithPrototype();
        if (obj.prototype_value as int != 5) {
            return 1;
        }
    }
    return 0;
}

//! CHECKER      Load value overridden in js object, JIT
//! RUN          force_jit: true, entry: "js_load_overridden", options: "--compiler-regex=ETSGLOBAL::js_load_overridden"
//! METHOD       "js_load_overridden"
//! PASS_AFTER   "IrBuilder"
//! INST_NOT         /CallStatic.*jsnew::invoke/
//! INST    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    /CallStatic.*JSRuntime::getValueInt/

//! CHECKER      Load value overridden in js object, AOT
//! RUN_PAOC     options: "--compiler-regex=ETSGLOBAL::js_load_overridden"
//! METHOD       "js_load_overridden"
//! PASS_AFTER   "IrBuilder"
//! INST_NOT         /CallStatic.*jsnew::invoke/
//! INST    "Intrinsic.JSRuntimeGetPropertyJSValue"
//! INST_NEXT    /CallStatic.*JSRuntime::getValueInt/
//! RUN          entry: "js_load_overridden"

function js_load_overridden(): int {
    for (let i = 1; i <= 5; i++) {
        let obj = new ObjectWithPrototype();
        if (obj.overridden_value as int != 4) {
            return 1;
        }
    }
    return 0;
}

//! CHECKER      Call function from js prototype, JIT
//! RUN          force_jit: true, entry: "js_call_from_prototype", options: "--compiler-regex=ETSGLOBAL::js_call_from_prototype"
//! METHOD       "js_call_from_prototype"
//! PASS_AFTER   "IrBuilder"
//! INST_NOT         /CallStatic.*jsnew::invoke/
//! INST_NOT    /CallStatic.*jscall::invoke/
//! INST    "Intrinsic.JSRuntimeGetValueString"

//! CHECKER      Call function from js prototype, AOT
//! RUN_PAOC     options: "--compiler-regex=ETSGLOBAL::js_call_from_prototype"
//! METHOD       "js_call_from_prototype"
//! PASS_AFTER   "IrBuilder"
//! INST_NOT         /CallStatic.*jsnew::invoke/
//! INST_NOT    /CallStatic.*jscall::invoke/
//! INST    "Intrinsic.JSRuntimeGetValueString"
//! RUN          entry: "js_call_from_prototype"

function js_call_from_prototype(): int {
    for (let i = 1; i <= 5; i++) {
        let obj = new ObjectWithPrototype();
        if (obj.prototype_function() as String != "prototype function") {
            return 1;
        }
    }
    return 0;
}

//! CHECKER      Call function overridden in js object, JIT
//! RUN          force_jit: true, entry: "js_call_overridden", options: "--compiler-regex=ETSGLOBAL::js_call_overridden"
//! METHOD       "js_call_overridden"
//! PASS_AFTER   "IrBuilder"

//! CHECKER      Call function overridden in js object, AOT
//! RUN_PAOC     options: "--compiler-regex=ETSGLOBAL::js_call_overridden"
//! METHOD       "js_call_overridden"
//! RUN          entry: "js_call_overridden"

function js_call_overridden(): int {
    for (let i = 1; i <= 5; i++) {
        let obj = new ObjectWithPrototype();
        if (obj.overridden_function() as String != "overridden") {
            return 1;
        }
    }
    return 0;
}