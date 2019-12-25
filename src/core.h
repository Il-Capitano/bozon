#ifndef CORE_H
#define CORE_H

#include <iostream>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <array>
#include <map>
#include <memory>
#include <utility>
#include <algorithm>
#include <chrono>

#include <bz/variant.h>
#include <bz/string.h>
#include <bz/vector.h>
#include <bz/optional.h>

#include "error.h"
#include "my_assert.h"

using int8   = std::int8_t;
using int16  = std::int16_t;
using int32  = std::int32_t;
using int64  = std::int64_t;
using uint8  = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

using float32 = float;
using float64 = double;

#endif // CORE_H
