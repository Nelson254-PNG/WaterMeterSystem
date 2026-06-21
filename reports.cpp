// ============================================================
//  reports.cpp  (DATABASE VERSION)
//  System-wide dashboard and per-customer account statement.
//
//  THE BIG SHIFT: your CLI version walked THREE NESTED LOOPS
//  (every customer -> every bill -> every payment) to compute
//  totals. Here, we ask Postgres to do that aggregation in a
//  handful of SQL statements — the database engine is built
//  specifically to do this kind of work efficiently, even
//  across millions of rows.
// ============================================================

#include "reports.h"
#include "db.h"
#include <iostream>
#include <iomanip>
using namespace std;

static void sep(int w = 60, char ch = '=') {
    cout << "  " << string(w, ch) << "\n";
}

// ============================================================
//  FUNCTION: systemDashboard
//
//  Each block below is ONE query replacing what used to be
//  a section of your nested loop. We run them as separate,
//  independent SELECTs (not all wrapped in one transaction)
//  because these are pure reads — nothing here needs to be
//  atomic with anything else.
// ============================================================
void systemDashboard() {
    cout << "\n";
    sep(); cout << "  ||     SYSTEM DASHBOARD                        ||\n"; sep();

    pqxx::work txn(getConnection());

    // ── Customers ──────────────────────────────────────────
    pqxx::result custCount = txn.exec("SELECT COUNT(*) FROM customers");
    int totalCustomers = custCount[0][0].as<int>();

    if (totalCustomers == 0) {
        cout << "  No data yet.\n";
        txn.commit();
        return;
    }

    // ── Water usage ────────────────────────────────────────
    // COALESCE handles the case where there are zero rows,
    // which would otherwise make SUM() return NULL
    pqxx::result usage = txn.exec(
        "SELECT COUNT(*) AS cnt, COALESCE(SUM(units_used), 0) AS total_units "
        "FROM water_records"
    );
    int    totalReadings  = usage[0]["cnt"].as<int>();
    double totalUnitsUsed = usage[0]["total_units"].as<double>();

    // ── Billing ────────────────────────────────────────────
    // FILTER lets us count conditionally within ONE query —
    // this replaces your C++ "b.paid ? totalPaidBills++ : ..."
    pqxx::result billing = txn.exec(
        "SELECT COUNT(*) AS total_bills, "
        "       COUNT(*) FILTER (WHERE paid = true)  AS paid_bills, "
        "       COUNT(*) FILTER (WHERE paid = false) AS unpaid_bills, "
        "       COALESCE(SUM(total_amount), 0) AS total_billed "
        "FROM bills"
    );
    int    totalBills       = billing[0]["total_bills"].as<int>();
    int    totalPaidBills   = billing[0]["paid_bills"].as<int>();
    int    totalUnpaidBills = billing[0]["unpaid_bills"].as<int>();
    double totalBilled      = billing[0]["total_billed"].as<double>();

    // ── Payments ───────────────────────────────────────────
    pqxx::result paymentsAgg = txn.exec(
        "SELECT COUNT(*) AS cnt, COALESCE(SUM(amount_paid), 0) AS total_collected "
        "FROM payments"
    );
    int    totalPayments  = paymentsAgg[0]["cnt"].as<int>();
    double totalCollected = paymentsAgg[0]["total_collected"].as<double>();

    // ── Balances ───────────────────────────────────────────
    // FILTER again — sum only the positive balances (owing)
    // separately from the negative ones (credit)
    pqxx::result balances = txn.exec(
        "SELECT "
        "  COALESCE(SUM(balance) FILTER (WHERE balance > 0), 0) AS outstanding, "
        "  COALESCE(SUM(ABS(balance)) FILTER (WHERE balance < 0), 0) AS credit "
        "FROM customers"
    );
    double totalOutstanding = balances[0]["outstanding"].as<double>();
    double totalCredit      = balances[0]["credit"].as<double>();

    txn.commit();

    // ── Print — identical formatting to your CLI version ────
    cout << fixed << setprecision(2);
    cout << "\n  CUSTOMERS\n";       sep(60, '-');
    cout << "  Total Registered    : " << totalCustomers << "\n";
    cout << "\n  WATER USAGE\n";     sep(60, '-');
    cout << "  Total Readings      : " << totalReadings  << "\n";
    cout << "  Total Units Used    : " << totalUnitsUsed << " m³\n";
    cout << "\n  BILLING\n";         sep(60, '-');
    cout << "  Bills Generated     : " << totalBills       << "\n";
    cout << "  Bills Paid          : " << totalPaidBills   << "\n";
    cout << "  Bills Unpaid        : " << totalUnpaidBills << "\n";
    cout << "  Total Billed  (KES) : " << totalBilled      << "\n";
    cout << "\n  PAYMENTS\n";        sep(60, '-');
    cout << "  Total Transactions  : " << totalPayments   << "\n";
    cout << "  Total Collected(KES): " << totalCollected  << "\n";
    cout << "  Outstanding   (KES) : " << totalOutstanding << "\n";
    cout << "  Credit        (KES) : " << totalCredit     << "\n";

    if (totalBilled > 0)
        cout << "\n  Collection Rate     : "
             << (totalCollected / totalBilled) * 100.0 << "%\n";

    sep(); cout << "\n";
}

// ============================================================
//  FUNCTION: accountStatement
//
//  Three independent SELECTs, each pre-filtered to one
//  customer_id. No struct traversal — every section pulls
//  exactly what it needs directly from its own table.
// ============================================================
void accountStatement() {
    cout << "\n--- Account Statement ---\n";

    string customerId;
    cout << "  Customer ID (paste UUID): ";
    cin >> customerId;

    pqxx::work txn(getConnection());

    pqxx::result cust = txn.exec(
        "SELECT name, meter_number, phone, last_reading, balance "
        "FROM customers WHERE id = $1",
        pqxx::params{customerId}
    );
    if (cust.empty()) { cout << "  ✘ Customer not found.\n"; return; }

    cout << fixed << setprecision(2) << "\n";
    sep(64); cout << "  ||        ACCOUNT STATEMENT                         ||\n"; sep(64);

    double balance = cust[0]["balance"].as<double>();
    cout << "  Name         : " << cust[0]["name"].as<string>()        << "\n"
         << "  Meter Number : " << cust[0]["meter_number"].as<string>() << "\n"
         << "  Phone        : " << cust[0]["phone"].as<string>()       << "\n"
         << "  Last Reading : " << cust[0]["last_reading"].as<double>() << " m³\n"
         << "  Balance      : KES " << balance
         << (balance < 0 ? "  (CREDIT)" : balance == 0 ? "  (CLEAR)" : "  (OWING)")
         << "\n";

    // ── Usage records ────────────────────────────────────────
    pqxx::result records = txn.exec(
        "SELECT reading_date, previous_reading, current_reading, units_used, billed "
        "FROM water_records WHERE customer_id = $1 ORDER BY reading_date",
        pqxx::params{customerId}
    );

    cout << "\n"; sep(64, '-');
    cout << "  USAGE RECORDS (" << records.size() << ")\n"; sep(64, '-');
    if (records.empty()) { cout << "  None.\n"; }
    else {
        cout << "  " << left << setw(14) << "Date"
             << setw(12) << "Prev(m³)" << setw(12) << "Curr(m³)"
             << setw(12) << "Used(m³)" << "Billed\n";
        double totalUsedSum = 0;
        for (const auto& row : records) {
            cout << "  " << left << setw(14) << row["reading_date"].as<string>()
                 << setw(12) << row["previous_reading"].as<double>()
                 << setw(12) << row["current_reading"].as<double>()
                 << setw(12) << row["units_used"].as<double>()
                 << (row["billed"].as<bool>() ? "Yes" : "No") << "\n";
            totalUsedSum += row["units_used"].as<double>();
        }
        cout << "  Total Usage: " << totalUsedSum << " m³\n";
    }

    // ── Bills ──────────────────────────────────────────────
    pqxx::result bills = txn.exec(
        "SELECT issue_date, due_date, total_units, total_amount, amount_paid, paid "
        "FROM bills WHERE customer_id = $1 ORDER BY issue_date",
        pqxx::params{customerId}
    );

    cout << "\n"; sep(64, '-');
    cout << "  BILLS (" << bills.size() << ")\n"; sep(64, '-');
    if (bills.empty()) { cout << "  None.\n"; }
    else {
        cout << "  " << left << setw(14) << "Issued" << setw(14) << "Due"
             << setw(10) << "Units" << setw(12) << "Total" << setw(12) << "Paid" << "Status\n";
        double tb = 0, tp = 0;
        for (const auto& row : bills) {
            double total = row["total_amount"].as<double>();
            double paid  = row["amount_paid"].as<double>();
            cout << "  " << left << setw(14) << row["issue_date"].as<string>()
                 << setw(14) << row["due_date"].as<string>()
                 << setw(10) << row["total_units"].as<double>()
                 << setw(12) << total << setw(12) << paid
                 << (row["paid"].as<bool>() ? "PAID" : "UNPAID") << "\n";
            tb += total; tp += paid;
        }
        cout << "  Billed: KES " << tb << "  |  Paid: KES " << tp << "\n";
    }

    // ── Payments ───────────────────────────────────────────
    pqxx::result payments = txn.exec(
        "SELECT payment_date, method, amount_paid, balance_after "
        "FROM payments WHERE customer_id = $1 ORDER BY payment_date",
        pqxx::params{customerId}
    );
    txn.commit();

    cout << "\n"; sep(64, '-');
    cout << "  PAYMENTS (" << payments.size() << ")\n"; sep(64, '-');
    if (payments.empty()) { cout << "  None.\n"; }
    else {
        cout << "  " << left << setw(14) << "Date" << setw(14) << "Method"
             << setw(12) << "Amount" << "Bal After\n";
        double totalPaid = 0;
        for (const auto& row : payments) {
            double amt = row["amount_paid"].as<double>();
            cout << "  " << left << setw(14) << row["payment_date"].as<string>()
                 << setw(14) << row["method"].as<string>()
                 << setw(12) << amt << row["balance_after"].as<double>() << "\n";
            totalPaid += amt;
        }
        cout << "  Total Paid: KES " << totalPaid << "\n";
    }

    // ── Summary ────────────────────────────────────────────
    cout << "\n"; sep(64);
    cout << "  Current Balance: KES " << balance
         << (balance < 0 ? "  ★ CREDIT" : balance == 0 ? "  ✔ CLEAR" : "  ✘ OWING")
         << "\n";
    sep(64); cout << "\n";
}