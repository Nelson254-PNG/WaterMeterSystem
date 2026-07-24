#include "mpesa_stk.h"
#include "constants.h"
#define byte win_byte_override
#include <windows.h>
#undef byte
#include <curl/curl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <stdexcept>
using namespace std;

#include "picojson/picojson.h"

// helper curlwritecallback
static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
  size_t totalSize = size * nmemb;
  output->append((char*)contents, totalSize);
  return totalSize;
}

// helper base64Encode
static string base64Encode(const string& input) {
  BIO* bio  = BIO_new(BIO_f_base64());
  BIO* bmem = BIO_new(BIO_s_mem());
  bio = BIO_push(bio, bmem);
  BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
  BIO_write(bio, input.c_str(), input.length());
  BIO_flush(bio);
 
  BUF_MEM* bufferPtr;
  BIO_get_mem_ptr(bio, &bufferPtr);
 
  string result(bufferPtr->data, bufferPtr->length);
  BIO_free_all(bio);
  return result;
}

//helper httppost
static string httpPost(const string& url, const string& body, const vector<string>& headers) {
    CURL* curl = curl_easy_init();
    if (!curl) throw runtime_error("Failed to initialize libcurl");
 
    string response;
    struct curl_slist* headerList = nullptr;
 
    for (const auto& h : headers) {
        headerList = curl_slist_append(headerList, h.c_str());
    }
 
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
 
    CURLcode res = curl_easy_perform(curl);
 
    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
 
    if (res != CURLE_OK) {
        throw runtime_error(string("libcurl error: ") + curl_easy_strerror(res));
    }
 
    return response;
}

// helper httpGet
static string httpGet(const string& url, const vector<string>& headers) {
    CURL* curl = curl_easy_init();
    if (!curl) throw runtime_error("Failed to initialize libcurl");
 
    string response;
    struct curl_slist* headerList = nullptr;
 
    for (const auto& h : headers) {
        headerList = curl_slist_append(headerList, h.c_str());
    }
 
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
 
    CURLcode res = curl_easy_perform(curl);
 
    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
 
    if (res != CURLE_OK) {
        throw runtime_error(string("libcurl error: ") + curl_easy_strerror(res));
    }
 
    return response;
}

//function format phonenumber
string formatPhoneNumber(const string& phone){
    string p = phone;

    p.erase(remove_if(p.begin(), p.end(), [](char c){
        return c == ' ' || c == '-';
    }), p.end());
    
    if (p.substr(0, 4) == "+254") return p.substr(1);
    if (p.substr(0, 3) == "254") return p;
    if (p.substr(0, 2) == "07") return "254" + p.substr(1);
    if (p.substr(0, 2) == "01") return "254" + p.substr(1);
    return p;
}

// function getcurrent time
string getCurrentTimestamp(){
    auto now = chrono::system_clock::now();
    time_t t = chrono::system_clock::to_time_t(now);
    struct tm* tm_info = localtime(&t);
    ostringstream oss;
    oss << put_time(tm_info, "%Y%m%d%H%M%S");
    return oss.str();
}

//function generateSTKPassword
string generateSTKPassword( const string& timestamp){
    string raw = DARAJA_SHORTCODE + DARAJA_PASSKEY + timestamp;
    return base64Encode(raw);
}

//function getDarajaToken
DarajaToken getDarajaToken(){
    DarajaToken result;
    try{

        string credentials = DARAJA_CONSUMER_KEY + ":" + DARAJA_CONSUMER_SECRET;
        string encoded = base64Encode(credentials);
        string url = DARAJA_BASE_URL + "/oauth/v1/generate?grant_type=client_credentials";
        string response = httpGet(url,{
            "Authorisation: Basic " + encoded, "Content-Type: application/json"
        });
        picojson::value v;
        string err = picojson::parse(v, response);
        if (!err.empty()) throw runtime_error("JSON parse: " + err);
        auto& obj = v.get<picojson::object>();
        if (obj.count("access_token")){
            result.accessToken = obj["access_token"].get<string>();
            result.success = true;
        }else if (obj.count("errorMessage")){
            result.errorMessage = obj["errorMessage"].get<string>();
            result.success = false;
        }else {
            result.errorMessage = "Unknown response: " + response;
            result.success = false;
        }
    }catch (const exception& e){
        result.success = false;
        result.errorMessage = e.what();
    }
    return result;
}

// function initiateSTKPush
STKPushResult initiateSTKPush(const string& phone, double amount, const string& accountRef, const string& description){
    STKPushResult result;
    try {
        DarajaToken tokenResult = getDarajaToken();
        if (!tokenResult.success){
            result.success = false;
            result.errorMessage = "Failed to get Daraja token: " + tokenResult.errorMessage;
            return result;
        }
        string timestamp = getCurrentTimestamp();
        string password = generateSTKPassword(timestamp);
        string formattedPhone = formatPhoneNumber(phone);

        int amountInt = static_cast<int>(ceil(amount));
        ostringstream bodyStream;
        bodyStream
            <<"{"
            <<"\"BusinessShortCode\":\""<< DARAJA_SHORTCODE << "\","
            <<"\"Password\":\""<< password << "\","
            <<"\"Timestamp\":\""<< timestamp << "\","
            <<"\"TransactionType\":\"CustomerPayBillOnline\","
            <<"\"Amount\":"<< amountInt <<","
            <<"\"PartyA\":\""<< formattedPhone << "\","
            <<"\"PartyB\":\""<< DARAJA_SHORTCODE << "\","
            <<"\"PhoneNumber\":\""<< formattedPhone << "\","
            <<"\"CallBackURL\":\""<< DARAJA_CALLBACK_URL << "\","
            <<"\"AccountReference\":\""<< accountRef << "\","
            <<"\"TransactionDesc\":\""<< description << "\","
            <<"}";
        string url = DARAJA_BASE_URL + "/mpesa/stkpush/v1/processrequest";
        string response = httpPost(url, bodyStream.str(),{
            "Authorisation: Bearer " + tokenResult.accessToken,
            "Content-Type: application/json"
        });  
        picojson::value v;
        string err = picojson::parse(v, response);
        if (!err.empty()) throw runtime_error("JSON parse error: " + err);
        auto& obj = v.get<picojson::object>();

        if (obj.count("ResponseCode") && obj["ResponseCode"].get<string>() == "0"){
            result.success = true;
            result.checkoutRequestId = obj["CheckoutRequestID"].get<string>();
            result.merchantRequestId = obj["merchantRequestID"].get<string>();
        }else {
            result.success = false;
            result.errorMessage = obj.count("ResponseDescription") ?obj["ResponseDescription"].get<string>() : "STK Push failed: " + response;
        }
    }catch (const exception& e){
        result.success = false;
        result.errorMessage = e.what();
    }
    return result;
}
// parseSTKCallback function
STKCallbackResult parseSTKCallback(const string& rawBody){
    STKCallbackResult result;
    try{
        picojson::value v;
        string err = picojson::parse(v, rawBody);
        if (!err.empty()) throw runtime_error("JSON parse error: " + err);
         
        auto& body = v.get<picojson::object>()["Body"].get<picojson::object>();
        auto& stkCallback = body["stkCallback"].get<picojson::object>();
        
        result.checkoutRequestId = stkCallback["CheckoutRequestID"].get<string>();
        
        double resultCode = stkCallback["ResultCode"].get<double>();

        if (resultCode == 0){
            result.success = true;
            auto& metadata = stkCallback["CallbackMetadata"].get<picojson::object>();
            auto& items = metadata["Item"].get<picojson::array>();

            for (const auto& item : items){
                auto& itemObj = item.get<picojson::object>();
                string name = itemObj.at("Name").get<string>();

                if (name == "MpesaReceiptNumber"){
                    result.mpesaReceiptNumber = itemObj.at("Value").get<string>();   
                } else if (name == "Amount"){
                    result.amount = itemObj.at("Value").get<double>();
                } else if (name == "PhoneNumber"){
                    result.phoneNumber = to_string(static_cast<long long>(itemObj.at("Value").get<double>()));
                }
            }
        } else{
            result.success = false;
            result.errorMessage = stkCallback.count("ResultDesc") ? stkCallback["ResultDesc"].get<string>() : "STK Callback failed: " + rawBody;
        }
        
    }catch (const exception& e){
        result.success = false;
        result.errorMessage = string("Callback parse error: ")+ e.what();
    }
    return result;
}