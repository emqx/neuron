BEGIN TRANSACTION;

ALTER TABLE tags RENAME TO temp_tags;

-- add bias column
CREATE TABLE
  IF NOT EXISTS tags (
    driver_name TEXT NOT NULL,
    group_name TEXT NOT NULL,
    name TEXT NULL check (length (name) <= 128),
    address TEXT NULL check (length (address) <= 128),
    attribute INTEGER NOT NULL check (attribute BETWEEN 0 AND 15),
    precision INTEGER NOT NULL check (precision BETWEEN 0 AND 17),
    decimal REAL NOT NULL,
    bias REAL NOT NULL check (bias BETWEEN -1000 AND 1000),
    type INTEGER NOT NULL check (type BETWEEN 0 AND 40),
    description TEXT NULL check (length (description) <= 512),
    value TEXT,
    UNIQUE (driver_name, group_name, name),
    FOREIGN KEY (driver_name, group_name) REFERENCES groups (driver_name, name) ON UPDATE CASCADE ON DELETE CASCADE
  );

INSERT INTO tags SELECT * FROM temp_tags;
DROP TABLE temp_tags;
COMMIT;
