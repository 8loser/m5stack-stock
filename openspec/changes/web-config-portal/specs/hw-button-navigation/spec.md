## MODIFIED Requirements

### Requirement: Middle button cycles Dashboard, Portal, and Server
The system SHALL treat btn 1 (middle virtual button) as a three-way cycle among SCREEN_DASHBOARD, SCREEN_PORTAL, and SCREEN_SERVER in that fixed order. Each press SHALL advance to the next screen in the cycle, wrapping from SCREEN_SERVER back to SCREEN_DASHBOARD.

#### Scenario: Enter Portal from Dashboard via middle button
- **WHEN** user presses btn 1 while on SCREEN_DASHBOARD
- **THEN** system switches to SCREEN_PORTAL

#### Scenario: Enter Server from Portal via middle button
- **WHEN** user presses btn 1 while on SCREEN_PORTAL
- **THEN** system switches to SCREEN_SERVER

#### Scenario: Return to Dashboard from Server via middle button
- **WHEN** user presses btn 1 while on SCREEN_SERVER
- **THEN** system switches to SCREEN_DASHBOARD

#### Scenario: Middle button from any nav screen enters Portal
- **WHEN** user presses btn 1 while on a screen other than SCREEN_PORTAL or SCREEN_SERVER
- **THEN** system switches to SCREEN_PORTAL

### Requirement: Left and right buttons exit Portal and Server to Dashboard
While SCREEN_PORTAL or SCREEN_SERVER is active, pressing btn 0 (left) or btn 2 (right) SHALL navigate to SCREEN_DASHBOARD.

#### Scenario: Left button pressed in Portal
- **WHEN** user presses btn 0 while on SCREEN_PORTAL
- **THEN** system switches to SCREEN_DASHBOARD

#### Scenario: Right button pressed in Portal
- **WHEN** user presses btn 2 while on SCREEN_PORTAL
- **THEN** system switches to SCREEN_DASHBOARD

#### Scenario: Left button pressed in Server
- **WHEN** user presses btn 0 while on SCREEN_SERVER
- **THEN** system switches to SCREEN_DASHBOARD

#### Scenario: Right button pressed in Server
- **WHEN** user presses btn 2 while on SCREEN_SERVER
- **THEN** system switches to SCREEN_DASHBOARD

### Requirement: Portal and Server are excluded from the pollable screen list
SCREEN_PORTAL and SCREEN_SERVER SHALL NOT appear in `s_nav_screens[]`. The pollable list SHALL contain only content screens (SCREEN_DASHBOARD, SCREEN_LOG, SCREEN_INFO, SCREEN_SETTINGS, SCREEN_HW_TEST).

#### Scenario: Pollable list does not include Portal or Server
- **WHEN** the system initialises `s_nav_screens[]`
- **THEN** neither SCREEN_PORTAL nor SCREEN_SERVER appears in the array
