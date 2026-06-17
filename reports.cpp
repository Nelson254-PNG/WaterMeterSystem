#include "reports.h"
#include "global.h"
#include "customer.h"
#include <iostream>
#include <iomanip>
using namespace std;


static void sep(int w = 60, char ch = '=') {
  cout <<" "<< string(w, ch) <<"\n";
}

//sytemDashboard
void systemDashboard(){
  cout <<"\n";
  cout <<" SYSTEM DASHBOARD \n";
  
  if (customers.empty()) {
    cout <<" No data yet. Register some customers first.\n"; return;
  }
  //counters to accumulate
  int totalCustomers = customers.size();
  int totalBills = 0;
  int totalPaidBills = 0;
  int totalUnpaidBills = 0;
  int totalPayments = 0;
  int totalReadings = 0;
  double totalUnitUsed = 0.0;
  double totalBilled = 0.0;
  double totalCollected = 0.0;
  double totalOutstanding = 0.0;
  double totalCredit = 0.0;

  //walks every customer and their data
  for (const auto& c : customers){
    //USAGE
    totalReadings += c.records.size();
    for (const auto& r: c.records)
        totalUnitUsed += r.unitsUsed;
    //BILLS  
    totalBills += c.records.size();
    for (const auto& b : c.bills)  {
      totalBilled += b.totalAmount;
      totalCollected += b.amountPaid;
      if (b.paid) totalPaidBills++;
      else totalUnpaidBills++;
    }
    //PAYMENTS
    totalPayments += c.payments.size();
    if (c.balance > 0) totalOutstanding += c.balance;
    else if (c.balance < 0) totalCredit += abs(c.balance);
  }
  //print dashboard
  cout << fixed <<setprecision(2);

  cout <<"\n CUSTOMERS \n";
  cout << " Total Registered : "<< totalCustomers <<"\n";

  cout <<"\n WATER USAGE \n";
  cout <<" Total Readings : "<< totalReadings <<"\n";
  cout <<" Total Units Used : "<< totalUnitUsed <<" m³\n";

  cout <<"\n BILLING \n";
  cout <<" Bills Generated : "<< totalBills<<"\n";
  cout <<" Bills Paid : "<< totalPaidBills <<"\n";
  cout <<" Bills Unpaid : "<< totalUnpaidBills <<"\n";
  cout <<" Total Billed : "<< totalBilled <<"\n";

  cout <<"\n PAYMENTS \n";
  cout <<" Total Transations : "<< totalPayments<<"\n";
  cout <<" Total Collected(KES) : "<< totalCollected <<"\n";
  cout <<" Outstanding(KES): "<< totalOutstanding <<"\n";
  cout <<" Credit on Accs(KES): "<< totalCredit <<"\n";

  //collection rate: how much of what was billed has been paid
  if (totalBilled > 0){
    double rate = (totalCollected / totalBilled) * 100.0;
    cout <<"\n Collection Rate : " << rate << "%\n";
  }
  cout <<"\n";
}

//accountstatement
void accountStatement(){
  cout <<"\n Account Statement \n";
  if (customers.empty()) {cout << " No customers yet.\n"; return; }

  int id;
  cout <<"Enter Customer ID: ";
  cin >>id;
  Customer* c = findCustomerById(id);
  if (!c){cout <<"Not found.\n"; return;}

  cout << fixed << setprecision(2);
  cout <<"\n";
  cout << "ACCOUNT STATEMENT \n";

  //customer info
  cout <<" Name : "<< c->name <<"\n";
  cout <<" Customer : "<< c->id <<"\n";
  cout <<" Meter Number : "<< c->meterNumber <<"\n";
  cout <<" Phone : "<< c->phone <<"\n";
  cout <<" Last Reading : "<< c->lastReading <<"\n";
  cout <<" Balance : "<< c->balance << (c->balance < 0 ? "(CREDIT)" : (c->balance == 0 ? "(CLEAR)" : "(OWING)")) << "\n";

  //usage records
  cout  <<"\n";
  cout << " WATER USAGE RECORDS ("<< c->records.size() <<"total)\n";

  if (c->records.empty()) {
    cout <<" No usage records.\n";
  }else {
    cout <<" "<< left << setw(5) <<"#" << setw(14) << "Date"
         << setw(12) << "Prev(m³)" << setw(12) << "Curr(m³)"
         << setw(12) << "Used(m³)" << setw(8) << "Billed" <<"\n";
    double totalUsed = 0;
    for (const auto& r : c->records){
      cout <<" " << left << setw(5) << r.recordId
           << setw (14) << r.date
           << setw (12) << r.previousReading
           << setw (12) << r.currentReading
           << setw (12) << r.unitsUsed
           << (r.billed ? "Yes" : "No") << "\n";
      totalUsed += r.unitsUsed;     
    } 
    cout <<" " << string(63, '-') <<"\n";
    cout <<"Total Usage: " << totalUsed << "m³\n";    
  }
  // bills
  cout <<"\n";
  cout <<" BILLS ("<< c->bills.size()<<" total)\n";

  if (c->bills.empty()) {
    cout <<" No bills generated.\n";
  }else{
    cout <<" "<< left << setw(8) <<"Bill #" << setw(14) <<"Issued"
         << setw(14) << "Due" << setw(10) << "Units" << setw(12) <<"Total(KES)" 
         << setw(12) << "Paid(KES)" << setw(8) <<"Status" <<"\n";
    double totalBilled = 0, totalPaid = 0;
    for (const auto& b : c->bills) {
      cout <<" " << left << setw(8) << b.billId
           << setw(14) << b.issueDate << setw(14) << b.dueDate
           << setw(10) << b.totalUnits
           << setw(12) << b.totalAmount
           << setw(12) << b.amountPaid
           <<(b.paid ? "PAID" : "UNPAID") <<"\n";
      totalBilled += b.totalAmount;
      totalPaid += b.amountPaid;     
    } 
    cout <<" " << string(63, '-') << "\n";
    cout <<" Total Billed : KES" << totalBilled <<"\n";
    cout <<" Total Paid : KES" << totalPaid <<"\n";   
  }
  //payments
  cout <<"\n";
  cout <<" PAYMENTS(" << c->payments.size() <<"total)\n";
  
  if (c->payments.empty()) {
    cout <<" No payments made.\n";
  }else{
    cout <<" " << left << setw(6) <<"Pay #"<< setw(8) <<"Bill #"
         << setw(14) << "Date" << setw(14) <<"Method"
         << setw(12) << "Amount" << setw(14) <<"Bal After" <<"\n";
    double totalPaid = 0;
    for (const auto& p : c->payments) {
      cout << " " << left << setw(6) << p.paymentId
           << setw(8) << p.billId
           << setw(14) << p.date
           << setw(14) << p.method
           << setw(12) <<p.amountPaid
           << p.balanceAfter <<"\n";
      totalPaid += p.amountPaid;     
    }
    cout <<" " << string(63, '=') <<"\n";
    cout << " Total Paid: KES "<< totalPaid <<"\n";       
  }
  //summary
  cout <<"\n";
  cout <<" ACCOUNT SUMMARY\n";
  cout << "Current Balance : KES" << c->balance;
  if (c->balance < 0) cout << "You have credit!\n";
  else if (c->balance == 0) cout <<"Account clear.\n";
  else cout << "Amount Owing.\n";
  cout <<"\n";
}