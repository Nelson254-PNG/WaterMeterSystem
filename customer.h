#pragma once
// ============================================================
//  customer.h
//  Declarations for all customer-related functions.
//  UPDATED for database version — IDs are now UUID strings,
//  not array indexes. No more in-memory vector<Customer>.
//
//  PHASE C ADDITION: registerCustomerLogic() is a "worker"
//  function — pure database logic, no cin/cout. Both the CLI
//  function (registerCustomer) and the future API handler
//  call into this SAME function, so the actual database
//  behavior only exists in ONE place.
// ============================================================

#include <string>
using namespace std;

// Generates a formatted meter number like "MTR-003" based on
// how many customers currently exist IN THE DATABASE.
string generateMeterNumber();

// Converts a string to lowercase (used for case-insensitive search)
string toLowerStr(const string& s);

// Checks if a customer with this UUID exists.
bool customerExists(const string& customerId);

// ── THE WORKER FUNCTION ───────────────────────────────────────
// Takes already-known values, does the database insert, and
// returns the new customer's UUID and meter number.
// Throws an exception on failure (caller decides how to report it —
// CLI prints to cerr, API turns it into an HTTP error response).
//
// We use a struct for the return value since we need to hand
// back TWO things (id and meterNumber) from one function call.
struct NewCustomerResult {
    string id;
    string meterNumber;
};
NewCustomerResult registerCustomerLogic(const string& name, const string& phone, double openingReading);

void registerCustomer();
void listCustomers();
void searchCustomerByName();