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

function main(): int {
    assert GC.getObjectAddress(null) == 0: "getObjectAddress for null must return 0";
    let obj = new Object();
    assert GC.getObjectAddress(obj) > 0: "getObjectAddress for regular object must return value > 0";
    assert GC.getObjectAddress("non-movable string") > 0: "getObjectAddress for non-movable must return value > 0";
    let h_obj : Long[] = new Long[1024 * 1024];
    assert GC.getObjectAddress(h_obj) > 0: "getObjectAddress for humongous must return value > 0";
    return 0;
}
