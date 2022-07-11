#!/usr/bin/env python3

import argparse
import os
import re
import sqlite3
import operator
import json


VERBOSE = False


def log(message):
    if VERBOSE:
        print(message)


def parse_args():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser()
    parser.add_argument("-v", "--verbose", action="count", help="verbosity")
    parser.add_argument("-d", "--data_dir", type=str, help="data directory")
    subparsers = parser.add_subparsers(
        dest="subcommand", help="supported subcommands"
    )

    show_parser = subparsers.add_parser("show")

    import_parser = subparsers.add_parser("import")

    up_parser = subparsers.add_parser("up")

    return parser.parse_args()


class Migration:
    def __init__(self, database_dir="persistence", database_name="sqlite.db"):
        try:
            database_path = os.path.join(database_dir, database_name)
            conn = sqlite3.connect(database_path)
        except Exception as e:
            log("Connecting database `{}` fail, {}".format(database_path, e))
            raise e

        self.conn_ = conn
        self.cursor_ = self.conn_.cursor()
        self.database_dir_ = database_dir
        self.database_name_ = database_name

        log("Creating migrations table if not exists")
        self.ensure_migrations_table_exists()

    def ensure_migrations_table_exists(self):
        """Create migration table if not exists."""

        self.cursor_.execute(
            """
            CREATE TABLE IF NOT EXISTS migrations (
                migration_id INTEGER PRIMARY KEY,
                version TEXT NOT NULL UNIQUE,
                description TEXT NOT NULL,
                dirty INTEGER NOT NULL,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)
            """
        )

    def head(self):
        """Read the current version from the database"""

        log("Reading migration head version")
        self.cursor_.execute(
            "SELECT version, description, dirty, created_at FROM migrations"
            " ORDER BY migration_id DESC LIMIT 1"
        )
        row = self.cursor_.fetchone()

        if not row:
            return {
                "version": None,
                "description": None,
                "dirty": None,
                "created_at": None,
            }

        return {
            "version": row[0],
            "description": row[1],
            "dirty": row[2],
            "created_at": row[3],
        }

    def print_head(self):
        head = self.head()

        if not head["version"]:
            print("No version.")
            return

        print(
            "Version: {}, ({} {})".format(
                head["version"], head["created_at"], head["description"]
            )
        )

        if head["dirty"]:
            print("Warning: the migration database is dirty")

    def get_migration_info(self, file_name):
        """Extract the migration info from migration file name."""

        match = re.search(
            "(?P<version>[^_]*)_?(?P<description>.*)\\.sql",
            file_name,
            re.IGNORECASE,
        )

        if not match:
            return {"version": None, "description": None}

        return {
            "version": match.group("version"),
            "description": match.group("description"),
        }

    def should_apply(self, migration_filename: str, target_version=None):
        """Check whether to apply the migration file."""

        head = self.head()
        current_version = head["version"] or ""

        if target_version and target_version == current_version:
            # already on the target version, don't apply the migration
            return False

        # extract the proposed version from the migration filename
        proposed_version = self.get_migration_info(migration_filename)[
            "version"
        ]

        if not proposed_version:
            # does not have a valid version in the name
            return False

        if proposed_version <= current_version:
            return False
        if target_version and proposed_version > target_version:
            return False

        return True

    def apply(self):
        """Migrate to the latest state."""
        head = self.head()
        if head["dirty"]:
            print("Latest version is dirty, please tune manually.")
            return

        migration_filenames = list(
            filter(self.should_apply, os.listdir(self.database_dir_))
        )

        # Sort the migrations based on the version encoded in the filename
        migration_filenames.sort()

        for migration_filename in migration_filenames:
            print("Applying migration " + migration_filename)

            info = self.get_migration_info(migration_filename)

            self.cursor_.execute(
                """INSERT INTO migrations (version, description, dirty)
                                   VALUES (?, ?, 1)""",
                (
                    info["version"],
                    info["description"],
                ),
            )
            self.conn_.commit()

            with open(
                os.path.join(self.database_dir_, migration_filename), "r"
            ) as f:
                sql = f.read()
                self.cursor_.executescript(sql)

            self.cursor_.execute(
                "UPDATE migrations SET dirty = 0 WHERE migration_id = ?",
                (self.cursor_.lastrowid,),
            )
            self.conn_.commit()

    def import_json(self):
        node_group_names = self.find_json_importable_node_and_group_names()
        node_names = self.find_json_importable_node_names()
        # 1.
        # self.import_plugins_json()
        # 2.
        self.import_nodes_json(node_names)
        # 3.
        self.import_settings_json(node_names)
        # 4.
        self.import_groups_json(node_group_names)
        # 5.
        self.import_tags_json(node_group_names)
        # 6.
        self.import_subscritions_json(node_names)

    def import_plugins_json(self):
        """Import data from persistence JSON files."""
        plugin_file = os.path.join(self.database_dir_, "plugins.json")
        if not os.path.exists(plugin_file):
            return
        data = json.load(open(plugin_file, "r"))
        for library in data["plugins"]:
            log("Import library {}".format(library))
            self.cursor_.execute(
                "INSERT INTO plugins (library) VALUES (?)", (library,)
            )
            self.conn_.commit()

    def import_nodes_json(self, node_names):
        """Import nodes data from persistence JSON files."""
        adapter_file = os.path.join(self.database_dir_, "adapters.json")
        if os.path.exists(adapter_file):
            data = json.load(open(adapter_file, "r"))
            for node in data["nodes"]:
                log("Import node {}".format(node))
                self.cursor_.execute(
                    """INSERT INTO nodes (name, type, state, plugin_name)
                                  VALUES (?, ?, ?, ?)""",
                    (
                        node["name"],
                        node["type"],
                        node["state"],
                        node["plugin_name"],
                    ),
                )
            self.conn_.commit()

        for node_name in node_names:
            adapter_file = os.path.join(
                self.database_dir_, "adapters", node_name, "adapter.json"
            )
            if not os.path.exists(adapter_file):
                continue
            data = json.load(open(adapter_file, "r"))
            log("Import node {}".format(data))
            self.cursor_.execute(
                """INSERT INTO nodes (name, type, state, plugin_name)
                              VALUES (?, ?, ?, ?)""",
                (
                    data["name"],
                    data["type"],
                    data["state"],
                    data["plugin_name"],
                ),
            )
            self.conn_.commit()

    def import_settings_json(self, node_names):
        """Import node settings from persistence JSON files."""
        for node_name in node_names:
            settings_file = os.path.join(
                self.database_dir_, "adapters", node_name, "settings.json"
            )

            if not os.path.exists(settings_file):
                continue

            data = open(settings_file, "r").read()
            log("Import setting {}".format(data))
            self.cursor_.execute(
                "INSERT INTO settings (node_name, setting) VALUES(?, ?)",
                (node_name, data),
            )
            self.conn_.commit()

    def import_groups_json(self, node_group_names):
        """Import node groups from persistence JSON files."""
        for node_name, group_name in node_group_names:
            group_file = os.path.join(
                self.database_dir_,
                "adapters",
                node_name,
                "groups",
                group_name,
                "config.json",
            )

            if not os.path.exists(group_file):
                continue

            data = json.load(open(group_file, "r"))
            log("Import group {}".format(data))
            self.cursor_.execute(
                """INSERT INTO groups (driver_name, name, interval)
                               VALUES (?, ?, ?)""",
                (node_name, group_name, data["read_interval"]),
            )
            self.conn_.commit()

    def import_tags_json(self, node_group_names):
        """Import node tags from persistence JSON files."""
        for node_name, group_name in node_group_names:
            tags_file = os.path.join(
                self.database_dir_,
                "adapters",
                node_name,
                "groups",
                group_name,
                "datatags.json",
            )

            if not os.path.exists(tags_file):
                continue

            data = json.load(open(tags_file, "r"))
            for tag in data["tags"]:
                log("Import tag {}".format(tag))
                self.cursor_.execute(
                    """INSERT INTO tags (driver_name, group_name, name, address,
                                         attribute, type, description)
                                 VALUES (?, ?, ?, ?, ?, ?, ?)""",
                    (
                        node_name,
                        group_name,
                        tag["name"],
                        tag["address"],
                        tag["attribute"],
                        tag["type"],
                        tag.get("description", ""),
                    ),
                )
            self.conn_.commit()

    def import_subscritions_json(self, node_names):
        """Import subscriptions from persistence JSON files."""
        for node_name in node_names:
            subscription_file = os.path.join(
                self.database_dir_, "adapters", node_name, "subscriptions.json"
            )
            if not os.path.exists(subscription_file):
                continue
            data = json.load(open(subscription_file, "r"))
            for subscription in data["subscriptions"]:
                log("Import subscription {}".format(subscription))
                self.cursor_.execute(
                    """INSERT INTO subscriptions (
                            app_name, driver_name, group_name
                       ) VALUES(?, ?, ?)""",
                    (
                        subscription["sub_adapter_name"],
                        subscription["src_adapter_name"],
                        subscription["group_config_name"],
                    ),
                )
            self.conn_.commit()

    def find_json_importable_node_names(self):
        """Find node names from persistence directory."""
        adapters_dir = os.path.join(self.database_dir_, "adapters")
        return (
            list(os.listdir(adapters_dir))
            if os.path.exists(adapters_dir)
            else []
        )

    def find_json_importable_node_and_group_names(self):
        """Find node names and group names from persistence directory."""
        res = []
        adapters_dir = os.path.join(self.database_dir_, "adapters")
        if not os.path.exists(adapters_dir):
            return res

        for adapter_name in os.listdir(adapters_dir):
            groups_dir = os.path.join(adapters_dir, adapter_name, "groups")
            if not os.path.exists(groups_dir):
                continue
            for group_name in os.listdir(groups_dir):
                res.append((adapter_name, group_name))

        return res


if __name__ == "__main__":
    args = parse_args()

    VERBOSE = args.verbose

    migrations = Migration(database_dir=args.data_dir)

    if args.subcommand == "import":
        migrations.import_json()
    elif args.subcommand == "show":
        migrations.print_head()
    elif args.subcommand == "up":
        migrations.apply()
