#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// Global web server handle
extern httpd_handle_t server;

// Function Prototypes
esp_err_t web_server_prescan_networks(void);  // Pre-scan function
esp_err_t web_server_root_handler(httpd_req_t *req);
esp_err_t web_server_save_handler(httpd_req_t *req);
httpd_handle_t web_server_start(void);
void web_server_stop(void);
void web_server_cleanup(void);  // Cleanup function

// Helper function to debug credentials
void web_server_debug_credentials(void);  // Debug function

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_H