-- Restore the EVE XML API server (src/eve-server/apiserver).
-- Holds the per-account API key pair the API uses to authenticate queries.
-- userID is the API-facing account identifier and is NOT the same as accountID.

-- +migrate Up
CREATE TABLE IF NOT EXISTS accountApi
(
    userID      INT(10) UNSIGNED NOT NULL AUTO_INCREMENT,
    accountID   INT(10) UNSIGNED NOT NULL,
    fullKey     CHAR(64) NOT NULL DEFAULT '',
    limitedKey  CHAR(64) NOT NULL DEFAULT '',
    apiRole     INT(10) UNSIGNED NOT NULL DEFAULT 100,
    PRIMARY KEY (userID),
    UNIQUE KEY accountID (accountID)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- +migrate Down
DROP TABLE accountApi;
