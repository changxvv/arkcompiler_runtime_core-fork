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
    failures += check((): int => { return key() }, "key");
    failures += check((): int => { return values() }, "values");
    failures += check((): int => { return iterator() }, "iterator");
    failures += check((): int => { return findWithFuncArg1() }, "findWithFuncArg1");
    failures += check((): int => { return findWithFuncArg2() }, "findWithFuncArg2");
    failures += check((): int => { return findWithFuncArg3ArrayNumber() }, "findWithFuncArg3ArrayNumber");
    failures += check((): int => { return findIndexFuncArg1() }, "findIndexFuncArg1");
    failures += check((): int => { return findIndexFuncArg2() }, "findIndexFuncArg2");
    failures += check((): int => { return findIndexFuncArg3ArrayNumber() }, "findIndexFuncArg3ArrayNumber");
    failures += check((): int => { return findLastIndexFuncArg1() }, "findLastIndexFuncArg1");
    failures += check((): int => { return findLastFuncArg1() }, "findLastFuncArg1");
    failures += check((): int => { return mapFuncArg1() }, "mapFuncArg1");
    failures += check((): int => { return mapFuncArg2() }, "mapFuncArg2");
    failures += check((): int => { return mapFuncArg3ArrayNumber() }, "mapFuncArg3ArrayNumber");
    return check(failures, "All tests run");
}


const d: Number[] = [10, -1, 237, -5, 148, 65, 3, 34, 0, 12];
const dd: Number[] = [1, 0, 3, 4, 0, 6];

function key(): int {
    let arr = new Array<Number>;
    let iterator = arr.keys();
    let i: int = 0;
    while (true) {
        const v = iterator.next();
        if (v.done) {
            if (i > 0){
                return fail;
            };
            break;
        }
        i = i + 1;
    }
    for (i = 0; i < dd.length; ++i) {
        arr.push(dd[i]);
    }
    iterator = arr.keys();
    let idx: Number = 0;
    while (true) {
        const v = iterator.next();
        if (v.done) {
            if ((idx != arr.length) || (idx != dd.length)) {
                return fail;
            }
            return success;
        }
        if ((v.value != idx) || (v.value == undefined)) {
            return fail;
        }
        idx = idx + 1;
    }
}

function values(): int {
    let arr = new Array<Number>;
    let iterator = arr.values();
    let i: int = 0;
    while (true) {
        const v = iterator.next();
        if (v.done) {
            if ((i != 0) && (i != arr.length)) {
                return fail;
            }
            break;
        }
        i++;
    }
    for (let i: int = 0; i < d.length; ++i) {
        arr.push(d[i])
    }
    iterator = arr.values();
    i = 0;
    while (true) {
        const v = iterator.next();
        if (v.done) {
            if ((i > 0) && (i != arr.length)) {
                return fail;
            }
            return success;
        }
        if (v.value != arr.at(i)) {
            return fail;
        }
        i++;
    }
}

function iterator(): int {
    let arr = new Array<Number>;
    let iterator = arr.iterator();
    let i: int = 0;
    while (true) {
        const v = iterator.next();
        if (v.done) {
            if ((i > 0) && (i != arr.length)) {
                return fail;
            }
            break;
        }
        i++;
    }
    for (let i: int = 0; i < d.length; ++i) {
        arr.push(d[i]);
    }
    iterator = arr.iterator();
    i = 0;
    while (true) {
        const v = iterator.next();
        if (v.done) {
            if ((i > 0) && (i != arr.length)) {
                return fail;
            }
            return success;
        }
        if (v.value != arr.at(i)) {
            return fail;
        }
        i++;
    }
}

function findWithFuncArg1(): int {
    let arr = new Array<Number>;
    for (let i: int = 0; i < d.length as int; ++i) {
        arr.push(d[i]);
    }
    const fnPositive = (elem: number): boolean => { return elem == 12; };
    const fnNegative = (elem: number): boolean => { return elem < 0; };
    const fnUndefined = (elem: Number): boolean => { return elem == undefined; };
    const fnNull = (elem: Number): boolean => { return elem == null; };
    if (arr.find(fnPositive) != 12) { return fail; }
    if (arr.find(fnNegative) != -1) { return fail; }
    if (arr.find(fnUndefined) != undefined) { return fail; }
    if (arr.find(fnNull) != undefined) { return fail; }
    return success;
}

function findWithFuncArg2(): int {
    let arr = new Array<Number>;
    for (let i: int = 0; i < d.length; ++i) {
        arr.push(d[i]);
    }
    const fnPositive = (elem: number, index: number): boolean => {
        return elem == 12 && index > 0;
    };
    const fnNegative = (elem: number, index: number): boolean => {
        return elem < 0 && index > 0;
    };
    const fnUndefined = (elem: Number, index: number): boolean => {
        return elem == undefined && index < 0;
    };
    const fnNull = (elem: Number, index: number): boolean => {
        return elem == null && index > 0;
    };
    if (arr.find(fnPositive) != 12) { return fail; }
    if (arr.find(fnNegative) != -1) { return fail; }
    if (arr.find(fnUndefined) != undefined) { return fail; }
    if (arr.find(fnNull) != undefined) { return fail; }
    return success;
}

function findWithFuncArg3ArrayNumber(): int {
    let arr = new Array<Number>;
    for (let i: int = 0; i < d.length; ++i) {
        arr.push(d[i]);
    }
    const fnPositive = (elem: number, index: number, obj: Array<Number>): boolean => {
        return obj[index] == elem && elem == 12 && index > 0;
    };
    const fnNegative = (elem: number, index: number, obj: Array<Number>): boolean => {
        return obj[index] == elem && elem < 0 && index > 0;
    };
    const fnUndefined = (elem: Number, index: number, obj: Array<Number>): boolean => {
        return obj[index] == elem && elem == undefined && index < 0;
    };
    const fnNull = (elem: Number, index: number, obj: Array<Number>): boolean => {
        return obj[index] == elem && elem == null && index > 0;
    };
    if (arr.find(fnPositive) != 12) { return fail; }
    if (arr.find(fnNegative) != -1) { return fail; }
    if (arr.find(fnUndefined) != undefined) { return fail; }
    if (arr.find(fnNull) != undefined) { return fail; }
    return success;
}

function findIndexFuncArg1(): int {
    let arr = new Array<Number>;
    for (let i: int = 0; i < d.length; ++i) {
        arr.push(d[i]);
    }
    const fnPositive = (elem: number): boolean => { return elem == 12; };
    const fnNegative = (elem: number): boolean => { return elem < 0; };
    const fnUndefined = (elem: Number): boolean => { return elem == undefined; };
    const fnNull = (elem: Number): boolean => { return elem == null; };
    if (arr.findIndex(fnPositive) != 9) { return fail; }
    if (arr.findIndex(fnNegative) != 1) { return fail; }
    if (arr.findIndex(fnUndefined) != -1) { return fail; }
    if (arr.findIndex(fnNull) != -1) { return fail; }
    return success;
}

function findIndexFuncArg2(): int {
    let arr = new Array<Number>;
    for (let i: int = 0; i < d.length; ++i) {
       arr.push(d[i]);
    }
    const fnPositive = (elem: number, index: number): boolean => {
        return elem == 12 && index > 0;
    };
    const fnNegative = (elem: number, index: number): boolean => {
        return elem < 0 && index > 0;
    };
    const fnUndefined = (elem: Number, index: number): boolean => {
        return elem == undefined && index < 0;
    };
    const fnNull = (elem: Number, index: number): boolean => {
        return elem == null && index > 0;
    };
    if (arr.findIndex(fnPositive) != 9) { return fail; }
    if (arr.findIndex(fnNegative) != 1) { return fail; }
    if (arr.findIndex(fnUndefined) != -1) { return fail; }
    if (arr.findIndex(fnNull) != -1) { return fail; }
    return success;
}

function findIndexFuncArg3ArrayNumber(): int {
    let arr = new Array<Number>;
    for (let i: int = 0; i < d.length; ++i) {
        arr.push(d[i]);
    }
    const fnPositive = (elem: number, index: number, obj: Array<Number>): boolean => {
        return obj.at(index)! == elem && elem == 12 && index > 0;
    };
    const fnNegative = (elem: number, index: number, obj: Array<Number>): boolean => {
        return obj.at(index)! == elem && elem < 0 && index > 0;
    };
    const fnUndefined = (elem: Number, index: number, obj: Array<Number>): boolean => {
        return obj.at(index)! == elem && elem == undefined && index < 0;
    };
    const fnNull = (elem: Number, index: number, obj: Array<Number>): boolean => {
        return obj.at(index)! == elem && elem == null && index > 0;
    };
    if (arr.findIndex(fnPositive) != 9) { return fail; }
    if (arr.findIndex(fnNegative) != 1) { return fail; }
    if (arr.findIndex(fnUndefined) != -1) { return fail; }
    if (arr.findIndex(fnNull) != -1) { return fail; }
    return success;
}

function findLastIndexFuncArg1(): int {
    let arr = new Array<Number>;
    for (let i: int = 0; i < d.length; ++i) {
        arr.push(d[i]);
    }
    const fnPositive = (elem: number): boolean => { return elem == 12; };
    const fnNegative = (elem: number): boolean => { return elem < 0; };
    const fnUndefined = (elem: Number): boolean => { return elem == undefined; };
    const fnNull = (elem: Number): boolean => { return elem == null; };
    if (arr.findLastIndex(fnPositive) != 9) { return fail; }
    if (arr.findLastIndex(fnNegative) != 3) { return fail; }
    if (arr.findLastIndex(fnUndefined) != -1) { return fail; }
    if (arr.findLastIndex(fnNull) != -1) { return fail; }
    return success;
}

function findLastFuncArg1(): int {
    let arr = new Array<Number>;
    for (let i: int = 0; i < d.length; ++i) {
        arr.push(d[i]);
    }
    const fnPositive = (elem: number): boolean => { return elem == 12; };
    const fnNegative = (elem: number): boolean => { return elem < 0; };
    const fnUndefined = (elem: Number): boolean => { return elem == undefined; };
    const fnNull = (elem: Number): boolean => { return elem == null; };
    if (arr.findLast(fnPositive) != 12) { return fail; }
    if (arr.findLast(fnNegative) != -5) { return fail; }
    if (arr.findLast(fnUndefined) != undefined) { return fail; }
    if (arr.findLast(fnNull) != undefined) { return fail; }
    return success;
}

function mapFuncArg1(): int {
    let fn = (elem: number): number => {
        return elem * elem;
    };
    let arrEmpty = new Array<Number>;
    let res0 = arrEmpty.map<Number>(fn);
    if (res0.length != arrEmpty.length) { return fail; }
    if (res0.length != 0) { return fail; }

    let arr = new Array<Number>;
    for (let i: int = 0; i < d.length; ++i) {
        arr.push(d[i]);
    }
    let res = arr.map<Number>(fn);
    if (res.length != arr.length) { return fail; }
    for (let i: int = 0; i < res.length; ++i) {
        if (res.at(i) == undefined) { return fail; }
        if (res.at(i)! != arr.at(i)! * arr.at(i)!) {
            return fail;
        }
    }
    return success;
}

function mapFuncArg2(): int {
    let fn = (elem: number, index: number): number => {
        if (index < 4) {
            return elem * elem;
        } else {
            return 0;
        }
    }
    let arrEmpty = new Array<Number>;
    let res0 = arrEmpty.map<Number>(fn);
    if (res0.length != arrEmpty.length) { return fail; }
    if (res0.length != 0) { return fail; }

    let arr = new Array<Number>;
    for (let i: int = 0; i < d.length; ++i) {
        arr.push(d[i]);
    }
    let res = arr.map<Number>(fn);
    if (res.length != arr.length) { return fail; }
    let tmp: Number;
    for (let i: int = 0; i < res.length; ++i) {
        if (res.at(i) == undefined) { return fail; }
        if (i < 4) {
            tmp = new Number(arr.at(i)! * arr.at(i)!);
        } else {
            tmp = 0;
        }
        if (res.at(i)! != tmp) {
            return fail;
        }
    }
    return success;
}

function mapFuncArg3ArrayNumber(): int {
    let fn = (elem: number, index: number, arr: Array<Number>): number => {
        if ((index < 4) && (index < arr.length)) {
            return arr.at(index)! * arr.at(index)!;
        } else {
            return 0;
        }
    }
    let arrEmpty = new Array<Number>;
    let res0 = arrEmpty.map<Number>(fn);
    if (res0.length != arrEmpty.length) { return fail; }
    if (res0.length != 0) { return fail; }

    let arr = new Array<Number>;
    for (let i: int = 0; i < d.length; ++i) {
        arr.push(d[i]);
    }
    let res = arr.map<Number>(fn);
    if (res.length != arr.length) { return fail; }
    let tmp: Number;
    for (let i: int = 0; i < res.length; ++i) {
        if (res.at(i) == undefined) { return fail; }
        if (i < 4) {
            tmp = new Number(arr.at(i)! * arr.at(i)!);
        } else {
            tmp = 0;
        }
        if (res.at(i)! != tmp) {
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
