// ============================================================
//  auth.cpp
// ============================================================

#include "auth.h"
#include "db.h"

// ── WINDOWS "byte" AMBIGUITY FIX ──────────────────────────────
// jwt-cpp's jwt.h includes OpenSSL's ssl.h, which transitively
// drags in windows.h. windows.h's rpcndr.h declares its own
// plain "byte" typedef (unsigned char). Meanwhile auth.h already
// included <string>, which pulls in <cstddef> and its C++17
// "std::byte" enum. With "using namespace std;" active, the
// bare word "byte" becomes ambiguous between ::byte and
// std::byte the moment windows.h loads — a well-known, specific
// Windows SDK problem unrelated to our own code's correctness.
//
// THE FIX: temporarily rename windows.h's "byte" to a harmless
// identifier BEFORE it gets defined, then restore normal "byte"
// usage immediately after. This is the standard workaround used
// across many real C++/Windows projects that hit this exact
// issue (Qt, Mozilla, and others have all documented it).
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

// ============================================================
//  SECRET KEY — used to SIGN every token.
//
//  CRITICAL: anyone who has this string can forge valid tokens
//  for ANY user. In a real production system this would come
//  from an environment variable, never hardcoded in source
//  control. For this portfolio project, keep it here but
//  understand this is the #1 thing to change before any real
//  deployment.
// ============================================================
static const string JWT_SECRET = "change-this-to-a-long-random-string-before-deploying";

// ============================================================
//  HELPER: bytesToHex
//  Converts raw binary hash bytes into a readable hex string
//  for storage, e.g. {0xAB, 0x3F} -> "ab3f"
// ============================================================
static string bytesToHex(const unsigned char* data, size_t len) {
    ostringstream oss;
    for (size_t i = 0; i < len; i++) {
        oss << hex << setw(2) << setfill('0') << (int)data[i];
    }
    return oss.str();
}

// ============================================================
//  FUNCTION: hashPassword
//
//  1. Generate a random 16-byte salt (RAND_bytes — OpenSSL's
//     cryptographically secure random generator, NOT rand()
//     which is predictable and unsafe for security purposes)
//  2. Hash (salt + password) together with SHA-256
//  3. Store as "saltHex:hashHex" so verifyPassword() can later
//     reconstruct the exact same salt for comparison
// ============================================================
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

// ============================================================
//  FUNCTION: verifyPassword
//  Splits the stored "salt:hash" back apart, re-hashes the
//  CANDIDATE password with that same salt, and compares.
// ============================================================
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

// ============================================================
//  FUNCTION: createToken
//  Embeds userId and role as claims, signs with HS256, sets
//  a 7-day expiry — after that, the customer must log in again.
// ============================================================
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

// ============================================================
//  FUNCTION: verifyToken
//  Decodes, checks the signature wasn't tampered with, checks
//  it hasn't expired, then extracts the claims. Throws on any
//  failure — the caller (route middleware) turns that into a
//  401 Unauthorized response.
// ============================================================
TokenPayload verifyToken(const string& token) {
    auto decoded = jwt::decode<jwt::traits::kazuho_picojson>(token);

    auto verifier = jwt::verify<jwt::traits::kazuho_picojson>()
        .with_issuer("watermeter-system")
        .allow_algorithm(jwt::algorithm::hs256{JWT_SECRET});

    verifier.verify(decoded);   // throws jwt::error::token_verification_exception on failure

    TokenPayload payload;
    payload.userId = decoded.get_payload_claim("userId").as_string();
    payload.role   = decoded.get_payload_claim("role").as_string();
    return payload;
}

// ============================================================
//  FUNCTION: customerSignupLogic
//
//  Creates a BRAND NEW customer record with login credentials.
//  This is for self-service signup — the customer doesn't need
//  to have been pre-registered by an admin first. We still
//  assign a meter number the same way registerCustomerLogic()
//  does in customer.cpp, so this customer behaves identically
//  to an admin-created one everywhere else in the system.
// ============================================================
AuthResult customerSignupLogic(const string& name, const string& phone,
                                const string& email, const string& password) {
    pqxx::work txn(getConnection());

    // Check email isn't already taken — the UNIQUE constraint
    // on customers.email would catch this anyway, but checking
    // first lets us give a clear error message instead of a
    // raw constraint-violation exception.
    pqxx::result existing = txn.exec(
        "SELECT id FROM customers WHERE email = $1",
        pqxx::params{email}
    );
    if (!existing.empty()) {
        throw runtime_error("An account with this email already exists");
    }

    // Generate meter number the SAME way generateMeterNumber()
    // in customer.cpp does — based on the highest existing
    // numeric suffix, not a row count. Using COUNT(*) here
    // (as an earlier version did) caused exactly the collision
    // bug seen in testing: after any customer is deleted, a
    // new signup could generate an already-used meter number.
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

// ============================================================
//  FUNCTION: customerLoginLogic
// ============================================================
AuthResult customerLoginLogic(const string& email, const string& password) {
    pqxx::work txn(getConnection());

    pqxx::result r = txn.exec(
        "SELECT id, password_hash FROM customers WHERE email = $1",
        pqxx::params{email}
    );
    txn.commit();

    if (r.empty()) {
        // Deliberately the SAME error message as "wrong password"
        // below — never reveal whether an email exists in your
        // system to an unauthenticated caller. This is a real
        // security practice: distinguishing "no such user" from
        // "wrong password" helps attackers enumerate valid emails.
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

// ============================================================
//  FUNCTION: requireOwnerOrAdmin
//
//  THE CORE AUTHORIZATION RULE for customer-scoped routes:
//  - If the caller is an admin, always allow (admins manage
//    everyone).
//  - If the caller is a customer, only allow if their OWN
//    userId matches the customerId in the URL they're trying
//    to access.
//  - Anything else (a customer trying to access someone else's
//    data) is rejected.
// ============================================================
void requireOwnerOrAdmin(const TokenPayload& payload, const string& customerId) {
    if (payload.role == "admin") {
        return;   // admins can access any customer's data
    }
    if (payload.role == "customer" && payload.userId == customerId) {
        return;   // customers can access their OWN data
    }
    throw runtime_error("Forbidden: you do not have access to this customer's data");
}

// ============================================================
//  FUNCTION: requireAdmin
//  For actions ONLY admins should ever perform.
// ============================================================
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