// ============================================================
//  customer_routes.cpp
//  HTTP handlers for customer-related endpoints.
//
//  THE PATTERN FOR EVERY HANDLER:
//    1. Parse the incoming JSON body
//    2. Validate the basics (is a required field missing?)
//    3. Call the existing worker function from customer.cpp
//    4. Build a JSON response, set the right HTTP status code
//    5. Catch exceptions, turn them into error responses
//
//  Nothing here talks to the database directly — that's all
//  still inside customer.cpp. This file is ONLY about
//  translating HTTP <-> your existing C++ functions.
// ============================================================

#include "customer_routes.h"
#include "customer.h"
#include "db.h"
#include <iostream>
using namespace std;

void registerCustomerRoutes(crow::SimpleApp& app) {

    // ========================================================
    //  POST /customers
    //  Body: { "name": "...", "phone": "...", "openingReading": 0 }
    //
    //  THIS REPLACES registerCustomer()'s interactive prompts.
    //  Instead of cin >> reading the terminal, we read the
    //  SAME three values out of a JSON request body.
    // ========================================================
    CROW_ROUTE(app, "/customers").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req) {

        // Parse the request body as JSON. If the body isn't
        // valid JSON at all, crow::json::load returns an
        // object that evaluates to false.
        auto body = crow::json::load(req.body);
        if (!body) {
            crow::json::wvalue err;
            err["error"] = "Invalid JSON body";
            return crow::response(400, err);   // 400 = Bad Request
        }

        // Check the required fields exist BEFORE trying to read them.
        // body.has("key") avoids a crash if the field is missing.
        if (!body.has("name") || !body.has("phone") || !body.has("openingReading")) {
            crow::json::wvalue err;
            err["error"] = "Missing required field: name, phone, openingReading";
            return crow::response(400, err);
        }

        string name  = body["name"].s();
        string phone = body["phone"].s();
        double openingReading = body["openingReading"].d();

        try {
            // THIS is the actual reuse — same function the CLI
            // calls, just fed by JSON instead of cin.
            NewCustomerResult result = registerCustomerLogic(name, phone, openingReading);

            crow::json::wvalue response;
            response["id"]          = result.id;
            response["meterNumber"] = result.meterNumber;
            response["name"]        = name;
            response["phone"]       = phone;

            return crow::response(201, response);   // 201 = Created

        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = string("Registration failed: ") + e.what();
            return crow::response(500, err);   // 500 = Server Error
        }
    });

    // ========================================================
    //  GET /customers
    //  Returns every customer as a JSON array.
    //
    //  THIS REPLACES listCustomers()'s cout table-printing —
    //  same underlying SELECT, just returned as JSON instead
    //  of formatted text.
    // ========================================================
    CROW_ROUTE(app, "/customers").methods(crow::HTTPMethod::Get)
    ([]() {
        try {
            pqxx::work txn(getConnection());
            pqxx::result r = txn.exec(
                "SELECT id, name, meter_number, phone, last_reading, balance "
                "FROM customers ORDER BY created_at"
            );
            txn.commit();

            // crow::json::wvalue can hold an ARRAY too — we build
            // one entry per database row, same loop structure as
            // your CLI's listCustomers(), just building JSON
            // instead of printing a table row.
            crow::json::wvalue::list customers;
            for (const auto& row : r) {
                crow::json::wvalue c;
                c["id"]          = row["id"].as<string>();
                c["name"]        = row["name"].as<string>();
                c["meterNumber"] = row["meter_number"].as<string>();
                c["phone"]       = row["phone"].as<string>();
                c["lastReading"] = row["last_reading"].as<double>();
                c["balance"]     = row["balance"].as<double>();
                customers.push_back(move(c));
            }

            crow::json::wvalue response;
            response["customers"] = move(customers);
            return crow::response(200, response);   // 200 = OK

        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            return crow::response(500, err);
        }
    });

    // ========================================================
    //  DELETE /customers/<id>
    //  Deletes a customer and everything linked to them
    //  (cascading via the schema's foreign keys).
    // ========================================================
    CROW_ROUTE(app, "/customers/<string>").methods(crow::HTTPMethod::Delete)
    ([](const string& customerId) {
        try {
            deleteCustomerLogic(customerId);

            crow::json::wvalue response;
            response["status"] = "deleted";
            response["id"]     = customerId;
            return crow::response(200, response);

        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();

            string msg = e.what();
            int status = (msg.find("not found") != string::npos) ? 404 : 500;
            return crow::response(status, err);
        }
    });

    // ========================================================
    //  GET /customers/search?name=...
    //  Query parameters, not a JSON body — common pattern for
    //  GET requests since GET requests don't usually have bodies.
    // ========================================================
    CROW_ROUTE(app, "/customers/search")
    ([](const crow::request& req) {
        auto query = req.url_params.get("name");
        if (!query) {
            crow::json::wvalue err;
            err["error"] = "Missing query parameter: name";
            return crow::response(400, err);
        }

        string pattern = string("%") + query + "%";

        try {
            pqxx::work txn(getConnection());
            pqxx::result r = txn.exec(
                "SELECT id, name, meter_number, phone, balance "
                "FROM customers WHERE name ILIKE $1",
                pqxx::params{pattern}
            );
            txn.commit();

            crow::json::wvalue::list results;
            for (const auto& row : r) {
                crow::json::wvalue c;
                c["id"]          = row["id"].as<string>();
                c["name"]        = row["name"].as<string>();
                c["meterNumber"] = row["meter_number"].as<string>();
                c["phone"]       = row["phone"].as<string>();
                c["balance"]     = row["balance"].as<double>();
                results.push_back(move(c));
            }

            crow::json::wvalue response;
            response["results"] = move(results);
            return crow::response(200, response);

        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            return crow::response(500, err);
        }
    });
}