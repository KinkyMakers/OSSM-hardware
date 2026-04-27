/**
 * Pure helper exported so the logic that drives the funscript player's
 * "Script max depth" safety marker can be unit-tested independently of
 * the Mintlify snippet (which is loaded at runtime as JSX and not
 * importable by a test runner).
 *
 * Mirrors the inline computation in `funscript-player.jsx`.
 *
 * See RAD-2053 (Issue 4): the existing position bar showed only the
 * highest depth observed up to the current playback time, which a user
 * could mistake for the script's true peak. The "static max" computed
 * here is rendered as a separate amber tick on the position bar so the
 * user can see the deepest stroke the script will ever request before
 * playback reaches that section.
 *
 * @param {Array<{pos?: unknown}>} actions
 * @returns {number} clamped 0-100 integer
 */
export const computeFunscriptMaxPosition = (actions) => {
  if (!Array.isArray(actions) || actions.length === 0) {
    return 0;
  }
  let maxPos = 0;
  for (const action of actions) {
    const pos = Number(action?.pos);
    if (Number.isFinite(pos) && pos > maxPos) {
      maxPos = pos;
    }
  }
  return Math.max(0, Math.min(100, Math.round(maxPos)));
};
