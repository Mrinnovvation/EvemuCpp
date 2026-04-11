#!/bin/bash
# Parallel market seeder.
# Spawns one MySQL connection per region concurrently (up to MAX_PARALLEL at a time).
# Replaces the sequential evedbtool seed command.
#
# Environment variables (all inherited from docker-compose):
#   SEED_REGIONS     — comma-separated region names
#   SEED_SATURATION  — integer 0-100 (default 100)
#   SEED_PARALLEL    — max concurrent connections (default 6)
#   MARIADB_*        — connection parameters

MARIADB_HOST="${MARIADB_HOST:-db}"
MARIADB_DATABASE="${MARIADB_DATABASE:-evemu}"
MARIADB_USER="${MARIADB_USER:-evemu}"
MARIADB_PORT="${MARIADB_PORT:-3306}"
SATURATION="${SEED_SATURATION:-100}"
MAX_PARALLEL="${SEED_PARALLEL:-3}"

# Use MYSQL_PWD to avoid the "password on command line" warning
export MYSQL_PWD="${MARIADB_PASSWORD:-evemu}"

MYSQL_BASE="mysql -h ${MARIADB_HOST} -P ${MARIADB_PORT} -u ${MARIADB_USER} ${MARIADB_DATABASE}"

# Convert saturation percentage to decimal (e.g. 100 -> 1.0000)
SAT_DECIMAL=$(awk "BEGIN { printf \"%.4f\", ${SATURATION} / 100 }")

# ── Skip if already seeded ────────────────────────────────────────────────────
existing=$(${MYSQL_BASE} -sN -e "SELECT COUNT(*) FROM mktOrders LIMIT 1;" 2>/dev/null)
if [ "${existing:-0}" -gt 0 ]; then
    echo "[SEED] Market already seeded (${existing} orders present), skipping."
    exit 0
fi

# ── Per-region seed function (runs in a subshell via xargs) ──────────────────
seed_region() {
    local region_name="$1"

    # Escape any single quotes in the name (region names don't have them, but be safe)
    local safe_name="${region_name//\'/\'\'}"

    local regionid
    regionid=$(mysql -h "${MARIADB_HOST}" -P "${MARIADB_PORT}" -u "${MARIADB_USER}" "${MARIADB_DATABASE}" \
        -sN -e "SELECT regionID FROM mapRegions WHERE regionName = '${safe_name}' LIMIT 1;" 2>/dev/null)

    if [ -z "$regionid" ]; then
        echo "[SEED] WARNING: '${region_name}' not found in mapRegions, skipping."
        return 0
    fi

    echo "[SEED] Starting : ${region_name} (regionID=${regionid})"

    mysql -h "${MARIADB_HOST}" -P "${MARIADB_PORT}" -u "${MARIADB_USER}" "${MARIADB_DATABASE}" \
        <<SQL
-- Select a random subset of stations for this region based on saturation
SET @lim = (SELECT ROUND(COUNT(stationID) * ${SAT_DECIMAL}) FROM staStations WHERE regionID = ${regionid});
SET @i = 0;

CREATE TEMPORARY TABLE tStations (
    stationID    INT,
    solarSystemID INT,
    regionID     INT,
    corporationID INT,
    security     FLOAT
);

INSERT INTO tStations
    SELECT stationID, solarSystemID, regionID, corporationID, security
    FROM   staStations
    WHERE  (@i := @i + 1) <= @lim
      AND  regionID = ${regionid}
    ORDER BY RAND();

-- Seed all published items into every selected station for this region
INSERT INTO mktOrders
    (typeID, ownerID, regionID, stationID, price,
     volEntered, volRemaining, issued, minVolume, duration, solarSystemID, jumps)
SELECT
    t.typeID,
    s.corporationID,
    s.regionID,
    s.stationID,
    IF(t.basePrice = 0 OR s.security <= 0, 100, t.basePrice / s.security),
    550, 550, 132478179209572976, 1, 250,
    s.solarSystemID, 1
FROM   tStations s
CROSS  JOIN invTypes t
INNER  JOIN invGroups g ON t.groupID = g.groupID
WHERE  t.published = 1
  AND  g.categoryID IN (4, 5, 6, 7, 8, 9, 16, 17, 18, 22, 23, 24, 25, 32, 34, 35, 39, 40, 41, 42, 43, 46);
SQL

    if [ $? -eq 0 ]; then
        echo "[SEED] Done     : ${region_name}"
    else
        echo "[SEED] FAILED   : ${region_name}"
        return 1
    fi
}

export -f seed_region
export MARIADB_HOST MARIADB_DATABASE MARIADB_USER MARIADB_PORT MYSQL_PWD SAT_DECIMAL

# ── Parse and run ─────────────────────────────────────────────────────────────
IFS=',' read -r -a REGION_ARRAY <<< "${SEED_REGIONS:-}"

if [ ${#REGION_ARRAY[@]} -eq 0 ]; then
    echo "[SEED] SEED_REGIONS is empty, nothing to seed."
    exit 0
fi

echo "[SEED] Seeding ${#REGION_ARRAY[@]} regions with up to ${MAX_PARALLEL} parallel connections..."
echo "[SEED] Saturation: ${SATURATION}% (factor ${SAT_DECIMAL})"
START=$(date +%s)

# Feed region names through xargs for a proper parallel job pool.
# -d '\n'   : treat each newline-delimited line as one argument (GNU xargs)
# -I{}      : substitute {} with the region name
# -P N      : run up to N jobs in parallel
printf '%s\n' "${REGION_ARRAY[@]}" \
    | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' \
    | grep -v '^$' \
    | xargs -d '\n' -I{} -P"${MAX_PARALLEL}" bash -c 'seed_region "$@"' _ {}

ELAPSED=$(( $(date +%s) - START ))
echo "[SEED] All regions seeded in ${ELAPSED}s."
