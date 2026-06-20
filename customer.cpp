// ============================================================
//  customer.cpp  (DATABASE VERSION)
//  Everything about registering, listing, and finding customers
//  now reads and writes through PostgreSQL instead of an
//  in-memory vector.
// ============================================================

#include "customer.h"
#include "db.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cctype>
using namespace std;

// ── Lowercases every character for case-insensitive search ────
// (Unchanged — this is pure string logic, no database involved)
string toLowerStr(const string& s) {
    string result = s;
    transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

// ============================================================
//  FUNCTION: generateMeterNumber
//
//  BEFORE: used customers.size() from the in-memory vector.
//  NOW: asks the database directly how many rows exist,
//  via a COUNT(*) query. This is the database equivalent
//  of c.size().
// ============================================================
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

// ============================================================
//  FUNCTION: customerExists
//  Checks the database directly — no more searching a vector.
// ============================================================
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

// ============================================================
//  FUNCTION: registerCustomer
//
//  BEFORE: built a Customer struct, pushed it into a vector.
//  NOW: collects the same input, then INSERTs a row.
//  Postgres generates the UUID automatically (DEFAULT
//  uuid_generate_v4() from our schema) — we don't set it.
// ============================================================
void registerCustomer() {
    cout << "\n--- Register New Customer ---\n";

    string name, phone, meterNumber;
    double openingReading;

    meterNumber = generateMeterNumber();

    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cout << "  Full Name    : "; getline(cin, name);
    cout << "  Phone Number : "; getline(cin, phone);
    cout << "  Opening Meter Reading (m³): "; cin >> openingReading;

    try {
        pqxx::work txn(getConnection());

        // RETURNING id lets us get back the auto-generated UUID
        // immediately after inserting — useful for confirming
        // to the user, or chaining further operations.
        pqxx::result r = txn.exec(
            "INSERT INTO customers (name, meter_number, phone, balance, last_reading) "
            "VALUES ($1, $2, $3, 0.00, $4) "
            "RETURNING id",
            pqxx::params{name, meterNumber, phone, openingReading}
        );
        txn.commit();

        string newId = r[0][0].as<string>();

        cout << "\n  ✔ Registered!\n";
        cout << "  Customer ID  : " << newId << "\n";
        cout << "  Meter Number : " << meterNumber << "\n";

    } catch (const exception& e) {
        // If anything goes wrong (e.g. duplicate meter_number,
        // which our schema's UNIQUE constraint would catch),
        // we land here instead of crashing.
        cerr << "  ✘ Registration failed: " << e.what() << "\n";
    }
}

// ============================================================
//  FUNCTION: listCustomers
//
//  BEFORE: looped through the vector with a for-range loop.
//  NOW: runs a SELECT, then loops through the RESULT SET —
//  conceptually identical, just the data source changed.
// ============================================================
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

// ============================================================
//  FUNCTION: searchCustomerByName
//
//  BEFORE: looped through every customer, calling toLowerStr()
//  and string::find() manually in C++.
//  NOW: we let PostgreSQL do the searching using ILIKE —
//  a built-in case-insensitive pattern match. Much faster on
//  large tables since the database can use an index for this.
// ============================================================
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