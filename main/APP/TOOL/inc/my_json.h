#ifndef _MY_JSON_H_
#define _MY_JSON_H_

#include "sys.h"
#include "jsmn.h"

/*
 * 自研 JSON 工具(不依赖 cJSON):
 *   1) 组包: my_json_t 逐段拼接, 自动处理逗号/引号/转义, 带缓冲越界保护, 全程无动态内存;
 *   2) 解析: 基于 jsmn 词法切分, 提供按 key 查找/取值等辅助接口。
 */

/* ============ 组包 ============ */

#define MY_JSON_MAX_DEPTH	8

typedef struct
{
	char    *buf;								/* 输出缓冲 */
	uint32_t cap;								/* 缓冲容量(含结束符'\0') */
	uint32_t len;								/* 当前已写入长度 */
	uint8_t  depth;								/* 当前容器层数(0=未开始) */
	uint8_t  type[MY_JSON_MAX_DEPTH];			/* 各层类型: 0-对象 1-数组 */
	uint8_t  first[MY_JSON_MAX_DEPTH];			/* 各层是否尚无成员 */
	uint8_t  err;								/* 1-溢出/错误 */
} my_json_t;

void     my_json_init(my_json_t *js, char *buf, uint32_t cap);
void     my_json_reset(my_json_t *js);
uint32_t my_json_len(my_json_t *js);
uint8_t  my_json_error(my_json_t *js);

/* 容器: key 为 NULL 时作为数组元素/根写入 */
void     my_json_object_begin(my_json_t *js, const char *key);
void     my_json_object_end(my_json_t *js);
void     my_json_array_begin(my_json_t *js, const char *key);
void     my_json_array_end(my_json_t *js);

/* 成员: key 为 NULL 时作为数组元素写入 */
void     my_json_add_str(my_json_t *js, const char *key, const char *val);
void     my_json_add_int(my_json_t *js, const char *key, int32_t val);
void     my_json_add_bool(my_json_t *js, const char *key, uint8_t val);
void     my_json_add_null(my_json_t *js, const char *key);
void     my_json_add_raw(my_json_t *js, const char *key, const char *raw);

/* ============ 解析(jsmn) ============ */

#define MY_JSON_MAX_TOKENS	64

/* 解析, 返回 token 数; <0 为错误(见 jsmnerr) */
int      my_json_parse(const char *js, jsmntok_t *toks, int max_tok);

/* 在对象 parent 中查找 key, 返回其 value 的 token 下标, 未找到返回 -1 */
int      my_json_find(const char *js, const jsmntok_t *toks, int ntoks, int parent, const char *key);

/* 数组元素个数 / 第 idx 个元素的 token 下标 */
int      my_json_arr_size(const jsmntok_t *toks, int arr);
int      my_json_arr_item(const jsmntok_t *toks, int arr, int idx);

/* 取值 */
int      my_json_to_int(const char *js, const jsmntok_t *tok);
void     my_json_to_str(const char *js, const jsmntok_t *tok, char *out, int outsz);
uint8_t  my_json_is_true(const char *js, const jsmntok_t *tok);		/* "true" 或 "1" */
uint8_t  my_json_is_array(const jsmntok_t *tok);
uint8_t  my_json_is_object(const jsmntok_t *tok);

#endif
