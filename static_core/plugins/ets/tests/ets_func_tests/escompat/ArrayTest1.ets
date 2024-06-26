/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

const success : int = 0;
const fail    : int = 1;

function main(): int {
    let failures: int = 0;
    failures += check((): int => { return createDefault()}, "Create default empty Array");
    failures += check((): int => { return createWithGivenZeroSize()}, "Create Array with given zero size");
    failures += check((): int => { return createWithGivenSize()}, "Create Array with given size");
    failures += check((): int => { return createWithNegativeInitialCapacity()}, "Create Array with negative capacity");
    failures += check((): int => { return isArrayRawArrayNumber()}, "isArrayRawArrayNumber");
    failures += check((): int => { return isArrayArrayNumber()}, "isArrayArrayNumber");
    failures += check((): int => { return isArrayNullUndefined()}, "isArrayNullUndefined");
    failures += check((): int => { return push()}, "push");
    failures += check((): int => { return pushECMA()}, "pushECMA");
    failures += check((): int => { return toSplicedArg2()}, "toSplicedArg2");
    failures += check((): int => { return toSplicedArg1()}, "toSplicedArg1");
    return check(failures, "All tests run");
}


const d: Number[] = [10, -1, 237, -5, 148, 65, 3, 34, 0, 12];
const dd: Number[] = [1, 0, 3, 4, 0, 6];

function createDefault(): int {
    let arr = new Array<Number>;
    if (!(arr instanceof Array)) {
        return fail;
    }
    return arr.length == 0 ? success : fail
}

function createWithNegativeInitialCapacity(): int {
    try {
        new Array<Number>(-1);
    } catch(nse: NegativeArraySizeException) {
        return success;
    }
    return fail;
}

function createWithGivenZeroSize(): int {
    let arr = new Array<Number>(0);
    let result = arr.length == 0 ? success : fail;
    return result;
}

function createWithGivenSize(): int {
    let arr = new Array<Number>(10);
    let result = arr.length == 10 ? success : fail;
    return result;
}

function isArrayRawArrayNumber(): int {
    let n: Number[] = [];
    return Array.isArray(n) == true ? success : fail;
}

function isArrayArrayNumber(): int {
    let arr = new Array<Number>;
    return Array.isArray(arr) == true ? success : fail;
}

function isArrayNullUndefined(): int {
    if (Array.isArray(null) == true) {
        return fail;
    }
    if (Array.isArray(undefined) == true) {
        return fail;
    }
    return success;
}

function push(): int {
    let arr = new Array<Number>;
    for (let i: int = 0; i < d.length; ++i) {
        arr.push(d[i])
        if (arr.at(i) != d[i]) {
            return fail;
        }
    }
    if (arr.length != d.length) {
        return fail;
    }
    return success;
}

function pushECMA(): int {
    let arr = new Array<Number>;
    arr.pushECMA(...d);
    if (arr.length != d.length) {
        return fail;
    }
    for (let i: int = 0; i < d.length; ++i) {
        if (arr.at(i) != d[i]) {
            return fail;
        }
    }
    return success;
}

function toSplicedArg1(): int {
    let arr = new Array<Number>;
    for (let i: int = 0; i < dd.length; ++i) {
        arr.push(dd[i]);
    }
    let newArr = arr.toSpliced(1);
    if ((newArr.at(0) != 1) || (newArr.length != 1)) {
        return fail;
    }
    if (arr.length != dd.length) {
        return fail;
    }
    for (let i = 0; i < dd.length; i++) {
        if (arr[i] != dd[i]) {
            return fail;
        }
    }
    return success;
}

function toSplicedArg2(): int {
    let arr = new Array<Number>;
    for (let i: int = 0; i < dd.length; ++i) {
        arr.push(dd[i]);
    }
    let newArr = arr.toSpliced(1, 2);
    if ((newArr.length != 4) || (newArr[0] != 1) ||
        (newArr[1] != 4) || (newArr[2] != 0) ||
        (newArr[3] != 6)) {
        return fail;
    }
    if (arr.length != dd.length) {
        return fail;
    }
    for (let i = 0; i < dd.length; i++) {
        if (arr[i] != dd[i]) {
            return fail;
        }
    }
    return success;
}

function check(result: int, message: String): int {
    if(result == success) {
        return success;
    }
    console.println("\nFAILED: " + message);
    return fail;
}

function check(fn: () => int, message: String): int {
    let result: int = 0;
    try {
        result = fn();
    } catch(e) {
        console.println("FAILED: " + message + " => Exception catched: " + e);
        return fail;
    }
    if(result == success) {
        return success;
    }
    console.println("\nFAILED: " + message);
    return fail;
}
