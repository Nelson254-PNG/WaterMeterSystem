#include "auth_routes.h"
#include "auth.h"
#include <iostream>
using namespace std;

void registerAuthRoutes(crow::SimpleApp& app) {

    CROW_ROUTE(app, "/auth/signup").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req) {

        auto body = crow::json::load(req.body);
        if (!body || !body.has("name") || !body.has("phone") ||
            !body.has("email") || !body.has("password")) {
            crow::json::wvalue err;
            err["error"] = "Missing required field: name, phone, email, password";
            return crow::response(400, err);
        }

        string name     = body["name"].s();
        string phone    = body["phone"].s();
        string email    = body["email"].s();
        string password = body["password"].s();

        if (password.length() < 6) {
            crow::json::wvalue err;
            err["error"] = "Password must be at least 6 characters";
            return crow::response(400, err);
        }

        try {
            AuthResult result = customerSignupLogic(name, phone, email, password);

            crow::json::wvalue response;
            response["token"]  = result.token;
            response["userId"] = result.userId;
            response["role"]   = result.role;
            return crow::response(201, response);

        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
           
            string msg = e.what();
            int status = (msg.find("already exists") != string::npos) ? 409 : 400;
            return crow::response(status, err);
        }
    });

    
    CROW_ROUTE(app, "/auth/login").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req) {

        auto body = crow::json::load(req.body);
        if (!body || !body.has("email") || !body.has("password")) {
            crow::json::wvalue err;
            err["error"] = "Missing required field: email, password";
            return crow::response(400, err);
        }

        string email    = body["email"].s();
        string password = body["password"].s();

        try {
            AuthResult result = customerLoginLogic(email, password);

            crow::json::wvalue response;
            response["token"]  = result.token;
            response["userId"] = result.userId;
            response["role"]   = result.role;
            return crow::response(200, response);

        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
           
            return crow::response(401, err);
        }
    });

   
    CROW_ROUTE(app, "/auth/admin-login").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req) {

        auto body = crow::json::load(req.body);
        if (!body || !body.has("username") || !body.has("password")) {
            crow::json::wvalue err;
            err["error"] = "Missing required field: username, password";
            return crow::response(400, err);
        }

        string username = body["username"].s();
        string password = body["password"].s();

        try {
            AuthResult result = adminLoginLogic(username, password);

            crow::json::wvalue response;
            response["token"]  = result.token;
            response["userId"] = result.userId;
            response["role"]   = result.role;
            return crow::response(200, response);

        } catch (const exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            return crow::response(401, err);
        }
    });
}