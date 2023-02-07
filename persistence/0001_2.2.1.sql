
BEGIN TRANSACTION;

ALTER TABLE nodes RENAME TO old_nodes;
CREATE TABLE
  nodes (
    name TEXT PRIMARY KEY check(length(name) <= 128),
    type integer(1) NOT NULL check(type IN (1, 2)),
    state integer(1) NOT NULL check(state BETWEEN 1 AND 4),
    plugin_name TEXT NOT NULL check(length(plugin_name) <= 32)
  );
INSERT INTO nodes SELECT * FROM old_nodes;

ALTER TABLE groups RENAME TO old_groups;
CREATE TABLE IF NOT EXISTS
  groups (
    driver_name TEXT NOT NULL,
    name TEXT NULL check(length(name) <= 128),
    interval INTEGER NOT NULL check(interval >= 100),
    UNIQUE (driver_name, name),
    FOREIGN KEY (driver_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE
  );
INSERT INTO groups SELECT * FROM old_groups;

ALTER TABLE tags RENAME TO old_tags;
CREATE TABLE IF NOT EXISTS
  tags (
    driver_name TEXT NOT NULL,
    group_name TEXT NOT NULL,
    name TEXT NULL check(length(name) <= 64),
    address TEXT NULL check(length(address) <= 128),
    attribute INTEGER NOT NULL check(attribute BETWEEN 0 AND 7),
    precision INTEGER NOT NULL check(precision BETWEEN 0 AND 17),
    decimal REAL NOT NULL,
    type INTEGER NOT NULL check(type BETWEEN 0 AND 19),
    description TEXT NULL check(length(description) <= 128),
    UNIQUE (driver_name, group_name, name),
    FOREIGN KEY (driver_name, group_name) REFERENCES groups (driver_name, name) ON UPDATE CASCADE ON DELETE CASCADE
  );
INSERT INTO tags SELECT * FROM old_tags;

ALTER TABLE subscriptions RENAME TO old_subscriptions;
CREATE TABLE IF NOT EXISTS
  subscriptions (
    app_name TEXT NOT NULL,
    driver_name TEXT NOT NULL,
    group_name TEXT NOT NULL,
    CHECK (app_name != driver_name),
    UNIQUE (app_name, driver_name, group_name),
    FOREIGN KEY (app_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE,
    FOREIGN KEY (driver_name, group_name) REFERENCES groups (driver_name, name) ON UPDATE CASCADE ON DELETE CASCADE
  );
INSERT INTO subscriptions SELECT * FROM old_subscriptions;

ALTER TABLE settings RENAME TO old_settings;
CREATE TABLE IF NOT EXISTS
  settings (
    node_name TEXT NOT NULL,
    setting TEXT NOT NULL,
    UNIQUE (node_name),
    FOREIGN KEY (node_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE
  );
INSERT INTO settings SELECT * FROM old_settings;
COMMIT;