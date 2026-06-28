#pragma once
// ============================================================
//  auth.h
//  Password hashing, JWT token creation/verification, and the
//  signup/login worker functions used by BOTH the CLI (if you
//  ever want admin login there) and the API routes.
// ============================================================

#include <string>
#include <stdexcept>   
using namespace std;

// ── PASSWORD HASHING ───────────────────────────────────────────
// We use SHA-256 with a random salt via OpenSSL — NOT real
// bcrypt (which needs a separate library MSYS2 doesn't package
// cleanly), but the same core principle: never store the raw
// password, and make brute-forcing expensive by adding a salt
// unique to each user so precomputed "rainbow tables" don't work.
//
// hashPassword(): generates a random salt, hashes password+salt,
// returns "salt:hash" as one string to store in the database.
string hashPassword(const string& plainPassword);

// verifyPassword(): re-hashes the given password with the SAME
// salt stored from before, and checks if it matches.
bool verifyPassword(const string& plainPassword, const string& storedHash);

// ── JWT TOKENS ─────────────────────────────────────────────────
// Creates a signed token containing the customer's UUID and
// a "role" claim ("customer" or "admin") — protected routes
// read this claim to decide what the requester is allowed to do.
string createToken(const string& userId, const string& role);

// Decodes and verifies a token. Throws if invalid, tampered,
// or expired. Returns the userId and role on success.
struct TokenPayload {
    string userId;
    string role;
};
TokenPayload verifyToken(const string& token);

// ── SIGNUP / LOGIN WORKERS ─────────────────────────────────────
// Customer signup: creates the email/password credentials on
// an EXISTING customer row (matched by meterNumber + phone,
// since the customer already exists from when admin registered
// them) OR creates a brand new customer record if none exists
// yet with the simplified "self-registration" fields.
struct AuthResult {
    string token;
    string userId;
    string role;
};

AuthResult customerSignupLogic(const string& name, const string& phone,
                                const string& email, const string& password);

AuthResult customerLoginLogic(const string& email, const string& password);

AuthResult adminLoginLogic(const string& username, const string& password);

// ── ROUTE PROTECTION HELPERS ───────────────────────────────────
// These are called at the TOP of every protected route handler.

// Extracts and verifies the token from a Crow request's
// Authorization header (expected format: "Bearer <token>").
// Throws runtime_error with a clear message if missing/invalid —
// the route's catch block turns that into a 401 response.
//
// We can't include crow_all.h here without dragging Crow into
// every file that includes auth.h, so this is declared as a
// template taking any object with a get_header_value(string)
// method — crow::request satisfies this without us needing
// to name its exact type.
template <typename RequestType>
TokenPayload requireAuth(const RequestType& req) {
    string authHeader = req.get_header_value("Authorization");

    const string prefix = "Bearer ";
    if (authHeader.size() <= prefix.size() || authHeader.substr(0, prefix.size()) != prefix) {
        throw runtime_error("Missing or malformed Authorization header");
    }

    string token = authHeader.substr(prefix.size());
    return verifyToken(token);   // throws if invalid/expired
}

// Checks that the authenticated caller is EITHER an admin
// (who can act on any customer) OR a customer acting on
// THEIR OWN customerId. Throws if neither is true.
//
// This is the core rule that stops customer A from viewing
// or modifying customer B's data just by changing the UUID
// in the URL.
void requireOwnerOrAdmin(const TokenPayload& payload, const string& customerId);

// Checks the caller is specifically an admin — used for
// admin-only actions like deleting a customer.
void requireAdmin(const TokenPayload& payload);