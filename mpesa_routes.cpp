#include "mpesa_stk.h"
#include "mpesa_routes.h"
#include "payment.h"
#include "auth.h"
#include "db.h"
#include <iostream>
#include <cmath>
using namespace std;

void registerMpesaRoutes(crow::SimpleApp& app){
  CROW_ROUTE(app, "/customers/<string>/payments/stk-push").methods(crow::HTTPMethod::Post)([](const crow::request& req, const string& customerId){
    //AUTH CHECK
    try{
      TokenPayload payload = requireAuth(req);
      requireOwnerOrAdmin(payload, customerId);
    }catch (const exception& e){
      crow::json::wvalue err;
      err["error"] = e.what();
      return crow::response(401, err);
    }
    auto body = crow::json::load(req.body);
    if (!body || !body.has("billId") || !body.has("phone") || !body.has("amount")){
      crow::json::wvalue err;
      err["error"] = "Missing required fields: billId, phone, amount";
      return crow::response(400, err);
    }
    string billId = body["billId"].s();
    string phone = body["phone"].s();
    double amount = body["amount"].d();

    if (amount <= 0){
      crow::json::wvalue err;
      err["error"] = "Amount must be greater than 0";
      return crow::response(400, err);
    }

    try{
      pqxx::work txn(getConnection());
      pqxx::result billCheck = txn.exec("SELECT id FROM bills WHERE id = $1 AND customer_id = $2 AND paid = false", pqxx::params{billId, customerId});
      txn.commit();

      if (billCheck.empty()){
        crow::json::wvalue err;
        err["error"] = "Bill not found, aready paid, or does not belong to this customer";
        return crow::response(404, err);
      }
      pqxx::work txn2(getConnection());
      pqxx::result custResult = txn2.exec("SELECT meter_number, name FROM customers WHERE id = $1", pqxx::params{customerId});
      txn2.commit();
      string meterNumber = custResult[0]["meter_number"].as<string>();
      string custName = custResult[0]["name"].as<string>();

      //initiate STK push
      STKPushResult stkResult = initiateSTKPush(phone, amount, meterNumber, "Water Bill Payment");
      if (!stkResult.success){
        crow::json::wvalue err;
        err["error"] = "STK Push failed: " + stkResult.errorMessage;
        return crow::response(502, err);
      }
      pqxx::work txn3(getConnection());
      txn3.exec("INSERT INTO pending_stk_payments "
        "(checkout_request_id, merchant_request_id, customer_id, bill_id, amount, phone) "
        "VALUES ($1, $2, $3, $4, $5, $6)",
        pqxx::params{stkResult.checkoutRequestId, stkResult.merchantRequestId, customerId, billId, amount, formatPhoneNumber(phone)}
      );
      txn3.commit();
      crow::json::wvalue response;
      response["status"] = "pending";
      response["checkoutRequestId"] = stkResult.checkoutRequestId;
      response["message"] = "M-Pesa prompt sent to " + phone + ". Enter your PIN to complete payment.";
      return crow::response(200, response);
    }catch (const exception& e){
      crow::json::wvalue err;
      err["error"] = e.what();
      return crow::response(500, err);
    }
  });

  // get stk status
  CROW_ROUTE(app, "/customers/<string>/payments/stk-status/<string>").methods(crow::HTTPMethod::Get)
    ([](const crow::request& req, const string& customerId, const string& checkoutRequestId){
      try{
        TokenPayload payload = requireAuth(req);
        requireOwnerOrAdmin(payload, customerId);
      }catch (const exception& e){
        crow::json::wvalue err;
        err["error"] = e.what();
        return crow::response(401, err);
      }
      try{
        pqxx::work txn(getConnection());
        pqxx::result result = txn.exec("SELECT status, amount FROM pending_stk_payments WHERE checkout_request_id = $1 AND customer_id = $2", pqxx::params{checkoutRequestId, customerId});
        txn.commit();

        if (result.empty()){
          crow::json::wvalue err;
          err["error"] = "payment request not found";
          return crow::response(404, err);
        }
        crow::json::wvalue response;
        response["status"] = result[0]["status"].as<string>();
        response["amount"] = result[0]["amount"].as<double>();
        return crow::response(200, response);
      } catch (const exception& e){
        crow::json::wvalue err;
        err["error"] = e.what();
        return crow::response(500, err);
      }
    });

    // post stk callback
    CROW_ROUTE(app, "/api/mpesa/callback").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req){
      string rawBody = req.body;
      cout << "[M-Pesa Callback] Received: " << rawBody << endl;

      try{
        STKCallbackResult callbackResult = parseSTKCallback(rawBody);
        pqxx::work txn(getConnection());
        pqxx::result pending = txn.exec("SELECT customer_id, bill_id, amount FROM pending_stk_payments WHERE checkout_request_id = $1 AND status = 'pending '", pqxx::params{callbackResult.checkoutRequestId});
        if (pending.empty()){
          cout << "[M-Pesa Callback] No pending record for: " << callbackResult.checkoutRequestId << "\n";
          txn.commit();
          crow::json::wvalue response;
          response["ResultCode"] = 0;
          response["ResultDesc"] = "Accepted";
          return crow::response(200, response);
        }
        string customerId = pending[0]["customer_id"].as<string>();
        string billId     = pending[0]["bill_id"].as<string>();
        double amount     = pending[0]["amount"].as<double>();

        if (callbackResult.success){
          pqxx::result custResult = txn.exec("SELECT balance FROM customers WHERE id = $1", pqxx::params{customerId});
          double balanceBefore = custResult[0]["balance"].as<double>();
          double balanceAfter  = balanceBefore - amount;
          txn.exec("INSERT INTO payments (customer_id, bill_id, payment_date, method, reference, amount_paid, balance_before, balance_after) VALUES($1, $2, CURRENT_DATE, 'M-Pesa', $3, $4, $5, $6)", 
            pqxx::params{customerId, billId, callbackResult.mpesaReceiptNumber, amount, balanceBefore, balanceAfter});
          
          txn.exec("UPDATE pending_stk_payments SET status = 'completed', completed_at = now() WHERE checkout_request_id = $1",
            pqxx::params{callbackResult.checkoutRequestId});
            
          txn.commit();
          cout << "[M-Pesa Callback] Payment completed: " << callbackResult.mpesaReceiptNumber << " KES: " << amount << "\n";  
        }else{
          txn.exec("UPDATE pending_stk_payments SET status = 'failed' WHERE checkout_request_id = $1",
            pqxx::params{callbackResult.checkoutRequestId});
          txn.commit();
          cout <<"[M-Pesa Callback] payments failed : " << callbackResult.errorMessage << "\n";  
        }
      }catch (const exception& e){
        cerr << "[M-Pesa Callback] Error: " << e.what() << "\n";
      }
      crow::json::wvalue response;
      response["ResultCode"] = 0;
      response["ResultDesc"] = "Accepted";
      return crow::response(200, response);
    });
}
