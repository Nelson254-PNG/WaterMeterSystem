#pragma once
// ============================================================
//  billing_routes.h
// ============================================================

#define ASIO_STANDALONE
#include "crow_all.h"

void registerBillingRoutes(crow::SimpleApp& app);