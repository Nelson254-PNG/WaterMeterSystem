#pragma once
#include <string>
using namespace std;

double recordUsageLogic(const string& customerId, double currentReading, const string& date);

void recordUsage();
void viewUsageHistory();