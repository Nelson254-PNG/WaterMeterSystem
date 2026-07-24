#pragma once
#include <string>

using namespace std;

struct DarajaToken{
  string accessToken;
  bool success;
  string errorMessage;
};

// intaitiate STK push result
struct STKPushResult{
  bool success;
  string checkoutRequestId;
  string merchantRequestId;
  string errorMessage;
};

//intaitiate the callback function
struct STKCallbackResult{
  bool success;
  string mpesaReceiptNumber;
  double amount;
  string phoneNumber;
  string checkoutRequestId;
  string errorMessage;
};

DarajaToken getDarajaToken();

STKPushResult initiateSTKPush(const string& phoneNumber, double amount, const string& accountRef, const string& description);

STKCallbackResult parseSTKCallback(const string& rawBody);

string formatPhoneNumber(const string& phone);

string generateSTKPassword( const string& timestamp);

string getCurrentTimestamp();