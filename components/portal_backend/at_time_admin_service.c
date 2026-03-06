#include "at_time_admin_service.h"
#include "storage.h"
#include "scheduler_service.h"
#include "app_config.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

#define PORTAL_BODY_MAX_LEN 4096

static const char *TAG = "at_time_admin";

static esp_err_t read_body_alloc(httpd_req_t *req, char **out)
{
    if (!req || !out) return ESP_ERR_INVALID_ARG;
    *out = NULL;
    if (req->content_len <= 0 || req->content_len > PORTAL_BODY_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    char *body = malloc((size_t)req->content_len + 1);
    if (!body) return ESP_ERR_NO_MEM;

    int remaining = req->content_len;
    int offset = 0;
    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, body + offset, remaining);
        if (recv_len <= 0) {
            free(body);
            return ESP_FAIL;
        }
        offset += recv_len;
        remaining -= recv_len;
    }
    body[offset] = '\0';
    *out = body;
    return ESP_OK;
}

static esp_err_t send_json(httpd_req_t *req, int status, const char *json)
{
    httpd_resp_set_status(req, status == 200 ? "200 OK"
                          : status == 400 ? "400 Bad Request"
                          : status == 404 ? "404 Not Found"
                          : "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json ? json : "{}");
}

static esp_err_t send_error(httpd_req_t *req, int status, const char *code)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"ok\":false,\"error\":\"%s\"}", code ? code : "internal_error");
    return send_json(req, status, buf);
}

static esp_err_t at_time_get_handler(httpd_req_t *req)
{
    at_time_entry_t entries[MAX_AT_TIME_COUNT] = {0};
    uint8_t count = 0;
    storage_at_time_load_all(entries, &count);

    cJSON *root = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    if (!root || !items) {
        cJSON_Delete(root);
        cJSON_Delete(items);
        return send_error(req, 500, "no_memory");
    }

    cJSON_AddBoolToObject(root, "ok", true);
    cJSON_AddNumberToObject(root, "count", count);
    cJSON_AddItemToObject(root, "items", items);

    for (uint8_t i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            cJSON_Delete(root);
            return send_error(req, 500, "no_memory");
        }
        cJSON_AddNumberToObject(item, "idx", i);
        cJSON_AddBoolToObject(item, "enabled", entries[i].enabled);
        cJSON_AddNumberToObject(item, "hour", entries[i].hour);
        cJSON_AddNumberToObject(item, "minute", entries[i].minute);
        cJSON_AddNumberToObject(item, "weekdays", entries[i].weekdays);
        cJSON_AddStringToObject(item, "prompt", entries[i].prompt);
        cJSON_AddItemToArray(items, item);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return send_error(req, 500, "encode_failed");

    esp_err_t ret = send_json(req, 200, json);
    free(json);
    return ret;
}

static esp_err_t at_time_save_handler(httpd_req_t *req)
{
    char *body = NULL;
    if (read_body_alloc(req, &body) != ESP_OK) {
        return send_error(req, 400, "invalid_format");
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return send_error(req, 400, "invalid_format");

    cJSON *j_idx     = cJSON_GetObjectItem(root, "idx");
    cJSON *j_enabled = cJSON_GetObjectItem(root, "enabled");
    cJSON *j_hour    = cJSON_GetObjectItem(root, "hour");
    cJSON *j_minute  = cJSON_GetObjectItem(root, "minute");
    cJSON *j_wdays   = cJSON_GetObjectItem(root, "weekdays");
    cJSON *j_prompt  = cJSON_GetObjectItem(root, "prompt");

    if (!cJSON_IsNumber(j_idx) || !cJSON_IsBool(j_enabled) ||
        !cJSON_IsNumber(j_hour) || !cJSON_IsNumber(j_minute) ||
        !cJSON_IsNumber(j_wdays) || !cJSON_IsString(j_prompt)) {
        cJSON_Delete(root);
        return send_error(req, 400, "invalid_format");
    }

    int idx = j_idx->valueint;
    int hour = j_hour->valueint;
    int minute = j_minute->valueint;
    int weekdays = j_wdays->valueint;

    if (idx < 0 || idx >= MAX_AT_TIME_COUNT) {
        cJSON_Delete(root);
        return send_error(req, 400, "invalid_index");
    }
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        cJSON_Delete(root);
        return send_error(req, 400, "invalid_time");
    }
    if (weekdays < 1 || weekdays > 127) {
        cJSON_Delete(root);
        return send_error(req, 400, "invalid_weekdays");
    }
    if (strlen(j_prompt->valuestring) > AT_TIME_PROMPT_MAX_LEN) {
        cJSON_Delete(root);
        return send_error(req, 400, "prompt_too_long");
    }

    at_time_entry_t entry = {0};
    entry.enabled  = cJSON_IsTrue(j_enabled);
    entry.hour     = (uint8_t)hour;
    entry.minute   = (uint8_t)minute;
    entry.weekdays = (uint8_t)weekdays;
    strlcpy(entry.prompt, j_prompt->valuestring, sizeof(entry.prompt));
    cJSON_Delete(root);

    esp_err_t ret = storage_at_time_save_entry((uint8_t)idx, &entry);
    if (ret != ESP_OK) {
        return send_error(req, 500, "save_failed");
    }

    scheduler_service_reload_at_time();
    ESP_LOGI(TAG, "at_time entry %d saved hour=%d min=%d wdays=0x%02x",
             idx, entry.hour, entry.minute, entry.weekdays);
    return send_json(req, 200, "{\"ok\":true}");
}

static esp_err_t at_time_remove_handler(httpd_req_t *req)
{
    char *body = NULL;
    if (read_body_alloc(req, &body) != ESP_OK) {
        return send_error(req, 400, "invalid_format");
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return send_error(req, 400, "invalid_format");

    cJSON *j_idx = cJSON_GetObjectItem(root, "idx");
    if (!cJSON_IsNumber(j_idx)) {
        cJSON_Delete(root);
        return send_error(req, 400, "invalid_format");
    }

    int idx = j_idx->valueint;
    cJSON_Delete(root);

    if (idx < 0 || idx >= MAX_AT_TIME_COUNT) {
        return send_error(req, 400, "invalid_index");
    }

    esp_err_t ret = storage_at_time_remove_entry((uint8_t)idx);
    if (ret == ESP_ERR_NOT_FOUND) {
        return send_error(req, 404, "not_found");
    }
    if (ret != ESP_OK) {
        return send_error(req, 500, "remove_failed");
    }

    scheduler_service_reload_at_time();
    ESP_LOGI(TAG, "at_time entry %d removed", idx);
    return send_json(req, 200, "{\"ok\":true}");
}

esp_err_t at_time_admin_service_register_handlers(httpd_handle_t httpd)
{
    if (!httpd) return ESP_ERR_INVALID_ARG;

    httpd_uri_t get_uri = {
        .uri = "/api/at_time",
        .method = HTTP_GET,
        .handler = at_time_get_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t save_uri = {
        .uri = "/api/at_time/save",
        .method = HTTP_POST,
        .handler = at_time_save_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t remove_uri = {
        .uri = "/api/at_time/remove",
        .method = HTTP_POST,
        .handler = at_time_remove_handler,
        .user_ctx = NULL,
    };

    httpd_register_uri_handler(httpd, &get_uri);
    httpd_register_uri_handler(httpd, &save_uri);
    httpd_register_uri_handler(httpd, &remove_uri);

    ESP_LOGI(TAG, "at_time admin handlers registered");
    return ESP_OK;
}
