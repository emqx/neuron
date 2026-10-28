#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "errcodes.h"
#include "persist/persist.h"
#include "persist/sqlite.h"
#include "utils/log.h"

zlog_category_t *neuron = nullptr;

namespace {

neu_node_property_t property(const char *key, const char *value)
{
    return { const_cast<char *>(key), const_cast<char *>(value) };
}

struct Properties {
    neu_node_property_t *items = nullptr;
    size_t               n     = 0;

    ~Properties() { neu_node_properties_free(items, n); }
};

class NodePropertyPersist : public testing::Test {
protected:
    int         cwd = -1;
    std::string directory;
    bool        opened = false;
    sqlite3 *   db     = nullptr;

    void SetUp() override
    {
        cwd = open(".", O_RDONLY | O_DIRECTORY);
        ASSERT_GE(cwd, 0);
        char path[] = "/tmp/neuron-property-XXXXXX";
        ASSERT_NE(nullptr, mkdtemp(path));
        directory = path;
        ASSERT_EQ(0, mkdir((directory + "/persistence").c_str(), 0700));
        ASSERT_EQ(0, mkdir((directory + "/schemas").c_str(), 0700));
        std::filesystem::copy_file(
            std::string(NEU_PROPERTY_SCHEMA_DIR) +
                "/0160_2.16.1_node_properties.sql",
            directory + "/schemas/0160_2.16.1_node_properties.sql");
        ASSERT_EQ(0, chdir(directory.c_str()));
        ASSERT_EQ(SQLITE_OK,
                  sqlite3_open_v2("persistence/sqlite.db", &db,
                                  SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                      SQLITE_OPEN_FULLMUTEX,
                                  nullptr));
        ASSERT_EQ(SQLITE_OK,
                  sqlite3_exec(
                      db,
                      "CREATE TABLE nodes(name TEXT PRIMARY KEY, type INTEGER, "
                      "state INTEGER, plugin_name TEXT, tags TEXT);"
                      "CREATE TABLE settings(node_name TEXT PRIMARY KEY, "
                      "setting TEXT, "
                      "FOREIGN KEY(node_name) REFERENCES nodes(name) ON UPDATE "
                      "CASCADE ON DELETE CASCADE);"
                      "CREATE TABLE groups(driver_name TEXT, name TEXT, "
                      "interval INTEGER, "
                      "context TEXT, PRIMARY KEY(driver_name,name), "
                      "FOREIGN KEY(driver_name) REFERENCES nodes(name) ON "
                      "UPDATE CASCADE ON DELETE CASCADE);"
                      "CREATE TABLE tags(driver_name TEXT, group_name TEXT, "
                      "name TEXT, "
                      "address TEXT, attribute INTEGER, precision INTEGER, "
                      "type INTEGER, "
                      "decimal REAL, bias REAL, description TEXT, value TEXT, "
                      "format TEXT, unit TEXT, "
                      "UNIQUE(driver_name,group_name,name), FOREIGN "
                      "KEY(driver_name,group_name) "
                      "REFERENCES groups(driver_name,name) ON UPDATE CASCADE "
                      "ON DELETE CASCADE);",
                      nullptr, nullptr, nullptr))
            << sqlite3_errmsg(db);
        ASSERT_EQ(SQLITE_OK, sqlite3_close(db));
        db = nullptr;
        ASSERT_EQ(0, neu_persister_create("schemas"));
        opened = true;
        db     = neu_persister_get_db();
        ASSERT_NE(nullptr, sqlite3_db_mutex(db));
        neu_persist_node_info_t driver = { const_cast<char *>("driver"), 1,
                                           const_cast<char *>("modbus"), 1,
                                           nullptr };
        neu_persist_node_info_t app    = { const_cast<char *>("app"), 2,
                                        const_cast<char *>("mqtt"), 1,
                                        nullptr };
        ASSERT_EQ(0, neu_persister_store_node(&driver));
        ASSERT_EQ(0, neu_persister_store_node(&app));
    }

    void TearDown() override
    {
        if (opened) {
            neu_persister_destroy();
        } else if (db) {
            sqlite3_close(db);
        }
        if (cwd >= 0) {
            EXPECT_EQ(0, fchdir(cwd));
            close(cwd);
        }
        if (!directory.empty()) {
            std::filesystem::remove_all(directory);
        }
    }

    void sql(const char *query)
    {
        ASSERT_EQ(SQLITE_OK, sqlite3_exec(db, query, nullptr, nullptr, nullptr))
            << sqlite3_errmsg(db);
    }

    int scalar(const char *query)
    {
        sqlite3_stmt *stmt = nullptr;
        if (SQLITE_OK != sqlite3_prepare_v2(db, query, -1, &stmt, nullptr)) {
            ADD_FAILURE() << sqlite3_errmsg(db);
            return -1;
        }
        int step = sqlite3_step(stmt);
        EXPECT_EQ(SQLITE_ROW, step);
        int value = step == SQLITE_ROW ? sqlite3_column_int(stmt, 0) : -1;
        EXPECT_EQ(SQLITE_OK, sqlite3_finalize(stmt));
        return value;
    }

    void expect(const char *                                            node,
                const std::vector<std::pair<std::string, std::string>> &values)
    {
        Properties loaded;
        ASSERT_EQ(
            0,
            neu_persister_load_node_properties(node, &loaded.items, &loaded.n));
        ASSERT_EQ(values.size(), loaded.n);
        if (values.empty()) {
            EXPECT_EQ(nullptr, loaded.items);
        }
        for (size_t i = 0; i < loaded.n; ++i) {
            EXPECT_EQ(values[i].first, loaded.items[i].key);
            EXPECT_EQ(values[i].second, loaded.items[i].value_json);
        }
    }

    void group()
    {
        neu_persist_group_info_t info = { 1000, const_cast<char *>("group"),
                                          nullptr };
        ASSERT_EQ(0, neu_persister_store_group("driver", &info, nullptr));
    }

    neu_datatag_t tag(const char *name)
    {
        neu_datatag_t value = {};
        value.name          = const_cast<char *>(name);
        value.address       = const_cast<char *>("1!400001");
        value.description   = const_cast<char *>("");
        value.unit          = const_cast<char *>("");
        value.type          = NEU_TYPE_INT16;
        value.attribute     = NEU_ATTRIBUTE_READ;
        value.decimal       = 1;
        return value;
    }

    void concurrent_rollback(bool tags);
};

TEST_F(NodePropertyPersist, CrudAndNodeIsolation)
{
    expect("driver", {});
    neu_node_property_t values[] = { property("z", "true"),
                                     property("a", "1") };
    ASSERT_EQ(0, neu_persister_add_node_properties("driver", values, 2));
    auto app = property("a", "\"north\"");
    ASSERT_EQ(0, neu_persister_add_node_properties("app", &app, 1));
    expect("driver", { { "a", "1" }, { "z", "true" } });
    expect("app", { { "a", "\"north\"" } });

    auto update = property("a", "[1,2.5]");
    ASSERT_EQ(0, neu_persister_update_node_property("driver", &update));
    ASSERT_EQ(0, neu_persister_update_node_property("driver", &update));
    auto missing = property("missing", "false");
    EXPECT_EQ(NEU_ERR_NODE_PROPERTY_NOT_EXIST,
              neu_persister_update_node_property("driver", &missing));
    expect("driver", { { "a", "[1,2.5]" }, { "z", "true" } });

    const char *keys[] = { "a", "missing" };
    ASSERT_EQ(0, neu_persister_delete_node_properties("driver", keys, 2));
    ASSERT_EQ(0, neu_persister_delete_node_properties("driver", keys, 2));
    expect("driver", { { "z", "true" } });
    expect("app", { { "a", "\"north\"" } });
    const char *last[] = { "z" };
    ASSERT_EQ(0, neu_persister_delete_node_properties("driver", last, 1));
    expect("driver", {});
}

TEST_F(NodePropertyPersist, ValueTypesAndCanonicalRoundTrip)
{
    const char *                     texts[] = { " \"text\\n\\\"中文\" ",
                            "\"\\u0000\"",
                            "-9223372036854775808",
                            "9223372036854775807",
                            " 1.25e2 ",
                            "1.7976931348623157e308",
                            "2.2250738585072014e-308",
                            "5e-324",
                            "[1.7976931348623157e308,5e-324]",
                            "true",
                            "false",
                            "[]",
                            "[\"a\",\"b\",\"\\u0000\"]",
                            "[1, -2, 3.5]",
                            "[1.0, 2]",
                            "[true,false]" };
    std::vector<std::string>         keys;
    std::vector<neu_node_property_t> values;
    for (size_t i = 0; i < std::size(texts); ++i) {
        keys.push_back("key" + std::to_string(100 + i));
    }
    for (size_t i = 0; i < keys.size(); ++i) {
        values.push_back(property(keys[i].c_str(), texts[i]));
    }
    ASSERT_EQ(0,
              neu_persister_add_node_properties("driver", values.data(),
                                                values.size()));
    Properties loaded;
    ASSERT_EQ(
        0,
        neu_persister_load_node_properties("driver", &loaded.items, &loaded.n));
    ASSERT_EQ(values.size(), loaded.n);
    for (size_t i = 0; i < loaded.n; ++i) {
        json_t *expected =
            json_loads(texts[i], JSON_DECODE_ANY | JSON_ALLOW_NUL, nullptr);
        json_t *actual = json_loads(loaded.items[i].value_json,
                                    JSON_DECODE_ANY | JSON_ALLOW_NUL, nullptr);
        ASSERT_NE(nullptr, expected);
        ASSERT_NE(nullptr, actual);
        EXPECT_TRUE(json_equal(expected, actual));
        char *canonical = neu_node_property_json_dump(expected);
        EXPECT_STREQ(canonical, loaded.items[i].value_json);
        free(canonical);
        json_decref(expected);
        json_decref(actual);
    }
}

TEST_F(NodePropertyPersist, ExistingAndBatchDuplicatesRollback)
{
    auto seed = property("existing", "1");
    ASSERT_EQ(0, neu_persister_add_node_properties("driver", &seed, 1));
    neu_node_property_t existing[] = { property("new", "2"),
                                       property("existing", "3") };
    EXPECT_EQ(NEU_ERR_NODE_PROPERTY_EXIST,
              neu_persister_add_node_properties("driver", existing, 2));
    neu_node_property_t duplicate[] = { property("new", "2"),
                                        property("new", "2") };
    EXPECT_EQ(NEU_ERR_NODE_PROPERTY_EXIST,
              neu_persister_add_node_properties("driver", duplicate, 2));
    expect("driver", { { "existing", "1" } });
    EXPECT_TRUE(sqlite3_get_autocommit(db));
}

TEST_F(NodePropertyPersist, DeleteDuplicateAndInvalidKeysRollback)
{
    neu_node_property_t values[] = { property("a", "1"), property("b", "2") };
    ASSERT_EQ(0, neu_persister_add_node_properties("driver", values, 2));
    const char *duplicate[] = { "a", "missing", "a" };
    EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
              neu_persister_delete_node_properties("driver", duplicate, 3));
    const char *empty[] = { "a", "" };
    EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
              neu_persister_delete_node_properties("driver", empty, 2));
    const char *null_key[] = { "a", nullptr };
    EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
              neu_persister_delete_node_properties("driver", null_key, 2));
    const char *missing_duplicate[] = { "missing", "missing" };
    EXPECT_EQ(
        NEU_ERR_PARAM_IS_WRONG,
        neu_persister_delete_node_properties("driver", missing_duplicate, 2));
    expect("driver", { { "a", "1" }, { "b", "2" } });
}

TEST_F(NodePropertyPersist, RejectInvalidParameters)
{
    auto        value  = property("key", "1");
    const char *keys[] = { "key" };
    for (const char *node : { static_cast<const char *>(nullptr), "" }) {
        EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
                  neu_persister_add_node_properties(node, &value, 1));
        EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
                  neu_persister_delete_node_properties(node, keys, 1));
        EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
                  neu_persister_update_node_property(node, &value));
        Properties loaded;
        EXPECT_EQ(
            NEU_ERR_PARAM_IS_WRONG,
            neu_persister_load_node_properties(node, &loaded.items, &loaded.n));
        EXPECT_EQ(nullptr, loaded.items);
        EXPECT_EQ(0u, loaded.n);
    }
    EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
              neu_persister_add_node_properties("driver", nullptr, 1));
    EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
              neu_persister_add_node_properties("driver", &value, 0));
    EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
              neu_persister_delete_node_properties("driver", nullptr, 1));
    EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
              neu_persister_delete_node_properties("driver", keys, 0));
    EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
              neu_persister_update_node_property("driver", nullptr));
    for (const char *key : { static_cast<const char *>(nullptr), "" }) {
        auto invalid = property(key, "1");
        EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
                  neu_persister_add_node_properties("driver", &invalid, 1));
        EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
                  neu_persister_update_node_property("driver", &invalid));
    }
    size_t n = 10;
    EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
              neu_persister_load_node_properties("driver", nullptr, &n));
    EXPECT_EQ(0u, n);
    auto *items = reinterpret_cast<neu_node_property_t *>(1);
    EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
              neu_persister_load_node_properties("driver", &items, nullptr));
    EXPECT_EQ(nullptr, items);
    EXPECT_EQ(
        NEU_ERR_PARAM_IS_WRONG,
        neu_sqlite_persister_add_node_properties(nullptr, "driver", &value, 1));
    expect("driver", {});
}

TEST_F(NodePropertyPersist, RejectInvalidValuesAndRollback)
{
    const char *invalid[] = {
        nullptr, "",         "null",        "{}",        "[{}]",
        "[[1]]", "[1,true]", "[false,0.5]", "[\"x\",1]", "[null]",
        "[1,]",  "1 2",      "NaN",         "Infinity",  "\"unfinished"
    };
    for (const char *text : invalid) {
        neu_node_property_t values[] = { property("first", "1"),
                                         property("invalid", text) };
        EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
                  neu_persister_add_node_properties("driver", values, 2))
            << (text ? text : "null pointer");
        expect("driver", {});
    }
    auto value = property("key", "1");
    ASSERT_EQ(0, neu_persister_add_node_properties("driver", &value, 1));
    for (const char *text : invalid) {
        auto update = property("key", text);
        EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
                  neu_persister_update_node_property("driver", &update));
    }
    expect("driver", { { "key", "1" } });
}

TEST_F(NodePropertyPersist, BoundTextAndCascades)
{
    neu_persist_node_info_t node = { const_cast<char *>("node'--"), 2,
                                     const_cast<char *>("mqtt"), 1, nullptr };
    ASSERT_EQ(0, neu_persister_store_node(&node));
    const char *key   = "中文'; DROP TABLE nodes;--";
    auto        value = property(key, "\"quote' and \\\"json\\\"\"");
    ASSERT_EQ(0, neu_persister_add_node_properties(node.name, &value, 1));
    expect(node.name, { { key, value.value_json } });
    ASSERT_EQ(0, neu_persister_update_node(node.name, "renamed"));
    expect(node.name, {});
    expect("renamed", { { key, value.value_json } });
    ASSERT_EQ(0, neu_persister_delete_node("renamed"));
    expect("renamed", {});
    EXPECT_EQ(2, scalar("SELECT count(*) FROM nodes"));
    EXPECT_EQ(0, scalar("SELECT count(*) FROM node_properties"));
    EXPECT_NE(0, neu_persister_add_node_properties("nonexistent", &value, 1));
    EXPECT_TRUE(sqlite3_get_autocommit(db));
}

TEST_F(NodePropertyPersist, PersistenceReopenAndMigration)
{
    auto value = property("key", "[true,false]");
    ASSERT_EQ(0, neu_persister_add_node_properties("driver", &value, 1));
    neu_persister_destroy();
    opened = false;
    db     = nullptr;
    ASSERT_EQ(0, neu_persister_create("schemas"));
    opened = true;
    db     = neu_persister_get_db();
    expect("driver", { { "key", "[true,false]" } });
    EXPECT_EQ(1,
              scalar("SELECT count(*) FROM migrations WHERE version='0160' AND "
                     "dirty=0"));
    EXPECT_EQ(1, scalar("PRAGMA foreign_keys"));
    EXPECT_NE(nullptr, sqlite3_db_mutex(db));
}

TEST_F(NodePropertyPersist, FailedLoadClearsOutputs)
{
    neu_node_property_t values[] = { property("a", "1"), property("b", "2") };
    ASSERT_EQ(0, neu_persister_add_node_properties("driver", values, 2));
    sql("UPDATE node_properties SET value_json='[1,true]' WHERE key='b'");
    auto * items = reinterpret_cast<neu_node_property_t *>(1);
    size_t n     = 42;
    EXPECT_EQ(NEU_ERR_EINTERNAL,
              neu_persister_load_node_properties("driver", &items, &n));
    EXPECT_EQ(nullptr, items);
    EXPECT_EQ(0u, n);
    sql("DROP TABLE node_properties");
    items = reinterpret_cast<neu_node_property_t *>(1);
    n     = 42;
    EXPECT_EQ(NEU_ERR_EINTERNAL,
              neu_persister_load_node_properties("driver", &items, &n));
    EXPECT_EQ(nullptr, items);
    EXPECT_EQ(0u, n);
    EXPECT_EQ(NEU_ERR_EINTERNAL,
              neu_persister_add_node_properties("driver", values, 2));
    EXPECT_TRUE(sqlite3_get_autocommit(db));
}

TEST_F(NodePropertyPersist, ExistingTransactionIsNeverRolledBack)
{
    group();
    sql("BEGIN");
    sql("UPDATE nodes SET state=2 WHERE name='driver'");
    auto        value  = property("key", "1");
    const char *keys[] = { "key" };
    EXPECT_EQ(NEU_ERR_EINTERNAL,
              neu_persister_add_node_properties("driver", &value, 1));
    EXPECT_EQ(NEU_ERR_EINTERNAL,
              neu_persister_delete_node_properties("driver", keys, 1));
    EXPECT_EQ(NEU_ERR_EINTERNAL,
              neu_persister_update_node_property("driver", &value));
    Properties loaded;
    EXPECT_EQ(
        NEU_ERR_EINTERNAL,
        neu_persister_load_node_properties("driver", &loaded.items, &loaded.n));
    auto value_tag = tag("tag");
    EXPECT_EQ(NEU_ERR_EINTERNAL,
              neu_persister_store_tags("driver", "group", &value_tag, 1));
    EXPECT_FALSE(sqlite3_get_autocommit(db));
    EXPECT_EQ(2, scalar("SELECT state FROM nodes WHERE name='driver'"));
    sql("ROLLBACK");
    EXPECT_EQ(1, scalar("SELECT state FROM nodes WHERE name='driver'"));
}

TEST_F(NodePropertyPersist, StepAndBindErrorsRollback)
{
    neu_node_property_t values[] = { property("a", "1"), property("b", "2") };
    ASSERT_EQ(0, neu_persister_add_node_properties("driver", values, 2));
    sql("CREATE TRIGGER fail_delete BEFORE DELETE ON node_properties "
        "WHEN OLD.key='b' BEGIN SELECT RAISE(ABORT, 'fail'); END");
    const char *keys[] = { "a", "b" };
    EXPECT_EQ(NEU_ERR_EINTERNAL,
              neu_persister_delete_node_properties("driver", keys, 2));
    expect("driver", { { "a", "1" }, { "b", "2" } });
    std::string large = "\"" + std::string(2048, 'x') + "\"";
    auto        value = property("large", large.c_str());
    int         old   = sqlite3_limit(db, SQLITE_LIMIT_LENGTH, 1024);
    EXPECT_EQ(NEU_ERR_EINTERNAL,
              neu_persister_add_node_properties("driver", &value, 1));
    sqlite3_limit(db, SQLITE_LIMIT_LENGTH, old);
    expect("driver", { { "a", "1" }, { "b", "2" } });
    EXPECT_TRUE(sqlite3_get_autocommit(db));
}

int deny_commit(void *, int action, const char *arg, const char *, const char *,
                const char *)
{
    return action == SQLITE_TRANSACTION && arg && strcmp(arg, "COMMIT") == 0
        ? SQLITE_DENY
        : SQLITE_OK;
}

TEST_F(NodePropertyPersist, CommitErrorsRollback)
{
    group();
    auto seed = property("a", "1");
    ASSERT_EQ(0, neu_persister_add_node_properties("driver", &seed, 1));
    ASSERT_EQ(SQLITE_OK, sqlite3_set_authorizer(db, deny_commit, nullptr));
    auto added = property("b", "2");
    EXPECT_EQ(NEU_ERR_EINTERNAL,
              neu_persister_add_node_properties("driver", &added, 1));
    auto updated = property("a", "3");
    EXPECT_EQ(NEU_ERR_EINTERNAL,
              neu_persister_update_node_property("driver", &updated));
    const char *keys[] = { "a" };
    EXPECT_EQ(NEU_ERR_EINTERNAL,
              neu_persister_delete_node_properties("driver", keys, 1));
    auto value_tag = tag("tag");
    EXPECT_EQ(NEU_ERR_EINTERNAL,
              neu_persister_store_tags("driver", "group", &value_tag, 1));
    EXPECT_TRUE(sqlite3_get_autocommit(db));
    ASSERT_EQ(SQLITE_OK, sqlite3_set_authorizer(db, nullptr, nullptr));
    expect("driver", { { "a", "1" } });
    EXPECT_EQ(0, scalar("SELECT count(*) FROM tags"));
}

struct TransactionGate {
    std::mutex              mutex;
    std::condition_variable changed;
    bool                    paused   = false;
    bool                    released = false;
    std::atomic<int>        started { 0 };
    std::atomic<bool>       ended { false };
    std::atomic<bool>       interleaved { false };
    std::thread::id         owner;
    std::string             insert;
};

int transaction_trace(unsigned, void *context, void *statement, void *)
{
    auto &      gate  = *static_cast<TransactionGate *>(context);
    const char *query = sqlite3_sql(static_cast<sqlite3_stmt *>(statement));
    if (std::this_thread::get_id() != gate.owner) {
        if (!gate.ended.load()) {
            gate.interleaved = true;
        }
        return 0;
    }
    if (strcmp(query, "ROLLBACK") == 0) {
        gate.ended = true;
    }
    if (strncmp(query, gate.insert.c_str(), gate.insert.size()) == 0) {
        std::unique_lock<std::mutex> lock(gate.mutex);
        if (!gate.paused) {
            gate.paused = true;
            gate.changed.notify_all();
            gate.changed.wait(lock, [&] { return gate.released; });
        }
    }
    return 0;
}

void NodePropertyPersist::concurrent_rollback(bool tags)
{
    group();
    TransactionGate gate;
    gate.insert = tags ? "INSERT INTO tags" : "INSERT INTO node_properties";
    ASSERT_EQ(
        SQLITE_OK,
        sqlite3_trace_v2(db, SQLITE_TRACE_STMT, transaction_trace, &gate));
    std::vector<std::string> names;
    for (int i = 0; i < 500; ++i) {
        names.push_back("batch" + std::to_string(i));
    }
    std::vector<neu_node_property_t> properties;
    std::vector<neu_datatag_t>       datatags;
    for (const auto &name : names) {
        properties.push_back(property(name.c_str(), "1"));
        datatags.push_back(tag(name.c_str()));
    }
    properties.push_back(properties[0]);
    datatags.push_back(datatags[0]);
    int         batch_result    = -1;
    int         other_result    = -1;
    int         ordinary_result = -1;
    std::thread batch([&] {
        gate.owner   = std::this_thread::get_id();
        batch_result = tags
            ? neu_persister_store_tags("driver", "group", datatags.data(),
                                       datatags.size())
            : neu_persister_add_node_properties("driver", properties.data(),
                                                properties.size());
    });
    bool        paused;
    {
        std::unique_lock<std::mutex> lock(gate.mutex);
        paused = gate.changed.wait_for(lock, std::chrono::seconds(5),
                                       [&] { return gate.paused; });
    }
    std::thread other([&] {
        ++gate.started;
        if (tags) {
            auto value   = property("saved", "true");
            other_result = neu_persister_add_node_properties("app", &value, 1);
        } else {
            auto value = tag("saved");
            other_result =
                neu_persister_store_tags("driver", "group", &value, 1);
        }
    });
    std::thread ordinary([&] {
        ++gate.started;
        ordinary_result = neu_persister_store_node_setting("driver", "saved");
    });
    while (gate.started.load() != 2) {
        std::this_thread::yield();
    }
    {
        std::lock_guard<std::mutex> lock(gate.mutex);
        gate.released = true;
    }
    gate.changed.notify_all();
    batch.join();
    other.join();
    ordinary.join();
    EXPECT_EQ(SQLITE_OK, sqlite3_trace_v2(db, 0, nullptr, nullptr));
    ASSERT_TRUE(paused);
    EXPECT_TRUE(gate.ended.load());
    EXPECT_FALSE(gate.interleaved.load());
    EXPECT_EQ(tags ? NEU_ERR_EINTERNAL : NEU_ERR_NODE_PROPERTY_EXIST,
              batch_result);
    EXPECT_EQ(0, other_result);
    EXPECT_EQ(0, ordinary_result);
    EXPECT_TRUE(sqlite3_get_autocommit(db));
    expect("driver", {});
    expect("app",
           tags
               ? std::vector<std::pair<std::string, std::string>> { { "saved",
                                                                      "true" } }
               : std::vector<std::pair<std::string, std::string>> {});
    EXPECT_EQ(tags ? 0 : 1, scalar("SELECT count(*) FROM tags"));
    const char *setting = nullptr;
    ASSERT_EQ(0, neu_persister_load_node_setting("driver", &setting));
    EXPECT_STREQ("saved", setting);
    free(const_cast<char *>(setting));
}

TEST_F(NodePropertyPersist, PropertyTransactionIsolatesTagsAndOrdinaryWrite)
{
    concurrent_rollback(false);
}

TEST_F(NodePropertyPersist, TagTransactionIsolatesPropertiesAndOrdinaryWrite)
{
    concurrent_rollback(true);
}

}
