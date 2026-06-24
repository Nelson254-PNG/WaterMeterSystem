// ============================================================
//  payment.cpp  (DATABASE VERSION)
//  Processing payments — both generic (Cash/Bank) and the
//  M-Pesa Paybill manual reconciliation flow.
// ============================================================

#include "payment.h"
#include "db.h"
#include "constants.h"
#include <iostream>
#include <iomanip>
#include <cctype>
using namespace std;

// ============================================================
//  FUNCTION: isValidMpesaCode  (UNCHANGED)
//  Pure string validation — no database involved, so this
//  function is identical to your CLI version.
// ============================================================
bool isValidMpesaCode(const string& code) {
    if (code.length() != 10) return false;
    for (char ch : code) {
        if (!isupper(ch) && !isdigit(ch)) return false;
    }
    return true;
}

// ============================================================
//  HELPER: applyPaymentToBill
//
//  Shared logic used by BOTH makePayment() and payByMpesaPaybill().
//  Rather than duplicating these database statements in two
//  places, we centralize them here.
//
//  Takes an OPEN transaction (txn) so the caller controls
//  when to commit — this lets each calling function decide
//  its own error handling and messaging around the same core
//  database operations.
// ============================================================
void applyPaymentToBill(pqxx::work& txn, const string& billId,
                         const string& customerId, double amount) {
    // Update the bill: add to amount_paid, and flip paid=true
    // if the new total meets or exceeds the bill's total_amount.
    // We do this comparison INSIDE SQL using a CASE expression,
    // so Postgres makes the decision using its own fresh data —
    // no risk of working with a stale value we read earlier.
    txn.exec(
        "UPDATE bills SET "
        "  amount_paid = amount_paid + $1, "
        "  paid = (amount_paid + $1 >= total_amount - 0.01) "
        "WHERE id = $2",
        pqxx::params{amount, billId}
    );

    // Update the customer's balance
    txn.exec(
        "UPDATE customers SET balance = balance - $1 WHERE id = $2",
        pqxx::params{amount, customerId}
    );
}

// ============================================================
//  FUNCTION: makePaymentLogic   (THE WORKER)
//
//  Pure database logic for a generic payment. Looks up the
//  customer's current balance itself (rather than trusting a
//  value the caller might have read earlier and gone stale),
//  inserts the payment record, then applies it to the bill.
// ============================================================
void makePaymentLogic(const string& customerId, const string& billId,
                      const string& method, const string& reference,
                      double amount, const string& payDate) {
    if (amount <= 0) {
        throw runtime_error("Amount must be greater than zero");
    }

    pqxx::work txn(getConnection());

    pqxx::result custResult = txn.exec(
        "SELECT balance FROM customers WHERE id = $1",
        pqxx::params{customerId}
    );
    if (custResult.empty()) {
        throw runtime_error("Customer not found");
    }
    double balanceBefore = custResult[0]["balance"].as<double>();

    // Confirm the bill exists, belongs to this customer, and is unpaid
    pqxx::result billCheck = txn.exec(
        "SELECT id FROM bills WHERE id = $1 AND customer_id = $2 AND paid = false",
        pqxx::params{billId, customerId}
    );
    if (billCheck.empty()) {
        throw runtime_error("Bill not found, already paid, or doesn't belong to this customer");
    }

    // Compute balanceAfter in C++, not in SQL. Postgres can't
    // infer the type of "$7 - $6" before it knows what $6/$7
    // actually are — both are just untyped placeholders at
    // that point, so the subtraction operator is ambiguous
    // and Postgres throws "operator is not unique." Computing
    // it here and passing it as its own parameter sidesteps
    // the issue entirely, and is simpler to read besides.
    double balanceAfter = balanceBefore - amount;

    txn.exec(
        "INSERT INTO payments "
        "(customer_id, bill_id, payment_date, method, reference, "
        " amount_paid, balance_before, balance_after) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8)",
        pqxx::params{customerId, billId, payDate, method, reference, amount, balanceBefore, balanceAfter}
    );

    applyPaymentToBill(txn, billId, customerId, amount);

    txn.commit();
}

// ============================================================
//  FUNCTION: makePayment   (THE CLI WRAPPER)
// ============================================================
void makePayment() {
    cout << "\n--- Make Payment ---\n";

    string customerId;
    cout << "  Customer ID (paste UUID): ";
    cin >> customerId;

    try {
        // Show unpaid bills first — this part stays interactive,
        // since it's purely a CLI convenience, not core logic.
        pqxx::work txn(getConnection());
        pqxx::result custResult = txn.exec(
            "SELECT name, balance FROM customers WHERE id = $1",
            pqxx::params{customerId}
        );
        if (custResult.empty()) { cout << "  ✘ Customer not found.\n"; return; }

        cout << "\n  " << custResult[0]["name"].as<string>()
             << "  |  Balance: KES "
             << fixed << setprecision(2) << custResult[0]["balance"].as<double>()
             << "\n\n  Unpaid Bills:\n";

        pqxx::result unpaidBills = txn.exec(
            "SELECT id, issue_date, total_amount, amount_paid "
            "FROM bills WHERE customer_id = $1 AND paid = false ORDER BY issue_date",
            pqxx::params{customerId}
        );
        txn.commit();   // read-only so far, safe to commit before asking more questions

        if (unpaidBills.empty()) { cout << "  ✔ No unpaid bills!\n"; return; }

        cout << "  " << left
             << setw(38) << "Bill ID" << setw(14) << "Issued"
             << setw(14) << "Total(KES)" << setw(14) << "Paid(KES)" << "Remaining\n";
        cout << "  " << string(110, '-') << "\n";
        for (const auto& row : unpaidBills) {
            double total = row["total_amount"].as<double>();
            double paid  = row["amount_paid"].as<double>();
            cout << "  " << left
                 << setw(38) << row["id"].as<string>()
                 << setw(14) << row["issue_date"].as<string>()
                 << setw(14) << fixed << setprecision(2) << total
                 << setw(14) << paid
                 << (total - paid) << "\n";
        }

        string billId;
        cout << "\n  Enter Bill ID to pay: ";
        cin >> billId;

        double amount;
        cout << "  Amount to Pay (KES): ";
        cin >> amount;

        cout << "  Method: 1.Cash  2.M-Pesa  3.Bank Transfer  Choose: ";
        int mc; cin >> mc;
        string method = (mc == 1 ? "Cash" : mc == 2 ? "M-Pesa" : "Bank Transfer");

        string ref;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        if (method != "Cash") { cout << "  Reference: "; getline(cin, ref); }
        else ref = "N/A";

        string payDate;
        cout << "  Date (YYYY-MM-DD): ";
        getline(cin, payDate);

        makePaymentLogic(customerId, billId, method, ref, amount, payDate);

        cout << "\n  ✔ Payment of KES " << fixed << setprecision(2) << amount << " recorded.\n";

    } catch (const exception& e) {
        cerr << "  ✘ Payment failed: " << e.what() << "\n";
    }
}

// ============================================================
//  FUNCTION: payByMpesaLogic   (THE WORKER)
//
//  Validates the code format, then attempts the insert.
//  Deliberately does NOT catch pqxx::unique_violation here —
//  we let it propagate up to the caller (CLI or API), since
//  each one wants to report a duplicate code differently
//  (CLI prints a message; API returns HTTP 409 Conflict).
// ============================================================
void payByMpesaLogic(const string& customerId, const string& billId,
                     const string& code, double amount, const string& payDate) {
    if (!isValidMpesaCode(code)) {
        throw runtime_error("Invalid M-Pesa code format. Must be 10 letters/digits.");
    }
    if (amount <= 0) {
        throw runtime_error("Amount must be greater than zero");
    }

    pqxx::work txn(getConnection());

    pqxx::result custResult = txn.exec(
        "SELECT balance FROM customers WHERE id = $1",
        pqxx::params{customerId}
    );
    if (custResult.empty()) {
        throw runtime_error("Customer not found");
    }
    double balanceBefore = custResult[0]["balance"].as<double>();

    pqxx::result billCheck = txn.exec(
        "SELECT id FROM bills WHERE id = $1 AND customer_id = $2 AND paid = false",
        pqxx::params{billId, customerId}
    );
    if (billCheck.empty()) {
        throw runtime_error("Bill not found, already paid, or doesn't belong to this customer");
    }

    // Same fix as makePaymentLogic — compute in C++, not SQL.
    double balanceAfter = balanceBefore - amount;

    // THE INSERT THAT MIGHT THROW pqxx::unique_violation if
    // this code was already used by anyone, ever.
    txn.exec(
        "INSERT INTO payments "
        "(customer_id, bill_id, payment_date, method, reference, "
        " amount_paid, balance_before, balance_after) "
        "VALUES ($1, $2, $3, 'M-Pesa', $4, $5, $6, $7)",
        pqxx::params{customerId, billId, payDate, code, amount, balanceBefore, balanceAfter}
    );

    applyPaymentToBill(txn, billId, customerId, amount);

    txn.commit();
}

// ============================================================
//  FUNCTION: payByMpesaPaybill   (THE CLI WRAPPER)
// ============================================================
void payByMpesaPaybill() {
    cout << "\n--- Pay via M-Pesa Paybill ---\n";

    string customerId;
    cout << "  Customer ID (paste UUID): ";
    cin >> customerId;

    try {
        pqxx::work txn(getConnection());

        pqxx::result custResult = txn.exec(
            "SELECT name, balance, meter_number FROM customers WHERE id = $1",
            pqxx::params{customerId}
        );
        if (custResult.empty()) { cout << "  ✘ Customer not found.\n"; return; }

        string custName    = custResult[0]["name"].as<string>();
        string meterNumber = custResult[0]["meter_number"].as<string>();

        cout << "\n  " << custName << "  |  Balance: KES "
             << fixed << setprecision(2) << custResult[0]["balance"].as<double>()
             << "\n\n  Unpaid Bills:\n";

        pqxx::result unpaidBills = txn.exec(
            "SELECT id, issue_date, total_amount, amount_paid "
            "FROM bills WHERE customer_id = $1 AND paid = false ORDER BY issue_date",
            pqxx::params{customerId}
        );
        txn.commit();   // read-only display, safe to commit before more prompts

        if (unpaidBills.empty()) { cout << "  ✔ No unpaid bills!\n"; return; }

        cout << "  " << left
             << setw(38) << "Bill ID" << setw(14) << "Issued"
             << setw(14) << "Total(KES)" << "Remaining\n";
        cout << "  " << string(98, '-') << "\n";
        for (const auto& row : unpaidBills) {
            double total = row["total_amount"].as<double>();
            double paid  = row["amount_paid"].as<double>();
            cout << "  " << left
                 << setw(38) << row["id"].as<string>()
                 << setw(14) << row["issue_date"].as<string>()
                 << setw(14) << fixed << setprecision(2) << total
                 << (total - paid) << "\n";
        }

        string billId;
        cout << "\n  Enter Bill ID to pay: ";
        cin >> billId;

        cout << "\n";
        cout << " PAY VIA M-PESA — FOLLOW THESE STEPS\n";
        cout << "\n";
        cout << " Business No. : " << MPESA_PAYBILL_NUMBER << "\n";
        cout << " Account No.  : " << meterNumber << "\n";
        cout << "\n";

        string code;
        bool gotValidFormat = false;
        while (!gotValidFormat) {
            cout << "\n  M-Pesa Transaction Code (or 0 to cancel): ";
            cin >> code;
            if (code == "0") { cout << "  Cancelled.\n"; return; }
            for (auto& ch : code) ch = toupper(ch);
            if (!isValidMpesaCode(code)) {
                cout << "  ✘ Invalid format. Must be 10 letters/digits (e.g. QGR7XYZ123).\n";
                continue;
            }
            gotValidFormat = true;
        }

        double amount;
        cout << "  Confirm Amount Paid (KES): ";
        cin >> amount;

        string payDate;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "  Date (YYYY-MM-DD): ";
        getline(cin, payDate);

        payByMpesaLogic(customerId, billId, code, amount, payDate);

        cout << "\n  ✔ M-Pesa payment recorded — Code: " << code << "\n";
        cout << "  Amount: KES " << fixed << setprecision(2) << amount << "\n";

    } catch (const pqxx::unique_violation& e) {
        cerr << "  ✘ This M-Pesa code has already been used. Please check and re-enter.\n";
    } catch (const exception& e) {
        cerr << "  ✘ " << e.what() << "\n";
    }
}

// ============================================================
//  FUNCTION: payByTillLogic   (THE WORKER)
//
//  Nearly identical to payByMpesaLogic — same validation, same
//  duplicate-code protection via the database's unique index.
//  The ONLY real difference is conceptual: a Till payment has
//  no separate account reference, since the customer just
//  enters the Till Number itself on their phone. We still
//  store the transaction code the same way (method = 'M-Pesa'),
//  since from the system's point of view it's still an M-Pesa
//  payment that needs the same duplicate-code protection —
//  the unique_mpesa_reference index in your schema doesn't
//  care WHICH M-Pesa flow produced the code.
// ============================================================
void payByTillLogic(const string& customerId, const string& billId,
                     const string& code, double amount, const string& payDate) {
    if (!isValidMpesaCode(code)) {
        throw runtime_error("Invalid M-Pesa code format. Must be 10 letters/digits.");
    }
    if (amount <= 0) {
        throw runtime_error("Amount must be greater than zero");
    }

    pqxx::work txn(getConnection());

    pqxx::result custResult = txn.exec(
        "SELECT balance FROM customers WHERE id = $1",
        pqxx::params{customerId}
    );
    if (custResult.empty()) {
        throw runtime_error("Customer not found");
    }
    double balanceBefore = custResult[0]["balance"].as<double>();

    pqxx::result billCheck = txn.exec(
        "SELECT id FROM bills WHERE id = $1 AND customer_id = $2 AND paid = false",
        pqxx::params{billId, customerId}
    );
    if (billCheck.empty()) {
        throw runtime_error("Bill not found, already paid, or doesn't belong to this customer");
    }

    double balanceAfter = balanceBefore - amount;

    // Same protection as Paybill — the unique_mpesa_reference
    // index throws pqxx::unique_violation on a duplicate code,
    // regardless of which M-Pesa flow it came from.
    txn.exec(
        "INSERT INTO payments "
        "(customer_id, bill_id, payment_date, method, reference, "
        " amount_paid, balance_before, balance_after) "
        "VALUES ($1, $2, $3, 'M-Pesa', $4, $5, $6, $7)",
        pqxx::params{customerId, billId, payDate, code, amount, balanceBefore, balanceAfter}
    );

    applyPaymentToBill(txn, billId, customerId, amount);

    txn.commit();
}

// ============================================================
//  FUNCTION: payByMpesaTill   (THE CLI WRAPPER)
//
//  Notice the instructions panel only shows the Till Number —
//  no "Account No." line, since Till payments don't have one.
// ============================================================
void payByMpesaTill() {
    cout << "\n--- Pay via M-Pesa Till Number ---\n";

    string customerId;
    cout << "  Customer ID (paste UUID): ";
    cin >> customerId;

    try {
        pqxx::work txn(getConnection());

        pqxx::result custResult = txn.exec(
            "SELECT name, balance FROM customers WHERE id = $1",
            pqxx::params{customerId}
        );
        if (custResult.empty()) { cout << "  ✘ Customer not found.\n"; return; }

        string custName = custResult[0]["name"].as<string>();

        cout << "\n  " << custName << "  |  Balance: KES "
             << fixed << setprecision(2) << custResult[0]["balance"].as<double>()
             << "\n\n  Unpaid Bills:\n";

        pqxx::result unpaidBills = txn.exec(
            "SELECT id, issue_date, total_amount, amount_paid "
            "FROM bills WHERE customer_id = $1 AND paid = false ORDER BY issue_date",
            pqxx::params{customerId}
        );
        txn.commit();

        if (unpaidBills.empty()) { cout << "  ✔ No unpaid bills!\n"; return; }

        cout << "  " << left
             << setw(38) << "Bill ID" << setw(14) << "Issued"
             << setw(14) << "Total(KES)" << "Remaining\n";
        cout << "  " << string(98, '-') << "\n";
        for (const auto& row : unpaidBills) {
            double total = row["total_amount"].as<double>();
            double paid  = row["amount_paid"].as<double>();
            cout << "  " << left
                 << setw(38) << row["id"].as<string>()
                 << setw(14) << row["issue_date"].as<string>()
                 << setw(14) << fixed << setprecision(2) << total
                 << (total - paid) << "\n";
        }

        string billId;
        cout << "\n  Enter Bill ID to pay: ";
        cin >> billId;

        cout << "\n";
        cout << " \n";
        cout << " PAY VIA M-PESA TILL — BUY GOODS    \n";
        cout << " \n";
        cout << "  Go to M-Pesa > Lipa na M-Pesa > Buy Goods\n";
        cout << "  Till Number  : " << MPESA_TILL_NUMBER << "\n";
        cout << " \n";

        string code;
        bool gotValidFormat = false;
        while (!gotValidFormat) {
            cout << "\n  M-Pesa Transaction Code (or 0 to cancel): ";
            cin >> code;
            if (code == "0") { cout << "  Cancelled.\n"; return; }
            for (auto& ch : code) ch = toupper(ch);
            if (!isValidMpesaCode(code)) {
                cout << "  ✘ Invalid format. Must be 10 letters/digits (e.g. QGR7XYZ123).\n";
                continue;
            }
            gotValidFormat = true;
        }

        double amount;
        cout << "  Confirm Amount Paid (KES): ";
        cin >> amount;

        string payDate;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "  Date (YYYY-MM-DD): ";
        getline(cin, payDate);

        payByTillLogic(customerId, billId, code, amount, payDate);

        cout << "\n  ✔ M-Pesa Till payment recorded — Code: " << code << "\n";
        cout << "  Amount: KES " << fixed << setprecision(2) << amount << "\n";

    } catch (const pqxx::unique_violation& e) {
        cerr << "  ✘ This M-Pesa code has already been used. Please check and re-enter.\n";
    } catch (const exception& e) {
        cerr << "  ✘ " << e.what() << "\n";
    }
}

void viewPaymentHistory() {
    cout << "\n--- Payment History ---\n";

    string customerId;
    cout << "  Customer ID (paste UUID): ";
    cin >> customerId;

    pqxx::work txn(getConnection());

    pqxx::result custResult = txn.exec(
        "SELECT name, balance FROM customers WHERE id = $1",
        pqxx::params{customerId}
    );
    if (custResult.empty()) { cout << "  ✘ Customer not found.\n"; return; }

    cout << "\n  " << custResult[0]["name"].as<string>()
         << "  |  Balance: KES "
         << fixed << setprecision(2) << custResult[0]["balance"].as<double>() << "\n";

    pqxx::result payments = txn.exec(
        "SELECT payment_date, method, reference, amount_paid, balance_after "
        "FROM payments WHERE customer_id = $1 ORDER BY payment_date",
        pqxx::params{customerId}
    );
    txn.commit();

    if (payments.empty()) { cout << "  No payments yet.\n"; return; }

    cout << "\n  " << left
         << setw(14) << "Date" << setw(16) << "Method"
         << setw(20) << "Reference" << setw(12) << "Amount" << "Bal After\n";
    cout << "  " << string(75, '-') << "\n";

    double total = 0;
    for (const auto& row : payments) {
        double amt = row["amount_paid"].as<double>();
        cout << "  " << left
             << setw(14) << row["payment_date"].as<string>()
             << setw(16) << row["method"].as<string>()
             << setw(20) << row["reference"].as<string>()
             << setw(12) << fixed << setprecision(2) << amt
             << row["balance_after"].as<double>() << "\n";
        total += amt;
    }
    cout << "  " << string(75, '-') << "\n";
    cout << "  Total Paid: KES " << total << "\n\n";
}