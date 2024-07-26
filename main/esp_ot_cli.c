/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * OpenThread Command Line Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "driver/uart.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_ieee802154.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_openthread.h"
#include "esp_openthread_cli.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include "esp_ot_config.h"
#include "esp_vfs_eventfd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/uart_types.h"
#include "nvs_flash.h"
#include "openthread/cli.h"
#include "openthread/instance.h"
#include "openthread/logging.h"
#include "openthread/tasklet.h"
#include "openthread/ping_sender.h"
#include "openthread/udp.h"

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
#include "esp_ot_cli_extension.h"
#endif // CONFIG_OPENTHREAD_CLI_ESP_EXTENSION

#define TAG "ot_esp_cli"

#define UDP_JOINER_PORT 49155

otUdpSocket socket_info;

static esp_netif_t *init_openthread_netif(const esp_openthread_platform_config_t *config)
{
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    esp_netif_t *netif = esp_netif_new(&cfg);
    assert(netif != NULL);
    ESP_ERROR_CHECK(esp_netif_attach(netif, esp_openthread_netif_glue_init(config)));

    return netif;
}


void udp_rx_callback(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
    char buf[1500];
    int length = otMessageRead(aMessage, otMessageGetOffset(aMessage), buf, sizeof(buf) - 1);
    buf[length] = '\0';  // Ensure null-termination
    ESP_LOGI(TAG, "Received UDP message: %s", buf);
    
}

void init_udp(void)
{
    otInstance * thread_instance = esp_openthread_get_instance();
    otSockAddr bind_info;
    otNetifIdentifier netif = OT_NETIF_THREAD;
    memset(&bind_info, 0, sizeof(otSockAddr));
    otIp6AddressFromString("::", &bind_info.mAddress);
    bind_info.mPort = UDP_JOINER_PORT;
    otError err = otUdpOpen(thread_instance, &socket_info, udp_rx_callback, NULL);    

    if (err != OT_ERROR_NONE)
    {
        ESP_LOGE(TAG, "UDP open error %d", err);
    {
        ESP_LOGI(TAG, "udp init done"); 
    }
    err = otUdpBind(thread_instance, &socket_info, &bind_info, netif);
    if (err != OT_ERROR_NONE)
    {
        ESP_LOGE(TAG, "UDP not bind error %d", err); 
    }
}

void send_udp()
{
    otMessageInfo messageInfo;
    otMessageSettings new_msg_settings;
    new_msg_settings.mLinkSecurityEnabled = true;
    new_msg_settings.mPriority = 1;
    otIp6AddressFromString("ff02::1", &messageInfo.mPeerAddr);
    messageInfo.mPeerPort = UDP_JOINER_PORT;
    otMessage * message = otUdpNewMessage(esp_openthread_get_instance(), &new_msg_settings);
    const char * buf1 = "hello";
    otError err = otMessageAppend(message, buf1, (uint16_t) strlen(buf1));
    if (err != OT_ERROR_NONE)
    {
        ESP_LOGE(TAG, "message create fail %d", err); 
    }
    err = otUdpSend(esp_openthread_get_instance(), &socket_info, message, &messageInfo);
    if (err != OT_ERROR_NONE)
    {
        ESP_LOGE(TAG, "fail to send %d", err); 
    }
}

static void udp_send_task()
{
    while(1)
    {
        send_udp();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void ot_task_worker(void *aContext) {
  esp_openthread_platform_config_t config = {
      .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
      .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
      .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
  };
  ESP_LOGI(TAG, "init");

  // Initialize the OpenThread stack
  ESP_ERROR_CHECK(esp_openthread_init(&config));

#if CONFIG_OPENTHREAD_LOG_LEVEL_DYNAMIC
  // The OpenThread log level directly matches ESP log level
  (void)otLoggingSetLevel(CONFIG_LOG_DEFAULT_LEVEL);
#endif
  // Initialize the OpenThread cli
#if CONFIG_OPENTHREAD_CLI
  esp_openthread_cli_init();
#endif

  esp_netif_t *openthread_netif;
  // Initialize the esp_netif bindings
  openthread_netif = init_openthread_netif(&config);
  esp_netif_set_default_netif(openthread_netif);

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
  esp_cli_custom_command_init();
#endif // CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
  ESP_LOGI(TAG, "start ");
  // Run the main loop
#if CONFIG_OPENTHREAD_CLI
  esp_openthread_cli_create_task();
#endif
#if CONFIG_OPENTHREAD_AUTO_START
  otOperationalDatasetTlvs dataset;
  otError error =
      otDatasetGetActiveTlvs(esp_openthread_get_instance(), &dataset);
  ESP_ERROR_CHECK(
      esp_openthread_auto_start((error == OT_ERROR_NONE) ? &dataset : NULL));
#endif
  ESP_LOGI(TAG, "done");
  
  init_udp();
  esp_openthread_launch_mainloop();

  // Clean up
  esp_netif_destroy(openthread_netif);
  esp_openthread_netif_glue_deinit();
  esp_vfs_eventfd_unregister();
  vTaskDelete(NULL);
}
void app_main(void)
{
    // Used eventfds:
    // * netif
    // * ot task queue
    // * radio driver
    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = 3,
    };

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));
    xTaskCreate(ot_task_worker, "ot_cli_main", 10240, xTaskGetCurrentTaskHandle(), 5, NULL);
    //xTaskCreate(udp_send_task, "udp_send", 4096, NULL, 5, NULL); 

}
