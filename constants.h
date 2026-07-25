#pragma once

#include <string>

const double SERVICE_CHARGE = 200.0;
const double TIER1_LIMIT = 6.0;
const double TIER2_LIMIT = 20.0;
const double TIER3_LIMIT = 50.0;
const double TIER1_RATE = 50.0;
const double TIER2_RATE = 75.0;
const double TIER3_RATE = 100.0;
const double TIER4_RATE = 150.0;

const std::string MPESA_PAYBILL_NUMBER = "000000";

const std::string MPESA_TILL_NUMBER = "9461523";

const std::string DARAJA_CONSUMER_KEY = "YOUR_CONSUMER_KEY";
const std::string DARAJA_CONSUMER_SECRET = "YOUR_CONSUMER_SECRET";
const std::string DARAJA_SHORTCODE = "YOUR_SHORTCODE"; // use paybill for the production.
const std::string DARAJA_PASSKEY = "YOUR_PASSKEY";
const std::string DARAJA_CALLBACK_URL = "https://unsidereal-justine-ovational.ngrok-free.dev/api/payment/callback";

// this the sandbox url for testing
const std::string DARAJA_BASE_URL = "https://sandbox.safaricom.co.ke";
// on production use this url https://api.safaricom.co.ke
