-- Widen mktOrders.orderID from INT UNSIGNED to BIGINT UNSIGNED.
-- With parallel bulk seeding, InnoDB's interleaved auto-increment mode
-- pre-allocates large ID blocks per concurrent session, exhausting the
-- 4-billion INT UNSIGNED ceiling almost immediately across 6 parallel inserts.
-- BIGINT UNSIGNED raises the ceiling to 18 quintillion.
ALTER TABLE mktOrders
    MODIFY COLUMN orderID BIGINT UNSIGNED NOT NULL AUTO_INCREMENT;
