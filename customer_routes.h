#pragma once

//  Declares ONE function that registers all customer-related
//  HTTP routes onto a Crow app. main.cpp calls this once at

#define ASIO_STANDALONE
#include "crow_all.h"

void registerCustomerRoutes(crow::SimpleApp& app);