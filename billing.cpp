#include "billing.h"
#include "global.h"
#include "constants.h"
#include "customer.h"
#include <iostream>
#include <iomanip>
using namespace std;

//biiling engine
void calculateTieredCost(double units, Bill& bill){
  bill.tier1Units = 0;
  bill.tier2Units = 0;
  bill.tier3Units = 0;
  bill.tier4Units = 0;

  double remaining = units;
  //0 - 6 m^3 at kes 50
  if (remaining > 0){
    bill.tier1Units = min(remaining, TIER1_LIMIT);
    remaining -= bill.tier1Units;
  }
  //7 - 20 at kes 75
  if (remaining > 0){
    double tier2Capacity = TIER2_LIMIT - TIER1_LIMIT;
    bill.tier2Units = min(remaining, tier2Capacity);
    remaining -= bill.tier2Units;
  }
  //21 - 50 at kes 100
  if (remaining > 0){
    double tier3Capacity = TIER3_LIMIT - TIER2_LIMIT;
    bill.tier3Units = min(remaining, tier3Capacity);
    remaining -= bill.tier3Units;
  }
  //51+ at kes 150
  if (remaining > 0){
    bill.tier4Units = remaining;
  }

  //calculate cost per tier
  bill.tier1Cost = bill.tier1Units * TIER1_RATE;
  bill.tier2Cost = bill.tier2Units * TIER2_RATE;
  bill.tier3Cost = bill.tier3Units * TIER3_RATE;
  bill.tier4Cost = bill.tier3Units * TIER4_RATE;

  //fixed charge
  bill.serviceCharge = SERVICE_CHARGE;
  
  //total
  bill.totalAmount = bill.tier1Cost + bill.tier2Cost + bill.tier3Cost + bill.tier4Cost + bill.serviceCharge;

}
void generateBill(){
  cout <<"\n Generate Bill \n";

  if (customers.empty()){
    cout <<"No customers registered yet.\n";
    return;
  }
  int id;
  cout << "Enter Customer ID: ";
  cin >>id;

  Customer* c = findCustomerById(id);
  if (c == nullptr) {
    cout <<"Customer not found. \n";
    return;
  }
  //find unbilled records
  double totalUnits = 0.0;
  int unbilledCount = 0;
  
  for (const auto& r : c->records) {
    if(!r.billed){
      totalUnits += r.unitsUsed;
      unbilledCount++;
    }
  }

  if (unbilledCount == 0) {
    cout <<"No unbilled usage records found for "<< c->name <<".\n";
    return;
  }

  //get bill dates from users
  string issueDate, dueDate;
  cin.ignore(numeric_limits<streamsize>::max(), '\n');
  cout <<"Issue Date (YYYY-MM-DD): ";
  getline(cin, issueDate);
  cout <<"Due Date (YYYY-MM-DD): ";
  getline(cin, dueDate);

  Bill bill;
  bill.billId = c->bills.size() + 1;
  bill.issueDate = issueDate;
  bill.dueDate = dueDate;
  bill.totalUnits = totalUnits;
  bill.paid = false;

  calculateTieredCost(totalUnits, bill);

  for (auto& r : c->records) {
    if (!r.billed){
      r.billed = true;
    }
  }
  c->bills.push_back(bill);
  c->balance += bill.totalAmount;

  //print the bill receipts
  cout <<"\n";
  cout <<" WATER BILL RECEIPT \n";
  cout <<"Customer :" << left << setw(27) <<c->name <<"\n";
  cout <<"Meter No :" << left << setw(27) <<c->meterNumber <<"\n";
  cout <<"Bill No :" << left << setw(27) <<bill.billId <<"\n";
  cout <<"Issued :" << left << setw(27) <<issueDate <<"\n";
  cout <<"Due Date :" << left << setw(27) <<dueDate <<"\n";
  cout <<"\n";
  cout <<" USAGE BREAKDOWN\n";
  cout << fixed << setprecision(2);
  cout << "Total Units : "<< setw(6) <<totalUnits<<"m³" << setw(18)<<"\n";
  cout <<"\n";
  cout <<" TIER CHARGES \n";
  if (bill.tier1Units > 0)
  cout <<"Tier 1("<<setw(5) <<bill.tier1Units <<"m³ * KES 50) = "<< setw(8) <<bill.tier1Cost <<"\n";
  if (bill.tier2Units > 0)
  cout <<"Tier 2("<<setw(5) <<bill.tier2Units <<"m³ * KES 75) = "<< setw(8) <<bill.tier2Cost <<"\n";
  if (bill.tier3Units > 0)
  cout <<"Tier 3("<<setw(5) <<bill.tier3Units <<"m³ * KES 100) = "<< setw(8) <<bill.tier3Cost <<"\n";
  if (bill.tier4Units > 0)
  cout <<"Tier 4("<<setw(5) <<bill.tier4Units <<"m³ * KES 150) = "<< setw(8) <<bill.tier4Cost <<"\n";

  cout <<"Service Charge =" <<setw(8) <<bill.serviceCharge <<"\n";
  cout <<"\n";
  cout <<"TOTAL DUE : KES "<< setw(21) <<bill.totalAmount <<"\n";
  cout <<"\n";
}
// view bills
void viewBills(){
  cout <<"View Bills\n";
  if (customers.empty()){
    cout <<" No Customers registered yet. \n";
    return;
  }

  int id;
  cout << " Enter Customer ID: ";
  cin >> id;

  Customer* c = findCustomerById(id);
  if (c == nullptr){
    cout << "Customer not found.\n";
    return;
  }
  cout <<"\n Customer : " << c->name <<"\n";
  cout << "Meter No. : " << c->meterNumber <<"\n";
  cout << "Balance : KES" << fixed << setprecision(2) <<c->balance <<"\n";

  if (c->bills.empty()){
    cout <<" No bills generated yet.\n";
    return;
  }
  cout << "\n"
       << left
       << setw(8) <<"Bill #"
       << setw(14) <<"Issued"
       << setw(14) <<"Due Date"
       << setw(12) <<"Units (m³)"
       << setw(14) <<"Amount (KES)"
       << setw(10) <<"Status"
       << "\n";
  cout <<" " << string(72, '-') <<"\n";
  
  for (const auto& b : c->bills) {
    cout <<" "
         << left
         << setw(8) <<b.billId
         << setw(14) <<b.issueDate
         << setw(14) <<b.dueDate
         << setw(12) <<b.totalUnits
         << setw(14) << fixed << setprecision(2)<<b.totalAmount
         << setw(10) <<(b.paid? "PAID" : "UNPAID")
         << "\n";
  }
  cout <<"\n";
}