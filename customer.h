#pragma once

#include <string>
using namespace std;

// Generates a formatted meter number like "MTR-003" based on
// how many customers currently exist IN THE DATABASE.
string generateMeterNumber();

// Converts a string to lowercase (used for case-insensitive search)
string toLowerStr(const string& s);

// Checks if a customer with this UUID exists.
bool customerExists(const string& customerId);


struct NewCustomerResult {
    string id;
    string meterNumber;
};
NewCustomerResult registerCustomerLogic(const string& name, const string& phone, double openingReading);

void registerCustomer();
void listCustomers();
void searchCustomerByName();