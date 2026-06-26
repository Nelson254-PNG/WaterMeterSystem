#pragma once
// ============================================================
//  auth_routes.h
// ============================================================

#define ASIO_STANDALONE
#include "crow_all.h"

void registerAuthRoutes(crow::SimpleApp& app);