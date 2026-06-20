#include "db.h"
#include "customer.h"
#include <iostream>
using namespace std;

int main() {
    if (!connectDB()) return 1;

    int choice;
    do {
        cout << "\n1. Register  2. List  3. Search  0. Exit\nChoice: ";
        cin >> choice;
        if (choice == 1) registerCustomer();
        else if (choice == 2) listCustomers();
        else if (choice == 3) searchCustomerByName();
    } while (choice != 0);

    disconnectDB();
    return 0;
}