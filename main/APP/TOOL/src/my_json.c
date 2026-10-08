#include "my_json.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

/* ==================== 组包 ==================== */

void my_json_init(my_json_t *js, char *buf, uint32_t cap)
{
	if (js == NULL) return;
	js->buf   = buf;
	js->cap   = cap;
	js->len   = 0;
	js->depth = 0;
	js->err   = 0;
	if (buf != NULL && cap > 0) buf[0] = '\0';
}

void my_json_reset(my_json_t *js)
{
	if (js == NULL) return;
	js->len   = 0;
	js->depth = 0;
	js->err   = 0;
	if (js->buf != NULL && js->cap > 0) js->buf[0] = '\0';
}

uint32_t my_json_len(my_json_t *js)
{
	return (js == NULL) ? 0 : js->len;
}

uint8_t my_json_error(my_json_t *js)
{
	return (js == NULL) ? 1 : js->err;
}

/* 写一个字节, 带越界保护 */
static void mj_putc(my_json_t *js, char c)
{
	if (js->err || js->buf == NULL) return;
	if (js->len + 1 >= js->cap)		/* 预留结束符位置 */
	{
		js->err = 1;
		return;
	}
	js->buf[js->len++] = c;
	js->buf[js->len]   = '\0';
}

static void mj_puts(my_json_t *js, const char *s)
{
	if (s == NULL) return;
	while (*s != '\0') mj_putc(js, *s++);
}

/* 以 JSON 字符串形式写入(含转义) */
static void mj_put_quoted(my_json_t *js, const char *s)
{
	mj_putc(js, '"');
	if (s != NULL)
	{
		while (*s != '\0')
		{
			unsigned char ch = (unsigned char)(*s++);
			switch (ch)
			{
				case '"' : mj_putc(js, '\\'); mj_putc(js, '"');  break;
				case '\\': mj_putc(js, '\\'); mj_putc(js, '\\'); break;
				case '\n': mj_putc(js, '\\'); mj_putc(js, 'n');  break;
				case '\r': mj_putc(js, '\\'); mj_putc(js, 'r');  break;
				case '\t': mj_putc(js, '\\'); mj_putc(js, 't');  break;
				default:
					if (ch < 0x20)
					{
						char u[8];
						sprintf(u, "\\u%04X", ch);
						mj_puts(js, u);
					}
					else
						mj_putc(js, (char)ch);
					break;
			}
		}
	}
	mj_putc(js, '"');
}

/* 成员前缀: 自动逗号 + (可选)键名 */
static void mj_prefix(my_json_t *js, const char *key)
{
	if (js == NULL || js->buf == NULL || js->err) return;
	if (js->depth == 0)		/* 未打开任何容器, 非法 */
	{
		js->err = 1;
		return;
	}
	if (!js->first[js->depth - 1]) mj_putc(js, ',');
	js->first[js->depth - 1] = 0;
	if (key != NULL)
	{
		mj_put_quoted(js, key);
		mj_putc(js, ':');
	}
}

static void mj_container_begin(my_json_t *js, const char *key, uint8_t type)
{
	if (js == NULL || js->buf == NULL || js->err) return;

	if (js->depth == 0)			/* 根容器: 无键、无逗号 */
	{
		if (key != NULL) { js->err = 1; return; }
	}
	else						/* 作为父容器的一个成员 */
	{
		if (!js->first[js->depth - 1]) mj_putc(js, ',');
		js->first[js->depth - 1] = 0;
		if (key != NULL)
		{
			mj_put_quoted(js, key);
			mj_putc(js, ':');
		}
	}

	if (js->depth >= MY_JSON_MAX_DEPTH) { js->err = 1; return; }
	js->type[js->depth]  = type;
	js->first[js->depth] = 1;
	mj_putc(js, (type == 0) ? '{' : '[');
	js->depth++;
}

static void mj_container_end(my_json_t *js)
{
	if (js == NULL || js->err) return;
	if (js->depth == 0) { js->err = 1; return; }
	js->depth--;
	mj_putc(js, (js->type[js->depth] == 0) ? '}' : ']');
}

void my_json_object_begin(my_json_t *js, const char *key)
{
	mj_container_begin(js, key, 0);
}

void my_json_object_end(my_json_t *js)
{
	mj_container_end(js);
}

void my_json_array_begin(my_json_t *js, const char *key)
{
	mj_container_begin(js, key, 1);
}

void my_json_array_end(my_json_t *js)
{
	mj_container_end(js);
}

void my_json_add_str(my_json_t *js, const char *key, const char *val)
{
	mj_prefix(js, key);
	mj_put_quoted(js, val);
}

void my_json_add_int(my_json_t *js, const char *key, int32_t val)
{
	char tmp[16];
	sprintf(tmp, "%d", (int)val);
	mj_prefix(js, key);
	mj_puts(js, tmp);
}

void my_json_add_bool(my_json_t *js, const char *key, uint8_t val)
{
	mj_prefix(js, key);
	mj_puts(js, (val != 0) ? "true" : "false");
}

void my_json_add_null(my_json_t *js, const char *key)
{
	mj_prefix(js, key);
	mj_puts(js, "null");
}

void my_json_add_raw(my_json_t *js, const char *key, const char *raw)
{
	mj_prefix(js, key);
	mj_puts(js, (raw != NULL) ? raw : "null");
}

/* ==================== 解析(jsmn) ==================== */

int my_json_parse(const char *js, jsmntok_t *toks, int max_tok)
{
	jsmn_parser parser;

	if (js == NULL || toks == NULL || max_tok <= 0) return JSMN_ERROR_INVAL;
	jsmn_init(&parser);
	return jsmn_parse(&parser, js, strlen(js), toks, (unsigned int)max_tok);
}

/* 返回跳过 token i 整个子树后的下一个下标 */
static int mj_tok_skip(const jsmntok_t *toks, int i)
{
	int n, j;
	n = toks[i].size;
	i++;
	for (j = 0; j < n; j++) i = mj_tok_skip(toks, i);
	return i;
}

static uint8_t mj_key_eq(const char *js, const jsmntok_t *tok, const char *key)
{
	int len = tok->end - tok->start;
	if (len <= 0) return 0;
	if (len != (int)strlen(key)) return 0;
	return (strncmp(js + tok->start, key, (size_t)len) == 0) ? 1 : 0;
}

int my_json_find(const char *js, const jsmntok_t *toks, int ntoks, int parent, const char *key)
{
	int i, end;

	if (js == NULL || toks == NULL || key == NULL) return -1;
	if (parent < 0 || parent >= ntoks) return -1;
	if (toks[parent].type != JSMN_OBJECT) return -1;

	end = mj_tok_skip(toks, parent);
	i = parent + 1;
	while (i < end)
	{
		int v = i + 1;
		if (toks[i].type == JSMN_STRING && mj_key_eq(js, &toks[i], key))
			return (v < ntoks) ? v : -1;
		i = mj_tok_skip(toks, v);
	}
	return -1;
}

int my_json_arr_size(const jsmntok_t *toks, int arr)
{
	if (toks == NULL || arr < 0) return 0;
	if (toks[arr].type != JSMN_ARRAY) return 0;
	return toks[arr].size;
}

int my_json_arr_item(const jsmntok_t *toks, int arr, int idx)
{
	int i;
	if (toks == NULL || arr < 0 || idx < 0) return -1;
	i = arr + 1;
	while (idx-- > 0) i = mj_tok_skip(toks, i);
	return i;
}

int my_json_to_int(const char *js, const jsmntok_t *tok)
{
	char tmp[24];
	int  len;

	if (js == NULL || tok == NULL) return 0;
	len = tok->end - tok->start;
	if (len <= 0) return 0;
	if (len > (int)sizeof(tmp) - 1) len = (int)sizeof(tmp) - 1;
	memcpy(tmp, js + tok->start, (size_t)len);
	tmp[len] = '\0';
	return (int)strtol(tmp, NULL, 10);
}

void my_json_to_str(const char *js, const jsmntok_t *tok, char *out, int outsz)
{
	int len;

	if (out == NULL || outsz <= 0) return;
	out[0] = '\0';
	if (js == NULL || tok == NULL) return;
	len = tok->end - tok->start;
	if (len <= 0) return;
	if (len > outsz - 1) len = outsz - 1;
	memcpy(out, js + tok->start, (size_t)len);
	out[len] = '\0';
}

uint8_t my_json_is_true(const char *js, const jsmntok_t *tok)
{
	int len;

	if (js == NULL || tok == NULL) return 0;
	len = tok->end - tok->start;
	if (len == 4 && strncmp(js + tok->start, "true", 4) == 0) return 1;
	if (len == 1 && *(js + tok->start) == '1') return 1;
	return 0;
}

uint8_t my_json_is_array(const jsmntok_t *tok)
{
	return (tok != NULL && tok->type == JSMN_ARRAY) ? 1 : 0;
}

uint8_t my_json_is_object(const jsmntok_t *tok)
{
	return (tok != NULL && tok->type == JSMN_OBJECT) ? 1 : 0;
}
