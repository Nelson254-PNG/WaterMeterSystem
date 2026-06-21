// ============================================================
//  crow_test.cpp
//  ONE JOB: prove Crow + Asio compile and run a basic HTTP
//  server on your toolchain. Throwaway scaffolding — same
//  spirit as db_test.cpp earlier.
// ============================================================

// ASIO_STANDALONE tells Crow to use standalone Asio (the
// package we just installed) instead of expecting it bundled
// inside Boost — which we don't have installed and don't need.
#define ASIO_STANDALONE

#include "crow_all.h"

int main() {
    crow::SimpleApp app;

    // Defines ONE route: GET http://localhost:8080/
    // The lambda runs every time someone visits that URL.
    CROW_ROUTE(app, "/")
    ([]() {
        return "✔ Crow server is running!";
    });

    // A second route to prove parameters/JSON-style responses work
    CROW_ROUTE(app, "/ping")
    ([]() {
        crow::json::wvalue result;
        result["status"] = "ok";
        result["message"] = "pong";
        return result;
    });

    // Starts the server, listening on port 8080, blocking here
    // until you press Ctrl+C in the terminal.
    app.port(8081).run();
}