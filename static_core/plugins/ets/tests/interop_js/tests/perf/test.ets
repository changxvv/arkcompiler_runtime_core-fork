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

class jsvars {
    static global = JSRuntime.getGlobal();
    static js_code = JSRuntime.getPropertyJSValue(jsvars.global, "js_code");
};

class $jscall {
    static _init = JSRuntime.__initJSCallClass("L$jscall;");

    native static void$(top: JSValue, qname: String): void;
    // CHECKER_IGNORE_NEXTLINE(AF0012)
    native static void$10s(top: JSValue, qname: String, v0: String, v1: String, v2: String, v3: String, v4: String, v5: String, v6: String, v7: String, v8: String, v9: String): void;
    // CHECKER_IGNORE_NEXTLINE(AF0012)
    native static void$10d(top: JSValue, qname: String, v0: double, v1: double, v2: double, v3: double, v4: double, v5: double, v6: double, v7: double, v8: double, v9: double): void;

    native static str$(top: JSValue, qname: String): String;
    native static any$(top: JSValue, qname: String): JSValue;
    native static num$(top: JSValue, qname: String): double;
};
class $jsnew {
    static _init = JSRuntime.__initJSNewClass("L$jsnew;");
};

function void_call(iters: int): boolean {
    for (let i = 0; i < iters; ++i) {
        $jscall.void$(jsvars.js_code, ".Nop");
    }
    return true;
}

function void_call_10str(iters: int): boolean {
    let u0: String = "foo0";
    let u1: String = "foo1";
    let u2: String = "foo2";
    let u3: String = "foo3";
    let u4: String = "foo4";
    let u5: String = "foo5";
    let u6: String = "foo6";
    let u7: String = "foo7";
    let u8: String = "foo8";
    let u9: String = "foo9";
    for (let i = 0; i < iters; ++i) {
        $jscall.void$10s(jsvars.js_code, ".Nop", u0, u1, u2, u3, u4, u5, u6, u7, u8, u9);
    }
    return true;
}

function void_call_10dbl(iters: int): boolean {
    let u0: double = 0;
    let u1: double = 1;
    let u2: double = 2;
    let u3: double = 3;
    let u4: double = 4;
    let u5: double = 5;
    let u6: double = 6;
    let u7: double = 7;
    let u8: double = 8;
    let u9: double = 9;
    for (let i = 0; i < iters; ++i) {
        $jscall.void$10d(jsvars.js_code, ".Nop", u0, u1, u2, u3, u4, u5, u6, u7, u8, u9);
    }
    return true;
}

class Empty {
};

function str_call(iters: int): boolean {
    for (let i = 0; i < iters; ++i) {
        let r = $jscall.str$(jsvars.js_code, ".GetString");
    }
    return true;
}

function anystr_call(iters: int): boolean {
    for (let i = 0; i < iters; ++i) {
        let r = $jscall.any$(jsvars.js_code, ".GetString");
    }
    return true;
}

function dbl_call(iters: int): boolean {
    for (let i = 0; i < iters; ++i) {
        let r = $jscall.num$(jsvars.js_code, ".GetNumber");
    }
    return true;
}

function anydbl_call(iters: int): boolean {
    for (let i = 0; i < iters; ++i) {
        let r = $jscall.any$(jsvars.js_code, ".GetNumber");
    }
    return true;
}

function anyobj_call(iters: int): boolean {
    for (let i = 0; i < iters; ++i) {
        let r = $jscall.any$(jsvars.js_code, ".GetObject");
    }
    return true;
}
