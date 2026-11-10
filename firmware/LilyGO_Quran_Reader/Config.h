#pragma once
// H752-01 Pro with TPS65185. NOT the older H752 or standard T5 S3.
constexpr bool FLIP_180 = true;
// Optional touch calibration, applied before mapping to display orientation.
constexpr bool TOUCH_MIRROR_X = false;
constexpr bool TOUCH_MIRROR_Y = false;
constexpr unsigned INPUT_SETTLE_MS = 180;
constexpr unsigned HOLD_MS = 900;
// Matches LilyGO's H752-01 display example; value is magnitude in mV.
constexpr int PANEL_VCOM_MV = 1560;
