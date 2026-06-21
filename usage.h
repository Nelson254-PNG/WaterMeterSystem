#pragma once
// ============================================================
//  usage.h
//  Declarations for water usage recording functions.
//  PHASE C: worker functions added for API reuse.
// ============================================================

#include <string>
using namespace std;

// ── THE WORKER ────────────────────────────────────────────────
// Pure logic: takes a customerId, reading, date — does the
// insert + balance update — returns the units used.
// Throws on failure (customer not found, reading invalid, etc.)
double recordUsageLogic(const string& customerId, double currentReading, const string& date);

void recordUsage();
void viewUsageHistory();