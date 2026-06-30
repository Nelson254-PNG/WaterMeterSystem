

#include "customer_routes.h"
#include "customer.h"
#include "db.h"
#include "auth.h"
#include <iostream>
using namespace std;

void registerCustomerRoutes(crow::SimpleApp& app) {

    CROW_ROUTE(app, "/customers").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req) {

       
        try {
            TokenPayload payload = requireAuth(req);
            requireAdmin(payload);
        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            return crow::response(401, err);
        }
        auto body = crow::json::load(req.body);
        if (!body) {
            crow::json::wvalue err;
            err["error"] = "Invalid JSON body";
            return crow::response(400, err);   
        }

        
        if (!body.has("name") || !body.has("phone") || !body.has("openingReading")) {
            crow::json::wvalue err;
            err["error"] = "Missing required field: name, phone, openingReading";
            return crow::response(400, err);
        }

        string name  = body["name"].s();
        string phone = body["phone"].s();
        double openingReading = body["openingReading"].d();

        try {
           
            NewCustomerResult result = registerCustomerLogic(name, phone, openingReading);

            crow::json::wvalue response;
            response["id"]          = result.id;
            response["meterNumber"] = result.meterNumber;
            response["name"]        = name;
            response["phone"]       = phone;

            return crow::response(201, response);   
        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = string("Registration failed: ") + e.what();
            return crow::response(500, err);   
        }
    });

    CROW_ROUTE(app, "/customers").methods(crow::HTTPMethod::Get)
    ([](const crow::request& req) {

        try {
            TokenPayload payload = requireAuth(req);
            requireAdmin(payload);
        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            return crow::response(401, err);
        }

        try {
            pqxx::work txn(getConnection());
            pqxx::result r = txn.exec(
                "SELECT id, name, meter_number, phone, last_reading, balance "
                "FROM customers ORDER BY created_at"
            );
            txn.commit();

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

    CROW_ROUTE(app, "/customers/<string>").methods(crow::HTTPMethod::Delete)
    ([](const crow::request& req, const string& customerId) {

        try {
            TokenPayload payload = requireAuth(req);
            requireAdmin(payload);
        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            return crow::response(401, err);
        }

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

    CROW_ROUTE(app, "/customers/<string>").methods(crow::HTTPMethod::Get)
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
            pqxx::result r = txn.exec(
                "SELECT id, name, meter_number, phone, last_reading, balance "
                "FROM customers WHERE id = $1",
                pqxx::params{customerId}
            );
            txn.commit();

            if (r.empty()) {
                crow::json::wvalue err;
                err["error"] = "Customer not found";
                return crow::response(404, err);
            }

            crow::json::wvalue c;
            c["id"]          = r[0]["id"].as<string>();
            c["name"]        = r[0]["name"].as<string>();
            c["meterNumber"] = r[0]["meter_number"].as<string>();
            c["phone"]       = r[0]["phone"].as<string>();
            c["lastReading"] = r[0]["last_reading"].as<double>();
            c["balance"]     = r[0]["balance"].as<double>();
            return crow::response(200, c);

        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            return crow::response(500, err);
        }
    });

    CROW_ROUTE(app, "/customers/search")
    ([](const crow::request& req) {

        try {
            TokenPayload payload = requireAuth(req);
            requireAdmin(payload);
        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            return crow::response(401, err);
        }

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