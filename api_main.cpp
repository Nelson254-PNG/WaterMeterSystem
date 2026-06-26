// ============================================================
//  api_main.cpp
//  Entry point for the API SERVER (separate from your CLI's
//  main.cpp, so you can still run the CLI for testing/demo
//  purposes independently).
// ============================================================

#define ASIO_STANDALONE
#include "crow_all.h"
#include "db.h"
#include "auth_routes.h"
#include "customer_routes.h"
#include "usage_routes.h"
#include "billing_routes.h"
#include "payment_routes.h"
#include <iostream>
using namespace std;

int main() {
    if (!connectDB()) {
        cerr << "Could not start: database connection failed.\n";
        return 1;
    }

    crow::SimpleApp app;

    registerAuthRoutes(app);
    registerCustomerRoutes(app);
    registerUsageRoutes(app);
    registerBillingRoutes(app);
    registerPaymentRoutes(app);

    cout << "API server starting on http://localhost:8090\n";
    app.port(8090).run();

    disconnectDB();
    return 0;
}