BEGIN TRANSACTION;

DROP TABLE IF EXISTS temp_subscriptions;
ALTER TABLE subscriptions RENAME TO temp_subscriptions;

CREATE TABLE
  IF NOT EXISTS subscriptions (
    app_name TEXT NOT NULL,
    driver_name TEXT NOT NULL,
    group_name TEXT NOT NULL,
    params TEXT DEFAULT NULL,
    static_tags TEXT DEFAULT NULL,
    CHECK (app_name != driver_name),
    UNIQUE (app_name, driver_name, group_name),
    FOREIGN KEY (app_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE,
    FOREIGN KEY (driver_name, group_name) REFERENCES groups (driver_name, name) ON UPDATE CASCADE ON DELETE CASCADE
  );
INSERT INTO subscriptions SELECT app_name, driver_name, group_name, params, NULL FROM temp_subscriptions;

DROP TABLE temp_subscriptions;

COMMIT;