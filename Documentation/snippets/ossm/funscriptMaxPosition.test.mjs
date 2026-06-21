import { strict as assert } from 'node:assert';
import { describe, it } from 'node:test';

import { computeFunscriptMaxPosition } from './funscriptMaxPosition.mjs';

describe('computeFunscriptMaxPosition (RAD-2053 Issue 4)', () => {
  it('returns 0 for empty / non-array input', () => {
    assert.equal(computeFunscriptMaxPosition([]), 0);
    assert.equal(computeFunscriptMaxPosition(null), 0);
    assert.equal(computeFunscriptMaxPosition(undefined), 0);
    assert.equal(computeFunscriptMaxPosition({}), 0);
  });

  it('returns the highest pos across all actions, not the last seen', () => {
    const actions = [
      { at: 0, pos: 10 },
      { at: 100, pos: 20 },
      { at: 200, pos: 95 }, // peak before mid playback
      { at: 300, pos: 30 },
      { at: 400, pos: 50 },
    ];
    assert.equal(computeFunscriptMaxPosition(actions), 95);
  });

  it('clamps values above 100 to 100', () => {
    const actions = [{ at: 0, pos: 250 }];
    assert.equal(computeFunscriptMaxPosition(actions), 100);
  });

  it('treats negative positions as 0 (clamps low end)', () => {
    const actions = [{ at: 0, pos: -10 }, { at: 100, pos: -5 }];
    assert.equal(computeFunscriptMaxPosition(actions), 0);
  });

  it('ignores non-numeric pos entries', () => {
    const actions = [
      { at: 0, pos: 'oops' },
      { at: 100, pos: NaN },
      { at: 200, pos: 42 },
      { at: 300 },
    ];
    assert.equal(computeFunscriptMaxPosition(actions), 42);
  });

  it('rounds fractional positions to nearest integer', () => {
    const actions = [
      { at: 0, pos: 49.4 },
      { at: 100, pos: 49.6 },
    ];
    assert.equal(computeFunscriptMaxPosition(actions), 50);
  });
});
