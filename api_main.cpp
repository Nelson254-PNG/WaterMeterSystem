
#define ASIO_STANDALONE
#include "crow_all.h"
#include "db.h"
#include "auth_routes.h"
#include "customer_routes.h"
#include "usage_routes.h"
#include "billing_routes.h"
#include "payment_routes.h"
#include "mpesa_routes.h"
#include "iot_routes.h"
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
    registerMpesaRoutes(app);
    registerIoTRoutes(app);

    cout << "API server starting on http://localhost:8090\n";
    app.port(8090).run();

    disconnectDB();
    return 0;
}