#pragma once
// ============================================================
//  auth.h
//  Password hashing, JWT token creation/verification, and the
//  signup/login worker functions used by BOTH the CLI (if you
//  ever want admin login there) and the API routes.
// ============================================================

#include <string>
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