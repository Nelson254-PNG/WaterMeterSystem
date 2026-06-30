#include "customer.h"
#include "db.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cctype>
using namespace std;


string toLowerStr(const string& s) {
    string result = s;
    transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

string generateMeterNumber() {
    pqxx::work txn(getConnection());

    pqxx::result r = txn.exec(
        "SELECT COALESCE(MAX(CAST(SUBSTRING(meter_number FROM 5) AS INTEGER)), 0) "
        "FROM customers"
    );
    txn.commit();

    int highestNumber = r[0][0].as<int>();
    int nextId = highestNumber + 1;

    string padded = to_string(nextId);
    while (padded.length() < 3) padded = "0" + padded;
    return "MTR-" + padded;
}

bool customerExists(const string& customerId) {
    pqxx::work txn(getConnection());

    pqxx::result r = txn.exec(
        "SELECT COUNT(*) FROM customers WHERE id = $1",
        pqxx::params{customerId}
    );
    txn.commit();

    return r[0][0].as<int>() > 0;
}

NewCustomerResult registerCustomerLogic(const string& name, const string& phone, double openingReading) {
    string meterNumber = generateMeterNumber();

    pqxx::work txn(getConnection());

    pqxx::result r = txn.exec(
        "INSERT INTO customers (name, meter_number, phone, balance, last_reading) "
        "VALUES ($1, $2, $3, 0.00, $4) "
        "RETURNING id",
        pqxx::params{name, meterNumber, phone, openingReading}
    );
    txn.commit();

    NewCustomerResult result;
    result.id          = r[0][0].as<string>();
    result.meterNumber = meterNumber;
    return result;
}

void registerCustomer() {
    cout << "\n--- Register New Customer ---\n";

    string name, phone;
    double openingReading;

    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cout << "  Full Name    : "; getline(cin, name);
    cout << "  Phone Number : "; getline(cin, phone);
    cout << "  Opening Meter Reading (m³): "; cin >> openingReading;

    try {
        NewCustomerResult result = registerCustomerLogic(name, phone, openingReading);

        cout << "\n  ✔ Registered!\n";
        cout << "  Customer ID  : " << result.id << "\n";
        cout << "  Meter Number : " << result.meterNumber << "\n";

    } catch (const exception& e) {
        cerr << "  ✘ Registration failed: " << e.what() << "\n";
    }
}

void deleteCustomerLogic(const string& customerId) {
    pqxx::work txn(getConnection());

    pqxx::result check = txn.exec(
        "SELECT id FROM customers WHERE id = $1",
        pqxx::params{customerId}
    );
    if (check.empty()) {
        throw runtime_error("Customer not found");
    }

    txn.exec(
        "DELETE FROM customers WHERE id = $1",
        pqxx::params{customerId}
    );

    txn.commit();
}

void deleteCustomer() {
    cout << "\n--- Delete Customer ---\n";

    string customerId;
    cout << "  Customer ID (paste UUID): ";
    cin >> customerId;

    cout << "  This will permanently delete this customer AND all their\n";
    cout << "  usage records, bills, and payments. Type YES to confirm: ";
    string confirm;
    cin >> confirm;

    if (confirm != "YES") {
        cout << "  Cancelled.\n";
        return;
    }

    try {
        deleteCustomerLogic(customerId);
        cout << "  ✔ Customer deleted.\n";
    } catch (const exception& e) {
        cerr << "  ✘ " << e.what() << "\n";
    }
}

void listCustomers() {
    cout << "\n--- Customers ---\n";

    pqxx::work txn(getConnection());
    pqxx::result r = txn.exec(
        "SELECT id, name, meter_number, phone, last_reading, balance "
        "FROM customers ORDER BY created_at"
    );
    txn.commit();

    if (r.empty()) { cout << "  None yet.\n"; return; }

    cout << "\n  " << left
         << setw(38) << "ID" << setw(22) << "Name"
         << setw(13) << "Meter" << setw(15) << "Phone"
         << setw(14) << "Last(m³)" << "Balance(KES)\n";
    cout << "  " << string(115, '-') << "\n";

    for (const auto& row : r) {
        cout << "  " << left
             << setw(38) << row["id"].as<string>()
             << setw(22) << row["name"].as<string>()
             << setw(13) << row["meter_number"].as<string>()
             << setw(15) << row["phone"].as<string>()
             << setw(14) << row["last_reading"].as<double>()
             << fixed << setprecision(2) << row["balance"].as<double>()
             << "\n";
    }
    cout << "\n";
}

void searchCustomerByName() {
    cout << "\n--- Search Customer by Name ---\n";
    string query;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cout << "  Enter name to search: ";
    getline(cin, query);

    string pattern = "%" + query + "%";

    pqxx::work txn(getConnection());
    pqxx::result r = txn.exec(
        "SELECT id, name, meter_number, phone, balance "
        "FROM customers WHERE name ILIKE $1",
        pqxx::params{pattern}
    );
    txn.commit();

    if (r.empty()) {
        cout << "  No match for \"" << query << "\".\n\n";
        return;
    }

    cout << "\n  " << left
         << setw(38) << "ID" << setw(22) << "Name"
         << setw(13) << "Meter" << setw(15) << "Phone" << "Balance(KES)\n";
    cout << "  " << string(100, '-') << "\n";

    for (const auto& row : r) {
        cout << "  " << left
             << setw(38) << row["id"].as<string>()
             << setw(22) << row["name"].as<string>()
             << setw(13) << row["meter_number"].as<string>()
             << setw(15) << row["phone"].as<string>()
             << fixed << setprecision(2) << row["balance"].as<double>()
             << "\n";
    }
    cout << "\n";
}