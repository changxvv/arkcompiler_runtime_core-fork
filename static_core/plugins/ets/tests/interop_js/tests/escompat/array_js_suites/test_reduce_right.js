/**
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

const { etsVm, getTestModule } = require("escompat.test.js")

const ets_mod = getTestModule("escompat_test");
const GCJSRuntimeCleanup = ets_mod.getFunction("GCJSRuntimeCleanup");
const FooClass = ets_mod.getClass("FooClass");
const CreateEtsSample = ets_mod.getFunction("Array_CreateEtsSample");
const TestJSReduceRight = ets_mod.getFunction("Array_TestJSReduceRight");

{   // Test JS Array<FooClass>
    TestJSReduceRight(new Array(new FooClass("zero"), new FooClass("one")));
}

{   // Test ETS Array<Object>
    let arr = CreateEtsSample();

    function fn_reduce(a, b) { return a; }
    let reduced = arr.reduceRight(fn_reduce);
    ASSERT_EQ(reduced, arr.at(0));

    function fn_reduce2(a, b) { return b; }
    let reduced2 = arr.reduceRight(fn_reduce2, "initVal");
    ASSERT_EQ(reduced, arr.at(1));
}

GCJSRuntimeCleanup();