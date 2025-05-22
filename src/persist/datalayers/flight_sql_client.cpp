#include "flight_sql_client.h"
#include <arrow/array.h>
#include <arrow/flight/client.h>
#include <arrow/flight/sql/client.h>
#include <arrow/status.h>
#include <arrow/table.h>
#include <arrow/type_fwd.h>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>

namespace flight    = arrow::flight;
namespace flightsql = arrow::flight::sql;

struct Client {
public:
    Client(const std::string &host, int port, const std::string &username,
           const std::string &password);
    ~Client() = default;

    arrow::Status Execute(const std::string &sql);
    arrow::Result<std::shared_ptr<arrow::Table>> Query(const std::string &sql);

    bool IsInitialized() const { return initialized_; }

private:
    bool                                        initialized_ = false;
    flight::Location                            location_;
    std::shared_ptr<flight::FlightClient>       flight_client_;
    std::unique_ptr<flightsql::FlightSqlClient> client_;
    std::string                                 bearer_token_;

    arrow::Result<std::string>
    AuthenticateBasicToken(const flight::FlightCallOptions &options,
                           const std::string &              username,
                           const std::string &              password);
};

Client::Client(const std::string &host, int port, const std::string &username,
               const std::string &password)
{
    arrow::Result<flight::Location> location_result =
        flight::Location::ForGrpcTcp(host, port);
    if (!location_result.ok()) {
        // std::cerr << "Failed to create Flight location: " <<
        // location_result.status().ToString() << std::endl;
        return;
    }
    location_ = location_result.ValueOrDie();

    flight::FlightClientOptions client_options;
    client_options.disable_server_verification = true;
    arrow::Result<std::shared_ptr<flight::FlightClient>> flight_client_result =
        flight::FlightClient::Connect(location_, client_options);
    if (!flight_client_result.ok()) {
        // std::cerr << "Failed to connect to Flight client: " <<
        // flight_client_result.status().ToString() << std::endl;
        return;
    }
    flight_client_ = flight_client_result.ValueOrDie();

    flight::FlightCallOptions call_options;
    auto auth_result = AuthenticateBasicToken(call_options, username, password);
    if (!auth_result.ok()) {
        // std::cerr << "Authentication failed: " <<
        // auth_result.status().ToString() << std::endl;
        return;
    }
    bearer_token_ = auth_result.ValueOrDie();
    client_ = std::make_unique<flightsql::FlightSqlClient>(flight_client_);

    initialized_ = true;
    // std::cerr << "Client initialization succeeded!" << std::endl;
}

arrow::Status Client::Execute(const std::string &sql)
{
    flight::FlightCallOptions call_options;
    call_options.headers.push_back(
        std::make_pair("authorization", "Bearer " + bearer_token_));

    // std::cout << "Executing SQL: " << sql << std::endl;

    arrow::Result<std::unique_ptr<flight::FlightInfo>> flight_info_result =
        client_->Execute(call_options, sql);
    if (!flight_info_result.ok()) {
        return flight_info_result.status();
    }
    /*auto flight_info = std::move(flight_info_result.ValueOrDie());

    for (const flight::FlightEndpoint &endpoint : flight_info->endpoints()) {
        arrow::Result<std::unique_ptr<flight::FlightStreamReader>>
            stream_result = client_->DoGet(call_options, endpoint.ticket);
        if (!stream_result.ok()) {
            return stream_result.status();
        }

        auto stream = std::move(stream_result.ValueOrDie());

        arrow::Result<std::shared_ptr<arrow::Table>> table_result =
            stream->ToTable();
        if (!table_result.ok()) {
            return table_result.status();
        }
    }*/

    return arrow::Status::OK();
}

arrow::Result<std::string>
Client::AuthenticateBasicToken(const flight::FlightCallOptions &options,
                               const std::string &              username,
                               const std::string &              password)
{
    auto auth_result =
        flight_client_->AuthenticateBasicToken(options, username, password);
    if (!auth_result.ok()) {
        return auth_result.status();
    }
    return auth_result.ValueOrDie().second;
}

arrow::Result<std::shared_ptr<arrow::Table>>
Client::Query(const std::string &sql)
{
    flight::FlightCallOptions call_options;
    call_options.headers.push_back(
        std::make_pair("authorization", "Bearer " + bearer_token_));

    arrow::Result<std::unique_ptr<flight::FlightInfo>> flight_info_result =
        client_->Execute(call_options, sql);
    if (!flight_info_result.ok()) {
        return flight_info_result.status();
    }
    auto flight_info = std::move(flight_info_result.ValueOrDie());

    std::vector<std::shared_ptr<arrow::Table>> tables;

    for (const flight::FlightEndpoint &endpoint : flight_info->endpoints()) {
        arrow::Result<std::unique_ptr<flight::FlightStreamReader>>
            stream_result = client_->DoGet(call_options, endpoint.ticket);
        if (!stream_result.ok()) {
            return stream_result.status();
        }

        auto stream = std::move(stream_result.ValueOrDie());

        arrow::Result<std::shared_ptr<arrow::Table>> table_result =
            stream->ToTable();
        if (!table_result.ok()) {
            return table_result.status();
        }

        tables.push_back(table_result.ValueOrDie());
    }

    return arrow::ConcatenateTables(tables);
}

extern "C" Client *client_create(const char *host, int port,
                                 const char *username, const char *password)
{
    Client *client = new Client(host, port, username, password);
    if (!client->IsInitialized()) {
        delete client;
        // std::cerr << "Client creation failed due to invalid parameters or
        // connection issues." << std::endl;
        return nullptr;
    }
    return client;
}

extern "C" int client_execute(Client *client, const char *sql)
{
    if (!client)
        return -1;
    auto status = client->Execute(sql);
    return status.ok() ? 0 : -1;
}

extern "C" void client_destroy(Client *client)
{
    delete client;
}

extern "C" int client_insert(Client *client, ValueType type, datatag *tags,
                             size_t tag_count)
{
    if (!client || !tags)
        return -1;
    std::string table_name;

    switch (type) {
    case INT_TYPE:
        table_name = "neuronex.neuron_int";
        break;
    case FLOAT_TYPE:
        table_name = "neuronex.neuron_float";
        break;
    case BOOL_TYPE:
        table_name = "neuronex.neuron_bool";
        break;
    case STRING_TYPE:
        table_name = "neuronex.neuron_string";
        break;
    default:
        return -1;
    }

    std::string sql = "INSERT INTO " + table_name +
        " (time, node_name, group_name, tag, value) VALUES ";

    for (size_t i = 0; i < tag_count; ++i) {
        if (i > 0) {
            sql += ", ";
        }
        datatag &tag = tags[i];

        std::string value_str;
        switch (tag.value_type) {
        case INT_TYPE:
            value_str = std::to_string(tag.value.int_value);
            break;
        case FLOAT_TYPE:
            value_str = std::to_string(tag.value.float_value);
            break;
        case BOOL_TYPE:
            value_str = tag.value.bool_value ? "true" : "false";
            break;
        case STRING_TYPE:
            value_str = "'" + std::string(tag.value.string_value) + "'";
            break;
        default:
            value_str = "NULL";
            break;
        }

        sql += "(CURRENT_TIMESTAMP, '" + std::string(tag.node_name) + "', '" +
            std::string(tag.group_name) + "', '" + std::string(tag.tag) +
            "', " + value_str + ")";
    }

    int status = client_execute(client, sql.c_str());
    /*if (status != 0) {
        std::cerr << "Error executing SQL: " << status << std::endl;
    }*/

    return status;
}

std::string convert_timestamp_to_utc8(int64_t timestamp)
{
    std::chrono::milliseconds                          ms(timestamp);
    std::chrono::time_point<std::chrono::system_clock> time_point(ms);
    time_point += std::chrono::hours(8);
    std::time_t time_t_val = std::chrono::system_clock::to_time_t(time_point);
    std::tm     local_tm   = *std::gmtime(&time_t_val);
    char        time_str[100];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", &local_tm);
    auto milliseconds_part = ms.count() % 1000;
    snprintf(time_str + strlen(time_str), sizeof(time_str) - strlen(time_str),
             ".%03ld", milliseconds_part);
    snprintf(time_str + strlen(time_str), sizeof(time_str) - strlen(time_str),
             "+08:00");
    return std::string(time_str);
}

extern "C" void client_query_free(query_result *result)
{
    if (result) {
        for (size_t i = 0; i < result->row_count; ++i) {
            if (result->rows[i].value_type == STRING_TYPE &&
                result->rows[i].value.string_value) {
                free((void *) result->rows[i].value.string_value);
            }
        }
        free(result->rows);
        free(result);
    }
}

extern "C" query_result *client_query(Client *client, ValueType type,
                                      const char *node_name,
                                      const char *group_name, const char *tag)
{
    if (!client || !node_name || !group_name || !tag)
        return nullptr;
    std::string table_name;
    switch (type) {
    case INT_TYPE:
        table_name = "neuronex.neuron_int";
        break;
    case FLOAT_TYPE:
        table_name = "neuronex.neuron_float";
        break;
    case BOOL_TYPE:
        table_name = "neuronex.neuron_bool";
        break;
    case STRING_TYPE:
        table_name = "neuronex.neuron_string";
        break;
    default:
        return nullptr;
    }

    std::string sql = "SELECT * FROM " + table_name + " WHERE node_name = '" +
        node_name + "'" + " AND group_name = '" + group_name + "'" +
        " AND tag = '" + tag + "'";

    std::cout << "Generated SQL: " << sql << std::endl;

    auto table_result = client->Query(sql);
    if (!table_result.ok()) {
        std::cerr << "Error executing query: "
                  << table_result.status().ToString() << std::endl;
        return nullptr;
    }
    auto table = table_result.ValueOrDie();

    int64_t num_rows = table->num_rows();
    if (num_rows == 0) {
        std::cout << "No rows found." << std::endl;
        return nullptr;
    }

    query_result *result = (query_result *) malloc(sizeof(query_result));
    result->row_count    = num_rows;
    result->rows         = (datarow *) malloc(num_rows * sizeof(datarow));

    auto time_col = std::dynamic_pointer_cast<arrow::TimestampArray>(
        table->column(0)->chunk(0));
    auto node_col = std::dynamic_pointer_cast<arrow::StringArray>(
        table->column(1)->chunk(0));
    auto group_col = std::dynamic_pointer_cast<arrow::StringArray>(
        table->column(2)->chunk(0));
    auto tag_col = std::dynamic_pointer_cast<arrow::StringArray>(
        table->column(3)->chunk(0));

    auto value_col_int = std::dynamic_pointer_cast<arrow::Int64Array>(
        table->column(4)->chunk(0));
    auto value_col_float = std::dynamic_pointer_cast<arrow::DoubleArray>(
        table->column(4)->chunk(0));
    auto value_col_bool = std::dynamic_pointer_cast<arrow::BooleanArray>(
        table->column(4)->chunk(0));
    auto value_col_string = std::dynamic_pointer_cast<arrow::StringArray>(
        table->column(4)->chunk(0));

    if (!time_col || !node_col || !group_col || !tag_col) {
        std::cerr << "Failed to cast column to StringArray" << std::endl;
        return nullptr;
    }

    for (int64_t i = 0; i < num_rows; ++i) {
        auto        timestamp = time_col->Value(i);
        std::string time_str  = convert_timestamp_to_utc8(timestamp);
        strncpy(result->rows[i].time, time_str.c_str(),
                sizeof(result->rows[i].time));

        strncpy(result->rows[i].node_name, node_col->GetString(i).c_str(),
                sizeof(result->rows[i].node_name));
        strncpy(result->rows[i].group_name, group_col->GetString(i).c_str(),
                sizeof(result->rows[i].group_name));
        strncpy(result->rows[i].tag, tag_col->GetString(i).c_str(),
                sizeof(result->rows[i].tag));

        if (value_col_int) {
            result->rows[i].value.int_value = value_col_int->Value(i);
            result->rows[i].value_type      = INT_TYPE;
        } else if (value_col_float) {
            result->rows[i].value.float_value = value_col_float->Value(i);
            result->rows[i].value_type        = FLOAT_TYPE;
        } else if (value_col_bool) {
            result->rows[i].value.bool_value = value_col_bool->Value(i);
            result->rows[i].value_type       = BOOL_TYPE;
        } else if (value_col_string) {
            result->rows[i].value.string_value =
                strdup(value_col_string->GetString(i).c_str());
            result->rows[i].value_type = STRING_TYPE;
        } else {
        }
    }

    return result;
}

extern "C" query_result *client_query_nodes_groups(Client *  client,
                                                   ValueType type)
{
    if (!client) {
        std::cerr << "Client is not initialized." << std::endl;
        return nullptr;
    }

    std::string table_name;
    switch (type) {
    case INT_TYPE:
        table_name = "neuronex.neuron_int";
        break;
    case FLOAT_TYPE:
        table_name = "neuronex.neuron_float";
        break;
    case BOOL_TYPE:
        table_name = "neuronex.neuron_bool";
        break;
    case STRING_TYPE:
        table_name = "neuronex.neuron_string";
        break;
    default:
        std::cerr << "Invalid ValueType." << std::endl;
        return nullptr;
    }

    std::string sql =
        "SELECT DISTINCT node_name, group_name FROM " + table_name;
    std::cout << "Generated SQL for nodes/groups: " << sql << std::endl;

    auto table_result = client->Query(sql);
    if (!table_result.ok()) {
        std::cerr << "Error executing query: "
                  << table_result.status().ToString() << std::endl;
        return nullptr;
    }
    auto table = table_result.ValueOrDie();

    int64_t num_rows = table->num_rows();
    if (num_rows == 0) {
        std::cout << "No nodes/groups found." << std::endl;
        return nullptr;
    }

    query_result *result = (query_result *) malloc(sizeof(query_result));
    if (!result) {
        std::cerr << "Failed to allocate memory for result." << std::endl;
        return nullptr;
    }
    result->row_count = num_rows;
    result->rows      = (datarow *) malloc(num_rows * sizeof(datarow));
    if (!result->rows) {
        std::cerr << "Failed to allocate memory for rows." << std::endl;
        free(result);
        return nullptr;
    }

    int64_t            row_index    = 0;
    arrow::ArrayVector node_chunks  = table->column(0)->chunks();
    arrow::ArrayVector group_chunks = table->column(1)->chunks();

    for (size_t chunk_idx = 0; chunk_idx < node_chunks.size(); ++chunk_idx) {
        auto node_col = std::dynamic_pointer_cast<arrow::StringArray>(
            node_chunks[chunk_idx]);
        auto group_col = std::dynamic_pointer_cast<arrow::StringArray>(
            group_chunks[chunk_idx]);

        if (!node_col || !group_col) {
            std::cerr << "Chunk " << chunk_idx << " type mismatch."
                      << std::endl;
            continue;
        }

        for (int64_t i = 0; i < node_col->length(); ++i) {
            snprintf(result->rows[row_index].node_name,
                     sizeof(result->rows[row_index].node_name), "%s",
                     node_col->GetString(i).c_str());
            snprintf(result->rows[row_index].group_name,
                     sizeof(result->rows[row_index].group_name), "%s",
                     group_col->GetString(i).c_str());
            row_index++;
        }
    }

    return result;
}

extern "C" query_result *client_query_all_data(Client *client, ValueType type)
{
    if (!client) {
        std::cerr << "Client is not initialized." << std::endl;
        return nullptr;
    }

    std::string table_name;
    switch (type) {
    case INT_TYPE:
        table_name = "neuronex.neuron_int";
        break;
    case FLOAT_TYPE:
        table_name = "neuronex.neuron_float";
        break;
    case BOOL_TYPE:
        table_name = "neuronex.neuron_bool";
        break;
    case STRING_TYPE:
        table_name = "neuronex.neuron_string";
        break;
    default:
        std::cerr << "Invalid ValueType." << std::endl;
        return nullptr;
    }

    std::string sql = "SELECT node_name, group_name, tag FROM " + table_name;

    std::cout << "Generated SQL for all data: " << sql << std::endl;

    auto table_result = client->Query(sql);
    if (!table_result.ok()) {
        std::cerr << "Error executing query: "
                  << table_result.status().ToString() << std::endl;
        return nullptr;
    }
    auto table = table_result.ValueOrDie();

    int64_t num_rows = table->num_rows();
    if (num_rows == 0) {
        std::cout << "No node/group/tag found." << std::endl;
        return nullptr;
    }

    query_result *result = (query_result *) malloc(sizeof(query_result));
    if (!result) {
        std::cerr << "Failed to allocate memory for result." << std::endl;
        return nullptr;
    }

    result->row_count = num_rows;
    result->rows      = (datarow *) malloc(num_rows * sizeof(datarow));
    if (!result->rows) {
        std::cerr << "Failed to allocate memory for rows." << std::endl;
        free(result);
        return nullptr;
    }

    int64_t            row_index    = 0;
    arrow::ArrayVector node_chunks  = table->column(0)->chunks();
    arrow::ArrayVector group_chunks = table->column(1)->chunks();
    arrow::ArrayVector tag_chunks   = table->column(2)->chunks();

    for (size_t chunk_idx = 0; chunk_idx < node_chunks.size(); ++chunk_idx) {
        auto node_col = std::dynamic_pointer_cast<arrow::StringArray>(
            node_chunks[chunk_idx]);
        auto group_col = std::dynamic_pointer_cast<arrow::StringArray>(
            group_chunks[chunk_idx]);
        auto tag_col = std::dynamic_pointer_cast<arrow::StringArray>(
            tag_chunks[chunk_idx]);

        if (!node_col || !group_col || !tag_col) {
            std::cerr << "Chunk " << chunk_idx << " type mismatch."
                      << std::endl;
            continue;
        }

        for (int64_t i = 0; i < node_col->length(); ++i) {
            snprintf(result->rows[row_index].node_name,
                     sizeof(result->rows[row_index].node_name), "%s",
                     node_col->GetString(i).c_str());
            snprintf(result->rows[row_index].group_name,
                     sizeof(result->rows[row_index].group_name), "%s",
                     group_col->GetString(i).c_str());
            snprintf(result->rows[row_index].tag,
                     sizeof(result->rows[row_index].tag), "%s",
                     tag_col->GetString(i).c_str());
            row_index++;
        }
    }

    return result;
}