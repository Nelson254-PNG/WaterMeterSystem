
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE TABLE customers (
    id              UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name            VARCHAR(100)    NOT NULL,
    meter_number    VARCHAR(20)     NOT NULL UNIQUE,
    phone           VARCHAR(20)     NOT NULL,
    balance         NUMERIC(12,2)   NOT NULL DEFAULT 0.00,
    last_reading    NUMERIC(12,2)   NOT NULL DEFAULT 0.00,
    created_at      TIMESTAMPTZ     NOT NULL DEFAULT now()
);

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

CREATE UNIQUE INDEX unique_mpesa_reference
    ON payments (reference)
    WHERE method = 'M-Pesa';

CREATE INDEX idx_water_records_customer ON water_records(customer_id);
CREATE INDEX idx_bills_customer          ON bills(customer_id);
CREATE INDEX idx_payments_customer       ON payments(customer_id);
CREATE INDEX idx_payments_bill           ON payments(bill_id);
CREATE INDEX idx_customers_meter_number  ON customers(meter_number);