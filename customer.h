#pragma once
#include <string>

#include "models.h"

string generateMeterNumber(int id);

Customer* findCustomerById(int id);

string toLowerStr(const string& s);

void registerCustomer();
void listCustomers();
void searchCustomerByName();