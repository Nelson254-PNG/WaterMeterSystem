#pragma once
#include <string>

using namespace std;

string generateMeterNumber(int id);

bool customerExists(const string& customerId);

string toLowerStr(const string& s);

void registerCustomer();
void listCustomers();
void searchCustomerByName();