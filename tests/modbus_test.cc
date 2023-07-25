#include <cstdint>
#include <gtest/gtest.h>
#include <neuron.h>
extern "C" {
#include "modbus.h"
#include "modbus_point.h"
}

zlog_category_t *neuron           = NULL;
int64_t          global_timestamp = 0;

TEST(test_modbus_header_wrap, should_return_right_header_value)
{
    uint8_t                 buf[16] = { 0 };
    neu_protocol_pack_buf_t pbuf    = { 0 };
    uint16_t                seq     = 0x0004;

    neu_protocol_pack_buf_init(&pbuf, buf, sizeof(buf));

    modbus_header_wrap(&pbuf, seq);

    struct modbus_header *header =
        (struct modbus_header *) neu_protocol_unpack_buf(
            &pbuf, sizeof(struct modbus_header));

    EXPECT_EQ(0x0004, ntohs(header->seq));
    EXPECT_EQ(0x0, header->protocol);
    EXPECT_EQ(0x0, ntohs(header->len));
}

TEST(test_modbus_header_unwrap, should_return_right_header_value)
{
    uint8_t                   buf[16] = { 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 };
    neu_protocol_unpack_buf_t pbuf    = { 0 };
    struct modbus_header      header  = { 0 };

    neu_protocol_unpack_buf_init(&pbuf, buf, sizeof(buf));

    modbus_header_unwrap(&pbuf, &header);

    EXPECT_EQ(0x1, header.seq);
    EXPECT_EQ(0x0, header.protocol);
    EXPECT_EQ(0x0, header.len);
}

TEST(test_modbus_code_wrap, should_return_right_code_value)
{
    uint8_t                 buf[16]  = { 0 };
    neu_protocol_pack_buf_t pbuf     = { 0 };
    uint8_t                 slave_id = 1;
    uint8_t                 function = MODBUS_READ_HOLD_REG;
    neu_protocol_pack_buf_init(&pbuf, buf, sizeof(buf));

    modbus_code_wrap(&pbuf, slave_id, function);

    struct modbus_code *code = (struct modbus_code *) neu_protocol_unpack_buf(
        &pbuf, sizeof(struct modbus_code));

    EXPECT_EQ(0x1, code->slave_id);
    EXPECT_EQ(0x3, code->function);
}

TEST(test_modbus_code_unwrap, should_return_right_code_value)
{
    uint8_t                   buf[16] = { 0x01, 0x03 };
    neu_protocol_unpack_buf_t pbuf    = { 0 };
    struct modbus_code        code    = { 0 };

    neu_protocol_unpack_buf_init(&pbuf, buf, sizeof(buf));

    modbus_code_unwrap(&pbuf, &code);

    EXPECT_EQ(0x1, code.slave_id);
    EXPECT_EQ(0x3, code.function);
}

TEST(test_modbus_address_wrap, should_return_right_address_value)
{
    uint8_t                 buf[16]    = { 0 };
    neu_protocol_pack_buf_t pbuf       = { 0 };
    uint16_t                start      = 0x1;
    uint16_t                n_register = 0x2;
    enum modbus_action      action     = MODBUS_ACTION_DEFAULT;

    neu_protocol_pack_buf_init(&pbuf, buf, sizeof(buf));

    modbus_address_wrap(&pbuf, start, n_register, action);

    struct modbus_address *address =
        (struct modbus_address *) neu_protocol_unpack_buf(
            &pbuf, sizeof(struct modbus_address));

    EXPECT_EQ(0x1, ntohs(address->start_address));
    EXPECT_EQ(0x2, ntohs(address->n_reg));
}

TEST(test_modbus_address_unwrap, should_return_right_address_value)
{
    uint8_t                   buf[16]     = { 0x00, 0x01, 0x00, 0x02 };
    neu_protocol_unpack_buf_t pbuf        = { 0 };
    struct modbus_address     out_address = { 0 };

    neu_protocol_unpack_buf_init(&pbuf, buf, sizeof(buf));

    modbus_address_unwrap(&pbuf, &out_address);

    struct modbus_address *address =
        (struct modbus_address *) neu_protocol_pack_buf(
            &pbuf, sizeof(struct modbus_address));

    EXPECT_EQ(0x1, ntohs(address->start_address));
    EXPECT_EQ(0x2, ntohs(address->n_reg));
}

TEST(test_modbus_data_wrap, should_return_right_data_value)
{
    uint8_t                 buf[16]  = { 0 };
    neu_protocol_pack_buf_t pbuf     = { 0 };
    uint8_t                 n_byte   = 4;
    uint8_t                 byte[16] = { "\x11\x22\x33\x44" };
    uint8_t *               bytes    = byte;
    enum modbus_action      action   = MODBUS_ACTION_DEFAULT;

    neu_protocol_pack_buf_init(&pbuf, buf, sizeof(buf));

    modbus_data_wrap(&pbuf, n_byte, bytes, action);

    struct modbus_data *data = (struct modbus_data *) neu_protocol_unpack_buf(
        &pbuf, sizeof(struct modbus_data));

    EXPECT_EQ(n_byte, data->n_byte);
    EXPECT_EQ(0x11, data->byte[0]);
    EXPECT_EQ(0x22, data->byte[1]);
    EXPECT_EQ(0x33, data->byte[2]);
    EXPECT_EQ(0x44, data->byte[3]);
}

TEST(test_modbus_data_unwrap, should_return_right_data_value)
{
    uint8_t                   buf[16] = { "\x04\x11\x22\x33\x44" };
    neu_protocol_unpack_buf_t pbuf    = { 0 };
    struct modbus_data        data    = { 0 };
    uint8_t *                 bytes   = NULL;

    neu_protocol_unpack_buf_init(&pbuf, buf, sizeof(buf));

    modbus_data_unwrap(&pbuf, &data);
    bytes = neu_protocol_unpack_buf(&pbuf, 4);

    EXPECT_EQ(4, data.n_byte);
    EXPECT_EQ(0x11, *bytes);
    EXPECT_EQ(0x22, *(bytes + 1));
    EXPECT_EQ(0x33, *(bytes + 2));
    EXPECT_EQ(0x44, *(bytes + 3));
}

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}