
#include "LCD_Private.h"      /* private modul definitions & config */
#include "GUI_Private.h"
#include "GUIDebug.h"

/*********************************************************************
*
*       Exported functions
*
**********************************************************************
*/

/*********************************************************************
*
*       LCD_L0_SetPixelIndex
*/
void LCD_L0_SetPixelIndex(int x, int y, int PixelIndex) {
  GUI_USE_PARA(x);
  GUI_USE_PARA(y);
  GUI_USE_PARA(PixelIndex);
  
  vga_set_pixel(x,y,PixelIndex);
}

/*********************************************************************
*
*       LCD_L0_GetPixelIndex
*/
unsigned int LCD_L0_GetPixelIndex(int x, int y) {
  GUI_USE_PARA(x);
  GUI_USE_PARA(y);
  return vga_get_pixel(x,y);
}

/*********************************************************************
*
*       LCD_L0_XorPixel
*/
void LCD_L0_XorPixel(int x, int y) {
  GUI_USE_PARA(x);
  GUI_USE_PARA(y);
  //vga_set_pixel(x,y, vga_get_pixel(x,y));
}

/*********************************************************************
*
*       LCD_L0_DrawHLine
*/
void LCD_L0_DrawHLine(int x0, int y,  int x1) {
  GUI_USE_PARA(x0);
  GUI_USE_PARA(y);
  GUI_USE_PARA(x1);
  for( int i =x0 ; i <  x1; i++)
  	vga_set_pixel(i,y,GUI_GetColorIndex());
}

/*********************************************************************
*
*       LCD_L0_DrawVLine
*/
void LCD_L0_DrawVLine(int x, int y0,  int y1) {
  GUI_USE_PARA(x);
  GUI_USE_PARA(y0);
  GUI_USE_PARA(y1);
  for( int i =y0 ; i <  y1; i++)
  	vga_set_pixel(x,i,GUI_GetColorIndex());
 
}

/*********************************************************************
*
*       LCD_L0_FillRect
*/
void LCD_L0_FillRect(int x0, int y0, int x1, int y1) {
  GUI_USE_PARA(x0);
  GUI_USE_PARA(y0);
  GUI_USE_PARA(x1);
  GUI_USE_PARA(y1);
  for( int i =x0 ; i <  x1; i++)
  	for( int j =y0 ; j <  y1; j++)
  		vga_set_pixel(i,j,GUI_GetColorIndex());
 
}

/*********************************************************************
*
*       LCD_L0_DrawBitmap
*/
void LCD_L0_DrawBitmap(int x0, int y0,
                       int xsize, int ysize,
                       int BitsPerPixel, 
                       int BytesPerLine,
                       const U8 GUI_UNI_PTR * pData, int Diff,
                       const LCD_PIXELINDEX* pTrans)
{
  GUI_USE_PARA(x0);
  GUI_USE_PARA(y0);
  GUI_USE_PARA(xsize);
  GUI_USE_PARA(ysize);
  GUI_USE_PARA(BitsPerPixel);
  GUI_USE_PARA(BytesPerLine);
  GUI_USE_PARA(pData);
  GUI_USE_PARA(Diff);
  GUI_USE_PARA(pTrans);
}

/*********************************************************************
*
*       LCD_L0_SetOrg
*/
void LCD_L0_SetOrg(int x, int y) {
  GUI_USE_PARA(x);
  GUI_USE_PARA(y);
}

/*********************************************************************
*
*       LCD_On / LCD_Off
*/
void LCD_On (void) {}
void LCD_Off(void) {}

/*********************************************************************
*
*       LCD_L0_Init
*/
int LCD_L0_Init(void) {
  return 0;
}

/*********************************************************************
*
*       LCD_L0_SetLUTEntry
*/
void LCD_L0_SetLUTEntry(U8 Pos, LCD_COLOR Color) {
  GUI_USE_PARA(Pos);
  GUI_USE_PARA(Color);
}

#include <sys/types.h>
#define _STRING_H
#define static 
#define inline 

#include "junos/string.h"

int memcmp(const void *buf1, const void *buf2, size_t n)
{
	char *s1 = (char *) buf1;
	char *s2 = (char *) buf2;

	while (n-- && *s1 == *s2) {
		s1++;
		s2++;
	}

	return (*s1 - *s2);
}

char *strcpy(char *dst, const char *src)
{
	char *tmp = dst;
	char *t =(char *)src;
	while (*t)
		*tmp++ = *t++;
	*tmp = 0;
	return dst;
}

void *memmove(void *s1, const void *s2, size_t n)
{
	char *p1 = (char*) s1;
	char *p2 = (char*) s2;

	if(!n)
		return s1;

	if(p2<=p1 &&p2+n>p1){
		p1+=n;
		p2+=n;
		while(n--){
			*--p1=*--p2;
		}
	}else{
		while(n--){
			*p1++=*p2++;
		}
	}

	return s1;
}

#undef static 
#undef inline

void GUI_X_Init(){}
void GUI_X_ExecIdle(){}
void GUI_X_Delay(int Period){}

int GUI_X_GetTime()
{
	return 0;
}

int logx (const char *fmt,...) 
{
	return 0;
}
