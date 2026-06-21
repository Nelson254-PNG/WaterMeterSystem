#pragma once
// ============================================================
//  payment_routes.h
// ============================================================

#define ASIO_STANDALONE
#include "crow_all.h"

void registerPaymentRoutes(crow::SimpleApp& app);