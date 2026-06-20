#pragma once
#include <pqxx/pqxx>

bool connectDB();

pqxx::connection& getConnection();

void disconnectDB();