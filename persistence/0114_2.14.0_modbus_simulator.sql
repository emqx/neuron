CREATE TABLE IF NOT EXISTS modbus_tcp_simulator (
  id INTEGER PRIMARY KEY CHECK (id = 1),
  enabled INTEGER NOT NULL DEFAULT 0,
  tags_json TEXT NOT NULL DEFAULT '[]',
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

DROP TABLE IF EXISTS old_nodes;
ALTER TABLE nodes RENAME TO old_nodes;
CREATE TABLE
  nodes (
    name TEXT PRIMARY KEY check(length(name) <= 128),
    type integer(1) NOT NULL check(type IN (1, 2)),
    state integer(1) NOT NULL check(state BETWEEN 1 AND 4),
    plugin_name TEXT NOT NULL check(length(plugin_name) <= 32),
    tags TEXT NULL check(length(tags) <= 256)
  );
INSERT INTO
  nodes (name, type, state, plugin_name, tags)
SELECT
  name, type, state, plugin_name, ''
FROM
  old_nodes;

DROP TABLE IF EXISTS old_nodes;