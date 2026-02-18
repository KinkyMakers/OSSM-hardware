#ifndef OSSM_PAGES_PREFLIGHT_H
#define OSSM_PAGES_PREFLIGHT_H

namespace pages {

/**
 * Draw the preflight safety check page
 * Blocks until speed is reduced to safe level before allowing session to start
 */
void drawPreflight();

}  // namespace pages

#endif  // OSSM_PAGES_PREFLIGHT_H
