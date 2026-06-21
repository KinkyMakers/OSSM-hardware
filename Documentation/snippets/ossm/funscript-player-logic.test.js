// Run with: node --test snippets/ossm/funscript-player-logic.test.js
//
// Regression tests for RAD-1993. The original bug: toggling the
// "Reverse" switch caused 1-2 seconds of non-reversed positions to be
// emitted before the player "fixed itself". Root cause was a stale
// React closure baked into a setInterval. These tests pin down the
// pure stepping behaviour so any future regression - in either
// direction or timing - shows up immediately.

import { test } from "node:test";
import assert from "node:assert/strict";
import {
  stepFunscriptSync,
  formatStreamCommand,
} from "./funscript-player-logic.js";

const ACTIONS = [
  { at: 100, pos: 10 },
  { at: 200, pos: 90 },
  { at: 300, pos: 30 },
  { at: 400, pos: 70 },
  { at: 500, pos: 0 },
];

const baseState = {
  actionsFull: ACTIONS,
  actionsSimple: ACTIONS,
  currentActionIndex: 0,
  lastSentTime: 0,
  currentVideoTimeMs: 350,
  timeOffset: 0,
  buffer: 0,
  isSimple: false,
  isReverse: false,
};

test("forward stepping emits raw positions of the next action", () => {
  const result = stepFunscriptSync(baseState);
  assert.deepEqual(
    result.commands.map((c) => c.pos),
    [90, 30, 70],
  );
  assert.equal(result.nextActionIndex, 3);
  assert.equal(result.nextLastSentTime, 300);
});

test("reverse stepping emits 100 - pos for every action", () => {
  // RAD-1993: The first emitted command after enabling Reverse must
  // already be inverted - the bug was that the first ~1-2 seconds were
  // forward because the running interval still saw isReverse=false.
  const result = stepFunscriptSync({ ...baseState, isReverse: true });
  assert.deepEqual(
    result.commands.map((c) => c.pos),
    [10, 70, 30],
  );
});

test("reverse flips synchronously when toggled between two steps", () => {
  // First tick (forward): drain actions up to t=200.
  const tick1 = stepFunscriptSync({
    ...baseState,
    currentVideoTimeMs: 200,
    isReverse: false,
  });
  assert.deepEqual(
    tick1.commands.map((c) => c.pos),
    [90, 30], // raw next-action positions from index 0 and 1
  );
  assert.equal(tick1.nextActionIndex, 2);
  assert.equal(tick1.nextLastSentTime, 200);

  // User flips Reverse before the next tick. The very next emitted
  // command must already be inverted - this is the regression we're
  // guarding against. With the old stale-closure code the next pos
  // would still be 70 (raw) instead of 30 (100-70).
  const tick2 = stepFunscriptSync({
    ...baseState,
    currentActionIndex: tick1.nextActionIndex,
    lastSentTime: tick1.nextLastSentTime,
    currentVideoTimeMs: 400,
    isReverse: true,
  });
  assert.deepEqual(
    tick2.commands.map((c) => c.pos),
    [30, 100], // = 100-70, 100-0
  );
});

test("isSimple selects the simplified action list", () => {
  const simple = [
    { at: 100, pos: 0 },
    { at: 400, pos: 100 },
  ];
  const result = stepFunscriptSync({
    ...baseState,
    actionsSimple: simple,
    isSimple: true,
    currentVideoTimeMs: 500,
  });
  assert.deepEqual(
    result.commands.map((c) => c.pos),
    [100],
  );
});

test("does not re-emit an action when lastSentTime already covers it", () => {
  const result = stepFunscriptSync({
    ...baseState,
    currentVideoTimeMs: 500,
    lastSentTime: 300,
  });
  // Only the action at index 3 (at=400) is past lastSentTime and has a
  // following action; index 4 has no successor so it doesn't emit.
  assert.deepEqual(
    result.commands.map((c) => c.pos),
    [0],
  );
});

test("respects timeOffset and buffer when deciding the cutoff", () => {
  const result = stepFunscriptSync({
    ...baseState,
    currentVideoTimeMs: 100,
    timeOffset: 50,
    buffer: 50,
  });
  // cutoff = 100 + 50 + 50 = 200, so we drain through index 1 (at=200)
  // and emit the next-action positions for indices 0 and 1.
  assert.deepEqual(
    result.commands.map((c) => c.pos),
    [90, 30],
  );
  assert.equal(result.nextActionIndex, 2);
});

test("empty action list is a no-op", () => {
  const result = stepFunscriptSync({
    ...baseState,
    actionsFull: [],
    actionsSimple: [],
  });
  assert.deepEqual(result.commands, []);
  assert.equal(result.nextActionIndex, 0);
});

test("formatStreamCommand clamps position and time", () => {
  assert.equal(formatStreamCommand(150, 5), "stream:100:5");
  assert.equal(formatStreamCommand(-10, 50), "stream:0:50");
  assert.equal(formatStreamCommand(50.4, 12345), "stream:50:10000");
  assert.equal(formatStreamCommand(50.5, -1), "stream:51:0");
});
