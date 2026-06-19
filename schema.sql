-- ============================================================
--  Smart Water Meter & Payment System — Database Schema
--  PostgreSQL
--
--  Run this ONCE against a fresh database, in order, top to
--  bottom. Each section is commented to explain WHY it exists,
--  mirroring the structs you already built in C++.
-- ============================================================

-- ── STEP 0: Enable UUID generation ───────────────────────────
-- PostgreSQL doesn't generate UUIDs natively without this.
-- uuid_generate_v4() creates a random 128-bit unique ID,
-- e.g. 'a3f1c2e4-8b9d-4f1a-9c3e-7d2b1a0f5e6c'
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";


-- ============================================================
--  TABLE: customers
--  Equivalent to your `struct Customer` (minus the nested
--  vectors — those become separate tables below, linked by
--  customer_id).
-- ============================================================
CREATE TABLE customers (
    id              UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name            VARCHAR(100)    NOT NULL,
    meter_number    VARCHAR(20)     NOT NULL UNIQUE,
    phone           VARCHAR(20)     NOT NULL,
    balance         NUMERIC(12,2)   NOT NULL DEFAULT 0.00,
    last_reading    NUMERIC(12,2)   NOT NULL DEFAULT 0.00,
    created_at      TIMESTAMPTZ     NOT NULL DEFAULT now()
);

-- Why these choices:
--   UUID PRIMARY KEY        -> matches your decision: random,
--                              collision-safe across devices
--   meter_number UNIQUE     -> enforces what your generateMeterNumber()
--                              function assumed but never checked
--   NUMERIC(12,2)           -> exact decimal math for money/units;
--                              avoids floating point rounding bugs
--                              you handled manually with the 0.01
--                              tolerance trick in C++
--   created_at TIMESTAMPTZ  -> automatic audit trail; you didn't
--                              have this in the CLI version


-- ============================================================
--  TABLE: water_records
--  Equivalent to your `struct WaterRecord`.
--  Each row belongs to exactly ONE customer.
-- ============================================================
CREATE TABLE water_records (
    id                  UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    customer_id         UUID            NOT NULL REFERENCES customers(id) ON DELETE CASCADE,
    reading_date        DATE            NOT NULL,
    previous_reading    NUMERIC(12,2)   NOT NULL,
    current_reading     NUMERIC(12,2)   NOT NULL,
    units_used          NUMERIC(12,2)   NOT NULL,
    billed              BOOLEAN         NOT NULL DEFAULT false,
    created_at          TIMESTAMPTZ     NOT NULL DEFAULT now()
);

-- Why these choices:
--   customer_id REFERENCES customers(id)
--       -> THE FOREIGN KEY. This replaces your nested
--          vector<WaterRecord> records inside Customer.
--          Every record "points back" to its owner.
--   ON DELETE CASCADE
--       -> if a customer is ever deleted, their records
--          are deleted too — no orphaned data left behind
--   DATE instead of VARCHAR
--       -> you stored dates as strings in C++ ("2024-06-14").
--          Postgres has a real DATE type: sorts correctly,
--          validates format automatically, supports date math


-- ============================================================
--  TABLE: bills
--  Equivalent to your `struct Bill`.
-- ============================================================
CREATE TABLE bills (
    id              UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    customer_id     UUID            NOT NULL REFERENCES customers(id) ON DELETE CASCADE,
    issue_date      DATE            NOT NULL,
    due_date        DATE            NOT NULL,
    total_units     NUMERIC(12,2)   NOT NULL,
    tier1_units     NUMERIC(12,2)   NOT NULL DEFAULT 0,
    tier2_units     NUMERIC(12,2)   NOT NULL DEFAULT 0,
    tier3_units     NUMERIC(12,2)   NOT NULL DEFAULT 0,
    tier4_units     NUMERIC(12,2)   NOT NULL DEFAULT 0,
    tier1_cost      NUMERIC(12,2)   NOT NULL DEFAULT 0,
    tier2_cost      NUMERIC(12,2)   NOT NULL DEFAULT 0,
    tier3_cost      NUMERIC(12,2)   NOT NULL DEFAULT 0,
    tier4_cost      NUMERIC(12,2)   NOT NULL DEFAULT 0,
    service_charge  NUMERIC(12,2)   NOT NULL DEFAULT 200.00,
    total_amount    NUMERIC(12,2)   NOT NULL,
    amount_paid     NUMERIC(12,2)   NOT NULL DEFAULT 0.00,
    paid            BOOLEAN         NOT NULL DEFAULT false,
    created_at      TIMESTAMPTZ     NOT NULL DEFAULT now()
);

-- This is a direct, near 1-to-1 mapping of your Bill struct.
-- Nothing conceptually new here — same fields, same meaning.


-- ============================================================
--  TABLE: payments
--  Equivalent to your `struct Payment`.
--  Notice TWO foreign keys: one to the customer, one to the
--  specific bill being paid. Your C++ Payment struct only
--  stored billId as a plain int; here it's an actual
--  enforced relationship.
-- ============================================================
CREATE TABLE payments (
    id                  UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    customer_id         UUID            NOT NULL REFERENCES customers(id) ON DELETE CASCADE,
    bill_id             UUID            NOT NULL REFERENCES bills(id)     ON DELETE CASCADE,
    payment_date        DATE            NOT NULL,
    method              VARCHAR(20)     NOT NULL,   -- 'Cash', 'M-Pesa', 'Bank Transfer'
    reference           VARCHAR(50)     NOT NULL,   -- M-Pesa code, or 'N/A' for cash
    amount_paid         NUMERIC(12,2)   NOT NULL,
    balance_before       NUMERIC(12,2)   NOT NULL,
    balance_after        NUMERIC(12,2)   NOT NULL,
    created_at          TIMESTAMPTZ     NOT NULL DEFAULT now()
);

-- Why bill_id REFERENCES bills(id):
--   Your C++ findCustomerById() + manual loop to find a Bill
--   by billId is replaced by this constraint — the database
--   itself guarantees you can never record a payment against
--   a bill that doesn't exist.


-- ============================================================
--  CONSTRAINT: enforce unique M-Pesa codes system-wide
--
--  This directly replaces your isCodeAlreadyUsed() function!
--  Instead of looping through every customer's payments in
--  C++ every time, the database enforces this automatically,
--  instantly, for every insert — no application code needed.
-- ============================================================
CREATE UNIQUE INDEX unique_mpesa_reference
    ON payments (reference)
    WHERE method = 'M-Pesa';

-- "WHERE method = 'M-Pesa'" is a PARTIAL INDEX — the uniqueness
-- rule only applies to M-Pesa rows. Cash payments can all share
-- reference = 'N/A' without conflict.


-- ============================================================
--  INDEXES for fast lookups
--  Your C++ findCustomerById() did a linear search (O(n)) —
--  fine for a CLI demo, too slow for a real app with thousands
--  of customers. These indexes make lookups by foreign key
--  near-instant, the same job a database index always does.
-- ============================================================
CREATE INDEX idx_water_records_customer ON water_records(customer_id);
CREATE INDEX idx_bills_customer          ON bills(customer_id);
CREATE INDEX idx_payments_customer       ON payments(customer_id);
CREATE INDEX idx_payments_bill           ON payments(bill_id);
CREATE INDEX idx_customers_meter_number  ON customers(meter_number);