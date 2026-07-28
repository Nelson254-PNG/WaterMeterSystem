-- add iot columns to customers table
ALTER TABLE customers
  ADD COLUMN valve_open  BOOLEAN NOT NULL DEFAULT true,
  ADD COLUMN prepaid_credit NUMERIC(12, 2) NOT NULL DEFAULT 0.00,
  ADD COLUMN meter_mode VARCHAR(10) NOT NULL DEFAULT 'postpaid';

-- iot table reading_date
CREATE TABLE iot_readings(
  id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
  customer_id UUID NOT NULL REFERENCES customers(id) ON DELETE CASCADE,
  reading_date NUMERIC(10,3) NOT NULL,
  flow_rate NUMERIC(8,4) NOT NULL DEFAULT 0,
  signal_strength INTEGER,
  battery_level INTEGER,
  transmitted_at TIMESTAMPTZ NOT NULL DEFAULT now()
); 

CREATE TABLE alerts(
  id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
  customer_id UUID NOT NULL REFERENCES customers(id) on DELETE CASCADE,
  alert_type VARCHAR(30) NOT NULL,
  message TEXT NOT NULL,
  severity VARCHAR(10) NOT NULL DEFAULT 'warning',
  resolved BOOLEAN NOT NULL DEFAULT false,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  resolved_at TIMESTAMPTZ
);

CREATE INDEX idx_alerts_customer ON alerts(customer_id, created_at DESC);
CREATE INDEX idx_alerts_unresolved ON alerts(resolved) WHERE resolved = false;

--prepaid token table
CREATE TABLE prepaid_tokens(
  id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
  customer_id UUID NOT NULL REFERENCES customers(id) ON DELETE CASCADE,
  token_code VARCHAR(20) UNIQUE NOT NULL,
  amount_kes NUMERIC(12,2) NOT NULL,
  units_m3 NUMERIC(8,3) NOT NULL,
  purchase_date TIMESTAMPTZ NOT NULL DEFAULT now(),
  redeemed BOOLEAN NOT NULL DEFAULT false,
  redeemed_at TIMESTAMPTZ
);
