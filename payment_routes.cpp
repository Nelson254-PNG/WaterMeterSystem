
//  HTTP handlers for payment endpoints — both generic
//  (Cash/Bank) and M-Pesa Paybill.
#include "payment_routes.h"
#include "payment.h"
#include "db.h"
#include <iostream>
using namespace std;

void registerPaymentRoutes(crow::SimpleApp& app) {

    
    //  POST /customers/<id>/payments
    //  Body: {
    //    "billId": "...", "method": "Cash", "reference": "N/A",
    //    "amount": 500, "date": "2026-06-21"
    //  }
    //  Generic payment — Cash, Bank Transfer, or Other.
    CROW_ROUTE(app, "/customers/<string>/payments").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req, const string& customerId) {

        auto body = crow::json::load(req.body);
        if (!body || !body.has("billId") || !body.has("method") ||
            !body.has("amount") || !body.has("date")) {
            crow::json::wvalue err;
            err["error"] = "Missing required field: billId, method, amount, date";
            return crow::response(400, err);
        }

        string billId    = body["billId"].s();
        string method    = body["method"].s();
        string reference = body.has("reference") ? string(body["reference"].s()) : "N/A";
        double amount     = body["amount"].d();
        string date        = body["date"].s();

        try {
            makePaymentLogic(customerId, billId, method, reference, amount, date);

            crow::json::wvalue response;
            response["status"]   = "success";
            response["billId"]   = billId;
            response["amount"]   = amount;
            return crow::response(201, response);

        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();

            string msg = e.what();
            int status = (msg.find("not found") != string::npos) ? 404 : 400;
            return crow::response(status, err);
        }
    });

    
    //  POST /customers/<id>/payments/mpesa
    //  Body: { "billId": "...", "code": "QGR7XYZ123", "amount": 500, "date": "..." }
    //
    //  THE KEY DIFFERENCE: we catch pqxx::unique_violation
    //  SPECIFICALLY and return HTTP 409 Conflict — the correct
    //  status code for "this resource already exists / this
    //  action conflicts with existing data." A generic 400
    //  would be technically wrong; 409 tells the mobile app
    //  exactly what kind of problem this is, so it can show
    //  a specific "code already used" message to the user.
    
    CROW_ROUTE(app, "/customers/<string>/payments/mpesa").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req, const string& customerId) {

        auto body = crow::json::load(req.body);
        if (!body || !body.has("billId") || !body.has("code") ||
            !body.has("amount") || !body.has("date")) {
            crow::json::wvalue err;
            err["error"] = "Missing required field: billId, code, amount, date";
            return crow::response(400, err);
        }

        string billId = body["billId"].s();
        string code   = body["code"].s();
        double amount  = body["amount"].d();
        string date     = body["date"].s();

        try {
            payByMpesaLogic(customerId, billId, code, amount, date);

            crow::json::wvalue response;
            response["status"] = "success";
            response["code"]   = code;
            response["amount"] = amount;
            return crow::response(201, response);

        } catch (const pqxx::unique_violation& e) {
            // THIS catch block must come BEFORE the generic
            // exception catch below — C++ checks catch blocks
            // top to bottom, most specific first.
            crow::json::wvalue err;
            err["error"] = "This M-Pesa code has already been used";
            return crow::response(409, err);   // 409 = Conflict

        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();

            string msg = e.what();
            int status = (msg.find("not found") != string::npos) ? 404 : 400;
            return crow::response(status, err);
        }
    });

    
    //  Full payment history for one customer.
    CROW_ROUTE(app, "/customers/<string>/payments").methods(crow::HTTPMethod::Get)
    ([](const string& customerId) {
        try {
            pqxx::work txn(getConnection());

            pqxx::result custResult = txn.exec(
                "SELECT name, balance FROM customers WHERE id = $1",
                pqxx::params{customerId}
            );
            if (custResult.empty()) {
                crow::json::wvalue err;
                err["error"] = "Customer not found";
                return crow::response(404, err);
            }

            pqxx::result payments = txn.exec(
                "SELECT payment_date, method, reference, amount_paid, balance_after "
                "FROM payments WHERE customer_id = $1 ORDER BY payment_date",
                pqxx::params{customerId}
            );
            txn.commit();

            crow::json::wvalue::list paymentList;
            for (const auto& row : payments) {
                crow::json::wvalue p;
                p["date"]          = row["payment_date"].as<string>();
                p["method"]        = row["method"].as<string>();
                p["reference"]     = row["reference"].as<string>();
                p["amountPaid"]    = row["amount_paid"].as<double>();
                p["balanceAfter"]  = row["balance_after"].as<double>();
                paymentList.push_back(move(p));
            }

            crow::json::wvalue response;
            response["customerName"] = custResult[0]["name"].as<string>();
            response["balance"]      = custResult[0]["balance"].as<double>();
            response["payments"]     = move(paymentList);
            return crow::response(200, response);

        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            return crow::response(500, err);
        }
    });
}