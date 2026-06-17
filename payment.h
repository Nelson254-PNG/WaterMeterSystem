#pragma once
#include <string>
using namespace std;

void makePayment();
void viewPaymentHistory();

bool isValidMpesaCode(const string& code);

bool isCodeAlreadyUsed(const string& code);

void payByMpesaPaybill();

