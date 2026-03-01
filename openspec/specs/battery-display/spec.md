# battery-display Specification

## Purpose
Define battery voltage parsing and status bar percentage display behavior.

## Requirements
### Requirement: Battery voltage ADC parsing
The system SHALL parse AXP192 battery voltage ADC registers according to datasheet encoding: REG 0x78 stores ADC bits [11:4] and REG 0x79 stores ADC bits [3:0], with resolution 1.1 mV/bit.

#### Scenario: Full charge reads 100%
- **WHEN** battery voltage ADC raw value corresponds to 4.2V
- **THEN** `axp192_get_battery_percent()` SHALL return 100

#### Scenario: Empty battery reads 0%
- **WHEN** battery voltage ADC raw value corresponds to 3.0V or below
- **THEN** `axp192_get_battery_percent()` SHALL return 0

#### Scenario: Mid-charge reads proportional percentage
- **WHEN** battery voltage ADC raw value corresponds to 3.6V
- **THEN** `axp192_get_battery_percent()` SHALL return 50 (±2)

### Requirement: Battery percentage display in status bar
The system SHALL display battery percentage in the status bar, updated every 10 seconds, with a `~` prefix when charging is detected.

#### Scenario: Charging state shown with prefix
- **WHEN** `axp192_is_charging()` returns true
- **THEN** status bar battery label SHALL display `~<N>%` where N is the current percentage

#### Scenario: Discharging state shown without prefix
- **WHEN** `axp192_is_charging()` returns false
- **THEN** status bar battery label SHALL display `<N>%` without any prefix

#### Scenario: Fully charged while plugged in
- **WHEN** device is connected to charger and battery is at 4.2V
- **THEN** status bar SHALL display `~100%`
