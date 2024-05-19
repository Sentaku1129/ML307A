#include "string.h"

#include "cm_http.h"

#include "bsp_http.h"
#include "bsp_uart.h"

static cm_httpclient_handle_t client = NULL;

bool http_post(const char *url, const char *path, const char *body, char *resp_buf, uint32_t resp_len)
{
    cm_httpclient_ret_code_e ret = CM_HTTP_RET_CODE_UNKNOWN_ERROR;

    if (client == NULL)
    {
        ret = cm_httpclient_create((const uint8_t *)url, NULL, &client);

        if (ret != CM_HTTP_RET_CODE_OK || client == NULL)
        {
            uart0_printf("_http_post_: http client create error\r\n");
            return false;
        }

        cm_httpclient_cfg_t client_cfg;
        client_cfg.ssl_enable = false; // 使用SSL，即HTTPS连接方式。使用HTTP方式时该值为false
        client_cfg.ssl_id = 0;         // 设置SSL索引号
        client_cfg.cid = 0;            // 设置PDP索引号，目前不支持该项设置，设置任意值即可
        client_cfg.conn_timeout = HTTPCLIENT_CONNECT_TIMEOUT_DEFAULT;
        client_cfg.rsp_timeout = HTTPCLIENT_WAITRSP_TIMEOUT_DEFAULT;
        client_cfg.dns_priority = 0; // 设置DNS解析优先级，ipv6解析优先

        ret = cm_httpclient_set_cfg(client, client_cfg);

        if (ret != CM_HTTP_RET_CODE_OK || client == NULL)
        {
            uart0_printf("_http_post_: http client set cfg error");
            return false;
        }
    }

    cm_httpclient_sync_response_t response = {};
    cm_httpclient_sync_param_t param = {HTTPCLIENT_REQUEST_POST, (const uint8_t *)path, strlen(body), (uint8_t *)body}; // POST

    ret = cm_httpclient_sync_request(client, param, &response);

    if (ret != CM_HTTP_RET_CODE_OK || client == NULL)
    {
        uart0_printf("_http_post_: http request error! ret is %d\r\n", ret);
        return false;
    }
    else
    {
        uart0_printf("response_code is %d\r\n", response.response_code);

        uart0_printf("response_header_len is %d\r\n", response.response_header_len);
        uart0_printf("response_content_len is %d\r\n", response.response_content_len);
    }
    if (response.response_code == 200)
    {
        strcpy(resp_buf, (char *)response.response_content);
        cm_httpclient_sync_free_data(client);
        return true;
    }
    else
    {
        cm_httpclient_sync_free_data(client);
        return false;
    }      
}

void http_get(const char *url, char *resp_buf)
{
}
