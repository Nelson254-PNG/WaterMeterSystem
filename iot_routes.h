#pragma once
// ============================================================
//  iot_routes.h
//  Routes for IoT meter data ingestion, valve control,
//  alert management, and prepaid token system.
// ============================================================

#define ASIO_STANDALONE
#include "crow_all.h"

void registerIoTRoutes(crow::SimpleApp& app);