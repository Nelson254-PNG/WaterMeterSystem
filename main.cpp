#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
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

struct Customer{
  int id;
  string name;
  string meterNumber;
  string phone;
  double balance;
  double lastReading;
  vector<WaterRecord> records;
};

vector<Customer> customers;

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

void displayMenu(){
  cout <<"\n";
  cout <<"SMART WATER METER & PAYMENT SYSTEM \n";
  cout <<"\n";
  cout <<" 1. Register New Customer\n";
  cout <<" 2. List All Customers\n";
  cout <<" 3. Record Water Usage\n";
  cout <<" 4. View Usage History\n";
  cout <<" 5. View Bill\n";
  cout <<" 6. Make Payment\n";
  cout <<" 7. View Payment History\n";
  cout <<" 0. Exit\n";
  cout <<"\n";
  cout <<" Enter your choice: ";
}
int main(){
  int choice;
  do{
    displayMenu();
    cin >> choice;

    switch(choice){
      case 1: registerCustomer(); break;
      case 2: listCustomers(); break;
      case 3: recordUsage(); break;
      case 4: viewUsageHistory(); break;    
      case 5:
          cout <<"\n View Bill\n";
          break;  
      case 6:
          cout <<"\n Make Payment\n";
          break; 
      case 7:
          cout <<"\n View History\n";
          break;  
      case 0:
          cout <<"\n Thankyou for visiting! Existing system\n";
          break;
      default:
          cout <<"\n Invalid Choice. Please select from Menu\n";
          break;               
    }
  } while (choice != 0);
  return 0;
}

