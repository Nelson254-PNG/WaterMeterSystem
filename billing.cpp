#include "billing.h"
#include "db.h"
#include "constants.h"
#include <iostream>
#include <iomanip>
using namespace std;

//  calculateTieredCost
TierBreakdown calculateTieredCost(double units) {
    TierBreakdown b{};   
    double rem = units;

    if (rem > 0) { b.tier1Units = min(rem, TIER1_LIMIT); rem -= b.tier1Units; }
    if (rem > 0) { b.tier2Units = min(rem, TIER2_LIMIT - TIER1_LIMIT); rem -= b.tier2Units; }
    if (rem > 0) { b.tier3Units = min(rem, TIER3_LIMIT - TIER2_LIMIT); rem -= b.tier3Units; }
    if (rem > 0) { b.tier4Units = rem; }

    b.tier1Cost     = b.tier1Units * TIER1_RATE;
    b.tier2Cost     = b.tier2Units * TIER2_RATE;
    b.tier3Cost     = b.tier3Units * TIER3_RATE;
    b.tier4Cost     = b.tier4Units * TIER4_RATE;
    b.serviceCharge = SERVICE_CHARGE;
    b.totalAmount   = b.tier1Cost + b.tier2Cost + b.tier3Cost + b.tier4Cost + b.serviceCharge;

    return b;
}

//   generateBillLogic  
GenerateBillResult generateBillLogic(const string& customerId, const string& issueDate, const string& dueDate) {
    pqxx::work txn(getConnection());

    pqxx::result custResult = txn.exec(
        "SELECT name FROM customers WHERE id = $1",
        pqxx::params{customerId}
    );
    if (custResult.empty()) {
        throw runtime_error("Customer not found");
    }

    pqxx::result sumResult = txn.exec(
        "SELECT COALESCE(SUM(units_used), 0) AS total_units, COUNT(*) AS cnt "
        "FROM water_records WHERE customer_id = $1 AND billed = false",
        pqxx::params{customerId}
    );

    double totalUnits = sumResult[0]["total_units"].as<double>();
    int unbilledCount  = sumResult[0]["cnt"].as<int>();

    if (unbilledCount == 0) {
        throw runtime_error("No unbilled usage records for this customer");
    }

    TierBreakdown tb = calculateTieredCost(totalUnits);

    pqxx::result billResult = txn.exec(
        "INSERT INTO bills "
        "(customer_id, issue_date, due_date, total_units, "
        " tier1_units, tier2_units, tier3_units, tier4_units, "
        " tier1_cost, tier2_cost, tier3_cost, tier4_cost, "
        " service_charge, total_amount, amount_paid, paid) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, 0, false) "
        "RETURNING id",
        pqxx::params{
            customerId, issueDate, dueDate, totalUnits,
            tb.tier1Units, tb.tier2Units, tb.tier3Units, tb.tier4Units,
            tb.tier1Cost, tb.tier2Cost, tb.tier3Cost, tb.tier4Cost,
            tb.serviceCharge, tb.totalAmount
        }
    );
    string billId = billResult[0]["id"].as<string>();

    txn.exec(
        "UPDATE water_records SET billed = true "
        "WHERE customer_id = $1 AND billed = false",
        pqxx::params{customerId}
    );

    txn.exec(
        "UPDATE customers SET balance = balance + $1 WHERE id = $2",
        pqxx::params{tb.totalAmount, customerId}
    );

    txn.commit();

    GenerateBillResult result;
    result.billId     = billId;
    result.totalUnits = totalUnits;
    result.breakdown  = tb;
    return result;
}

//  generateBill  
void generateBill() {
    cout << "\n--- Generate Bill ---\n";

    string customerId;
    cout << "  Customer ID (paste UUID): ";
    cin >> customerId;

    string issueDate, dueDate;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cout << "  Issue Date (YYYY-MM-DD): "; getline(cin, issueDate);
    cout << "  Due Date   (YYYY-MM-DD): "; getline(cin, dueDate);

    try {
        GenerateBillResult result = generateBillLogic(customerId, issueDate, dueDate);

        cout << "\n  ✔ Bill generated.\n";
        cout << "  Bill ID     : " << result.billId << "\n";
        cout << "  Total Units : " << result.totalUnits << " m³\n";
        cout << "  Total Due   : KES " << fixed << setprecision(2)
             << result.breakdown.totalAmount << "\n";

    } catch (const exception& e) {
        cerr << "  ✘ " << e.what() << "\n";
    }
}

//  viewBills
void viewBills() {
    cout << "\n--- Bills ---\n";

    string customerId;
    cout << "  Customer ID (paste UUID): ";
    cin >> customerId;

    pqxx::work txn(getConnection());

    pqxx::result custResult = txn.exec(
        "SELECT name, balance FROM customers WHERE id = $1",
        pqxx::params{customerId}
    );
    if (custResult.empty()) {
        cout << "  ✘ Customer not found.\n";
        return;
    }

    cout << "\n  " << custResult[0]["name"].as<string>()
         << "  |  Balance: KES "
         << fixed << setprecision(2) << custResult[0]["balance"].as<double>()
         << "\n\n";

    pqxx::result bills = txn.exec(
        "SELECT id, issue_date, due_date, total_units, total_amount, amount_paid, paid "
        "FROM bills WHERE customer_id = $1 ORDER BY issue_date",
        pqxx::params{customerId}
    );
    txn.commit();

    if (bills.empty()) { cout << "  No bills yet.\n"; return; }

    cout << "  " << left
         << setw(38) << "Bill ID" << setw(14) << "Issued"
         << setw(14) << "Due" << setw(10) << "Units"
         << setw(14) << "Total(KES)" << setw(12) << "Paid(KES)" << "Status\n";
    cout << "  " << string(112, '-') << "\n";

    for (const auto& row : bills) {
        cout << "  " << left
             << setw(38) << row["id"].as<string>()
             << setw(14) << row["issue_date"].as<string>()
             << setw(14) << row["due_date"].as<string>()
             << setw(10) << row["total_units"].as<double>()
             << setw(14) << fixed << setprecision(2) << row["total_amount"].as<double>()
             << setw(12) << row["amount_paid"].as<double>()
             << (row["paid"].as<bool>() ? "PAID" : "UNPAID") << "\n";
    }
    cout << "\n";
}