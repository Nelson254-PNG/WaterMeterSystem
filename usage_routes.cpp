

#include "usage_routes.h"
#include "usage.h"
#include "db.h"
#include <iostream>
using namespace std;

void registerUsageRoutes(crow::SimpleApp& app) {

   
    //  Notice the customer ID is in the URL PATH this time,
    //  not the JSON body — this is a common REST convention:
    //  "usage records belonging to THIS specific customer."
    //  Crow lets us capture path segments with <string> in
    //  the route pattern.
    
    CROW_ROUTE(app, "/customers/<string>/usage").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req, const string& customerId) {

        auto body = crow::json::load(req.body);
        if (!body || !body.has("currentReading") || !body.has("date")) {
            crow::json::wvalue err;
            err["error"] = "Missing required field: currentReading, date";
            return crow::response(400, err);
        }

        double currentReading = body["currentReading"].d();
        string date           = body["date"].s();

        try {
            double unitsUsed = recordUsageLogic(customerId, currentReading, date);

            crow::json::wvalue response;
            response["customerId"]     = customerId;
            response["currentReading"] = currentReading;
            response["unitsUsed"]      = unitsUsed;
            response["date"]           = date;

            return crow::response(201, response);

        } catch (const exception& e) {
            // recordUsageLogic() throws for BOTH "not found" and
            // "reading too low" — we use the message text itself
            // to decide which HTTP status fits best.
            crow::json::wvalue err;
            err["error"] = e.what();

            string msg = e.what();
            int status = (msg.find("not found") != string::npos) ? 404 : 400;
            return crow::response(status, err);
        }
    });

    //  Returns the full usage history for one customer.
   
    CROW_ROUTE(app, "/customers/<string>/usage").methods(crow::HTTPMethod::Get)
    ([](const string& customerId) {
        try {
            pqxx::work txn(getConnection());

            pqxx::result custResult = txn.exec(
                "SELECT name FROM customers WHERE id = $1",
                pqxx::params{customerId}
            );
            if (custResult.empty()) {
                crow::json::wvalue err;
                err["error"] = "Customer not found";
                return crow::response(404, err);
            }

            pqxx::result records = txn.exec(
                "SELECT reading_date, previous_reading, current_reading, units_used, billed "
                "FROM water_records WHERE customer_id = $1 ORDER BY reading_date",
                pqxx::params{customerId}
            );
            txn.commit();

            crow::json::wvalue::list history;
            for (const auto& row : records) {
                crow::json::wvalue r;
                r["date"]             = row["reading_date"].as<string>();
                r["previousReading"]  = row["previous_reading"].as<double>();
                r["currentReading"]   = row["current_reading"].as<double>();
                r["unitsUsed"]        = row["units_used"].as<double>();
                r["billed"]           = row["billed"].as<bool>();
                history.push_back(move(r));
            }

            crow::json::wvalue response;
            response["customerName"] = custResult[0]["name"].as<string>();
            response["records"]      = move(history);
            return crow::response(200, response);

        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            return crow::response(500, err);
        }
    });
}