-- Adds composite indexes to mktOrders to speed up market browser queries.
-- GetOrders uses (regionID, typeID, bid); GetRegionBest uses (regionID, bid).
-- Also adds the solarSystemID index in case the earlier extensionless
-- migration file (202406242143-mktOrderIndex) was never applied.
-- +migrate Up
ALTER TABLE mktOrders
    ADD INDEX IF NOT EXISTS idx_solar_system   (solarSystemID),
    ADD INDEX IF NOT EXISTS idx_region_type_bid (regionID, typeID, bid),
    ADD INDEX IF NOT EXISTS idx_region_bid      (regionID, bid);
-- +migrate Down
ALTER TABLE mktOrders
    DROP INDEX IF EXISTS idx_solar_system,
    DROP INDEX IF EXISTS idx_region_type_bid,
    DROP INDEX IF EXISTS idx_region_bid;
