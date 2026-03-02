## 1. Dashboard Rotation Model Refactor

- [ ] 1.1 Replace page-flip state in `screen_dashboard.c` with fixed-slot rotation state (slot mapping, next slot index, next stock index).
- [ ] 1.2 Implement 2-second UI rotation tick that updates exactly one visible slot per tick when stock count > 5.
- [ ] 1.3 Keep <=5 stock behavior stable (fixed rows, no row replacement cycle).

## 2. Render and Color Consistency

- [ ] 2.1 Ensure slot refresh path always reapplies text and accent color from the currently displayed quote data.
- [ ] 2.2 Preserve existing market-closed text behavior and bottom `Upd/Next` labels during slot rotation.
- [ ] 2.3 Remove or disable page-level flip logic/constants no longer used by dashboard rendering.

## 3. Screen Lifecycle and Integration

- [ ] 3.1 Add dashboard enter/leave hooks in UI manager flow to reset rotation pointer on entering Dashboard.
- [ ] 3.2 Ensure non-active dashboard does not perform unnecessary LVGL redraws while maintaining cached quote updates.
- [ ] 3.3 Keep mutex usage aligned with LVGL thread-safety rules in all new refresh paths.

## 4. Validation

- [ ] 4.1 Build check with `./flash.sh --build-only`.
- [ ] 4.2 Manual verify: <=5 stocks fixed display; >5 stocks one-slot-per-tick rotation; return-to-dashboard resets to first stock.
- [ ] 4.3 Manual verify color-direction mapping remains correct after each slot replacement.
