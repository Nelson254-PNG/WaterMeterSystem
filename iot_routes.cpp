// ============================================================
//  iot_routes.cpp
//  All IoT-related HTTP routes:
//
//  POST /iot/reading            ← meter sends live reading
//  GET  /iot/readings/:id       ← admin views a meter's readings
//  POST /iot/valve/:id          ← admin opens/closes a valve
//  GET  /iot/alerts             ← admin sees all alerts
//  POST /iot/alerts/:id/resolve ← admin resolves an alert
//  POST /iot/prepaid/:id/topup  ← customer buys credit
//  GET  /customers/:id/iot      ← customer sees their IoT data
// ============================================================

#include "iot_routes.h"
#include "auth.h"
#include "db.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>
using namespace std;

// ── IoT API Key (must match iot_simulator.cpp) ────────────────
static const string IOT_API_KEY = "iot-simulator-secret-key-2026";

// ── Leak detection threshold ──────────────────────────────────
// If a meter shows continuous flow for more than this many
// consecutive readings, flag it as a potential leak.
static const int LEAK_READING_THRESHOLD = 4; // 4 × 30s = 2 minutes in demo
// In production: 240 readings × 30s = 2 hours

// ── Token generator ───────────────────────────────────────────
static string generateTokenCode() {
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    uniform_int_distribution<int> dist(0, 9999);
    ostringstream oss;
    oss << setw(4) << setfill('0') << dist(rng) << "-"
        << setw(4) << setfill('0') << dist(rng) << "-"
        << setw(4) << setfill('0') << dist(rng);
    return oss.str();
}

// KES per m³ for prepaid (same as tier 1 rate for simplicity)
static const double PREPAID_RATE = 50.0;

void registerIoTRoutes(crow::SimpleApp& app) {

    // ========================================================
    //  POST /iot/reading
    //  Receives a reading from a physical (or simulated) meter.
    //
    //  NO JWT AUTH — uses X-IoT-Key header instead.
    //  In production, each meter would have a device certificate.
    //  For simulation, we use a shared secret key.
    //
    //  Body: { customerId, readingValue, flowRate,
    //          batteryLevel, signalStrength, leaking, tampered }
    // ========================================================
    CROW_ROUTE(app, "/iot/reading").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req) {

        // Verify IoT API key
        string iotKey = req.get_header_value("X-IoT-Key");
        if (iotKey != IOT_API_KEY) {
            crow::json::wvalue err;
            err["error"] = "Invalid IoT API key";
            return crow::response(401, err);
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("customerId") || !body.has("readingValue")) {
            crow::json::wvalue err;
            err["error"] = "Missing required field: customerId, readingValue";
            return crow::response(400, err);
        }

        string customerId   = body["customerId"].s();
        double readingValue = body["readingValue"].d();
        double flowRate     = body.has("flowRate")     ? body["flowRate"].d()          : 0.0;
        int    battery      = body.has("batteryLevel") ? (int)body["batteryLevel"].d() : 100;
        int    signal       = body.has("signalStrength") ? (int)body["signalStrength"].d() : -70;
        bool   leaking      = body.has("leaking")  && body["leaking"].b();
        bool   tampered     = body.has("tampered") && body["tampered"].b();

        try {
            pqxx::work txn(getConnection());

            // Verify customer exists
            pqxx::result cust = txn.exec(
                "SELECT id, valve_open, prepaid_credit, meter_mode FROM customers WHERE id = $1",
                pqxx::params{customerId}
            );
            if (cust.empty()) {
                crow::json::wvalue err; err["error"] = "Customer not found";
                return crow::response(404, err);
            }

            bool   valveOpen     = cust[0]["valve_open"].as<bool>();
            double prepaidCredit = cust[0]["prepaid_credit"].as<double>();
            string meterMode     = cust[0]["meter_mode"].as<string>();

            // Store the reading
            txn.exec(
                "INSERT INTO iot_readings "
                "(customer_id, reading_value, flow_rate, signal_strength, battery_level) "
                "VALUES ($1, $2, $3, $4, $5)",
                pqxx::params{customerId, readingValue, flowRate, signal, battery}
            );

            // ── LEAK DETECTION ───────────────────────────────
            if (leaking || flowRate > 20.0) {
                // Check how many consecutive high-flow readings
                pqxx::result recentReadings = txn.exec(
                    "SELECT COUNT(*) FROM iot_readings "
                    "WHERE customer_id = $1 "
                    "AND flow_rate > 15 "
                    "AND transmitted_at > now() - interval '10 minutes'",
                    pqxx::params{customerId}
                );
                int highFlowCount = recentReadings[0][0].as<int>();

                if (highFlowCount >= LEAK_READING_THRESHOLD) {
                    // Check if we already have an unresolved leak alert
                    pqxx::result existingAlert = txn.exec(
                        "SELECT id FROM alerts WHERE customer_id = $1 "
                        "AND alert_type = 'leak_detected' AND resolved = false",
                        pqxx::params{customerId}
                    );
                    if (existingAlert.empty()) {
                        txn.exec(
                            "INSERT INTO alerts (customer_id, alert_type, message, severity) "
                            "VALUES ($1, 'leak_detected', "
                            "'Continuous high water flow detected. Possible leak or burst pipe.', "
                            "'critical')",
                            pqxx::params{customerId}
                        );
                        cout << "🚨 LEAK ALERT created for customer: " << customerId << "\n";
                    }
                }
            }

            // ── TAMPER DETECTION ─────────────────────────────
            if (tampered) {
                pqxx::result existingTamper = txn.exec(
                    "SELECT id FROM alerts WHERE customer_id = $1 "
                    "AND alert_type = 'tamper_warning' AND resolved = false",
                    pqxx::params{customerId}
                );
                if (existingTamper.empty()) {
                    txn.exec(
                        "INSERT INTO alerts (customer_id, alert_type, message, severity) "
                        "VALUES ($1, 'tamper_warning', "
                        "'Physical meter tampering detected. Immediate inspection required.', "
                        "'critical')",
                        pqxx::params{customerId}
                    );
                    cout << "🚨 TAMPER ALERT created for customer: " << customerId << "\n";
                }
            }

            // ── PREPAID CREDIT DEDUCTION ─────────────────────
            if (meterMode == "prepaid" && flowRate > 0) {
                // Deduct credit based on flow rate and interval
                double litresConsumed = (flowRate * 30.0) / 60.0;  // 30s interval
                double m3Consumed     = litresConsumed / 1000.0;
                double costKes        = m3Consumed * PREPAID_RATE;

                double newCredit = max(0.0, prepaidCredit - costKes);

                txn.exec(
                    "UPDATE customers SET prepaid_credit = $1 WHERE id = $2",
                    pqxx::params{newCredit, customerId}
                );

                // Low credit warning
                if (newCredit < 50.0 && newCredit > 0) {
                    pqxx::result existing = txn.exec(
                        "SELECT id FROM alerts WHERE customer_id = $1 "
                        "AND alert_type = 'low_credit' AND resolved = false",
                        pqxx::params{customerId}
                    );
                    if (existing.empty()) {
                        txn.exec(
                            "INSERT INTO alerts (customer_id, alert_type, message, severity) "
                            "VALUES ($1, 'low_credit', "
                            "'Prepaid credit is below KES 50. Top up to avoid interruption.', "
                            "'warning')",
                            pqxx::params{customerId}
                        );
                    }
                }

                // Zero credit — close the valve automatically
                if (newCredit <= 0 && valveOpen) {
                    txn.exec(
                        "UPDATE customers SET valve_open = false WHERE id = $1",
                        pqxx::params{customerId}
                    );
                    txn.exec(
                        "INSERT INTO alerts (customer_id, alert_type, message, severity) "
                        "VALUES ($1, 'zero_credit', "
                        "'Prepaid credit exhausted. Water supply suspended until top-up.', "
                        "'critical')",
                        pqxx::params{customerId}
                    );
                    cout << "🔒 Valve AUTO-CLOSED for customer (zero credit): " << customerId << "\n";
                }
            }

            txn.commit();

            crow::json::wvalue response;
            response["status"]      = "received";
            response["valveOpen"]   = valveOpen;
            response["customerId"]  = customerId;
            return crow::response(200, response);

        } catch (const exception& e) {
            crow::json::wvalue err; err["error"] = e.what();
            return crow::response(500, err);
        }
    });

    // ========================================================
    //  GET /iot/readings/:id
    //  Returns recent IoT readings for one customer.
    //  OWNER-OR-ADMIN.
    // ========================================================
    CROW_ROUTE(app, "/iot/readings/<string>").methods(crow::HTTPMethod::Get)
    ([](const crow::request& req, const string& customerId) {
        try {
            TokenPayload payload = requireAuth(req);
            requireOwnerOrAdmin(payload, customerId);
        } catch (const exception& e) {
            crow::json::wvalue err; err["error"] = e.what();
            return crow::response(401, err);
        }

        try {
            pqxx::work txn(getConnection());
            pqxx::result readings = txn.exec(
                "SELECT reading_value, flow_rate, signal_strength, battery_level, transmitted_at "
                "FROM iot_readings WHERE customer_id = $1 "
                "ORDER BY transmitted_at DESC LIMIT 50",
                pqxx::params{customerId}
            );
            txn.commit();

            crow::json::wvalue::list list;
            for (const auto& r : readings) {
                crow::json::wvalue item;
                item["readingValue"]   = r["reading_value"].as<double>();
                item["flowRate"]       = r["flow_rate"].as<double>();
                item["signalStrength"] = r["signal_strength"].as<int>();
                item["batteryLevel"]   = r["battery_level"].as<int>();
                item["transmittedAt"]  = r["transmitted_at"].as<string>();
                list.push_back(move(item));
            }

            crow::json::wvalue response;
            response["readings"] = move(list);
            return crow::response(200, response);

        } catch (const exception& e) {
            crow::json::wvalue err; err["error"] = e.what();
            return crow::response(500, err);
        }
    });

    // ========================================================
    //  POST /iot/valve/:id
    //  Admin opens or closes a customer's meter valve remotely.
    //  Body: { "open": true/false, "reason": "..." }
    //  ADMIN-ONLY.
    // ========================================================
    CROW_ROUTE(app, "/iot/valve/<string>").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req, const string& customerId) {
        try {
            TokenPayload payload = requireAuth(req);
            requireAdmin(payload);
        } catch (const exception& e) {
            crow::json::wvalue err; err["error"] = e.what();
            return crow::response(401, err);
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("open")) {
            crow::json::wvalue err; err["error"] = "Missing required field: open (true/false)";
            return crow::response(400, err);
        }

        bool   open   = body["open"].b();
        string reason = body.has("reason") ? string(body["reason"].s()) : "Admin action";

        try {
            pqxx::work txn(getConnection());
            txn.exec(
                "UPDATE customers SET valve_open = $1 WHERE id = $2",
                pqxx::params{open, customerId}
            );

            // Create an alert so there's an audit trail
            string alertType = open ? "valve_opened" : "valve_closed";
            string message   = open
                ? "Valve remotely opened by administrator. Reason: " + reason
                : "Valve remotely closed by administrator. Reason: " + reason;

            txn.exec(
                "INSERT INTO alerts (customer_id, alert_type, message, severity) "
                "VALUES ($1, $2, $3, 'info')",
                pqxx::params{customerId, alertType, message}
            );
            txn.commit();

            crow::json::wvalue response;
            response["valveOpen"] = open;
            response["customerId"] = customerId;
            response["message"]    = open ? "Valve opened" : "Valve closed";
            return crow::response(200, response);

        } catch (const exception& e) {
            crow::json::wvalue err; err["error"] = e.what();
            return crow::response(500, err);
        }
    });

    // ========================================================
    //  GET /iot/alerts
    //  Returns all unresolved alerts across all customers.
    //  ADMIN-ONLY.
    // ========================================================
    CROW_ROUTE(app, "/iot/alerts").methods(crow::HTTPMethod::Get)
    ([](const crow::request& req) {
        try {
            TokenPayload payload = requireAuth(req);
            requireAdmin(payload);
        } catch (const exception& e) {
            crow::json::wvalue err; err["error"] = e.what();
            return crow::response(401, err);
        }

        try {
            pqxx::work txn(getConnection());
            pqxx::result alerts = txn.exec(
                "SELECT a.id, a.customer_id, c.name, c.meter_number, "
                "a.alert_type, a.message, a.severity, a.created_at "
                "FROM alerts a JOIN customers c ON c.id = a.customer_id "
                "WHERE a.resolved = false "
                "ORDER BY a.created_at DESC"
            );
            txn.commit();

            crow::json::wvalue::list list;
            for (const auto& r : alerts) {
                crow::json::wvalue item;
                item["id"]           = r["id"].as<string>();
                item["customerId"]   = r["customer_id"].as<string>();
                item["customerName"] = r["name"].as<string>();
                item["meterNumber"]  = r["meter_number"].as<string>();
                item["alertType"]    = r["alert_type"].as<string>();
                item["message"]      = r["message"].as<string>();
                item["severity"]     = r["severity"].as<string>();
                item["createdAt"]    = r["created_at"].as<string>();
                list.push_back(move(item));
            }

            crow::json::wvalue response;
            response["alerts"] = move(list);
            return crow::response(200, response);

        } catch (const exception& e) {
            crow::json::wvalue err; err["error"] = e.what();
            return crow::response(500, err);
        }
    });

    // ========================================================
    //  POST /iot/alerts/:id/resolve
    //  Admin marks an alert as resolved.
    //  ADMIN-ONLY.
    // ========================================================
    CROW_ROUTE(app, "/iot/alerts/<string>/resolve").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req, const string& alertId) {
        try {
            TokenPayload payload = requireAuth(req);
            requireAdmin(payload);
        } catch (const exception& e) {
            crow::json::wvalue err; err["error"] = e.what();
            return crow::response(401, err);
        }

        try {
            pqxx::work txn(getConnection());
            txn.exec(
                "UPDATE alerts SET resolved = true, resolved_at = now() WHERE id = $1",
                pqxx::params{alertId}
            );
            txn.commit();

            crow::json::wvalue response;
            response["status"] = "resolved";
            return crow::response(200, response);

        } catch (const exception& e) {
            crow::json::wvalue err; err["error"] = e.what();
            return crow::response(500, err);
        }
    });

    // ========================================================
    //  POST /iot/prepaid/:id/topup
    //  Customer buys prepaid water credit.
    //  Body: { "amountKes": 200 }
    //  OWNER-OR-ADMIN.
    // ========================================================
    CROW_ROUTE(app, "/iot/prepaid/<string>/topup").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req, const string& customerId) {
        try {
            TokenPayload payload = requireAuth(req);
            requireOwnerOrAdmin(payload, customerId);
        } catch (const exception& e) {
            crow::json::wvalue err; err["error"] = e.what();
            return crow::response(401, err);
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("amountKes")) {
            crow::json::wvalue err; err["error"] = "Missing required field: amountKes";
            return crow::response(400, err);
        }

        double amountKes = body["amountKes"].d();
        if (amountKes < 50) {
            crow::json::wvalue err; err["error"] = "Minimum top-up is KES 50";
            return crow::response(400, err);
        }

        // Calculate units: KES 50/m³ (tier 1 rate)
        double unitsM3 = amountKes / PREPAID_RATE;

        try {
            pqxx::work txn(getConnection());

            // Generate a unique token code
            string tokenCode = generateTokenCode();

            txn.exec(
                "INSERT INTO prepaid_tokens (customer_id, token_code, amount_kes, units_m3) "
                "VALUES ($1, $2, $3, $4)",
                pqxx::params{customerId, tokenCode, amountKes, unitsM3}
            );

            // Add credit to customer and re-open valve if it was closed for zero credit
            txn.exec(
                "UPDATE customers "
                "SET prepaid_credit = prepaid_credit + $1, "
                "    valve_open = CASE WHEN prepaid_credit + $1 > 0 THEN true ELSE valve_open END "
                "WHERE id = $2",
                pqxx::params{amountKes, customerId}
            );

            // Resolve any zero_credit or low_credit alerts
            txn.exec(
                "UPDATE alerts SET resolved = true, resolved_at = now() "
                "WHERE customer_id = $1 "
                "AND alert_type IN ('zero_credit', 'low_credit') "
                "AND resolved = false",
                pqxx::params{customerId}
            );

            txn.commit();

            crow::json::wvalue response;
            response["tokenCode"]  = tokenCode;
            response["amountKes"]  = amountKes;
            response["unitsM3"]    = unitsM3;
            response["message"]    = "Top-up successful! Credit added to your meter.";
            return crow::response(201, response);

        } catch (const exception& e) {
            crow::json::wvalue err; err["error"] = e.what();
            return crow::response(500, err);
        }
    });

    // ========================================================
    //  GET /customers/:id/iot
    //  Customer-facing: their meter's live status.
    //  Returns valve state, prepaid credit, latest reading,
    //  flow rate, battery, signal — everything a customer
    //  would want to see in "My Meter" tab.
    //  OWNER-OR-ADMIN.
    // ========================================================
    CROW_ROUTE(app, "/customers/<string>/iot").methods(crow::HTTPMethod::Get)
    ([](const crow::request& req, const string& customerId) {
        try {
            TokenPayload payload = requireAuth(req);
            requireOwnerOrAdmin(payload, customerId);
        } catch (const exception& e) {
            crow::json::wvalue err; err["error"] = e.what();
            return crow::response(401, err);
        }

        try {
            pqxx::work txn(getConnection());

            // Customer's meter status
            pqxx::result cust = txn.exec(
                "SELECT valve_open, prepaid_credit, meter_mode FROM customers WHERE id = $1",
                pqxx::params{customerId}
            );
            if (cust.empty()) {
                crow::json::wvalue err; err["error"] = "Customer not found";
                return crow::response(404, err);
            }

            // Latest IoT reading
            pqxx::result latest = txn.exec(
                "SELECT reading_value, flow_rate, signal_strength, battery_level, transmitted_at "
                "FROM iot_readings WHERE customer_id = $1 "
                "ORDER BY transmitted_at DESC LIMIT 1",
                pqxx::params{customerId}
            );

            // Unresolved alerts for this customer
            pqxx::result alerts = txn.exec(
                "SELECT alert_type, message, severity, created_at "
                "FROM alerts WHERE customer_id = $1 AND resolved = false "
                "ORDER BY created_at DESC",
                pqxx::params{customerId}
            );

            txn.commit();

            crow::json::wvalue response;
            response["valveOpen"]     = cust[0]["valve_open"].as<bool>();
            response["prepaidCredit"] = cust[0]["prepaid_credit"].as<double>();
            response["meterMode"]     = cust[0]["meter_mode"].as<string>();

            if (!latest.empty()) {
                response["latestReading"]  = latest[0]["reading_value"].as<double>();
                response["flowRate"]       = latest[0]["flow_rate"].as<double>();
                response["signalStrength"] = latest[0]["signal_strength"].as<int>();
                response["batteryLevel"]   = latest[0]["battery_level"].as<int>();
                response["lastSeen"]       = latest[0]["transmitted_at"].as<string>();
            } else {
                response["latestReading"] = 0;
                response["flowRate"]      = 0;
                response["lastSeen"]      = "No readings yet";
            }

            crow::json::wvalue::list alertList;
            for (const auto& a : alerts) {
                crow::json::wvalue item;
                item["alertType"] = a["alert_type"].as<string>();
                item["message"]   = a["message"].as<string>();
                item["severity"]  = a["severity"].as<string>();
                item["createdAt"] = a["created_at"].as<string>();
                alertList.push_back(move(item));
            }
            response["alerts"] = move(alertList);

            return crow::response(200, response);

        } catch (const exception& e) {
            crow::json::wvalue err; err["error"] = e.what();
            return crow::response(500, err);
        }
    });
}