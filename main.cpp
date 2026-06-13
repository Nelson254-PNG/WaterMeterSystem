#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <limits>

using namespace std;

struct Customer{
  int id;
  string name;
  string meterNumber;
  string phone;
  double balance;
};

vector<Customer> customers;

string generateMeterNumber(int id) {
  string padded = to_string(id);
  while (padded.length() < 3) {
    padded = "0" + padded;
  }
  return "MTR-" + padded;
}

void registerCustomer() {
  cout <<" Register New Customer \n";
  Customer c;

  c.id = customers.size() + 1;
  c.meterNumber = generateMeterNumber(c.id);
  c.balance = 0.0;

  cin.ignore(numeric_limits<streamsize>::max(), '\n');
  
  cout << " Full Name :";
  getline(cin, c.name);

  cout <<" Phone Number :";
  getline(cin, c.phone);

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
       << setw(10) <<"Balance (KES)"
       <<"\n"; 
       
  cout << " " <<string(65, '-') << "\n";


  for (const auto& c : customers){
    cout <<" "
         << left
         << setw(5) <<c.id
         << setw(20) <<c.name 
         << setw(12) <<c.meterNumber  
         << setw(15) <<c.phone 
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
  cout <<" 2. Login\n";
  cout <<" 3. Record Water Usage\n";
  cout <<" 4. View Bill\n";
  cout <<" 5. Make Payment\n";
  cout <<" 6. View Payment History\n";
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
      case 1:
        registerCustomer();
        break;
      case 2:
        listCustomers();
        break;
      case 3:
          cout <<"\n Record Water Customer\n";
          break;
      case 4:
          cout <<"\n View Bill\n";
          break;  
      case 5:
          cout <<"\n Make Payment\n";
          break; 
      case 6:
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

