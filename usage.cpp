#include "usage.h"
#include "customer.h"
#include "global.h"
#include <iomanip>
#include <iostream>

using namespace std;

void recordUsage() {
  cout <<"Record Water Usage \n";
  if (customers.empty()){
    cout <<" No customers registered yet. Please register first.\n";
    return;
  }
  int id;
  cout <<"Enter Customer ID: ";
  cin >>id;

  Customer* c = findCustomerById(id);
  if (c == nullptr) {
    cout <<"Customer ID "<< id <<"not found.\n";
    return;
  }

  cout << " Customer : "<< c->name <<"\n";
  cout << "Meter Number : "<< c->meterNumber <<"\n";
  cout << "Last Reading : "<< c->lastReading <<"\n";

  double currentReading;
  cout <<"Current Reading (m³): ";
  cin >> currentReading;

  if (currentReading < c->lastReading) {
    cout <<"Error: Current reading ("<< currentReading << ") cannot be less than last reading ("<< c->lastReading <<").\n";
    return;
  }
  
  string date;
  cin.ignore(numeric_limits<streamsize>::max(), '\n');
  cout <<" Date (YYYY-MM-DD) : ";
  getline(cin, date);

  WaterRecord wr;
  wr.recordId = c->records.size() + 1;
  wr.date = date;
  wr.previousReading = c->lastReading;
  wr.currentReading = currentReading;
  wr.unitsUsed = currentReading - c->lastReading;
  wr.billed = false;

  c->records.push_back(wr);
  c->lastReading = currentReading;

  cout <<"Usage recorded! \n";
  cout << " Units Used this period : " << wr.unitsUsed <<"m³\n";

}
void viewUsageHistory(){
  cout <<"View Usage History\n";
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

  if (c->records.empty()){
    cout <<" No usage records yet.\n";
    return;
  }
  cout << "\n"
       << left
       << setw(5) <<"#"
       << setw(14) <<"Date"
       << setw(14) <<"Prev (m³)"
       << setw(14) <<"Current (m³)"
       << setw(12) <<"Used (m³)"
       << setw(10) <<"Billed?"
       << "\n";
  cout <<" " << string(70, '-') <<"\n";
  
  for (const auto& r : c->records) {
    cout <<" "
         << left
         << setw(5) <<r.recordId
         << setw(14) <<r.date
         << setw(14) <<r.previousReading
         << setw(14) <<r.currentReading
         << setw(12) <<r.unitsUsed
         << setw(10) <<(r.billed ? "Yes" : "No")
         << "\n";
  }
  cout <<"\n";
}