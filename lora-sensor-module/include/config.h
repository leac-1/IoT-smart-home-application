/** Behavioural settings */

#pragma once

constexpr unsigned long SLEEP_SECONDS = 600; // 10 minutes
constexpr bool USE_DHT = true;
constexpr bool USE_LDR = true;

#ifndef NODE_ID
#define NODE_ID 1
#endif