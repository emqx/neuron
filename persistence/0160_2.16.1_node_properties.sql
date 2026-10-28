CREATE TABLE node_properties (
    node_name TEXT NOT NULL,
    key TEXT NOT NULL CHECK(length(key) > 0),
    value_json TEXT NOT NULL,
    PRIMARY KEY (node_name, key),
    FOREIGN KEY (node_name) REFERENCES nodes(name)
        ON UPDATE CASCADE ON DELETE CASCADE
);
