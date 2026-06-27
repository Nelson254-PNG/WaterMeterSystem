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
//  BUG FIX: this used to be based on COUNT(*) — but if any
//  customer is ever deleted, the count drops while the highest
//  METER NUMBER EVER ISSUED stays the same, causing a future
//  signup/registration to generate a number that collides with
//  an existing customer (exactly what happened in testing —
//  delete one customer, the next signup reuses their number).
//
//  THE FIX: look at the highest existing meter number's numeric
//  suffix directly, and add 1 to THAT — deletions can never
//  cause a number to be reused this way.
// ============================================================
string generateMeterNumber() {
    pqxx::work txn(getConnection());

    // Extract the numeric part of every meter number (e.g.
    // "MTR-007" -> 7), and take the largest one. COALESCE
    // handles the case where there are zero customers yet —
    // MAX() of nothing is NULL, so we default to 0.
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
//  FUNCTION: registerCustomerLogic   (THE WORKER)
//
//  THIS is the actual database work — no cin, no cout.
//  Takes values directly as parameters, returns the result,
//  throws on failure. Both registerCustomer() (CLI) and the
//  future API handler call THIS SAME function.
//
//  Why this matters: if you ever need to change HOW a customer
//  gets inserted (add a field, change a default), you change
//  it in exactly one place, and both the CLI and the API
//  automatically get the fix.
// ============================================================
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

// ============================================================
//  FUNCTION: registerCustomer   (THE CLI WRAPPER)
//
//  Now just asks questions, then hands the answers straight
//  to registerCustomerLogic(). All the actual database work
//  has moved out of this function entirely.
// ============================================================
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

// ============================================================
//  FUNCTION: deleteCustomerLogic   (THE WORKER)
//
//  Thanks to "ON DELETE CASCADE" on every foreign key in
//  schema.sql (water_records.customer_id, bills.customer_id,
//  payments.customer_id), ONE DELETE statement here is enough —
//  Postgres automatically removes every water record, bill,
//  and payment that referenced this customer. We don't need
//  to manually delete from four tables in the right order,
//  the way you'd have to without those CASCADE rules.
// ============================================================
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

// ============================================================
//  FUNCTION: deleteCustomer   (THE CLI WRAPPER)
// ============================================================
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