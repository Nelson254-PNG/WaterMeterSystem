#include "customer.h"
#include "global.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cctype>

using namespace std;


string generateMeterNumber(int id) {
  string padded = to_string(id);
  while (padded.length() < 3) {
    padded = "0" + padded;
  }
  return "MTR-" + padded;
}
//returns a pointer to the found customer or nullptr if not found
Customer* findCustomerById(int id) {
  for (auto& c : customers){
    if (c.id == id) {
      return &c;
    }
  }
  return nullptr;
}

// helps to search even when the user types in small letters
string toLowerStr(const string& s){
  string result = s;
  transform(result.begin(), result.end(), result.begin(), ::tolower); return result;
}
void searchCustomerByName(){
  cout <<"\n Search Customer by Name \n";
  string query;
  cin.ignore(numeric_limits<streamsize>::max(), '\n');
  cout <<" Enter name to search: ";
  getline(cin, query);
  string lowerQuery = toLowerStr(query);
  bool found = false;
  cout << "\n "<< left << setw(5) <<"ID" <<setw(22)<<"Name"
       << setw(13) <<"Meter" << setw(15) <<"Phone" << setw(14) <<"Balance(KES)"<<"\n\n";
  
  for (const auto& c : customers){
    if(toLowerStr(c.name).find(lowerQuery) != string::npos){
      cout <<" "<< left << setw(5) << c.id << setw(22) << c.name
          << setw(13) << c.meterNumber << setw(15) << c.phone << fixed << setprecision(2) << c.balance <<"\n";
      found = true;    
    }
  }
  
  if (!found)
    cout << " No customer found matching \"" << query << "\".\n";
  cout <<"\n";
}
void registerCustomer() {
  cout <<" Register New Customer \n";
  Customer c;

  c.id = customers.size() + 1;
  c.meterNumber = generateMeterNumber(c.id);
  c.balance = 0.0;
  c.lastReading = 0.0;

  cin.ignore(numeric_limits<streamsize>::max(), '\n');
  
  cout << " Full Name :";
  getline(cin, c.name);

  cout <<" Phone Number :";
  getline(cin, c.phone);

  cout <<" Opening Meter Reading (m³): ";
  cin >> c.lastReading;

  customers.push_back(c);

  cout <<"\n Customer registered successfully!\n";
  cout <<" Meter Number assigned: "<<c.meterNumber <<"\n";
  cout <<" Customer ID: "<<c.id<<"\n";

}
void listCustomers() {
  cout <<"\n Registered Customers\n";

  if (customers.empty()) {
    cout <<" No customers registered yet.\n";
    return;
  }

  cout <<"\n"
       << left
       << setw(5) <<"ID"
       << setw(20) <<"Name"
       << setw(12) <<"Meter No."
       << setw(15) <<"Phone"
       << setw(14) <<"Last Read (m³)"
       << setw(10) <<"Balance (KES)"
       <<"\n"; 
       
  cout << " " <<string(78, '-') << "\n";


  for (const auto& c : customers){
    cout <<" "
         << left
         << setw(5) <<c.id
         << setw(20) <<c.name 
         << setw(12) <<c.meterNumber  
         << setw(15) <<c.phone 
         << setw(14) <<c.lastReading
         << setw(10) <<c.balance 
         <<"\n";      
  } 
  cout <<"\n";     
}