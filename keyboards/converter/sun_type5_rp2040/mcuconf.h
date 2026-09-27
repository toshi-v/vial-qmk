// Copyright 2026 toshi-v
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include_next <mcuconf.h>

/* UART0 drives the Sun serial link. */
#undef RP_SIO_USE_UART0
#define RP_SIO_USE_UART0 TRUE
