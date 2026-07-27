#include <stdint.h>
#include <string.h>

// 定义OTA运行状态枚举
typedef enum {
    OTA_STATUS_IDLE       = 0xFFFFFFFF,     // 擦除后的初始状态
    OTA_STATUS_DOWNLOADING = 0xAAAA5555,    // 正在下载固件
    OTA_STATUS_VERIFYING  = 0x5555AAAA,     // 下载完成，正在校验CRC/ECC
    OTA_STATUS_READY      = 0x3333CCCC,     // 校验通过，等待重启刷写
    OTA_STATUS_UPGRADING  = 0x00000000      // 正在执行Bootloader刷写
} OtaStatus_t;

#define FLASH_PAGE_SIZE   2048
#define FLASH_WORD_SIZE   4
#define OTA_PAGE_WORDS    (FLASH_PAGE_SIZE / FLASH_WORD_SIZE)

static bool OTA_IsValidStatus(uint32_t v) {
    return v == (uint32_t)OTA_STATUS_DOWNLOADING ||
           v == (uint32_t)OTA_STATUS_VERIFYING   ||
           v == (uint32_t)OTA_STATUS_READY       ||
           v == (uint32_t)OTA_STATUS_UPGRADING;             // IDLE 不写入日志，故排除
}

OtaStatus_t OTA_Get_Current_Status(void) {
    uint32_t *ptr = (uint32_t *)FLASH_OTA_STAGE_PAGE_ADDR;
    OtaStatus_t current = OTA_STATUS_IDLE;

    for (uint32_t i = 0; i < OTA_PAGE_WORDS; i++) {
        if (ptr[i] == 0xFFFFFFFF) return current;           // 到前沿，当前即上一合法值
        if (OTA_IsValidStatus(ptr[i])) {
            current = (OtaStatus_t)ptr[i];                  // 只接受合法值，过滤脏字
        } else {
            return current;                                 // 脏字=前沿半写入，停下
        }
    }
    return current;                                         // 整页写满：最后一合法值
}

int OTA_Update_Status(OtaStatus_t set_status) {
    uint32_t *ptr = (uint32_t *)FLASH_OTA_STAGE_PAGE_ADDR;

    for (uint32_t i = 0; i < OTA_PAGE_WORDS; i++) {
        if (ptr[i] == 0xFFFFFFFF) {
            return (HAL_Flash_Write_Word(FLASH_OTA_STAGE_PAGE_ADDR + i * FLASH_WORD_SIZE,
                                        (uint32_t)set_status) == 0) ? 0 : -1;
        }
    }
    /* 页满才走这里：升级态下擦除有掉电丢状态风险，调用方应保证不在此态压缩 */
    if (HAL_Flash_Erase_Page(FLASH_OTA_STAGE_PAGE_ADDR) != 0) return -1;
    return (HAL_Flash_Write_Word(FLASH_OTA_STAGE_PAGE_ADDR, (uint32_t)set_status) == 0) ? 0 : -1;
}