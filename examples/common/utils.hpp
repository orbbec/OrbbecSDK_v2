#pragma once
#include <stdint.h>

namespace ob_sample_utils {
char waitForKeyPressed(uint32_t timeout_ms = 0);

uint64_t getNowTimesMs();

int getInputOption();

}