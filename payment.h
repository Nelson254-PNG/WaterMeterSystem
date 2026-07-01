#pragma once
#include <string>
using namespace std;

void makePayment();
void viewPaymentHistory();

bool isValidMpesaCode(const string& code);
void payByMpesaPaybill();

void payByMpesaTill();

void makePaymentLogic(const string& customerId, const string& billId,
  const string& method, const string& reference,
  double amount, const string& payDate);

void payByMpesaLogic(const string& customerId, const string& billId,
  const string& code, double amount, const string& payDate);

void payByTillLogic(const string& customerId, const string& billId,
  const string& code, double amount, const string& payDate);                     