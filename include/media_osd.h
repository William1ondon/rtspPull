#ifndef _MEDIA_OSD_H_
#define _MEDIA_OSD_H_

#define MEDIA_MAX_STRING_LEN		64	
#define MEDIA_FONT_FILE_PATH    "/root/simqing.ttf"   /* 字库路径 */

typedef struct
{
	void *data; /* 指向堆上的一个512K的空间 */
	unsigned int width;
	unsigned int height;
}BITMAP_INFO;

class media_osd
{
private:
	/* 保存上一次显示的字符串 */
	char m_str[MEDIA_MAX_STRING_LEN]; 
	BITMAP_INFO m_bitmap[3];		//D1, 720P, 1080P
	void* m_glyph[3][MEDIA_MAX_STRING_LEN];

public:	
	media_osd();
	~media_osd();
	int setString(const char *str);
	const char* getString();
	BITMAP_INFO *getBitMap(unsigned int picWidth, unsigned int picHeight);

private:
	int fontPainter(unsigned char *buf, unsigned int width, unsigned int height, const char *string);
	int font_UpdateStrToARGB1555(const char *pszString, int fontCharSize, void *glyph, unsigned short *pu16Buf, unsigned int *width, unsigned int *height);
};

#endif /* _MEDIA_OSD_H_ */

