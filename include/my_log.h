#pragma once

#define DEBUG(format, ...) do {} while (0)

#define HL_TWK_RED_YEL  "\033[1m\033[5;31;43m"
#define HL_RED_WRT      "\033[1;31;47m"
#define HL_RED          "\033[1;31m"
#define HL_YEL          "\033[1;33m"

#define PF_CLR  "\033[0m"

enum LOG_LEVEL
{
    LOG_LEVEL_OFF = 0,
    LOG_LEVEL_FATAL,
    LOG_LEVEL_ERR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DBG,
    LOG_LEVEL_ALL,
};

static const int log_level = LOG_LEVEL_OFF;

#define logFatal(format, ...) do {} while (0)
#define logError(format, ...) do {} while (0)
#define logWarn(format, ...) do {} while (0)
#define logInfo(format, ...) do {} while (0)
#define logDebug(format, ...) do {} while (0)
#define logr do {} while (0)
