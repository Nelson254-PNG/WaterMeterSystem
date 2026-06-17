#include <iostream>
#include "customer.h"
#include "usage.h"
#include "billing.h"
#include "payment.h"
#include "reports.h"

using namespace std;

void displayMenu(){
  cout <<"\n";
  cout <<"SMART WATER METER & PAYMENT SYSTEM \n";
  cout <<"\n";
  cout <<" CUSTOMERS\n";
  cout <<" 1. Register New Customer\n";
  cout <<" 2. List All Customers\n";
  cout <<" 3. Search Customer By Name\n";
  cout <<"\n";
  cout <<" USAGE & BILLING \n";
  cout <<" 4. Record Water Usage\n";
  cout <<" 5. View Usage History\n";
  cout <<" 6. Generate Bill\n";
  cout <<" 7. View Bill\n";
  cout <<"\n";
  cout <<" PAYMENTS\n";
  cout <<" 8. Make Payment\n";
  cout <<" 9. Pay via M-Pesa Paybill\n";
  cout <<" 10.View Payment History\n";
  cout <<"\n";
  cout <<" REPORTS\n";
  cout <<" 11. Account Statement\n";
  cout <<" 12. System Dashboard\n";
  cout <<"\n";
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
      case 3: searchCustomerByName(); break;
      case 4: recordUsage(); break;
      case 5: viewUsageHistory(); break; 
      case 6: generateBill(); break;   
      case 7: viewBills(); break;   
      case 8: makePayment(); break; 
      case 9: payByMpesaPaybill(); break;
      case 10: viewPaymentHistory(); break; 
      case 11: accountStatement(); break;
      case 12: systemDashboard(); break; 
      case 0:cout <<"\n Goodbye!\n\n"; break;
      default:cout <<"\n Invalid Choice. Enter 0-11\n"; break;               
    }
  } while (choice != 0);
  return 0;
}

