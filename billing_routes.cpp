#include "billing_routes.h"
#include "billing.h"
#include "db.h"
#include "auth.h"
#include <iostream>
using namespace std;

static crow::json::wvalue tierBreakdownToJson(const TierBreakdown& tb) {
    crow::json::wvalue j;
    j["tier1Units"] = tb.tier1Units; j["tier1Cost"] = tb.tier1Cost;
    j["tier2Units"] = tb.tier2Units; j["tier2Cost"] = tb.tier2Cost;
    j["tier3Units"] = tb.tier3Units; j["tier3Cost"] = tb.tier3Cost;
    j["tier4Units"] = tb.tier4Units; j["tier4Cost"] = tb.tier4Cost;
    j["serviceCharge"] = tb.serviceCharge;
    j["totalAmount"]   = tb.totalAmount;
    return j;
}

void registerBillingRoutes(crow::SimpleApp& app) {

    CROW_ROUTE(app, "/customers/<string>/bills").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req, const string& customerId) {

        try {
            TokenPayload payload = requireAuth(req);
            requireAdmin(payload);
        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            return crow::response(401, err);
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("issueDate") || !body.has("dueDate")) {
            crow::json::wvalue err;
            err["error"] = "Missing required field: issueDate, dueDate";
            return crow::response(400, err);
        }

        string issueDate = body["issueDate"].s();
        string dueDate   = body["dueDate"].s();

        try {
            GenerateBillResult result = generateBillLogic(customerId, issueDate, dueDate);

            crow::json::wvalue response;
            response["billId"]     = result.billId;
            response["totalUnits"] = result.totalUnits;
            response["breakdown"]  = tierBreakdownToJson(result.breakdown);

            return crow::response(201, response);

        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();

            string msg = e.what();
            int status = (msg.find("not found") != string::npos) ? 404 : 400;
            return crow::response(status, err);
        }
    });

    CROW_ROUTE(app, "/customers/<string>/bills").methods(crow::HTTPMethod::Get)
    ([](const crow::request& req, const string& customerId) {

        try {
            TokenPayload payload = requireAuth(req);
            requireOwnerOrAdmin(payload, customerId);
        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            return crow::response(401, err);
        }

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

            pqxx::result bills = txn.exec(
                "SELECT id, issue_date, due_date, total_units, total_amount, amount_paid, paid "
                "FROM bills WHERE customer_id = $1 ORDER BY issue_date",
                pqxx::params{customerId}
            );
            txn.commit();

            crow::json::wvalue::list billList;
            for (const auto& row : bills) {
                crow::json::wvalue b;
                b["id"]          = row["id"].as<string>();
                b["issueDate"]   = row["issue_date"].as<string>();
                b["dueDate"]     = row["due_date"].as<string>();
                b["totalUnits"]  = row["total_units"].as<double>();
                b["totalAmount"] = row["total_amount"].as<double>();
                b["amountPaid"]  = row["amount_paid"].as<double>();
                b["paid"]        = row["paid"].as<bool>();
                billList.push_back(move(b));
            }

            crow::json::wvalue response;
            response["customerName"] = custResult[0]["name"].as<string>();
            response["balance"]      = custResult[0]["balance"].as<double>();
            response["bills"]        = move(billList);
            return crow::response(200, response);

        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            return crow::response(500, err);
        }
    });
}