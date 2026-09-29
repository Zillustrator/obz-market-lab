#pragma once

#if defined(OBZ_MARKET_LAB_ENABLE_TRACY)
#include <tracy/Tracy.hpp>

#define OBZ_MARKET_TRACE_SCOPE() ZoneScoped
#define OBZ_MARKET_TRACE_SCOPE_N(name) ZoneScopedN(name)
#else
#define OBZ_MARKET_TRACE_SCOPE() static_cast<void>(0)
#define OBZ_MARKET_TRACE_SCOPE_N(name) static_cast<void>(0)
#endif
