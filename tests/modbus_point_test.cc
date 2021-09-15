#include <netinet/in.h>

#include <gtest/gtest.h>

#include "modbus/modbus_point.h"

TEST(ModbusPointTest, AddPoint)
{
    modbus_point_context_t *ctx = modbus_point_init(NULL);
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400001", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!300001", MODBUS_B32));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400002", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400002", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!000002", MODBUS_B8));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!000001", MODBUS_B8));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!100001", MODBUS_B8));
    EXPECT_EQ(-1, modbus_point_add(ctx, (char *) "1!100001", MODBUS_B16));
    EXPECT_EQ(-1, modbus_point_add(ctx, (char *) "1!100001", MODBUS_B32));
    EXPECT_EQ(-1, modbus_point_add(ctx, (char *) "1!000001", MODBUS_B16));
    EXPECT_EQ(-1, modbus_point_add(ctx, (char *) "1!000001", MODBUS_B32));

    modbus_point_destory(ctx);
}

TEST(ModbusPointTest, NewSameDeviceCmd)
{
    modbus_point_context_t *ctx = modbus_point_init(NULL);
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400001", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!300001", MODBUS_B32));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400002", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400002", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!000002", MODBUS_B8));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!000001", MODBUS_B8));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!100001", MODBUS_B8));

    modbus_point_new_cmd(ctx);
    EXPECT_EQ(4, modbus_point_get_cmd_size(ctx));

    modbus_point_destory(ctx);
}

TEST(ModbusPointTest, NewSameAddrCmd)
{
    modbus_point_context_t *ctx = modbus_point_init(NULL);
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400001", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!300001", MODBUS_B32));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400002", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400002", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400003", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400002", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400004", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400007", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!000002", MODBUS_B8));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!000001", MODBUS_B8));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!100001", MODBUS_B8));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!000003", MODBUS_B8));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400008", MODBUS_B16));

    modbus_point_new_cmd(ctx);
    EXPECT_EQ(5, modbus_point_get_cmd_size(ctx));

    modbus_point_destory(ctx);
}

TEST(ModbusPointTest, NewCmd)
{
    modbus_point_context_t *ctx = modbus_point_init(NULL);
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "2!400001", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!300001", MODBUS_B32));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400002", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400002", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400003", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "2!400002", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400004", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "3!400007", MODBUS_B16));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "5!000002", MODBUS_B8));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!000001", MODBUS_B8));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!100001", MODBUS_B8));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!000003", MODBUS_B8));
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!400008", MODBUS_B16));

    modbus_point_new_cmd(ctx);
    EXPECT_EQ(10, modbus_point_get_cmd_size(ctx));

    modbus_point_destory(ctx);
}

static ssize_t read_error(void *arg, char *send_buf, ssize_t send_len,
                          char *recv_buf, ssize_t recv_len)
{
    return -1;
}

TEST(ModbusPointTest, ReadPointError)
{
    modbus_point_context_t *ctx = modbus_point_init(NULL);
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!300001", MODBUS_B32));

    modbus_point_new_cmd(ctx);
    EXPECT_EQ(1, modbus_point_get_cmd_size(ctx));
    EXPECT_EQ(-1, modbus_point_all_read(ctx, true, read_error));

    modbus_point_destory(ctx);
}

static ssize_t write_coil_ok(void *arg, char *send_buf, ssize_t send_len,
                             char *recv_buf, ssize_t recv_len)
{
    EXPECT_EQ(arg, (void *) NULL);
    EXPECT_EQ(send_len,
              sizeof(struct modbus_header) + sizeof(struct modbus_code) +
                  sizeof(struct modbus_pdu_m_write_request) + 1);
    struct modbus_header *header = (struct modbus_header *) send_buf;
    struct modbus_code *  code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_m_write_request *request =
        (struct modbus_pdu_m_write_request *) &code[1];
    uint8_t *data = (uint8_t *) &request[1];

    EXPECT_EQ(header->process_no, 0);
    EXPECT_EQ(header->flag, 0);
    EXPECT_EQ(htons(header->len), send_len - sizeof(struct modbus_header));

    EXPECT_EQ(code->device_address, 1);
    EXPECT_EQ(code->function_code, MODBUS_WRITE_M_COIL);

    EXPECT_EQ(htons(request->n_reg), 1);
    EXPECT_EQ(request->n_byte, 1);

    header = (struct modbus_header *) recv_buf;
    code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_write_response *response =
        (struct modbus_pdu_write_response *) &code[1];

    header->process_no   = 0;
    header->flag         = 0;
    header->len          = htons(sizeof(struct modbus_code) +
                        sizeof(struct modbus_pdu_write_response));
    code->device_address = 1;
    code->function_code  = MODBUS_WRITE_M_COIL;
    response->start_addr = 0;
    response->n_reg      = htons(1);

    return sizeof(struct modbus_header) + sizeof(struct modbus_code) +
        sizeof(struct modbus_pdu_write_response);
}

TEST(ModbusPointTest, WriteCoilPoint)
{
    modbus_point_context_t *ctx = modbus_point_init(NULL);

    modbus_data_t data;
    data.type       = MODBUS_B8;
    data.val.val_u8 = 1;

    EXPECT_EQ(
        0, modbus_point_write((char *) "1!00001", &data, write_coil_ok, NULL));

    modbus_point_destory(ctx);
}

static ssize_t write_hold16_ok(void *arg, char *send_buf, ssize_t send_len,
                               char *recv_buf, ssize_t recv_len)
{
    EXPECT_EQ(arg, (void *) NULL);
    EXPECT_EQ(send_len,
              sizeof(struct modbus_header) + sizeof(struct modbus_code) +
                  sizeof(struct modbus_pdu_m_write_request) + 2);
    struct modbus_header *header = (struct modbus_header *) send_buf;
    struct modbus_code *  code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_m_write_request *request =
        (struct modbus_pdu_m_write_request *) &code[1];
    uint8_t *data = (uint8_t *) &request[1];

    EXPECT_EQ(header->process_no, 0);
    EXPECT_EQ(header->flag, 0);
    EXPECT_EQ(htons(header->len), send_len - sizeof(struct modbus_header));

    EXPECT_EQ(code->device_address, 1);
    EXPECT_EQ(code->function_code, MODBUS_WRITE_M_HOLD_REG);

    EXPECT_EQ(htons(request->n_reg), 1);
    EXPECT_EQ(request->n_byte, 2);

    header = (struct modbus_header *) recv_buf;
    code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_write_response *response =
        (struct modbus_pdu_write_response *) &code[1];

    header->process_no   = 0;
    header->flag         = 0;
    header->len          = htons(sizeof(struct modbus_code) +
                        sizeof(struct modbus_pdu_write_response));
    code->device_address = 1;
    code->function_code  = MODBUS_WRITE_M_HOLD_REG;
    response->start_addr = 0;
    response->n_reg      = htons(1);

    return sizeof(struct modbus_header) + sizeof(struct modbus_code) +
        sizeof(struct modbus_pdu_write_response);
}

TEST(ModbusPointTest, WriteHold16Point)
{
    modbus_point_context_t *ctx = modbus_point_init(NULL);

    modbus_data_t data;
    data.type        = MODBUS_B16;
    data.val.val_u16 = 1;

    EXPECT_EQ(
        0,
        modbus_point_write((char *) "1!40001", &data, write_hold16_ok, NULL));

    modbus_point_destory(ctx);
}

static ssize_t write_hold32_ok(void *arg, char *send_buf, ssize_t send_len,
                               char *recv_buf, ssize_t recv_len)
{
    EXPECT_EQ(arg, (void *) NULL);
    EXPECT_EQ(send_len,
              sizeof(struct modbus_header) + sizeof(struct modbus_code) +
                  sizeof(struct modbus_pdu_m_write_request) + 4);
    struct modbus_header *header = (struct modbus_header *) send_buf;
    struct modbus_code *  code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_m_write_request *request =
        (struct modbus_pdu_m_write_request *) &code[1];
    uint8_t *data = (uint8_t *) &request[1];

    EXPECT_EQ(header->process_no, 0);
    EXPECT_EQ(header->flag, 0);
    EXPECT_EQ(htons(header->len), send_len - sizeof(struct modbus_header));

    EXPECT_EQ(code->device_address, 1);
    EXPECT_EQ(code->function_code, MODBUS_WRITE_M_HOLD_REG);

    EXPECT_EQ(htons(request->n_reg), 2);
    EXPECT_EQ(request->n_byte, 4);

    header = (struct modbus_header *) recv_buf;
    code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_write_response *response =
        (struct modbus_pdu_write_response *) &code[1];

    header->process_no = 0;
    header->flag       = 0;
    header->len        = htons(sizeof(struct modbus_code) +
                        sizeof(struct modbus_pdu_write_response));

    code->device_address = 1;
    code->function_code  = MODBUS_WRITE_M_HOLD_REG;
    response->start_addr = 0;
    response->n_reg      = htons(2);

    return sizeof(struct modbus_header) + sizeof(struct modbus_code) +
        sizeof(struct modbus_pdu_write_response);
}

TEST(ModbusPointTest, WriteHold32Point)
{
    modbus_point_context_t *ctx = modbus_point_init(NULL);

    modbus_data_t data;
    data.type        = MODBUS_B32;
    data.val.val_u32 = 1;

    EXPECT_EQ(
        0,
        modbus_point_write((char *) "1!40001", &data, write_hold32_ok, NULL));

    modbus_point_destory(ctx);
}

static ssize_t read_coil(void *arg, char *send_buf, ssize_t send_len,
                         char *recv_buf, ssize_t recv_len)
{
    EXPECT_EQ(arg, (void *) NULL);
    EXPECT_EQ(send_len,
              sizeof(struct modbus_header) + sizeof(struct modbus_code) +
                  sizeof(struct modbus_pdu_read_request));
    struct modbus_header *          header = (struct modbus_header *) send_buf;
    struct modbus_code *            code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_read_request *request =
        (struct modbus_pdu_read_request *) &code[1];

    EXPECT_EQ(htons(header->process_no), 1);
    EXPECT_EQ(header->flag, 0);
    EXPECT_EQ(htons(header->len), send_len - sizeof(struct modbus_header));

    EXPECT_EQ(code->device_address, 1);
    EXPECT_EQ(code->function_code, MODBUS_READ_COIL);

    EXPECT_EQ(htons(request->start_addr), 0);
    EXPECT_EQ(htons(request->n_reg), 1);

    header = (struct modbus_header *) recv_buf;
    code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_read_response *response =
        (struct modbus_pdu_read_response *) &code[1];

    header->process_no = htons(1);
    header->flag       = 0;
    header->len        = htons(sizeof(struct modbus_code) +
                        sizeof(struct modbus_pdu_read_response) + 1);

    code->device_address = 1;
    code->function_code  = MODBUS_READ_COIL;
    response->n_byte     = htons(1);

    return sizeof(struct modbus_header) + sizeof(struct modbus_code) +
        sizeof(struct modbus_pdu_read_response) + 1;
}

TEST(ModbusPointTest, ReadCoilPoint)
{
    modbus_point_context_t *ctx = modbus_point_init(NULL);
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!00001", MODBUS_B8));

    modbus_point_new_cmd(ctx);
    EXPECT_EQ(1, modbus_point_get_cmd_size(ctx));
    EXPECT_EQ(0, modbus_point_all_read(ctx, true, read_coil));

    modbus_point_destory(ctx);
}

static ssize_t read_input(void *arg, char *send_buf, ssize_t send_len,
                          char *recv_buf, ssize_t recv_len)
{
    EXPECT_EQ(arg, (void *) NULL);
    EXPECT_EQ(send_len,
              sizeof(struct modbus_header) + sizeof(struct modbus_code) +
                  sizeof(struct modbus_pdu_read_request));
    struct modbus_header *          header = (struct modbus_header *) send_buf;
    struct modbus_code *            code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_read_request *request =
        (struct modbus_pdu_read_request *) &code[1];

    EXPECT_EQ(htons(header->process_no), 1);
    EXPECT_EQ(header->flag, 0);
    EXPECT_EQ(htons(header->len), send_len - sizeof(struct modbus_header));

    EXPECT_EQ(code->device_address, 1);
    EXPECT_EQ(code->function_code, MODBUS_READ_INPUT_CONTACT);

    EXPECT_EQ(htons(request->start_addr), 0);
    EXPECT_EQ(htons(request->n_reg), 1);

    header = (struct modbus_header *) recv_buf;
    code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_read_response *response =
        (struct modbus_pdu_read_response *) &code[1];

    header->process_no = htons(1);
    header->flag       = 0;
    header->len        = htons(sizeof(struct modbus_code) +
                        sizeof(struct modbus_pdu_read_response) + 1);

    code->device_address = 1;
    code->function_code  = MODBUS_READ_INPUT_CONTACT;
    response->n_byte     = htons(1);

    return sizeof(struct modbus_header) + sizeof(struct modbus_code) +
        sizeof(struct modbus_pdu_read_response) + 1;
}

TEST(ModbusPointTest, ReadInputPoint)
{
    modbus_point_context_t *ctx = modbus_point_init(NULL);
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!10001", MODBUS_B8));

    modbus_point_new_cmd(ctx);
    EXPECT_EQ(1, modbus_point_get_cmd_size(ctx));
    EXPECT_EQ(0, modbus_point_all_read(ctx, true, read_input));

    modbus_point_destory(ctx);
}

static ssize_t read_hold_register16(void *arg, char *send_buf, ssize_t send_len,
                                    char *recv_buf, ssize_t recv_len)
{
    EXPECT_EQ(arg, (void *) NULL);
    EXPECT_EQ(send_len,
              sizeof(struct modbus_header) + sizeof(struct modbus_code) +
                  sizeof(struct modbus_pdu_read_request));
    struct modbus_header *          header = (struct modbus_header *) send_buf;
    struct modbus_code *            code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_read_request *request =
        (struct modbus_pdu_read_request *) &code[1];

    EXPECT_EQ(htons(header->process_no), 1);
    EXPECT_EQ(header->flag, 0);
    EXPECT_EQ(htons(header->len), send_len - sizeof(struct modbus_header));

    EXPECT_EQ(code->device_address, 1);
    EXPECT_EQ(code->function_code, MODBUS_READ_HOLD_REG);

    EXPECT_EQ(htons(request->start_addr), 0);
    EXPECT_EQ(htons(request->n_reg), 1);

    header = (struct modbus_header *) recv_buf;
    code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_read_response *response =
        (struct modbus_pdu_read_response *) &code[1];

    header->process_no = htons(1);
    header->flag       = 0;
    header->len        = htons(sizeof(struct modbus_code) +
                        sizeof(struct modbus_pdu_read_response) + 2);

    code->device_address = 1;
    code->function_code  = MODBUS_READ_HOLD_REG;
    response->n_byte     = htons(2);

    return sizeof(struct modbus_header) + sizeof(struct modbus_code) +
        sizeof(struct modbus_pdu_read_response) + 2;
}

TEST(ModbusPointTest, ReadHoldRegister16Point)
{
    modbus_point_context_t *ctx = modbus_point_init(NULL);
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!40001", MODBUS_B16));

    modbus_point_new_cmd(ctx);
    EXPECT_EQ(1, modbus_point_get_cmd_size(ctx));
    EXPECT_EQ(0, modbus_point_all_read(ctx, true, read_hold_register16));

    modbus_point_destory(ctx);
}

static ssize_t read_hold_register32(void *arg, char *send_buf, ssize_t send_len,
                                    char *recv_buf, ssize_t recv_len)
{
    EXPECT_EQ(arg, (void *) NULL);
    EXPECT_EQ(send_len,
              sizeof(struct modbus_header) + sizeof(struct modbus_code) +
                  sizeof(struct modbus_pdu_read_request));
    struct modbus_header *          header = (struct modbus_header *) send_buf;
    struct modbus_code *            code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_read_request *request =
        (struct modbus_pdu_read_request *) &code[1];

    EXPECT_EQ(htons(header->process_no), 1);
    EXPECT_EQ(header->flag, 0);
    EXPECT_EQ(htons(header->len), send_len - sizeof(struct modbus_header));

    EXPECT_EQ(code->device_address, 1);
    EXPECT_EQ(code->function_code, MODBUS_READ_HOLD_REG);

    EXPECT_EQ(htons(request->start_addr), 0);
    EXPECT_EQ(htons(request->n_reg), 2);

    header = (struct modbus_header *) recv_buf;
    code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_read_response *response =
        (struct modbus_pdu_read_response *) &code[1];

    header->process_no = htons(1);
    header->flag       = 0;
    header->len        = htons(sizeof(struct modbus_code) +
                        sizeof(struct modbus_pdu_read_response) + 4);

    code->device_address = 1;
    code->function_code  = MODBUS_READ_HOLD_REG;
    response->n_byte     = htons(4);

    return sizeof(struct modbus_header) + sizeof(struct modbus_code) +
        sizeof(struct modbus_pdu_read_response) + 4;
}

TEST(ModbusPointTest, ReadHoldRegister32Point)
{
    modbus_point_context_t *ctx = modbus_point_init(NULL);
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!40001", MODBUS_B32));

    modbus_point_new_cmd(ctx);
    EXPECT_EQ(1, modbus_point_get_cmd_size(ctx));
    EXPECT_EQ(0, modbus_point_all_read(ctx, true, read_hold_register32));

    modbus_point_destory(ctx);
}

static ssize_t read_input_register(void *arg, char *send_buf, ssize_t send_len,
                                   char *recv_buf, ssize_t recv_len)
{
    EXPECT_EQ(arg, (void *) NULL);
    EXPECT_EQ(send_len,
              sizeof(struct modbus_header) + sizeof(struct modbus_code) +
                  sizeof(struct modbus_pdu_read_request));
    struct modbus_header *          header = (struct modbus_header *) send_buf;
    struct modbus_code *            code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_read_request *request =
        (struct modbus_pdu_read_request *) &code[1];

    EXPECT_EQ(htons(header->process_no), 1);
    EXPECT_EQ(header->flag, 0);
    EXPECT_EQ(htons(header->len), send_len - sizeof(struct modbus_header));

    EXPECT_EQ(code->device_address, 1);
    EXPECT_EQ(code->function_code, MODBUS_READ_INPUT_REG);

    EXPECT_EQ(htons(request->start_addr), 0);
    EXPECT_EQ(htons(request->n_reg), 1);

    header = (struct modbus_header *) recv_buf;
    code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_read_response *response =
        (struct modbus_pdu_read_response *) &code[1];

    header->process_no = htons(1);
    header->flag       = 0;
    header->len        = htons(sizeof(struct modbus_code) +
                        sizeof(struct modbus_pdu_read_response) + 2);

    code->device_address = 1;
    code->function_code  = MODBUS_READ_INPUT_REG;
    response->n_byte     = htons(2);

    return sizeof(struct modbus_header) + sizeof(struct modbus_code) +
        sizeof(struct modbus_pdu_read_response) + 2;
}

TEST(ModbusPointTest, ReadInputRegisterPoint)
{
    modbus_point_context_t *ctx = modbus_point_init(NULL);
    EXPECT_EQ(0, modbus_point_add(ctx, (char *) "1!30001", MODBUS_B16));

    modbus_point_new_cmd(ctx);
    EXPECT_EQ(1, modbus_point_get_cmd_size(ctx));
    EXPECT_EQ(0, modbus_point_all_read(ctx, true, read_input_register));

    modbus_point_destory(ctx);
}
