/*************************************************************************
    > File Name: rktools.cpp
    > Author: jkand.huang
    > Mail: jkand.huang@rock-chips.com
    > Created Time: Fri 17 May 2019 07:30:44 PM CST
 ************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "log.h"
#include "rktools.h"
#include "download.h"

#define LOCAL_VERSION_PATH "/etc/version"
#define DOWNLOAD_VERSION_PATH "/tmp/version"


bool getVersionFromfile(const char * filepath,char *version, int maxLength) {
    if (version == NULL || filepath == NULL) {
        LOGE("getLocalVersion is error, version == null.\n");
        return false;
    }
    FILE *fp = fopen(filepath, "r");
    if (fp == NULL) {
        LOGE("open %s failed, error is %s.\n", filepath, strerror(errno));
        return false;
    }

    char *line = NULL;
    size_t len = 0;
    size_t read;
    while ((read = getline(&line, &len, fp)) != -1) {
        if (read == 0 || line[0] == '#') {
            continue;
        }
        char *pline = strstr(line, "RK_VERSION");
        if (pline != NULL && (pline = strstr(pline, "=")) != NULL) {
            pline++; //过滤掉等于号
            //过滤掉空格
            while (*pline == ' ') {
                pline++;
            }
            int pline_len = strlen(pline) - 1;
            int version_len = (pline_len > maxLength ? maxLength:pline_len);
            memcpy(version, pline, version_len);
            LOGI("version = %s.\n", version);
            break;
        }
    }
    free(line);
    fclose(fp);
    return true;
}

//下载服务器版本号文件
bool getRemoteVersion(char *url, char *version, int maxLength) {
    if (url == NULL) {
        LOGE("getRemoteVersion url is null.\n");
        return false;
    }

    if (download_file(url, DOWNLOAD_VERSION_PATH) == -1){
        LOGE("getRemoteVersion failed, url is %s.\n", url);
        return false;
    }

    return getVersionFromfile(DOWNLOAD_VERSION_PATH, version, maxLength);
}

//获取本地版本号
bool getLocalVersion(char *version, int maxLength) {
    return getVersionFromfile(LOCAL_VERSION_PATH, version, maxLength);
}

//判断是MTD还是block 设备
bool isMtdDevice() {
    if ( !access(MTD_PATH, F_OK) ) {
        return true;
    } else {
        LOGI("Current device is not MTD");
        return false;
    }
}

/**
 * 从cmdline 获取从哪里引导
 * 返回值：
 *     0: a分区
 *     1: b分区
 *    -1: recovery 模式
 */
int getCurrentSlot(){
    char cmdline[CMDLINE_LENGTH];
    int fd = open("/proc/cmdline", O_RDONLY);
    read(fd, (char*)cmdline, CMDLINE_LENGTH);
    close(fd);
    char *slot = strstr(cmdline, "androidboot.slot_suffix");
    if(slot != NULL){
        slot = strstr(slot, "=");
        if(slot != NULL && *(++slot) == '_'){
            slot += 1;
            if((*slot) == 'a'){
                return 0;
            }else if((*slot) == 'b'){
                return 1;
            }
        }
    }
    LOGI("Current Mode is recovery.\n");
    return -1;
}

void getFlashPoint(char *path) {
    char *emmc_point = getenv(EMMC_POINT_NAME);
    if ( !access(emmc_point, F_OK) ) {
        LOGI("Current device is emmc : %s.\n", emmc_point);
        strcpy(path, emmc_point);
    } else {
        LOGI("Current device is nand : %s.\n", NAND_DRIVER_DEV_LBA);
        strcpy(path, NAND_DRIVER_DEV_LBA);
    }
}
/*
 * 获得flash 的大小，和块数
 */
int getFlashSize(char *path, long long* flash_size, long long* block_num) {

    int fd_dest = open(path, O_RDWR);
    if (fd_dest < 0) {
        LOGE("Can't open %s\n", path);
        return -2;
    }
    if ((*flash_size = lseek64(fd_dest, 0, SEEK_END)) == -1) {
        LOGE("getFlashInfo lseek64 failed.\n");
        return -2;
    }
    lseek64(fd_dest, 0, SEEK_SET);
    *flash_size = *flash_size / (1024);    //Kib
    *block_num = *flash_size * 2;
    return 0;
}
