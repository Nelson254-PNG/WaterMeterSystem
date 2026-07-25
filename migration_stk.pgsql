CREATE TABLE pending_stk_payments(
  id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
  checkout_request_id VARCHAR(100) UNIQUE NOT NULL,
  merchant_request_id VARCHAR(100) NOT NULL,
  customer_id UUID NOT NULL REFERENCES customers(id) ON DELETE CASCADE,
  bill_id UUID NOT NULL REFERENCES bills(id) ON DELETE CASCADE,
  amount NUMERIC(12, 2) NOT NULL,
  status VARCHAR(20) NOT NULL DEFAULT 'pending',
  -- 'pending' - waiting for callback, 'completed' - payment confirmed, bill updated, 'failed' - customer cancelled or wrong pin
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  completed_at TIMESTAMPTZ 
);
-- INDEX FOR FAST LOOKUP WHEN CALLBACK ARRIVES
CREATE INDEX idx_pending_stk_checkout ON pending_stk_payments(checkout_request_id);