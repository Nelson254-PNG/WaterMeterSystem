#pragma once
#include <string>
using namespace std;

struct TierBreakdown {
    double tier1Units, tier2Units, tier3Units, tier4Units;
    double tier1Cost,  tier2Cost,  tier3Cost,  tier4Cost;
    double serviceCharge;
    double totalAmount;
};

TierBreakdown calculateTieredCost(double units);
struct GenerateBillResult {
    string billId;
    double totalUnits;
    TierBreakdown breakdown;
};
GenerateBillResult generateBillLogic(const string& customerId, const string& issueDate, const string& dueDate);

void generateBill();
void viewBills();