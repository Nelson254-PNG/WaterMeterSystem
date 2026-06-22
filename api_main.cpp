
#define ASIO_STANDALONE
#include "crow_all.h"
#include "db.h"
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

    // Each module registers its own routes onto the same app.
    // main.cpp stays small — it's just wiring, not logic.
    registerCustomerRoutes(app);
    registerUsageRoutes(app);
    registerBillingRoutes(app);
    registerPaymentRoutes(app);

    cout << "API server starting on http://localhost:8081\n";
    app.port(8081).run();

    disconnectDB();
    return 0;
}