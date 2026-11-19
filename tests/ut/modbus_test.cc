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

static neu_datatag_t modbus_tag(const char *address, neu_type_e type,
                                neu_attribute_e attribute)
{
    neu_datatag_t tag = {};
    tag.name          = const_cast<char *>("tag");
    tag.address       = const_cast<char *>(address);
    tag.type          = type;
    tag.attribute     = attribute;
    tag.description   = const_cast<char *>("");
    return tag;
}

TEST(modbus_auto_type, address_selects_concrete_point_type)
{
    struct {
        const char *    address;
        neu_attribute_e attribute;
        neu_type_e      type;
        modbus_area_e   area;
        uint8_t         bit;
        bool            has_bit;
    } accepted[] = {
        { "1!000001", NEU_ATTRIBUTE_READ, NEU_TYPE_BIT, MODBUS_AREA_COIL, 0,
          false },
        { "1!100001", NEU_ATTRIBUTE_READ, NEU_TYPE_BIT, MODBUS_AREA_INPUT, 0,
          false },
        { "1!300001", NEU_ATTRIBUTE_READ, NEU_TYPE_UINT16,
          MODBUS_AREA_INPUT_REGISTER, 0, false },
        { "1!400001", NEU_ATTRIBUTE_READ, NEU_TYPE_UINT16,
          MODBUS_AREA_HOLD_REGISTER, 0, false },
        { "1!400001.15", NEU_ATTRIBUTE_READ, NEU_TYPE_BIT,
          MODBUS_AREA_HOLD_REGISTER, 15, true },
        { "1!300010.0", NEU_ATTRIBUTE_READ, NEU_TYPE_BIT,
          MODBUS_AREA_INPUT_REGISTER, 0, true },
    };

    for (auto item : accepted) {
        neu_datatag_t tag =
            modbus_tag(item.address, NEU_TYPE_AUTO, item.attribute);
        modbus_point_t point = {};
        EXPECT_EQ(NEU_ERR_SUCCESS, modbus_tag_to_point(&tag, &point, base_1))
            << item.address;
        EXPECT_EQ(NEU_TYPE_AUTO, tag.type);
        EXPECT_EQ(item.type, point.type);
        EXPECT_EQ(item.area, point.area);
        EXPECT_EQ(item.has_bit, point.option.bit.op);
        if (item.has_bit) {
            EXPECT_EQ(item.bit, point.option.bit.bit);
        }
    }

    neu_datatag_t endian =
        modbus_tag("1!400002#B", NEU_TYPE_AUTO, NEU_ATTRIBUTE_READ);
    modbus_point_t point = {};
    EXPECT_EQ(NEU_ERR_SUCCESS, modbus_tag_to_point(&endian, &point, base_1));
    EXPECT_EQ(NEU_TYPE_UINT16, point.type);
    EXPECT_EQ(NEU_DATATAG_ENDIAN_B16, point.option.value16.endian);

    neu_datatag_t explicit_tag =
        modbus_tag("1!400001", NEU_TYPE_INT32, NEU_ATTRIBUTE_READ);
    EXPECT_EQ(NEU_ERR_SUCCESS,
              modbus_tag_to_point(&explicit_tag, &point, base_1));
    EXPECT_EQ(NEU_TYPE_INT32, point.type);
    EXPECT_EQ(2, point.n_register);

    for (const char *address :
         { "1!000001.1", "1!400001.16", "1!400001.1#B", "1!200001" }) {
        neu_datatag_t rejected =
            modbus_tag(address, NEU_TYPE_AUTO, NEU_ATTRIBUTE_READ);
        EXPECT_EQ(NEU_ERR_TAG_ADDRESS_FORMAT_INVALID,
                  modbus_tag_to_point(&rejected, &point, base_1))
            << address;
    }

    neu_datatag_t write_input = modbus_tag(
        "1!300001", NEU_TYPE_AUTO,
        static_cast<neu_attribute_e>(NEU_ATTRIBUTE_READ | NEU_ATTRIBUTE_WRITE));
    EXPECT_EQ(NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT,
              modbus_tag_to_point(&write_input, &point, base_1));
    neu_datatag_t write_bit = modbus_tag(
        "1!400001.1", NEU_TYPE_AUTO,
        static_cast<neu_attribute_e>(NEU_ATTRIBUTE_READ | NEU_ATTRIBUTE_WRITE));
    EXPECT_EQ(NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT,
              modbus_tag_to_point(&write_bit, &point, base_1));
}

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}