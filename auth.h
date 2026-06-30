#pragma once
#include <string>
#include <stdexcept>   
using namespace std;

string hashPassword(const string& plainPassword);

bool verifyPassword(const string& plainPassword, const string& storedHash);

string createToken(const string& userId, const string& role);

struct TokenPayload {
    string userId;
    string role;
};
TokenPayload verifyToken(const string& token);
struct AuthResult {
    string token;
    string userId;
    string role;
};

AuthResult customerSignupLogic(const string& name, const string& phone,
                                const string& email, const string& password);

AuthResult customerLoginLogic(const string& email, const string& password);

AuthResult adminLoginLogic(const string& username, const string& password);

template <typename RequestType>
TokenPayload requireAuth(const RequestType& req) {
    string authHeader = req.get_header_value("Authorization");

    const string prefix = "Bearer ";
    if (authHeader.size() <= prefix.size() || authHeader.substr(0, prefix.size()) != prefix) {
        throw runtime_error("Missing or malformed Authorization header");
    }

    string token = authHeader.substr(prefix.size());
    return verifyToken(token);   
}
void requireOwnerOrAdmin(const TokenPayload& payload, const string& customerId);

void requireAdmin(const TokenPayload& payload);