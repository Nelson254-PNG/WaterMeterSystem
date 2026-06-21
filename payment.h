#pragma once
// ============================================================
//  payment.h
//  Declarations for payment processing functions.
//  PHASE C: worker functions added for API reuse.
// ============================================================

#include <string>
using namespace std;

void makePayment();
void viewPaymentHistory();

bool isValidMpesaCode(const string& code);
void payByMpesaPaybill();

// ── THE WORKERS ────────────────────────────────────────────────
// Generic payment (Cash/Bank/Other). Returns nothing — caller
// already knows the amount and method; nothing new to report
// besides success/failure, which is conveyed via exception.
void makePaymentLogic(const string& customerId, const string& billId,
                      const string& method, const string& reference,
                      double amount, const string& payDate);

// M-Pesa payment. Validates format, relies on the database's
// unique index for duplicate detection (throws pqxx::unique_violation
// if the code was already used).
void payByMpesaLogic(const string& customerId, const string& billId,
                     const string& code, double amount, const string& payDate);