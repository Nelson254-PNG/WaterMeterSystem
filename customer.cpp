
//  Everything about registering, listing, and finding customers
#include "customer.h"
#include "db.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cctype>
using namespace std;

// Lowercases every character for case-insensitive search 

string toLowerStr(const string& s) {
    string result = s;
    transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}
// asks the database directly how many rows exist,via a COUNT(*) query. This is the database equivalent
//  of c.size().

string generateMeterNumber() {
    pqxx::work txn(getConnection());

    // exec() runs a query and returns a pqxx::result (like a
    // table of rows). For COUNT(*), there's exactly one row,
    // one column — we read it with r[0][0].
    pqxx::result r = txn.exec("SELECT COUNT(*) FROM customers");
    txn.commit();

    int count = r[0][0].as<int>();   // .as<int>() converts the
                                       // text result into a real int
    int nextId = count + 1;

    string padded = to_string(nextId);
    while (padded.length() < 3) padded = "0" + padded;
    return "MTR-" + padded;
}


bool customerExists(const string& customerId) {
    pqxx::work txn(getConnection());

    // pqxx::params{} wraps your values; exec() detects this and
    // safely substitutes them for the $1, $2... placeholders.
    // This is the modern replacement for exec_params().
    pqxx::result r = txn.exec(
        "SELECT COUNT(*) FROM customers WHERE id = $1",
        pqxx::params{customerId}
    );
    txn.commit();

    return r[0][0].as<int>() > 0;
}

//  Why this matters: if you ever need to change HOW a customer
//  gets inserted (add a field, change a default), you change
//  it in exactly one place, and both the CLI and the API
//  automatically get the fix.

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


//  Now just asks questions, then hands the answers straight
//  to registerCustomerLogic(). All the actual database work
//  has moved out of this function entirely.
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


//  BEFORE: looped through the vector with a for-range loop.
//  NOW: runs a SELECT, then loops through the RESULT SET —
//  conceptually identical, just the data source changed.
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

    // pqxx::result supports a range-based for loop, just like
    // your vector<Customer> did. Each "row" here is one customer.
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


//  BEFORE: looped through every customer, calling toLowerStr()
//  and string::find() manually in C++.
//  NOW: we let PostgreSQL do the searching using ILIKE —
//  a built-in case-insensitive pattern match. Much faster on
//  large tables since the database can use an index for this.
void searchCustomerByName() {
    cout << "\n--- Search Customer by Name ---\n";
    string query;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cout << "  Enter name to search: ";
    getline(cin, query);

    // The % signs are SQL wildcards meaning "anything before/after".
    // ILIKE is Postgres's case-INSENSITIVE version of LIKE.
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