#pragma once
// ============================================================
//  customer_routes.h
//  Declares ONE function that registers all customer-related
//  HTTP routes onto a Crow app. main.cpp calls this once at
//  startup.
// ============================================================

#define ASIO_STANDALONE
#include "crow_all.h"

void registerCustomerRoutes(crow::SimpleApp& app);