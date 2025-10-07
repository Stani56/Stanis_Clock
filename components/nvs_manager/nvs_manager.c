// nvs_manager.c - NVS Storage Manager for WiFi Credentials
#include "nvs_manager.h"

static const char *TAG = "NVS_MANAGER";

esp_err_t nvs_manager_init(void) {
    ESP_LOGI(TAG, "Initializing NVS manager");
    
    // Initialize NVS flash
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS flash needs to be erased, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS flash: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "NVS manager initialized successfully");
    return ESP_OK;
}

esp_err_t nvs_manager_save_wifi_credentials(const char* ssid, const char* password) {
    ESP_LOGI(TAG, "Saving WiFi credentials to NVS");
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_str(nvs_handle, NVS_SSID_KEY, ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    err = nvs_set_str(nvs_handle, NVS_PASSWORD_KEY, password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error committing to NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "WiFi credentials saved successfully");
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_manager_load_wifi_credentials(char* ssid, char* password, size_t ssid_len, size_t pass_len) {
    ESP_LOGI(TAG, "Loading WiFi credentials from NVS");
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error opening NVS handle for reading: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_get_str(nvs_handle, NVS_SSID_KEY, ssid, &ssid_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error loading SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    err = nvs_get_str(nvs_handle, NVS_PASSWORD_KEY, password, &pass_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error loading password: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "WiFi credentials loaded successfully");
    }
    
    nvs_close(nvs_handle);
    return err;
}

void nvs_manager_clear_wifi_credentials(void) {
    ESP_LOGI(TAG, "Clearing WiFi credentials from NVS");
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        esp_err_t erase_err1 = nvs_erase_key(nvs_handle, NVS_SSID_KEY);
        esp_err_t erase_err2 = nvs_erase_key(nvs_handle, NVS_PASSWORD_KEY);
        
        if (erase_err1 == ESP_OK && erase_err2 == ESP_OK) {
            ESP_LOGI(TAG, "WiFi credentials cleared successfully");
        } else {
            ESP_LOGW(TAG, "Some credentials may not have been cleared (SSID: %s, Password: %s)", 
                     esp_err_to_name(erase_err1), esp_err_to_name(erase_err2));
        }
        
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for clearing credentials: %s", esp_err_to_name(err));
    }
}