/*---
Copyright (c) 2021-2022 Huawei Device Co., Ltd.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
---*/

/*---
desc: Interface level scope, public access
---*/

let x = 10

interface I {
    static foo(): int {
        return x + 1;
    }
    static bar(): int {
        return I.foo() + 2;
    }
}

function main(): void {
    assert I.foo() == 11
    assert I.bar() == 13

    // module level scope is not affected
    assert x == 10
}
