-- ============================================================
--  migration_auth.sql
--  Adds authentication support to the existing customers table.
--  Run this ONCE against your existing watermeter_system database.
-- ============================================================

-- ── Add credential columns to customers ───────────────────────
-- password_hash stores a BCRYPT HASH, never the real password.
-- email is used as the login identifier (you could use phone
-- instead — we picked email since it's simpler to validate
-- uniqueness on, but phone works the same way).
ALTER TABLE customers
    ADD COLUMN email VARCHAR(150) UNIQUE,
    ADD COLUMN password_hash VARCHAR(255);

-- Existing customers (created via the admin app, no password)
-- will have NULL here until/unless they sign up for app access.
-- This is intentional: not every customer needs app access —
-- some may only ever be managed by an admin directly.

-- ============================================================
--  TABLE: admin_users
--  Completely separate from customers — admins are STAFF,
--  not customers. Kept in their own table so a compromised
--  customer account can never accidentally have admin rights,
--  and so admin login logic stays simple (no need to check
--  "is this customer also an admin?" anywhere).
-- ============================================================
CREATE TABLE admin_users (
    id              UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    username        VARCHAR(50) UNIQUE NOT NULL,
    password_hash   VARCHAR(255) NOT NULL,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Create your first admin account manually after running this
-- migration — see the instructions in auth.cpp for how to
-- generate a proper password hash to insert here.