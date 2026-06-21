// ============================================================
//  usage.cpp  (DATABASE VERSION)
//  Recording meter readings and viewing usage history.
//  Every record now belongs to a customer via customer_id —
//  a real foreign key, not a nested vector.
// ============================================================

#include "usage.h"
#include "db.h"
#include <iostream>
#include <iomanip>
using namespace std;

// ============================================================
//  FUNCTION: recordUsageLogic   (THE WORKER)
//
//  Pure database logic — no cin/cout. Validates that the
//  reading isn't going backwards, then does the insert +
//  balance update in one transaction. Throws on any failure,
//  including "customer not found" and "reading too low" —
//  these are now exceptions, not printed messages, since the
//  caller (CLI or API) decides how to report them.
// ============================================================
double recordUsageLogic(const string& customerId, double currentReading, const string& date) {
    pqxx::work txn(getConnection());

    pqxx::result custResult = txn.exec(
        "SELECT last_reading FROM customers WHERE id = $1",
        pqxx::params{customerId}
    );

    if (custResult.empty()) {
        throw runtime_error("Customer not found");
    }

    double lastReading = custResult[0]["last_reading"].as<double>();

    if (currentReading < lastReading) {
        throw runtime_error("Reading cannot be less than the last recorded reading (" +
                             to_string(lastReading) + ")");
    }

    double unitsUsed = currentReading - lastReading;

    txn.exec(
        "INSERT INTO water_records "
        "(customer_id, reading_date, previous_reading, current_reading, units_used, billed) "
        "VALUES ($1, $2, $3, $4, $5, false)",
        pqxx::params{customerId, date, lastReading, currentReading, unitsUsed}
    );

    txn.exec(
        "UPDATE customers SET last_reading = $1 WHERE id = $2",
        pqxx::params{currentReading, customerId}
    );

    txn.commit();
    return unitsUsed;
}

// ============================================================
//  FUNCTION: recordUsage   (THE CLI WRAPPER)
//  Now just gathers input and reports the result/error —
//  all database logic lives in recordUsageLogic().
// ============================================================
void recordUsage() {
    cout << "\n--- Record Water Usage ---\n";

    string customerId;
    cout << "  Customer ID (paste UUID): ";
    cin >> customerId;

    double currentReading;
    cout << "  Current Reading (m³): ";
    cin >> currentReading;

    string date;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cout << "  Date (YYYY-MM-DD): ";
    getline(cin, date);

    try {
        double unitsUsed = recordUsageLogic(customerId, currentReading, date);
        cout << "  ✔ Recorded. Units used: " << unitsUsed << " m³\n";
    } catch (const exception& e) {
        cerr << "  ✘ " << e.what() << "\n";
    }
}

// ============================================================
//  FUNCTION: viewUsageHistory
//
//  BEFORE: looped through c->records (a vector already
//  filtered to one customer because it lived INSIDE that
//  customer's struct).
//
//  NOW: we must explicitly filter with WHERE customer_id = $1
//  — the database doesn't "know" which records belong to whom
//  until we ask it directly.
// ============================================================
void viewUsageHistory() {
    cout << "\n--- Usage History ---\n";

    string customerId;
    cout << "  Customer ID (paste UUID): ";
    cin >> customerId;

    pqxx::work txn(getConnection());

    // First confirm the customer exists and get their name for display
    pqxx::result custResult = txn.exec(
        "SELECT name FROM customers WHERE id = $1",
        pqxx::params{customerId}
    );

    if (custResult.empty()) {
        cout << "  ✘ Customer not found.\n";
        return;
    }

    cout << "\n  Customer: " << custResult[0]["name"].as<string>() << "\n\n";

    // ORDER BY reading_date keeps the history in chronological
    // order, same as your vector naturally preserved insertion order
    pqxx::result records = txn.exec(
        "SELECT reading_date, previous_reading, current_reading, units_used, billed "
        "FROM water_records WHERE customer_id = $1 ORDER BY reading_date",
        pqxx::params{customerId}
    );
    txn.commit();

    if (records.empty()) {
        cout << "  No records yet.\n";
        return;
    }

    cout << "  " << left
         << setw(14) << "Date"
         << setw(12) << "Prev(m³)" << setw(12) << "Curr(m³)"
         << setw(12) << "Used(m³)" << "Billed?\n";
    cout << "  " << string(65, '-') << "\n";

    for (const auto& row : records) {
        cout << "  " << left
             << setw(14) << row["reading_date"].as<string>()
             << setw(12) << row["previous_reading"].as<double>()
             << setw(12) << row["current_reading"].as<double>()
             << setw(12) << row["units_used"].as<double>()
             << (row["billed"].as<bool>() ? "Yes" : "No") << "\n";
    }
    cout << "\n";
}