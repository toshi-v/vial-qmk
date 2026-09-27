// Copyright 2026 toshi-v
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include_next <halconf.h>

#undef HAL_USE_SIO
#define HAL_USE_SIO TRUE
