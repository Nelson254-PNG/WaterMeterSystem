#pragma once

#include <string>
using namespace std;


string generateMeterNumber();

string toLowerStr(const string& s);


bool customerExists(const string& customerId);


struct NewCustomerResult {
    string id;
    string meterNumber;
};
NewCustomerResult registerCustomerLogic(const string& name, const string& phone, double openingReading);

void deleteCustomerLogic(const string& customerID);

void registerCustomer();
void listCustomers();
void searchCustomerByName();
void deleteCustomer();