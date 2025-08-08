// 报告服务属性定义文件

#include "hidd_le_prf_int.h"
#include <string.h>
#include "esp_log.h"
#include "main.h"
/// @brief Characteristic Presentation Format 结构体
/// 用于描述 BLE HID 设备中某个 Characteristic 的数据表示格式信息
struct prf_char_pres_fmt
{
    /// Unit (The Unit is a UUID)
    uint16_t unit;
    /// Description
    uint16_t description;
    /// Format
    uint8_t format;
    /// Exponent
    uint8_t exponent;
    /// Name space
    uint8_t name_space;
};

// 存储 HID报告的映射信息
static hid_report_map_t hid_rpt_map[HID_NUM_REPORTS];
// static hid_report_map_t hid_rpt_map_gamepad[HID_NUM_REPORTS];



// MYGT 格式：定义游戏手柄的 HID 报告描述符

static const uint8_t hidReportMapMYGTGamePad[] = {
    // ID 1--------------------------------------------
    0x05, 0x01, // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02, // 用途：鼠标
    0xA1, 0x01, // Collection (Application)开集合
    0x09, 0x01, //   用途：指针
    0xA1, 0x00, //   Collection (Physical)开集合
    0x85, 0x01, //     Report ID (1)
    0x05, 0x09, //     Usage Page (Button)
    0x19, 0x01, //     Usage Minimum (0x01)
    0x29, 0x03, //     Usage Maximum (0x03)
    0x15, 0x00, //     Logical Minimum (0)
    0x25, 0x01, //     Logical Maximum (1)
    0x75, 0x01, //     Report Size (1)
    0x95, 0x03, //     Report Count (3)
    0x81, 0x02, //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)//三个按键，每个按键占用1位
    0x75, 0x05, //     Report Size (5)
    0x95, 0x01, //     Report Count (1)
    0x81, 0x03, //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)//填充5位，方便对齐到一个字节
    0x05, 0x01, //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30, //     Usage (X)
    0x09, 0x31, //     Usage (Y)
    0x09, 0x38, //     Usage (Wheel)
    0x15, 0x81, //     Logical Minimum (-127)
    0x25, 0x7F, //     Logical Maximum (127)
    0x75, 0x08, //     Report Size (8)
    0x95, 0x03, //     Report Count (3)
    0x81, 0x06, //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,       //   End Collection关集合
    0xC0,       // End Collection关集合

    // ID 3--------------------------------------------
    0x05, 0x0C,       // Usage Page (Consumer)
    0x09, 0x01,       // Usage (Consumer Control)
    0xA1, 0x01,       // Collection (Application)
    0x85, 0x03,       //   Report ID (3)
    0x19, 0x01,       //   Usage Minimum (Consumer Control)
    0x2A, 0x9C, 0x02, //   Usage Maximum (AC Distribute Vertically)
    0x15, 0x01,       //   Logical Minimum (1)
    0x26, 0x9C, 0x02, //   Logical Maximum (668)
    0x75, 0x10,       //   Report Size (16)
    0x95, 0x01,       //   Report Count (1)
    0x81, 0x00,       //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,             // End Collection

    // ID 4--------------------------------------------
    0x05, 0x01, // 使用页面 (通用桌面控制)
    0x09, 0x05, // 使用 (游戏手柄)
    0xA1, 0x01, // 集合 (应用层) - 开始定义游戏手柄设备

    0x09, 0x01,       //   使用 (指针) - 定义指针控制组件
    0xA1, 0x00,       //   集合 (物理层) - 开始定义物理控制组件
    0x85, 0x04,       //     报告ID (4) - 此报告的标识符为4
    0x09, 0x30,       //     使用 (X轴) - X轴控制
    0x09, 0x31,       //     使用 (Y轴) - Y轴控制
    0x09, 0x32,       //     使用 (Z轴) - Z轴控制
    0x09, 0x35,       //     使用 (Rz轴) - 旋转Z轴控制
    0x15, 0x00,       //     逻辑最小值 (0) - 轴值范围起始
    0x26, 0xFF, 0x00, //     逻辑最大值 (255) - 轴值范围结束
    0x75, 0x08,       //     报告大小 (8位) - 每个轴值占8位
    0x95, 0x04,       //     报告数量 (4) - 共4个轴(X,Y,Z,Rz)
    0x81, 0x02,       //     输入 (数据,变量,绝对值) - 轴数据以绝对值形式输入
    0xC0,             //   结束集合 - 结束物理层集合

    0x09, 0x39,       //   使用 (方向键) - 定义方向键
    0x15, 0x00,       //   逻辑最小值 (0) - 方向键最小值
    0x25, 0x07,       //   逻辑最大值 (7) - 方向键最大值(8个方向)
    0x35, 0x00,       //   物理最小值 (0) - 物理最小值(角度)
    0x46, 0x3B, 0x01, //   物理最大值 (315) - 物理最大值(角度，315度)
    0x65, 0x14,       //   单位 (角度单位:度)
    0x75, 0x04,       //   报告大小 (4位) - 方向键占4位
    0x95, 0x01,       //   报告数量 (1) - 1个方向键
    0x81, 0x42,       //   输入 (数据,变量,绝对值,包含空状态) - 方向键数据
    0x75, 0x04,       //   报告大小 (4位) - 填充位大小
    0x95, 0x01,       //   报告数量 (1) - 1组填充位
    0x81, 0x03,       //   输入 (常量,变量,绝对值) - 填充位(无实际意义)

    0x05, 0x09, //   使用页面 (按钮) - 定义按钮
    0x19, 0x01, //   使用最小值 (0x01) - 按钮起始编号
    0x29, 0x0F, //   使用最大值 (0x0F) - 按钮结束编号(共16个按钮)
    0x15, 0x00, //   逻辑最小值 (0) - 按钮状态:0=未按下
    0x25, 0x01, //   逻辑最大值 (1) - 按钮状态:1=按下
    0x75, 0x01, //   报告大小 (1位) - 每个按钮占1位
    0x95, 0x10, //   报告数量 (16) - 共16个按钮
    0x45, 0x00, //   物理最大值 (0) - 物理最大值
    0x65, 0x00, //   单位 (无) - 无单位
    0x81, 0x02, //   输入 (数据,变量,绝对值) - 按钮状态数据

    0x05, 0x02,       //   使用页面 (模拟控制) - 定义模拟控制
    0x09, 0xC4,       //   使用 (加速踏板) - 加速控制
    0x09, 0xC5,       //   使用 (制动踏板) - 制动控制
    0x15, 0x00,       //   逻辑最小值 (0) - 踏板值范围起始
    0x26, 0xFF, 0x00, //   逻辑最大值 (255) - 踏板值范围结束
    0x35, 0x00,       //   物理最小值 (0) - 物理最小值
    0x46, 0x3B, 0x01, //   物理最大值 (315) - 物理最大值
    0x65, 0x14,       //   单位 (角度单位:度)
    0x75, 0x08,       //   报告大小 (8位) - 每个踏板值占8位
    0x95, 0x02,       //   报告数量 (2) - 共2个踏板(加速和制动)
    0x81, 0x02,       //   输入 (数据,变量,绝对值) - 踏板数据
    0xC0,             // 结束集合 - 结束应用层集合
};

/// Battery Service Attributes Indexes
enum
{
    BAS_IDX_SVC,

    BAS_IDX_BATT_LVL_CHAR,
    BAS_IDX_BATT_LVL_VAL,
    BAS_IDX_BATT_LVL_NTF_CFG,
    BAS_IDX_BATT_LVL_PRES_FMT,

    BAS_IDX_NB,
};

#define HI_UINT16(a) (((a) >> 8) & 0xFF)
#define LO_UINT16(a) ((a) & 0xFF)
#define PROFILE_NUM 1
#define PROFILE_APP_IDX 0

struct gatts_profile_inst
{
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
};

// hidd_clcb[HID_MAX_APPS]：HID 连接控制块数组，用于管理多个 HID 应用的连接
// gatt_if：GATT 接口标识，用于蓝牙 GATT 层通信
// enabled：表示 HID 服务是否启用的标志
// is_take：可能用于标识资源是否被占用
// is_primery：可能表示是否为主要服务
// hidd_inst：HID 实例相关信息
// hidd_cb：HID 事件回调函数，用于处理 HID 相关事件
// inst_id：实例 ID，用于标识不同的 HID 实例
hidd_le_env_t hidd_le_env;

// HID report map length

uint8_t hidReportMapLen = sizeof(hidReportMapMYGTGamePad);
uint8_t hidProtocolMode = HID_PROTOCOL_MODE_REPORT;

// HID report mapping table
// static hidRptMap_t  hidRptMap[HID_NUM_REPORTS];

// HID Information characteristic value
static const uint8_t hidInfo[HID_INFORMATION_LEN] = {
    LO_UINT16(0x0111), HI_UINT16(0x0111), // bcdHID (USB HID 版本)
    0x00,                                 // 国家码
    HID_KBD_FLAGS         // 支持远程唤醒，不支持正常连接
};

// HID 外部报告引用描述符
static uint16_t hidExtReportRefDesc = ESP_GATT_UUID_BATTERY_LEVEL;


// 合并报文
// ID1鼠标
static uint8_t hidReportRefGamePadMouseIn[HID_REPORT_REF_LEN] =
    {HID_RPT_ID_GAMEPAD_MOUSE_IN, HID_REPORT_TYPE_INPUT};

// ID3消费类
static uint8_t hidReportRefGamePadCCIn[HID_REPORT_REF_LEN] =
    {HID_RPT_ID_GAMEPAD_CC_IN, HID_REPORT_TYPE_INPUT};

// ID4手柄
static uint8_t hidReportRefGamePadStickIn[HID_REPORT_REF_LEN] =
    {HID_RPT_ID_GAMEPAD_STICK_IN, HID_REPORT_TYPE_INPUT};
// 不知道是啥
static uint8_t hidReportRefFeature[HID_REPORT_REF_LEN] =
    {HID_RPT_ID_FEATURE, HID_REPORT_TYPE_FEATURE};


/*
 *  Heart Rate PROFILE ATTRIBUTES
 ****************************************************************************************
 */

/// hid Service uuid
static uint16_t hid_le_svc = ATT_SVC_HID;
uint16_t hid_count = 0;
esp_gatts_incl_svc_desc_t incl_svc = {0};

#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))
/// the uuid definition
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t include_service_uuid = ESP_GATT_UUID_INCLUDE_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint16_t hid_info_char_uuid = ESP_GATT_UUID_HID_INFORMATION;
static const uint16_t hid_report_map_uuid = ESP_GATT_UUID_HID_REPORT_MAP;
static const uint16_t hid_control_point_uuid = ESP_GATT_UUID_HID_CONTROL_POINT;
static const uint16_t hid_report_uuid = ESP_GATT_UUID_HID_REPORT;
static const uint16_t hid_proto_mode_uuid = ESP_GATT_UUID_HID_PROTO_MODE;
// static const uint16_t hid_kb_input_uuid = ESP_GATT_UUID_HID_BT_KB_INPUT;
// static const uint16_t hid_kb_output_uuid = ESP_GATT_UUID_HID_BT_KB_OUTPUT;
// static const uint16_t hid_mouse_input_uuid = ESP_GATT_UUID_HID_BT_MOUSE_INPUT;
static const uint16_t hid_repot_map_ext_desc_uuid = ESP_GATT_UUID_EXT_RPT_REF_DESCR;
static const uint16_t hid_report_ref_descr_uuid = ESP_GATT_UUID_RPT_REF_DESCR;
/// the propoty definition
// static const uint8_t char_prop_notify = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_write_nr = ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
static const uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_read_notify = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
// static const uint8_t char_prop_read_write_notify = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
// static const uint8_t char_prop_read_write_write_nr = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR;

/// 电池服务
static const uint16_t battary_svc = ESP_GATT_UUID_BATTERY_SERVICE_SVC;

static const uint16_t bat_lev_uuid = ESP_GATT_UUID_BATTERY_LEVEL;
static const uint8_t bat_lev_ccc[2] = {0x00, 0x00};
static const uint16_t char_format_uuid = ESP_GATT_UUID_CHAR_PRESENT_FORMAT;

static uint8_t battary_lev = 100;
/// 完整的HRS数据库描述-用于向数据库中添加属性
static const esp_gatts_attr_db_t bas_att_db[BAS_IDX_NB] =
    {
        // 电池使用声明
        [BAS_IDX_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(battary_svc), (uint8_t *)&battary_svc}},

        // 电池电量声明
        [BAS_IDX_BATT_LVL_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_notify}},

        // 电池电量值
        [BAS_IDX_BATT_LVL_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&bat_lev_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), &battary_lev}},

        // 电池电量特性-客户端特性配置描述符
        [BAS_IDX_BATT_LVL_NTF_CFG] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t), sizeof(bat_lev_ccc), (uint8_t *)bat_lev_ccc}},

        // 电池电量报告描述符
        [BAS_IDX_BATT_LVL_PRES_FMT] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&char_format_uuid, ESP_GATT_PERM_READ, sizeof(struct prf_char_pres_fmt), 0, NULL}},
};

///
/// \brief 定义HID设备的GATT数据库结构
///
/// 该数组定义了HID设备在GATT服务中的各个属性描述符。
/// 每个条目对应一个特定的GATT属性，包括服务声明、特征值声明、特征值内容等。
/// 这些属性用于实现HID服务的蓝牙低功耗（BLE）通信。
///
/// \note 该数组大小为 HIDD_LE_IDX_NB，每个元素对应一个特定的GATT属性描述符。
///
static esp_gatts_attr_db_t hidd_le_gatt_db[HIDD_LE_IDX_NB] =
    {
        // 属性数据表
        // ESP_GATT_AUTO_RSP自动回复

        /**typedef struct
        {
        uint16_t uuid_length;              UUID长度
        uint8_t  *uuid_p;                  UUID指针
        uint16_t perm;                     权限
        uint16_t max_length;               value的最大长度
        uint16_t length;                   当前长度
        uint8_t  *value;                   指针
        } esp_attr_desc_t;
        **/

        // Primary：2800，只能有一个。但是可以被多次使用。用于标识自己是主服务
        [HIDD_LE_IDX_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ_ENCRYPTED, sizeof(uint16_t), sizeof(hid_le_svc), (uint8_t *)&hid_le_svc}},

        // HID Service Declaration
        // Include：0x2802
        // 包含声明
        [HIDD_LE_IDX_INCL_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&include_service_uuid, ESP_GATT_PERM_READ, sizeof(esp_gatts_incl_svc_desc_t), sizeof(esp_gatts_incl_svc_desc_t), (uint8_t *)&incl_svc}},

        // 特征声明
        [HIDD_LE_IDX_HID_INFO_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read}},
        // USB HID版本、不支持国家代码、远程唤醒和正常连接
        [HIDD_LE_IDX_HID_INFO_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&hid_info_char_uuid, ESP_GATT_PERM_READ, sizeof(hids_hid_info_t), sizeof(hidInfo), (uint8_t *)&hidInfo}},

        // HID控制点特性声明，write with no reply
        [HIDD_LE_IDX_HID_CTNL_PT_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write_nr}},
        // HID控制点特征值
        [HIDD_LE_IDX_HID_CTNL_PT_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&hid_control_point_uuid, ESP_GATT_PERM_WRITE, sizeof(uint8_t), 0, NULL}},




        // 报告描述符特征声明
        [HIDD_LE_IDX_REPORT_MAP_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read}},
        // 报告描述符特征值
        //-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
        [HIDD_LE_IDX_REPORT_MAP_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&hid_report_map_uuid, ESP_GATT_PERM_READ, HIDD_LE_REPORT_MAP_MAX_LEN, sizeof(hidReportMapMYGTGamePad), (uint8_t *)&hidReportMapMYGTGamePad}},
        //-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
        // Report Map Characteristic - External Report Reference Descriptor
        [HIDD_LE_IDX_REPORT_MAP_EXT_REP_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&hid_repot_map_ext_desc_uuid, ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&hidExtReportRefDesc}},

        
        
        // Protocol Mode Characteristic Declaration
        // 协议模式：是启动模式还是报告模式（一般都是报告模式）
        [HIDD_LE_IDX_PROTO_MODE_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},
        // 协议模式值
        [HIDD_LE_IDX_PROTO_MODE_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&hid_proto_mode_uuid, (ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE), sizeof(uint8_t), sizeof(hidProtocolMode), (uint8_t *)&hidProtocolMode}},


        // FIXME:写一下手柄的描述符
        // 手柄鼠标输入报告声明
        [HIDD_LE_IDX_REPORT_GAMEPAD_MOUSE_IN_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_notify}},
        // 手柄鼠标输入报告值（只读，无需分配空间）
        [HIDD_LE_IDX_REPORT_GAMEPAD_MOUSE_IN_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&hid_report_uuid, ESP_GATT_PERM_READ, HIDD_LE_REPORT_MAX_LEN, 0, NULL}},
        // 手柄客户端配置描述符
        [HIDD_LE_IDX_REPORT_GAMEPAD_MOUSE_IN_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, (ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE), sizeof(uint16_t), 0, NULL}},
        // 手柄鼠标输入报告参数：包含鼠标输入ID，输入还是输出，报文长度
        [HIDD_LE_IDX_REPORT_GAMEPAD_MOUSE_REP_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&hid_report_ref_descr_uuid, ESP_GATT_PERM_READ, sizeof(hidReportRefGamePadMouseIn), sizeof(hidReportRefGamePadMouseIn), hidReportRefGamePadMouseIn}},

        // 手柄CC
        [HIDD_LE_IDX_REPORT_GAMEPAD_CC_IN_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_notify}},
        [HIDD_LE_IDX_REPORT_GAMEPAD_CC_IN_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&hid_report_uuid, ESP_GATT_PERM_READ, HIDD_LE_REPORT_MAX_LEN, 0, NULL}},
        [HIDD_LE_IDX_REPORT_GAMEPAD_CC_IN_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, (ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE), sizeof(uint16_t), 0, NULL}},
        [HIDD_LE_IDX_REPORT_GAMEPAD_CC_REP_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&hid_report_ref_descr_uuid, ESP_GATT_PERM_READ, sizeof(hidReportRefGamePadCCIn), sizeof(hidReportRefGamePadCCIn), hidReportRefGamePadCCIn}},
        // 手柄Stick
        [HIDD_LE_IDX_REPORT_GAMEPAD_STICK_IN_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_notify}},
        [HIDD_LE_IDX_REPORT_GAMEPAD_STICK_IN_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&hid_report_uuid, ESP_GATT_PERM_READ, HIDD_LE_REPORT_MAX_LEN, 0, NULL}},
        [HIDD_LE_IDX_REPORT_GAMEPAD_STICK_IN_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, (ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE), sizeof(uint16_t), 0, NULL}},
        [HIDD_LE_IDX_REPORT_GAMEPAD_STICK_REP_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&hid_report_ref_descr_uuid, ESP_GATT_PERM_READ, sizeof(hidReportRefGamePadStickIn), sizeof(hidReportRefGamePadStickIn), hidReportRefGamePadStickIn}},




        // Report Characteristic Declaration
        [HIDD_LE_IDX_REPORT_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},
        // Report Characteristic Value
        [HIDD_LE_IDX_REPORT_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&hid_report_uuid, ESP_GATT_PERM_READ, HIDD_LE_REPORT_MAX_LEN, 0, NULL}},
        // Report Characteristic - Report Reference Descriptor
        [HIDD_LE_IDX_REPORT_REP_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&hid_report_ref_descr_uuid, ESP_GATT_PERM_READ, sizeof(hidReportRefFeature), sizeof(hidReportRefFeature), hidReportRefFeature}},
};

static void hid_add_id_tbl(void);

/**
 * @brief HID设备GATT服务回调处理函数（important）
 *
 * 处理来自GATT服务器的各种事件，包括注册、连接、断开连接、写入等事件
 * 根据不同的事件类型执行相应的操作，如创建服务、处理连接、处理写入请求等
 *
 * @param[in] event GATT服务器事件类型
 * @param[in] gatts_if GATT服务接口标识符
 * @param[in] param 指向事件参数结构体的指针
 */
//（important）
void esp_hidd_prf_cb_hdl(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                         esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GATTS_REG_EVT:
    {
        // 配置本地蓝牙图标为GamePad
        esp_ble_gap_config_local_icon(ESP_BLE_APPEARANCE_HID_GAMEPAD);
        esp_hidd_cb_param_t hidd_param;
        hidd_param.init_finish.state = param->reg.status;
        if (param->reg.app_id == HIDD_APP_ID)
        {
            hidd_le_env.gatt_if = gatts_if;
            if (hidd_le_env.hidd_cb != NULL)
            {
                (hidd_le_env.hidd_cb)(ESP_HIDD_EVENT_REG_FINISH, &hidd_param);
                // 创建电池服务HID属性表
                // 先创建电池的属性表，后续判断电池属性表是否创建成功，如果创建成功，则创建HID属性表（在下面一点的case：）
                hidd_battery_create_service(hidd_le_env.gatt_if);
            }
        }
        if (param->reg.app_id == BATTRAY_APP_ID)
        {
            hidd_param.init_finish.gatts_if = gatts_if;
            if (hidd_le_env.hidd_cb != NULL)
            {
                (hidd_le_env.hidd_cb)(ESP_BAT_EVENT_REG, &hidd_param);
            }
        }

        break;
    }
    case ESP_GATTS_CONF_EVT:
    {
        break;
    }
    case ESP_GATTS_CREATE_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT:
    {
        // 处理HID设备连接事件
        esp_hidd_cb_param_t cb_param = {0};
        // ESP_LOGI(HID_LE_PRF_TAG, "HID connection establish, conn_id = %x", param->connect.conn_id);
        memcpy(cb_param.connect.remote_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        cb_param.connect.conn_id = param->connect.conn_id;
        hidd_clcb_alloc(param->connect.conn_id, param->connect.remote_bda);
        esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_NO_MITM);
        if (hidd_le_env.hidd_cb != NULL)
        {
            (hidd_le_env.hidd_cb)(ESP_HIDD_EVENT_BLE_CONNECT, &cb_param);
        }
        break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
    {
        // 处理HID设备断开连接事件
        if (hidd_le_env.hidd_cb != NULL)
        {
            (hidd_le_env.hidd_cb)(ESP_HIDD_EVENT_BLE_DISCONNECT, NULL);
        }
        hidd_clcb_dealloc(param->disconnect.conn_id);
        break;
    }
    case ESP_GATTS_CLOSE_EVT:
        break;
    case ESP_GATTS_WRITE_EVT:
    {
        // 处理客户端写入特征值事件（不处理）
        // esp_hidd_cb_param_t cb_param = {0};
        // if (param->write.handle == hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_REPORT_LED_OUT_VAL])
        // {
        //     cb_param.led_write.conn_id = param->write.conn_id;
        //     cb_param.led_write.report_id = HID_RPT_ID_LED_OUT;
        //     cb_param.led_write.length = param->write.len;
        //     cb_param.led_write.data = param->write.value;
        //     (hidd_le_env.hidd_cb)(ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT, &cb_param);
        // }
#if (SUPPORT_REPORT_VENDOR == true)
        if (param->write.handle == hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_REPORT_VENDOR_OUT_VAL] &&
            hidd_le_env.hidd_cb != NULL)
        {
            cb_param.vendor_write.conn_id = param->write.conn_id;
            cb_param.vendor_write.report_id = HID_RPT_ID_VENDOR_OUT;
            cb_param.vendor_write.length = param->write.len;
            cb_param.vendor_write.data = param->write.value;
            (hidd_le_env.hidd_cb)(ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT, &cb_param);
        }
#endif
        break;
    }
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
    {
        // 处理属性表创建事件，初始化电池服务和HID服务
        // 如果电池的属性表创建成功，则初始化HID服务
        if (param->add_attr_tab.num_handle == BAS_IDX_NB &&
            param->add_attr_tab.svc_uuid.uuid.uuid16 == ESP_GATT_UUID_BATTERY_SERVICE_SVC &&
            param->add_attr_tab.status == ESP_GATT_OK)
        {
            incl_svc.start_hdl = param->add_attr_tab.handles[BAS_IDX_SVC];
            incl_svc.end_hdl = incl_svc.start_hdl + BAS_IDX_NB - 1;
            // ESP_LOGI(HID_LE_PRF_TAG, "%s(), start added the hid service to the stack database. incl_handle = %d",
            //          __func__, incl_svc.start_hdl);
            // 创建HID服务
            esp_ble_gatts_create_attr_tab(hidd_le_gatt_db, gatts_if, HIDD_LE_IDX_NB, 0);
        }
        // 如果HID服务也创建成功，那么开始start_service
        if (param->add_attr_tab.num_handle == HIDD_LE_IDX_NB &&
            param->add_attr_tab.status == ESP_GATT_OK)
        {
            memcpy(hidd_le_env.hidd_inst.att_tbl, param->add_attr_tab.handles,
                   HIDD_LE_IDX_NB * sizeof(uint16_t));
            // ESP_LOGI(HID_LE_PRF_TAG, "hid svc handle = %x", hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_SVC]);
            // 创建报告ID映射表
            hid_add_id_tbl();
            // 开启HID服务
            esp_ble_gatts_start_service(hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_SVC]);
        }
        else
        {
            // 如果只创建了电池服务，那么则先开启电池服务
            esp_ble_gatts_start_service(param->add_attr_tab.handles[0]);
        }
        break;
    }

    default:
        break;
    }
}

void hidd_battery_create_service(esp_gatt_if_t gatts_if)
{
    // config_adv_data后就创建属性表，创建电池服务的属性表
    esp_ble_gatts_create_attr_tab(bas_att_db, gatts_if, BAS_IDX_NB, 0);
}

void hidd_le_init(void)
{

    // Reset the hid device target environment
    memset(&hidd_le_env, 0, sizeof(hidd_le_env_t));
}

void hidd_clcb_alloc(uint16_t conn_id, esp_bd_addr_t bda)
{
    uint8_t i_clcb = 0;
    hidd_clcb_t *p_clcb = NULL;

    for (i_clcb = 0, p_clcb = hidd_le_env.hidd_clcb; i_clcb < HID_MAX_APPS; i_clcb++, p_clcb++)
    {
        if (!p_clcb->in_use)
        {
            p_clcb->in_use = true;
            p_clcb->conn_id = conn_id;
            p_clcb->connected = true;
            memcpy(p_clcb->remote_bda, bda, ESP_BD_ADDR_LEN);
            break;
        }
    }
    return;
}

bool hidd_clcb_dealloc(uint16_t conn_id)
{
    uint8_t i_clcb = 0;
    hidd_clcb_t *p_clcb = NULL;

    for (i_clcb = 0, p_clcb = hidd_le_env.hidd_clcb; i_clcb < HID_MAX_APPS; i_clcb++, p_clcb++)
    {
        memset(p_clcb, 0, sizeof(hidd_clcb_t));
        return true;
    }

    return false;
}

static struct gatts_profile_inst heart_rate_profile_tab[PROFILE_NUM] = {
    [PROFILE_APP_IDX] = {
        .gatts_cb = esp_hidd_prf_cb_hdl,
        .gatts_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },

};

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            heart_rate_profile_tab[PROFILE_APP_IDX].gatts_if = gatts_if;
        }
        else
        {
            ESP_LOGI(HID_LE_PRF_TAG, "Reg app failed, app_id %04x, status %d",
                     param->reg.app_id,
                     param->reg.status);
            return;
        }
    }

    do
    {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++)
        {
            if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                gatts_if == heart_rate_profile_tab[idx].gatts_if)
            {
                if (heart_rate_profile_tab[idx].gatts_cb)
                {
                    heart_rate_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

esp_err_t hidd_register_cb(void)
{
    esp_err_t status;
    status = esp_ble_gatts_register_callback(gatts_event_handler);
    return status;
}

void hidd_set_attr_value(uint16_t handle, uint16_t val_len, const uint8_t *value)
{
    hidd_inst_t *hidd_inst = &hidd_le_env.hidd_inst;
    if (hidd_inst->att_tbl[HIDD_LE_IDX_HID_INFO_VAL] <= handle &&
        hidd_inst->att_tbl[HIDD_LE_IDX_REPORT_REP_REF] >= handle)
    {
        esp_ble_gatts_set_attr_value(handle, val_len, value);
    }
    else
    {
        ESP_LOGE(HID_LE_PRF_TAG, "%s error:Invalid handle value.", __func__);
    }
    return;
}

void hidd_get_attr_value(uint16_t handle, uint16_t *length, uint8_t **value)
{
    hidd_inst_t *hidd_inst = &hidd_le_env.hidd_inst;
    if (hidd_inst->att_tbl[HIDD_LE_IDX_HID_INFO_VAL] <= handle &&
        hidd_inst->att_tbl[HIDD_LE_IDX_REPORT_REP_REF] >= handle)
    {
        esp_ble_gatts_get_attr_value(handle, length, (const uint8_t **)value);
    }
    else
    {
        ESP_LOGE(HID_LE_PRF_TAG, "%s error:Invalid handle value.", __func__);
    }

    return;
}

static void hid_add_id_tbl(void)
{
    // ID==1mouse
    hid_rpt_map[0].id = hidReportRefGamePadMouseIn[0];
    hid_rpt_map[0].type = hidReportRefGamePadMouseIn[1];
    hid_rpt_map[0].handle = hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_REPORT_GAMEPAD_MOUSE_IN_VAL];
    hid_rpt_map[0].cccdHandle = hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_REPORT_GAMEPAD_MOUSE_IN_CCC];
    hid_rpt_map[0].mode = HID_PROTOCOL_MODE_REPORT;
    ESP_LOGI("MAP[0]","MOUSE headle = %d,ReportID = %d,type = %d",hid_rpt_map[0].handle, hid_rpt_map[0].id, hid_rpt_map[0].type);

    // ID==3cc
    hid_rpt_map[1].id = hidReportRefGamePadCCIn[0];
    hid_rpt_map[1].type = hidReportRefGamePadCCIn[1];
    hid_rpt_map[1].handle = hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_REPORT_GAMEPAD_CC_IN_VAL];
    hid_rpt_map[1].cccdHandle = hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_REPORT_GAMEPAD_CC_IN_CCC];
    hid_rpt_map[1].mode = HID_PROTOCOL_MODE_REPORT;
    ESP_LOGI("MAP[1]","CC headle = %d,ReportID = %d,type = %d",hid_rpt_map[1].handle, hid_rpt_map[1].id, hid_rpt_map[1].type);

    // ID==4，手柄stick
    hid_rpt_map[2].id = hidReportRefGamePadStickIn[0];
    hid_rpt_map[2].type = hidReportRefGamePadStickIn[1];
    hid_rpt_map[2].handle = hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_REPORT_GAMEPAD_STICK_IN_VAL];
    hid_rpt_map[2].cccdHandle = hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_REPORT_GAMEPAD_STICK_IN_CCC];
    hid_rpt_map[2].mode = HID_PROTOCOL_MODE_REPORT;
    ESP_LOGI("MAP[2]","STICK headle = %d,ReportID = %d,type = %d",hid_rpt_map[2].handle, hid_rpt_map[2].id, hid_rpt_map[2].type);
    // 不知道是啥玩意
    hid_rpt_map[3].id = hidReportRefFeature[0];
    hid_rpt_map[3].type = hidReportRefFeature[1];
    hid_rpt_map[3].handle = hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_REPORT_VAL];
    hid_rpt_map[3].cccdHandle = 0;
    hid_rpt_map[3].mode = HID_PROTOCOL_MODE_REPORT;
    ESP_LOGI("MAP[3]","Report headle = %d,ReportID = %d,type = %d",hid_rpt_map[7].handle, hid_rpt_map[7].id, hid_rpt_map[7].type);

    // Setup report ID map
    hid_dev_register_reports(HID_NUM_REPORTS, hid_rpt_map);
}