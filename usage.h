#pragma once


#include <string>
using namespace std;

// Pure logic: takes a customerId, reading, date — does the
// insert + balance update — returns the units used.
// Throws on failure (customer not found, reading invalid, etc.)
double recordUsageLogic(const string& customerId, double currentReading, const string& date);

void recordUsage();
void viewUsageHistory();