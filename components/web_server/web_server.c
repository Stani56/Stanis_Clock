// web_server.c - Safe pre-scan approach
#include "web_server.h"
#include "nvs_manager.h"

static const char *TAG = "WEB_SERVER";

// Global web server handle
httpd_handle_t server = NULL;

// Static storage for pre-scanned networks
static wifi_ap_record_t *cached_scan_results = NULL;
static uint16_t cached_scan_count = 0;
static bool scan_completed = false;

// Function to perform WiFi scan in AP mode (no pre-scan needed)
esp_err_t web_server_prescan_networks(void) {
    ESP_LOGI(TAG, "Skipping pre-scan - will scan on-demand in AP mode");
    
    // Clean up any previous scan results
    if (cached_scan_results) {
        free(cached_scan_results);
        cached_scan_results = NULL;
        cached_scan_count = 0;
    }
    
    // Mark scan as not completed - will be done on-demand via web interface
    scan_completed = false;
    cached_scan_count = 0;
    
    ESP_LOGI(TAG, "On-demand scanning will be available in web interface");
    return ESP_OK;
}

// Generate network list HTML from cached results
char* web_server_generate_network_list(void) {
    if (!scan_completed || cached_scan_count == 0) {
        char* empty_list = malloc(200);
        if (empty_list) {
            strcpy(empty_list, "<div class=\"info-box\"><p>No networks found in pre-scan. Please enter your network details manually.</p></div>");
        }
        return empty_list;
    }
    
    // Calculate required buffer size
    size_t buffer_size = 1000 + (cached_scan_count * 300); // Base size + space per network
    char* network_html = malloc(buffer_size);
    if (!network_html) {
        return NULL;
    }
    
    strcpy(network_html, "<div class=\"wifi-list\"><h3>Available Networks (from pre-scan):</h3>");
    
    for (int i = 0; i < cached_scan_count; i++) {
        char network_item[250];
        const char* security;
        
        // Determine security type
        switch (cached_scan_results[i].authmode) {
            case WIFI_AUTH_OPEN: security = "Open"; break;
            case WIFI_AUTH_WEP: security = "WEP"; break;
            case WIFI_AUTH_WPA_PSK: security = "WPA"; break;
            case WIFI_AUTH_WPA2_PSK: security = "WPA2"; break;
            case WIFI_AUTH_WPA_WPA2_PSK: security = "WPA/WPA2"; break;
            case WIFI_AUTH_WPA3_PSK: security = "WPA3"; break;
            default: security = "Unknown"; break;
        }
        
        // Signal strength indicator
        const char* signal_icon;
        if (cached_scan_results[i].rssi > -50) signal_icon = "📶";
        else if (cached_scan_results[i].rssi > -70) signal_icon = "📶";
        else signal_icon = "📶";
        
        snprintf(network_item, sizeof(network_item),
                "<div class=\"network-btn\" onclick=\"selectNetwork('%.32s')\">"
                "%s %.32s <small>(%s, %d dBm)</small>"
                "</div>",
                (char*)cached_scan_results[i].ssid,
                signal_icon,
                (char*)cached_scan_results[i].ssid, 
                security,
                cached_scan_results[i].rssi);
        
        // Check if we have space to add this item
        if (strlen(network_html) + strlen(network_item) + 50 < buffer_size) {
            strcat(network_html, network_item);
        } else {
            ESP_LOGW(TAG, "Network list buffer full, truncating at %d networks", i);
            break;
        }
    }
    
    strcat(network_html, "</div>");
    return network_html;
}

// Enhanced HTML page with pre-scanned network integration
const char* html_page_template = 
"<!DOCTYPE html>"
"<html><head><title>ESP32 LED Controller - WiFi Config</title>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<style>"
"body{font-family:Arial,sans-serif;margin:20px;background:#f0f0f0;color:#333;}"
"form{background:white;padding:30px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1);max-width:600px;margin:0 auto;}"
"input[type=text],input[type=password]{width:100%%;padding:12px;margin:10px 0;border:1px solid #ddd;border-radius:4px;box-sizing:border-box;}"
"input[type=submit]{background:#007bff;color:white;padding:12px 30px;border:none;border-radius:4px;cursor:pointer;margin:5px;}"
"input[type=submit]:hover{background:#0056b3;}"
".info-box{margin:15px 0;padding:15px;border:1px solid #ddd;border-radius:4px;background:#f8f9fa;}"
".wifi-list{margin:15px 0;padding:10px;border:1px solid #ddd;border-radius:4px;background:#f8f9fa;max-height:300px;overflow-y:auto;}"
".network-btn{display:block;margin:5px 0;padding:10px;background:white;border:1px solid #ddd;border-radius:4px;cursor:pointer;text-decoration:none;color:#333;}"
".network-btn:hover{background:#e9ecef;}"
".common-networks{margin:15px 0;}"
".common-btn{display:inline-block;margin:5px;padding:8px 12px;background:#e9ecef;border:1px solid #ddd;border-radius:4px;cursor:pointer;}"
".common-btn:hover{background:#dee2e6;}"
"h1,h2{text-align:center;color:#343a40;}"
".small{font-size:0.9em;color:#666;}"
"</style>"
"<script>"
"function selectNetwork(ssid) {"
"  document.getElementById('ssid').value = ssid;"
"  document.getElementById('password').focus();"
"}"
"</script></head><body>"
"<h1>ESP32 LED Controller</h1>"
"<h2>WiFi Configuration</h2>"
"<form method=\"post\" action=\"/save\">"
"%s" // Network list will be inserted here
"<div class=\"common-networks\">"
"<h3>Quick Select:</h3>"
"<span class=\"common-btn\" onclick=\"selectNetwork('MyWiFi')\">MyWiFi</span>"
"<span class=\"common-btn\" onclick=\"selectNetwork('Home_WiFi')\">Home_WiFi</span>"
"<span class=\"common-btn\" onclick=\"selectNetwork('NETGEAR')\">NETGEAR</span>"
"<span class=\"common-btn\" onclick=\"selectNetwork('Linksys')\">Linksys</span>"
"</div>"
"<p><label><strong>WiFi Network Name (SSID):</strong></label><br>"
"<input type=\"text\" id=\"ssid\" name=\"ssid\" placeholder=\"Enter your WiFi network name\" required></p>"
"<p><label><strong>WiFi Password:</strong></label><br>"
"<input type=\"password\" id=\"password\" name=\"password\" placeholder=\"Enter your WiFi password\" required></p>"
"<p><input type=\"submit\" value=\"Save & Connect\"></p>"
"</form>"
"<div class=\"info-box\">"
"<h3>Instructions</h3>"
"<p>1. Select a network from the list above or enter manually</p>"
"<p>2. Enter the WiFi password</p>"
"<p>3. Click 'Save & Connect'</p>"
"<p>4. Device will restart and connect to your network</p>"
"<p class=\"small\">Networks shown were found during device startup scan.</p>"
"</div>"
"</body></html>";

esp_err_t web_server_root_handler(httpd_req_t *req) {
    // Generate the network list HTML
    char* network_list = web_server_generate_network_list();
    if (!network_list) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Calculate total size needed
    size_t total_size = strlen(html_page_template) + strlen(network_list) + 100;
    char* complete_html = malloc(total_size);
    if (!complete_html) {
        free(network_list);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Generate complete HTML page
    snprintf(complete_html, total_size, html_page_template, network_list);
    
    // Send response
    httpd_resp_send(req, complete_html, HTTPD_RESP_USE_STRLEN);
    
    // Cleanup
    free(network_list);
    free(complete_html);
    
    return ESP_OK;
}

esp_err_t web_server_save_handler(httpd_req_t *req) {
    char buf[512];
    char ssid[64] = {0};
    char password[64] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    
    buf[ret] = '\0';
    ESP_LOGI(TAG, "Received form data: %s", buf);  // Debug: show raw form data
    
    // Parse form data (same as before)
    char *ssid_start = strstr(buf, "ssid=");
    char *pass_start = strstr(buf, "password=");
    
    if (ssid_start && pass_start) {
        // Extract and decode SSID
        ssid_start += 5;
        char *ssid_end = strchr(ssid_start, '&');
        if (ssid_end) {
            strncpy(ssid, ssid_start, ssid_end - ssid_start);
        } else {
            strcpy(ssid, ssid_start);
        }
        
        ESP_LOGI(TAG, "Raw SSID: %s", ssid);  // Debug: show raw SSID
        
        // URL decode SSID
        char decoded_ssid[64] = {0};
        int j = 0;
        for (int i = 0; ssid[i] && j < 63; i++) {
            if (ssid[i] == '+') {
                decoded_ssid[j++] = ' ';
            } else if (ssid[i] == '%' && ssid[i+1] && ssid[i+2]) {
                char hex[3] = {ssid[i+1], ssid[i+2], 0};
                decoded_ssid[j++] = (char)strtol(hex, NULL, 16);
                i += 2;
            } else {
                decoded_ssid[j++] = ssid[i];
            }
        }
        strcpy(ssid, decoded_ssid);
        
        // Extract and decode password
        pass_start += 9;
        char *pass_end = strchr(pass_start, '&');
        if (pass_end) {
            strncpy(password, pass_start, pass_end - pass_start);
        } else {
            strcpy(password, pass_start);
        }
        
        ESP_LOGI(TAG, "Raw password length: %d", strlen(password));  // Debug: show password length (not content)
        
        char decoded_password[64] = {0};
        j = 0;
        for (int i = 0; password[i] && j < 63; i++) {
            if (password[i] == '+') {
                decoded_password[j++] = ' ';
            } else if (password[i] == '%' && password[i+1] && password[i+2]) {
                char hex[3] = {password[i+1], password[i+2], 0};
                decoded_password[j++] = (char)strtol(hex, NULL, 16);
                i += 2;
            } else {
                decoded_password[j++] = password[i];
            }
        }
        strcpy(password, decoded_password);
        
        ESP_LOGI(TAG, "Final credentials - SSID: '%s', Password length: %d", ssid, strlen(password));
        
        // Validate credentials before saving
        if (strlen(ssid) == 0) {
            ESP_LOGE(TAG, "Empty SSID provided");
            const char* error_resp = 
                "<!DOCTYPE html><html><head><title>Error</title></head><body>"
                "<h1>Error</h1><p>SSID cannot be empty. Please try again.</p>"
                "<a href=\"/\">Go back</a></body></html>";
            httpd_resp_send(req, error_resp, HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }
        
        if (strlen(password) == 0) {
            ESP_LOGW(TAG, "Empty password provided - assuming open network");
        }
        
        // Save credentials
        esp_err_t save_ret = nvs_manager_save_wifi_credentials(ssid, password);
        if (save_ret == ESP_OK) {
            ESP_LOGI(TAG, "WiFi credentials saved successfully to NVS");
            
            // Use a simpler, safer HTML response
            const char* success_template = 
                "<!DOCTYPE html>"
                "<html><head><title>Success</title>"
                "<style>"
                "body{font-family:Arial;text-align:center;margin:50px;background:#f0f0f0;}"
                ".success{background:white;padding:30px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1);display:inline-block;}"
                "</style></head><body>"
                "<div class=\"success\">"
                "<h1>✓ Credentials Saved!</h1>"
                "<p>Device will restart in 3 seconds and connect to your network</p>"
                "<p>You can close this window.</p>"
                "</div>"
                "<script>setTimeout(function(){window.close();},3000);</script>"
                "</body></html>";
            
            httpd_resp_send(req, success_template, HTTPD_RESP_USE_STRLEN);
            
            ESP_LOGI(TAG, "Restarting device to connect to: %s", ssid);
            vTaskDelay(pdMS_TO_TICKS(3000));
            esp_restart();
        } else {
            ESP_LOGE(TAG, "Failed to save WiFi credentials: %s", esp_err_to_name(save_ret));
            const char* error_resp = 
                "<!DOCTYPE html><html><head><title>Error</title></head><body>"
                "<h1>Error</h1><p>Failed to save credentials. Please try again.</p>"
                "<a href=\"/\">Go back</a></body></html>";
            httpd_resp_send(req, error_resp, HTTPD_RESP_USE_STRLEN);
        }
    } else {
        ESP_LOGE(TAG, "Invalid form data received");
        const char* error_resp = 
            "<!DOCTYPE html><html><head><title>Error</title></head><body>"
            "<h1>Error</h1><p>Invalid form data. Please try again.</p>"
            "<a href=\"/\">Go back</a></body></html>";
        httpd_resp_send(req, error_resp, HTTPD_RESP_USE_STRLEN);
    }
    
    return ESP_OK;
}

httpd_handle_t web_server_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.stack_size = 6144;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = web_server_root_handler
        };
        httpd_register_uri_handler(server, &root);
        
        httpd_uri_t save = {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = web_server_save_handler
        };
        httpd_register_uri_handler(server, &save);
        
        ESP_LOGI(TAG, "Web server started with pre-scanned network list");
        return server;
    }
    
    return NULL;
}

void web_server_stop(void) {
    if (server != NULL) {
        httpd_stop(server);
        server = NULL;
    }
}

void web_server_debug_credentials(void) {
    char ssid[64] = {0};
    char password[64] = {0};
    size_t ssid_len = sizeof(ssid);
    size_t pass_len = sizeof(password);
    
    esp_err_t ret = nvs_manager_load_wifi_credentials(ssid, password, ssid_len, pass_len);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "=== STORED CREDENTIALS DEBUG ===");
        ESP_LOGI(TAG, "SSID: '%s' (length: %d)", ssid, strlen(ssid));
        ESP_LOGI(TAG, "Password length: %d", strlen(password));
        ESP_LOGI(TAG, "SSID bytes: ");
        for (int i = 0; i < strlen(ssid); i++) {
            printf("0x%02X ", (unsigned char)ssid[i]);
        }
        printf("\n");
        ESP_LOGI(TAG, "================================");
    } else {
        ESP_LOGE(TAG, "Failed to load credentials for debug: %s", esp_err_to_name(ret));
    }
}

void web_server_cleanup(void) {
    if (cached_scan_results) {
        free(cached_scan_results);
        cached_scan_results = NULL;
        cached_scan_count = 0;
    }
    scan_completed = false;
}