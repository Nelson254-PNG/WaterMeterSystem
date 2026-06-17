#pragma once

#include <string>
#include <vector>
#include <limits>

using namespace std;

struct WaterRecord {
  int recordId;
  string date;
  double currentReading;
  double previousReading;
  double unitsUsed;
  bool billed;
};
//BILLS
struct Bill {
  int billId;
  string issueDate;
  string dueDate;
  double totalUnits, tier1Units, tier2Units, tier3Units, tier4Units;
  double tier1Cost, tier2Cost, tier3Cost, tier4Cost;
  double serviceCharge;
  double totalAmount;
  double amountPaid;
  bool paid;
};
struct Payment{
  int paymentId;
  int billId;
  string date;
  string method;
  string reference;
  double amountPaid;
  double balanceBefore;
  double balanceAfter;
};

struct Customer{
  int id;
  string name;
  string meterNumber;
  string phone;
  double balance;
  double lastReading;
  vector<WaterRecord> records;
  vector<Bill> bills;
  vector<Payment> payments;
};