#include <iostream>
#include <string>
#include <limits>

using namespace std;

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
          cout <<"\n Register Customer\n";
          break;
      case 2:
          cout <<"\n Login\n";
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

