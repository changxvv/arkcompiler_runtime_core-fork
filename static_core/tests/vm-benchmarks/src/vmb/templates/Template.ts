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

/*
    This file was generated by VMB using these parameters:

        Benchmarks Class:     $STATE_NAME
        Bench Setup:          $STATE_SETUP
        Bench Method:         $METHOD_NAME -> $METHOD_RETTYPE
        Parameter Set:        #$FIX_ID $FIXTURE
        Bench Full Name:      $BENCH_NAME
        WarmUp Iterations:    $WI
        Measure Iterations:   $MI
        Iteration Time (sec): $IT
        Warmup Time (sec):    $WT
        Fast Iterations:      $FI
        GC pause (ms):        $GC (N/A)
*/

$IMPORTS

// map ets numerical types to number
type int = number;
type float = number;
type double = number;
type byte = number;

// params
let WI: int = $WI;
let MI: int = $MI;
let IT: int = $IT;
let WT: int = $WT;
let FI: int = $FI;

const MAX_LOOP_COUNT = 10_000_000_000;
const MS2NS = 1_000_000;
const S2MS = 1_000;

class Consumer {
    static x1: number = 0x41c64e6d;
    static x2: number = 0xd431;
    static x3: number = 1;
    static boola: boolean = false;
    static boolb: boolean = true;
    static chara: string = 'X';
    static charb: string = 'Y';
    static floata: number = 24.0;
    static floatb: number = 53.0;
    static localObj: Object = new Object();
    static localObjs: Object[] = [new Object()];
    static pseudorand: number = Date.now();

    static consume_boolean (boolc: boolean): void {
        if (boolc === this.boola && boolc === this.boolb) {
            throw new Error();
        }
    }

    static consume_string(charc: string): void {
        if (charc === this.chara && charc === this.charb) {
            throw new Error();
        }
    }

    static consume_number(floatc: number): void {
        if (floatc === this.floata && floatc === this.floatb) {
            throw new Error();
        }
    }

    static consume_Object(obj: Object): void {
        this.pseudorand = (this.pseudorand * this.x1 + this.x2);
        if ((this.pseudorand & this.x3) === 0) {
            this.x3 = (this.x3 << 1) + 0xad;
            this.localObj = obj;
        }
    }

    static consume_Objects(objs: Object[]): void {
        this.pseudorand = (this.pseudorand * this.x1 + this.x2);
        if ((this.pseudorand & this.x3) === 0) {
            this.x3 = (this.x3 << 1) + 0xad;
            this.localObjs = objs;
        }
    }
}

function log(msg: string) {
    print(msg);
}

$COMMON

$SRC

let bench = new $STATE_NAME();
$STATE_PARAMS
$STATE_SETUP

var loopCount1: number;
var totalOps: number = 0;
var totalMs: number = 0;
var iter: number;

function tune() {
    let iterMs: number = 1 * S2MS;
    let loopMs: number = 0;
    let loopCount: number = 1;
    while (loopMs < iterMs && loopCount < MAX_LOOP_COUNT) {
        loopCount = loopCount * 2;
        let start: number = Date.now();
        for (let i: number = 0; i < loopCount; i++) {
            $METHOD_CALL
        }
        loopMs = Date.now() - start;
    }
    loopCount1 = loopCount * iterMs / loopMs >> 0;
    if (loopCount1 == 0) loopCount1++;
    log("Tuning: " + loopCount + " ops, " +
        loopMs*MS2NS/loopCount + " ns/op => " +
        loopCount1 + " reps");
}

function runIters(phase: string, count: number, time: number) {
    let iterMs = time * S2MS;
    totalOps = 0;
    totalMs = 1/Infinity; // make sure zero is encoded as double to avoid deoptimization on overflow later
    for (let k = 0; k < count; k++, iter++) {
        let ops = 0;
        let elapsedMs = 0;
        let start = Date.now();
        while (elapsedMs < iterMs) {
            for (let i = 0; i < loopCount1; i++) {
                $METHOD_CALL
            }
            elapsedMs = Date.now() - start;
            ops += loopCount1;
        }
        totalOps += ops;
        totalMs += elapsedMs;
        log(phase + " " + iter + ":" + ops + " ops, " + elapsedMs*MS2NS/ops + " ns/op");
    }
}

log("Startup execution started: " + Date.now() * MS2NS);
if (FI > 0) {
    let start = Date.now();
    for (let i = 0; i < FI; i++) {
        $METHOD_CALL
    }
    let elapsed = Date.now() - start;
    if (elapsed <= 0) {
        elapsed = 1;  // 0 is invalid result
    }
    log("Benchmark result: $BENCH_NAME " + elapsed*MS2NS / FI);
} else {
    tune();
    if (WI > 0) {
        iter = 1;
        // Re-entering runIters in warmup loop to allow profiler complete the method.
        // Possible deoptimizations and recompilations is done in warmup instead of measure phase.
        for (let wi = 0; wi < WI; ++wi) {
            runIters("Warmup", 1, WT);
        }
    }
    iter = 1;
    var measure_iters = MI >> 0; // make sure it has int type
    runIters("Iter", measure_iters, IT);
    log("Benchmark result: $BENCH_NAME " + totalMs*MS2NS/totalOps);
}

Consumer.consume_Object(bench);