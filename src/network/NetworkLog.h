// ABOUTME: Logging macros for network clients: ArduinoLog on device, no-ops in host tests.
// ABOUTME: Shared by the weather.gov clients and src/ota/ so they log without Arduino headers in host builds.
#ifndef NETWORK_LOG_H
#define NETWORK_LOG_H

#ifdef ARDUINO
#include <ArduinoLog.h>
#define NET_LOG_ERROR(fmt, ...) Log.errorln(fmt, ##__VA_ARGS__)
#define NET_LOG_WARNING(fmt, ...) Log.warningln(fmt, ##__VA_ARGS__)
#define NET_LOG_NOTICE(fmt, ...) Log.noticeln(fmt, ##__VA_ARGS__)
#define NET_LOG_INFO(fmt, ...) Log.infoln(fmt, ##__VA_ARGS__)
#else
#define NET_LOG_ERROR(fmt, ...)
#define NET_LOG_WARNING(fmt, ...)
#define NET_LOG_NOTICE(fmt, ...)
#define NET_LOG_INFO(fmt, ...)
#endif

#endif  // NETWORK_LOG_H
