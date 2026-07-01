#include <iostream>
#include "db.h"
#include "customer.h"
#include "usage.h"
#include "billing.h"
#include "payment.h"
#include "reports.h"

using namespace std;

void displayMenu() {
    cout << "\n";
    cout << " SMART WATER METER & PAYMENT SYSTEM  \n";
    cout << "\n";
    cout << " CUSTOMERS \n";
    cout << "  1. Register New Customer\n";
    cout << "  2. List All Customers   \n";
    cout << "  3. Search Customer by Name\n";
    cout << "  4. Delete Customer \n";
    cout << "\n";
    cout << " USAGE & BILLING \n";
    cout << "  5. Record Water Usage \n";
    cout << "  6. View Usage History \n";
    cout << "  7. Generate Bill \n";
    cout << "  8. View Bills\n";
    cout << "\n";
    cout << " PAYMENTS\n";
    cout << "  9. Make Payment (Cash/Bank/Other)\n";
    cout << "  10. Pay via M-Pesa Paybill \n";
    cout << "  11. Pay via M-Pesa Till \n";
    cout << "  12. View Payment History \n";
    cout << "\n";
    cout << " REPORTS\n";
    cout << "  13. Account Statement\n";
    cout << "  14. System Dashboard \n";
    cout << "\n";
    cout << "  0. Exit\n";
    cout << "\n";
    cout << "  Enter your choice: ";
}

int main() {
    
    if (!connectDB()) {
        cerr << "Could not start: database connection failed.\n";
        return 1;
    }

    int choice;
    do {
        displayMenu();
        cin >> choice;
        switch (choice) {
            case 1:  registerCustomer();    break;
            case 2:  listCustomers();       break;
            case 3:  searchCustomerByName();break;
            case 4:  deleteCustomer();break;
            case 5:  recordUsage();         break;
            case 6:  viewUsageHistory();    break;
            case 7:  generateBill();        break;
            case 8:  viewBills();           break;
            case 9:  makePayment();         break;
            case 10: payByMpesaPaybill();   break;
            case 11: payByMpesaTill(); break;
            case 12: viewPaymentHistory();  break;
            case 13: accountStatement();    break;
            case 14: systemDashboard();     break;
            case 0:  cout << "\n  Goodbye!\n\n"; break;
            default: cout << "\n  Invalid. Enter 0–12.\n"; break;
        }
    } while (choice != 0);

    disconnectDB();
    return 0;
}