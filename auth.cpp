#include "auth.h"
#include "db.h"
#define byte win_byte_override
#include <windows.h>
#undef byte
#define JWT_DISABLE_PICOJSON
#include "jwt-cpp/jwt.h"
#include "jwt-cpp/traits/kazuho-picojson/traits.h"

#include <openssl/sha.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include <stdexcept>
using namespace std;

static const string JWT_SECRET = "change-this-to-a-long-random-string-before-deploying";

static string bytesToHex(const unsigned char* data, size_t len) {
    ostringstream oss;
    for (size_t i = 0; i < len; i++) {
        oss << hex << setw(2) << setfill('0') << (int)data[i];
    }
    return oss.str();
}

string hashPassword(const string& plainPassword) {
    unsigned char salt[16];
    if (RAND_bytes(salt, sizeof(salt)) != 1) {
        throw runtime_error("Failed to generate random salt");
    }
    string saltHex = bytesToHex(salt, sizeof(salt));

    string combined = saltHex + plainPassword;

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(combined.data()), combined.size(), hash);
    string hashHex = bytesToHex(hash, SHA256_DIGEST_LENGTH);

    return saltHex + ":" + hashHex;
}


bool verifyPassword(const string& plainPassword, const string& storedHash) {
    size_t sep = storedHash.find(':');
    if (sep == string::npos) return false;

    string saltHex = storedHash.substr(0, sep);
    string expectedHashHex = storedHash.substr(sep + 1);

    string combined = saltHex + plainPassword;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(combined.data()), combined.size(), hash);
    string actualHashHex = bytesToHex(hash, SHA256_DIGEST_LENGTH);

    return actualHashHex == expectedHashHex;
}

string createToken(const string& userId, const string& role) {
    return jwt::create<jwt::traits::kazuho_picojson>()
        .set_type("JWT")
        .set_issuer("watermeter-system")
        .set_payload_claim("userId", picojson::value(userId))
        .set_payload_claim("role", picojson::value(role))
        .set_issued_at(std::chrono::system_clock::now())
        .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(24 * 7))
        .sign(jwt::algorithm::hs256{JWT_SECRET});
}

TokenPayload verifyToken(const string& token) {
    auto decoded = jwt::decode<jwt::traits::kazuho_picojson>(token);

    auto verifier = jwt::verify<jwt::traits::kazuho_picojson>()
        .with_issuer("watermeter-system")
        .allow_algorithm(jwt::algorithm::hs256{JWT_SECRET});

    verifier.verify(decoded);   

    TokenPayload payload;
    payload.userId = decoded.get_payload_claim("userId").as_string();
    payload.role   = decoded.get_payload_claim("role").as_string();
    return payload;
}


AuthResult customerSignupLogic(const string& name, const string& phone,
                                const string& email, const string& password) {
    pqxx::work txn(getConnection());

    pqxx::result existing = txn.exec(
        "SELECT id FROM customers WHERE email = $1",
        pqxx::params{email}
    );
    if (!existing.empty()) {
        throw runtime_error("An account with this email already exists");
    }

    pqxx::result maxResult = txn.exec(
        "SELECT COALESCE(MAX(CAST(SUBSTRING(meter_number FROM 5) AS INTEGER)), 0) "
        "FROM customers"
    );
    int highestNumber = maxResult[0][0].as<int>();
    string padded = to_string(highestNumber + 1);
    while (padded.length() < 3) padded = "0" + padded;
    string meterNumber = "MTR-" + padded;

    string passwordHash = hashPassword(password);

    pqxx::result r = txn.exec(
        "INSERT INTO customers (name, meter_number, phone, balance, last_reading, email, password_hash) "
        "VALUES ($1, $2, $3, 0.00, 0.00, $4, $5) "
        "RETURNING id",
        pqxx::params{name, meterNumber, phone, email, passwordHash}
    );
    txn.commit();

    string customerId = r[0][0].as<string>();

    AuthResult result;
    result.token  = createToken(customerId, "customer");
    result.userId = customerId;
    result.role   = "customer";
    return result;
}

AuthResult customerLoginLogic(const string& email, const string& password) {
    pqxx::work txn(getConnection());

    pqxx::result r = txn.exec(
        "SELECT id, password_hash FROM customers WHERE email = $1",
        pqxx::params{email}
    );
    txn.commit();

    if (r.empty()) {
      
        throw runtime_error("Invalid email or password");
    }

    string customerId    = r[0]["id"].as<string>();
    string storedHash    = r[0]["password_hash"].is_null() ? "" : r[0]["password_hash"].as<string>();

    if (storedHash.empty() || !verifyPassword(password, storedHash)) {
        throw runtime_error("Invalid email or password");
    }

    AuthResult result;
    result.token  = createToken(customerId, "customer");
    result.userId = customerId;
    result.role   = "customer";
    return result;
}

void requireOwnerOrAdmin(const TokenPayload& payload, const string& customerId) {
    if (payload.role == "admin") {
        return;   
    }
    if (payload.role == "customer" && payload.userId == customerId) {
        return;   
    }
    throw runtime_error("Forbidden: you do not have access to this customer's data");
}

void requireAdmin(const TokenPayload& payload) {
    if (payload.role != "admin") {
        throw runtime_error("Forbidden: admin access required");
    }
}
AuthResult adminLoginLogic(const string& username, const string& password) {
    pqxx::work txn(getConnection());

    pqxx::result r = txn.exec(
        "SELECT id, password_hash FROM admin_users WHERE username = $1",
        pqxx::params{username}
    );
    txn.commit();

    if (r.empty()) {
        throw runtime_error("Invalid username or password");
    }

    string adminId    = r[0]["id"].as<string>();
    string storedHash = r[0]["password_hash"].as<string>();

    if (!verifyPassword(password, storedHash)) {
        throw runtime_error("Invalid username or password");
    }

    AuthResult result;
    result.token  = createToken(adminId, "admin");
    result.userId = adminId;
    result.role   = "admin";
    return result;
}