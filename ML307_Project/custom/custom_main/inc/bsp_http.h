#ifndef _BSP_HTTP_H_
#define _BSP_HTTP_H_

bool http_post(const char *url, const char *path,const char *body, char *resp_buf, uint32_t resp_len);

#endif
