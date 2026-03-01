## ADDED Requirements

### Requirement: Middle button toggles Portal
The system SHALL treat btn 1 (middle virtual button) as a global Portal toggle. From any non-Portal screen, btn 1 SHALL navigate to SCREEN_PORTAL and record the originating screen. From SCREEN_PORTAL, btn 1 SHALL return to the previously recorded screen.

#### Scenario: Enter Portal from main screen
- **WHEN** user presses btn 1 while on a non-Portal screen
- **THEN** system switches to SCREEN_PORTAL and stores the current screen as the return target

#### Scenario: Exit Portal via middle button
- **WHEN** user presses btn 1 while on SCREEN_PORTAL
- **THEN** system switches back to the screen that was active before entering Portal

### Requirement: Left and right buttons cycle through pollable screens
The system SHALL maintain a static ordered list of pollable screens (`s_nav_screens[]`). Btn 0 (left) SHALL decrement the current index with wraparound; btn 2 (right) SHALL increment the current index with wraparound. The system SHALL then switch to the screen at the new index.

#### Scenario: Cycle right from last screen
- **WHEN** user presses btn 2 while the current nav index is at the last entry of s_nav_screens[]
- **THEN** system wraps to index 0 and switches to s_nav_screens[0]

#### Scenario: Cycle left from first screen
- **WHEN** user presses btn 0 while the current nav index is 0
- **THEN** system wraps to the last entry of s_nav_screens[] and switches to that screen

#### Scenario: Cycle right from middle screen
- **WHEN** user presses btn 2 while the current nav index is not the last entry
- **THEN** system increments the index by 1 and switches to the corresponding screen

### Requirement: Left and right buttons are ignored in Portal
While SCREEN_PORTAL is active, btn 0 and btn 2 SHALL have no effect on navigation.

#### Scenario: Left button pressed in Portal
- **WHEN** user presses btn 0 while on SCREEN_PORTAL
- **THEN** system takes no navigation action

#### Scenario: Right button pressed in Portal
- **WHEN** user presses btn 2 while on SCREEN_PORTAL
- **THEN** system takes no navigation action

### Requirement: Portal is excluded from the pollable screen list
SCREEN_PORTAL SHALL NOT appear in `s_nav_screens[]`. The pollable list SHALL contain only content screens (e.g., SCREEN_DASHBOARD, SCREEN_LOG, SCREEN_INFO).

#### Scenario: Pollable list does not include Portal
- **WHEN** the system initialises `s_nav_screens[]`
- **THEN** SCREEN_PORTAL is absent from the array

### Requirement: Dashboard refresh is decoupled from hardware buttons
The system SHALL NOT invoke any stock quote refresh logic from `handle_hw_button()`. Refreshing the displayed quote SHALL be triggered solely through the Dashboard screen's touch UI.

#### Scenario: Middle button on Dashboard does not refresh quote
- **WHEN** user presses btn 1 while on SCREEN_DASHBOARD
- **THEN** system navigates to SCREEN_PORTAL without triggering a quote refresh
