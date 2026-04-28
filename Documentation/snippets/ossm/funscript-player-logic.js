// Pure helpers extracted from funscript-player.jsx so the
// reverse/simple stepping behaviour can be unit-tested without a DOM
// or BLE connection. See RAD-1993 for the bug this guards against:
// when a `setInterval` based sync loop captured a stale React closure,
// toggling Reverse mid-playback (or rapid play/pause) would briefly
// emit un-reversed positions for ~1-2 seconds before "fixing itself".
//
// The fix is to read `isReverse`/`isSimple` through a ref-like getter
// so each step always observes the current value. `stepFunscriptSync`
// below is a pure function that mirrors the inner loop of
// `syncFunscript` and returns the position commands it would emit.

/**
 * Step the funscript sync loop once.
 *
 * @param {{
 *   actionsFull: Array<{ at: number, pos: number }>,
 *   actionsSimple: Array<{ at: number, pos: number }>,
 *   currentActionIndex: number,
 *   lastSentTime: number,
 *   currentVideoTimeMs: number,
 *   timeOffset: number,
 *   buffer: number,
 *   isSimple: boolean,
 *   isReverse: boolean,
 * }} state
 * @returns {{
 *   nextActionIndex: number,
 *   nextLastSentTime: number,
 *   commands: Array<{ pos: number, timeToNext: number }>,
 * }}
 */
export function stepFunscriptSync(state) {
  const {
    actionsFull,
    actionsSimple,
    currentActionIndex,
    lastSentTime,
    currentVideoTimeMs,
    timeOffset,
    buffer,
    isSimple,
    isReverse,
  } = state;

  const actions = isSimple ? actionsSimple : actionsFull;
  let index = currentActionIndex;
  let nextLastSentTime = lastSentTime;
  const commands = [];

  if (!actions || actions.length === 0) {
    return { nextActionIndex: index, nextLastSentTime, commands };
  }

  const cutoffMs = currentVideoTimeMs + timeOffset + buffer;

  while (index < actions.length) {
    const action = actions[index];

    if (action.at > cutoffMs) {
      break;
    }

    if (index < actions.length - 1) {
      const nextAction = actions[index + 1];
      const timeToNext = nextAction.at - action.at;

      if (action.at > nextLastSentTime) {
        const targetPos = isReverse ? 100 - nextAction.pos : nextAction.pos;
        commands.push({ pos: targetPos, timeToNext });
        nextLastSentTime = action.at;
      }
    }

    index += 1;
  }

  return { nextActionIndex: index, nextLastSentTime, commands };
}

// Mirror of the clamping done by `sendStreamPosition` so tests can
// assert the wire format we'd send to BLE.
export function formatStreamCommand(rawPos, rawTimeMs) {
  const clampedPos = Math.max(0, Math.min(100, Math.round(rawPos)));
  const clampedTime = Math.max(0, Math.min(10000, Math.round(rawTimeMs)));
  return `stream:${clampedPos}:${clampedTime}`;
}
