# Smart Water Meter & Payment System — Backend API

> C++ REST API backend for the Smart Water Meter & Payment System.  
> Part of a three-repo full-stack portfolio project.

**Related repos:**

 [WaterMeterApp](https://github.com/Nelson254-PNG/WaterMeterApp) — Admin mobile app (React Native)
 [CustomerApp](https://github.com/Nelson254-PNG/CustomerApp) — Customer mobile app (React Native)

## What This Does

A production-style C++ HTTP server that handles all business logic for a water utility:

 **Customer management** — register, list, search, delete
 **Meter reading & billing** — record water usage, generate bills with 4-tier KES pricing
 **Payments** — Cash, M-Pesa Paybill, M-Pesa Till Number, Bank Transfer
 **Authentication** — JWT-based login for admin and customer roles
 **Authorization** — route-level protection (admin-only vs owner-or-admin)
 **M-Pesa duplicate prevention** — enforced by a PostgreSQL partial unique index, not application code


## Tech Stack

 Component                Technology
 Language           -     C++17 (g++ 16.1, MSYS2 UCRT64) 
 HTTP Framework     -     [Crow](https://crowcpp.org) (single-header) 
 Database           -     PostgreSQL 16 
 DB Client          -     libpqxx 7.10 
 Authentication     -     [jwt-cpp](https://github.com/Thalhammer/jwt-cpp) + OpenSSL 
 Password Hashing   -     SHA-256 + random salt (OpenSSL `RAND_bytes`) 

## Architecture

api_main.cpp           ← entry point; wires all route modules
auth.h/.cpp            ← password hashing, JWT creation/verification
auth_routes.h/.cpp     ← POST /auth/signup, /login, /admin-login
customer.h/.cpp        ← customer business logic (worker functions)
customer_routes.h/.cpp ← HTTP handlers for /customers
usage.h/.cpp           ← water reading logic
usage_routes.h/.cpp    ← HTTP handlers for /customers/:id/usage
billing.h/.cpp         ← tiered billing calculation
billing_routes.h/.cpp  ← HTTP handlers for /customers/:id/bills
payment.h/.cpp         ← payment processing (all methods)
payment_routes.h/.cpp  ← HTTP handlers for /customers/:id/payments
reports.h/.cpp         ← dashboard & statements (CLI)
db.h / db.cpp          ← shared PostgreSQL connection singleton
constants.h            ← billing rates, M-Pesa numbers
schema.sql             ← full database schema
migration_auth.sql     ← adds email, password_hash, admin_users table

## API Endpoints

All protected routes require: `Authorization: Bearer <token>`

### Auth (no token required)

POST /auth/signup { name, phone, email, password }
POST /auth/login { email, password }
POST /auth/admin-login { username, password }


### Customers

GET /customers [admin] List all
POST /customers [admin] Register
GET /customers/:id [owner/admin] Get profile
DELETE /customers/:id [admin] Delete + cascade
GET /customers/search?name= [admin] Search by name

```

### Usage, Billing, Payments
```

POST /customers/:id/usage [admin] Record reading
GET /customers/:id/usage [owner/admin] View history
POST /customers/:id/bills [admin] Generate bill
GET /customers/:id/bills [owner/admin] View bills
POST /customers/:id/payments [owner/admin] Cash/Bank payment
POST /customers/:id/payments/mpesa [owner/admin] M-Pesa Paybill
POST /customers/:id/payments/mpesa-till[owner/admin] M-Pesa Till
GET /customers/:id/payments [owner/admin] View history

````

---

## Billing Model

 Tier        Units (m³)      Rate (KES/m³)
  1          0 – 6           50.00 
  2          7 – 20          75.00 
  3          21 – 50         100.00 
  4          51+             150.00 

Plus a fixed **KES 200 service charge** per bill.


## Setup

### Prerequisites
# MSYS2 UCRT64 terminal
pacman -S mingw-w64-ucrt-x86_64-libpqxx
pacman -S mingw-w64-ucrt-x86_64-openssl
pacman -S mingw-w64-ucrt-x86_64-asio

### Database

```sql
CREATE DATABASE watermeter_system;
-- then run schema.sql, then migration_auth.sql
```

### Configure

Edit `db.cpp`:

```cpp
static const string CONNECTION_STRING =
    "dbname=watermeter_system user=postgres password=YOUR_PW host=127.0.0.1 port=5432";
```

Edit `constants.h`:

```cpp
const std::string MPESA_PAYBILL_NUMBER = "YOUR_PAYBILL";
const std::string MPESA_TILL_NUMBER    = "YOUR_TILL";
```

### Build & Run

```in the bash terminal run
g++ -std=c++17 -I include api_main.cpp auth_routes.cpp customer_routes.cpp usage_routes.cpp \
    billing_routes.cpp payment_routes.cpp auth.cpp customer.cpp usage.cpp billing.cpp payment.cpp db.cpp \
    -o api_main.exe -lpqxx -lpq -lssl -lcrypto -lws2_32 -lwsock32

./api_main.exe   # starts on port 8090
```

### Create First Admin

```in the bash terminal run
g++ -std=c++17 -I include create_admin_hash.cpp auth.cpp db.cpp \
    -o create_admin_hash.exe -lpqxx -lpq -lssl -lcrypto
./create_admin_hash.exe
# Paste the printed SQL into your database
```


## Security Highlights
- **No SQL injection possible** — parameterized queries (`pqxx::params{}`) used exclusively
- **Passwords never stored plain** — SHA-256 + per-user random salt via `RAND_bytes()`
- **Duplicate M-Pesa codes** — rejected by a PostgreSQL partial unique index, not application code
- **JWT tokens** — HS256-signed, 7-day expiry, role claims (`admin`/`customer`)
- **Atomic transactions** — bill generation and payment recording are single-transaction operations

## Notable Technical Challenges Solved

Problem                                          Solution
MSYS2 UCRT64 vs MINGW64 DLL mismatch           - Lock shell to UCRT64 via `MSYSTEM=UCRT64`
std::byte vs windows.h byte macro conflict     - #define byte win_byte_override before OpenSSL includes 
PostgreSQL operator ambiguity on `$7 - $6`     - Compute `balance_after` in C++ before the query          
Meter number collision after customer deletion - Use `MAX(suffix)` instead of `COUNT(*)                  |
Crow `HTTPMethod::POST` not found              - Crow version uses `Post`/`Get` casing, not `POST`/`GET`
