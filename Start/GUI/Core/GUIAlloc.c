/*
*********************************************************************************************************
*                                                uC/GUI
*                        Universal graphic software for embedded applications
*
*                       (c) Copyright 2002, Micrium Inc., Weston, FL
*                       (c) Copyright 2002, SEGGER Microcontroller Systeme GmbH
*
*              �C/GUI is protected by international copyright laws. Knowledge of the
*              source code may not be used to write a similar product. This file may
*              only be used in accordance with a license and should not be redistributed
*              in any way. We appreciate your understanding and fairness.
*
----------------------------------------------------------------------
File        : GUIAlloc.C
Purpose     : Dynamic memory management
----------------------------------------------------------------------
*/

#include <stddef.h>           /* needed for definition of NULL */
#include <string.h>           /* for memcpy, memset */

#include "GUI_Protected.h"
#include "GUIDebug.h"

/*********************************************************************
*
*       Internal memory management
*
**********************************************************************
*/

#ifndef GUI_ALLOC_ALLOC

#if GUI_ALLOC_SIZE==0
  #error GUI_ALLOC_SIZE needs to be > 0 when using this module
#endif

/*********************************************************************
*
*       Defines, config defaults
*
**********************************************************************
*/

/* Permit automatic defragmentation when necessary */
#ifndef GUI_ALLOC_AUTDEFRAG
  #define GUI_ALLOC_AUTDEFRAG 1		/* �����ֵΪ1 ��ʱ�򣬻��Զ�������Ƭ */
#endif

#ifndef GUI_BLOCK_ALIGN        /* 2 means 4 bytes, 1 means 2 bytes      */	
  #define GUI_BLOCK_ALIGN 2    /* 1 can be used on 16-bit CPUs and CPUs */
#endif                         /* which do not require aligned 32-bit   */
                               /* values (such as x86)                  */ 

#ifndef GUI_MAXBLOCKS
  #define GUI_MAXBLOCKS (2 + GUI_ALLOC_SIZE / 32)		/* ����ط��е�����,��ʼһֱû��������� 32 magic  number �Ǹ���ģ�������ϸdebug�����֣�
														 * ���32��һ������ֵ��̫���ˣ���С�������ʳ����� 
														 * ̫С�ˣ���̫�󣬺ܿ���������Ŀһ�¾����ˣ��ۺ�һ�£�����32������ô����
													     */
#endif

#ifndef GUI_ALLOC_LOCATION
  #define GUI_ALLOC_LOCATION
#endif

#ifndef GUI_MEM_ALLOC          /* Allows us in some systems to place the GUI memory */
  #define GUI_MEM_ALLOC        /* in a different memory space ... eg "__far"        */
#endif

/*********************************************************************
*
*       Defines
*
**********************************************************************
*/

#define Min(v0,v1) ((v0>v1) ? v1 : v0)
#define Max(v0,v1) ((v0>v1) ? v0 : v1)
#define ASSIGN_IF_LESS(v0,v1) if (v1<v0) v0=v1
#define HMEM2PTR(hMem) (void*)&GUI_Heap.abHeap[aBlock[hMem].Off]	// ȡ�� hmem �������ָ����ڴ�飬�����ַ����һֱ����д������ǲ���ȫ��

#if GUI_MAXBLOCKS >= 256
  #define HANDLE U16
#else
  #define HANDLE U8
#endif

/*********************************************************************
*
*       Types
*
**********************************************************************
*/

// ������һ�� ���ϣ�Ҫô�� U8 ��Ҫô�� INT ������
typedef union {
  int aintHeap[GUI_ALLOC_SIZE / 4];   /* required for proper alignement */
  U8  abHeap[GUI_ALLOC_SIZE];
} GUI_HEAP;

typedef struct {
  GUI_ALLOC_DATATYPE Off;       /* Offset of memory area          */
  GUI_ALLOC_DATATYPE Size;      /* usable size of allocated block */
  HANDLE Next;         /* next handle in linked list     */
  HANDLE Prev;
} tBlock;

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/

GUI_MEM_ALLOC GUI_HEAP GUI_Heap GUI_ALLOC_LOCATION;         /* Public for debugging only */

static tBlock aBlock[GUI_MAXBLOCKS];

struct {
  int       NumUsedBlocks, NumFreeBlocks, NumFreeBlocksMin; /* For statistical purposes only */
  GUI_ALLOC_DATATYPE NumUsedBytes,  NumFreeBytes,  NumFreeBytesMin;
} GUI_ALLOC;

static char   IsInitialized =0;

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/
/*********************************************************************
*
*       _Size2LegalSize
*
* Return value:
*   Legal allocation size
*/
static GUI_ALLOC_DATATYPE _Size2LegalSize(GUI_ALLOC_DATATYPE size) {
  return (size + ((1 << GUI_BLOCK_ALIGN) - 1)) & ~((1 << GUI_BLOCK_ALIGN) - 1);
}
  
/*********************************************************************
*
*       _GetSize
*/
static GUI_ALLOC_DATATYPE _GetSize(GUI_HMEM  hMem) {	// �� ucgui �У�hmem hwin ����һ��������һ���ڴ�������
  return aBlock[hMem].Size;	// ��� ��ǰ���block �Ĵ�С
}

/*********************************************************************
*
*       _Free
*/
static void _Free(GUI_HMEM hMem) {
  GUI_ALLOC_DATATYPE Size;
  GUI_DEBUG_LOG1("\nGUI_ALLOC_Free(%d)", hMem);
  /* Do some error checking ... */
  #if GUI_DEBUG_LEVEL>0
    /* Block not allocated ? */
  if (aBlock[hMem].Size == 0) { // size ���Ե����Ƿ�Ϊ���п�ı�־
      GUI_DEBUG_ERROROUT("GUI_ALLOC_Free(): Invalid hMem");
      return;
    }
  #endif
  Size = aBlock[hMem].Size;
  #ifdef WIN32
		// ȡ�� hmem ���ڿ�ĵ�ַ
		GUI_MEMSET(&GUI_Heap.abHeap[aBlock[hMem].Off], 0xcc, Size);// win32 �£�Ĭ�ϵ�δ��ʼ���ڴ�Ϊ 0xCC 
  #endif
  GUI_ALLOC.NumFreeBytes += Size;
  GUI_ALLOC.NumUsedBytes -= Size;	// ˢ��ͳ�ƿ��к�ʹ�õ��ֽڴ�С
  aBlock[hMem].Size = 0;
  {
    int Next = aBlock[hMem].Next;	// �������Ϥ�ˣ���һ����̬���� 
    int Prev = aBlock[hMem].Prev;	// ɾ��һ���ڵ�
    aBlock[Prev].Next = Next;
    if (Next) {
      aBlock[Next].Prev = Prev;
    }
  }  
  GUI_ALLOC.NumFreeBlocks++;
  GUI_ALLOC.NumUsedBlocks--;	// ˢ��ͳ�ƿ��к�ʹ�õĿ��С
}

/*********************************************************************
*
*       _FindFreeHandle
*
* Return value:
*   Free handle
*/
static GUI_HMEM _FindFreeHandle(void) {
  int i;
  for (i=1; i< GUI_MAXBLOCKS; i++) {	// ����0 ��Ĭ�ϲ��õģ�����0��ʱ���� ��Ч��� 
    if (aBlock[i].Size ==0)		// ͨ���ж�ʹ�õ� size  �������Ƿ��ǿ��п�
	  return i;
  }
  GUI_DEBUG_ERROROUT1("Insufficient memory handles configured (GUI_MAXBLOCKS == %d (See GUIConf.h))", GUI_MAXBLOCKS);
  // �ڴ治�� 
  return GUI_HMEM_NULL;
}

/*********************************************************************
*
*       _FindHole
*
* Return value:
*   Offset to the memory hole (if available)
*   -1 if not available
*/

static GUI_HMEM _FindHole(GUI_ALLOC_DATATYPE Size) {
  int i, iNext;
  
  for (i=0; (iNext = aBlock[i].Next) != 0; i = iNext) {// ������̬�����ҵ�һ�����ʵ�λ�ù���
    int NumFreeBytes = aBlock[iNext].Off- (aBlock[i].Off+aBlock[i].Size);	//�������ҵ�ǰһ�� block �ͺ�һ�� block ֮��� ��϶��С
    if (NumFreeBytes>=Size) {// �����϶�Ĵ�С�����С Size,��ô�ͻ��ڵ�ǰ�� block  (i)  ֮�����һ���ڵ� 
							// �������������㣬˵����ǰ���������ͷŵ��µ� hole ��
							// �����ͷŵĿռ������ܴ��ڵ�������Ŀռ䣬���ڿ��Ա��û���
      return i;
    }
  }
  /* Check last block */
  if (GUI_ALLOC_SIZE - (aBlock[i].Off+aBlock[i].Size) >= Size) {// ������������㣬˵������β��׷������һ��������
    return i;
  }
  return -1;	/* �����û�õض���  */
}

/*********************************************************************
*
*       _CreateHole
*
* Return value:
*   Offset to the memory hole (if available)
*   -1 if not available
*/

/* �����������Ƭ��һ������ */

static GUI_HMEM _CreateHole(GUI_ALLOC_DATATYPE Size) {		
  int i, iNext;
  int r = -1;
  for (i=0; (iNext =aBlock[i].Next) !=0; i= iNext) {	// ��ǰ�������������
    GUI_ALLOC_DATATYPE NumFreeBytes = aBlock[iNext].Off- (aBlock[i].Off+aBlock[i].Size); // �õ�����ڵ�ָ��Ŀռ��С
    if (NumFreeBytes < Size) {	// �������ռ��С
      GUI_ALLOC_DATATYPE NumBytesBeforeBlock = aBlock[iNext].Off - (aBlock[i].Off+aBlock[i].Size);
      if (NumBytesBeforeBlock) {	// ���ǰһ��block�ͺ�һ�� block���ڿ�϶����ô�ͽ���һ��block��ǰŲ��
        U8* pData = &GUI_Heap.abHeap[aBlock[iNext].Off];
        memmove(pData-NumBytesBeforeBlock, pData, aBlock[iNext].Size);	// �ڴ��ƶ�
        aBlock[iNext].Off -=NumBytesBeforeBlock;//	��һ����  block ƫ�ƣ���ǰŲ��
      }
    }
  }
  /* Check last block */
  if (GUI_ALLOC_SIZE - (aBlock[i].Off+aBlock[i].Size) >= Size) {		// ���û�п�϶����ô�ͺ� find_hole һ����
    r = i;
  }
  return r;
}

/*********************************************************************
*
*       _CheckInit
*/
static void _CheckInit(void) {
  if (!IsInitialized) {
    GUI_ALLOC_Init();
  }
}

/*********************************************************************
*
*       _Alloc
*/
static GUI_HMEM _Alloc(GUI_ALLOC_DATATYPE size) {
  GUI_HMEM hMemNew, hMemIns;
  _CheckInit();		//�ȼ���Ƿ��Ѿ���ʼ���˷����
  size = _Size2LegalSize(size);	// ����Сǿ��4�ֽڶ���
  /* Check if memory is available at all ...*/
  if (size > GUI_ALLOC.NumFreeBytes) {//�Ƿ�����Ĺ���
    GUI_DEBUG_WARN1("GUI_ALLOC_Alloc: Insufficient memory configured (Trying to alloc % bytes)", size);
    return 0;
  }
  /* Locate free handle */
  if ((hMemNew = _FindFreeHandle()) == 0)	//  ����һ�����е� block slot 
    return 0;	// �������ʧ�� 
  /* Locate or Create hole of sufficient size */
  hMemIns = _FindHole(size);// ����ɹ����ҵ�һ�����ʵĹ��ص㣨������ʹ�õĽڵ�������
  #if GUI_ALLOC_AUTDEFRAG	// ��������Զ�������Ƭ 
    if (hMemIns == -1) {
      hMemIns = _CreateHole(size);// �����ڴ棬��˼���ǵ�С�����������ô�࣬��������Ŀ�û�У��Ǿͽ����ڴ棬��������
    }
  #endif
  /* Occupy hole */
  if (hMemIns==-1) {	// ��������ˣ�����û�У���ô�ʹ�����
    GUI_DEBUG_ERROROUT1("GUI_ALLOC_Alloc: Could not allocate %d bytes",size);
    return 0;
	}
  {  // �����ǹ���ʹ�ýڵ㵽ʹ������Ĺ���
    GUI_ALLOC_DATATYPE Off = aBlock[hMemIns].Off + aBlock[hMemIns].Size;// �����һ�����õ�block offset ��ʼ��ַ
    int Next = aBlock[hMemIns].Next;
    aBlock[hMemNew].Size  = size; // ��ֵ size ��Ա
    aBlock[hMemNew].Off   = Off; // ��ֵ offset 
    if ((aBlock[hMemNew].Next  = Next) >0) { // ���� new block slot������(�Ӳ�������) 
      aBlock[Next].Prev = hMemNew;  // ˫����
    }
    aBlock[hMemNew].Prev  = hMemIns;
    aBlock[hMemIns].Next  = hMemNew;
  }// ���ˣ�����OK��
  /* Keep track of number of blocks and av. memory */	// ����ͳ�Ƶ����ݣ��飬�ֽڣ��������飬��С�� 
  GUI_ALLOC.NumUsedBlocks++;
  GUI_ALLOC.NumFreeBlocks--;
  if (GUI_ALLOC.NumFreeBlocksMin > GUI_ALLOC.NumFreeBlocks) {
    GUI_ALLOC.NumFreeBlocksMin = GUI_ALLOC.NumFreeBlocks;
  }
  GUI_ALLOC.NumUsedBytes += size;
  GUI_ALLOC.NumFreeBytes -= size;
  if (GUI_ALLOC.NumFreeBytesMin > GUI_ALLOC.NumFreeBytes) {
    GUI_ALLOC.NumFreeBytesMin = GUI_ALLOC.NumFreeBytes;
  }
  return hMemNew;
}

/*********************************************************************
*
*       Exported routines
*
**********************************************************************
*/
/*********************************************************************
*
*       GUI_ALLOC_Init
*/
void GUI_ALLOC_Init(void) {
  GUI_DEBUG_LOG("\nGUI_ALLOC_Init...");
  GUI_ALLOC.NumFreeBlocksMin = GUI_ALLOC.NumFreeBlocks = GUI_MAXBLOCKS-1;// �ܷ����������
  GUI_ALLOC.NumFreeBytesMin  = GUI_ALLOC.NumFreeBytes  = GUI_ALLOC_SIZE;	//  �ܷ��������ֽ���
  GUI_ALLOC.NumUsedBlocks = 0;	//��ʹ�õĿ���Ŀ
  GUI_ALLOC.NumUsedBytes = 0;	// ��ʹ�õ��ֽ���
  aBlock[0].Size = (1<<GUI_BLOCK_ALIGN);  /* occupy minimum for a block */   //  ��0 ��Ĭ�ϲ��ã�ռ��4�ֽ�(��ablock �ṹ���Сô��)
  aBlock[0].Off  = 0; // 
  aBlock[0].Next = 0;
  IsInitialized =1;
}

/*********************************************************************
*
*       GUI_ALLOC_AllocNoInit
*/
GUI_HMEM GUI_ALLOC_AllocNoInit(GUI_ALLOC_DATATYPE Size) {
  GUI_HMEM hMem;
  if (Size == 0) {//�൱�ڲ�����
    return (GUI_HMEM)0;
  }
  GUI_LOCK();//�ٴ�lock ������ֻ�ǽ� k_entrance ���Ӷ���
  GUI_DEBUG_LOG2("\nGUI_ALLOC_AllocNoInit... requesting %d, %d avail", Size, GUI_ALLOC.NumFreeBytes);
  hMem = _Alloc(Size); // ����size�ֽ�
  GUI_DEBUG_LOG1("\nGUI_ALLOC_AllocNoInit : Handle", hMem);
  GUI_UNLOCK();
  return hMem;
}

/*********************************************************************
*
*       GUI_ALLOC_h2p
*/
void* GUI_ALLOC_h2p(GUI_HMEM  hMem) {
  GUI_ASSERT_LOCK();
  #if GUI_DEBUG_LEVEL > 0
    if (!hMem) {
      GUI_DEBUG_ERROROUT("\n"__FILE__ " GUI_ALLOC_h2p: illegal argument (0 handle)");
      return 0;
    }
    if (aBlock[hMem].Size == 0) {
      GUI_DEBUG_ERROROUT("Dereferencing free block");
    }

  #endif
  return HMEM2PTR(hMem);
}

/*********************************************************************
*
*       GUI_ALLOC_GetNumFreeBytes
*/
GUI_ALLOC_DATATYPE GUI_ALLOC_GetNumFreeBytes(void) {
  _CheckInit();
  return GUI_ALLOC.NumFreeBytes;  
}

/*********************************************************************
*
*       GUI_ALLOC_GetMaxSize
*
* Purpose:
*   Returns the biggest available blocksize (without relocation).
*/
GUI_ALLOC_DATATYPE GUI_ALLOC_GetMaxSize(void) {
  GUI_ALLOC_DATATYPE r = 0;
  GUI_ALLOC_DATATYPE NumFreeBytes;
  int i, iNext;

  GUI_LOCK();
  _CheckInit();
  for (i=0; (iNext =aBlock[i].Next) !=0; i= iNext) {
    NumFreeBytes = aBlock[iNext].Off- (aBlock[i].Off+aBlock[i].Size);
    if (NumFreeBytes > r) { // �����������ǰ���ͷŵõ��Ŀ�϶�������������Ҫ
      r = NumFreeBytes;
    }
  }
  /* Check last block */
  NumFreeBytes = (GUI_ALLOC_SIZE - (aBlock[i].Off+aBlock[i].Size));
  if (NumFreeBytes > r) {	// ��������Ļ��������һ�����Ŀ���
    r = NumFreeBytes;
  }
  GUI_UNLOCK();
  return r;
}

#else

/*********************************************************************
*
*       External memory management functions
*
* The functions below will generate code only if the GUI memory
* management is not used (GUI_ALLOC_ALLOC defined).
*
* Note:
* The memory block allocated is bigger than the requested one, as we
* store some add. information (size of the memory block) there.
*
**********************************************************************
*/

typedef struct {
  union {
    GUI_ALLOC_DATATYPE Size;
    int Dummy;               /* Needed to guarantee alignment on 32 / 64 bit CPUs */
  } Info;      /* Unnamed would be best, but is not supported by all compilers */
} INFO;

/*********************************************************************
*
*       _GetSize
*/
static GUI_ALLOC_DATATYPE _GetSize(GUI_HMEM  hMem) {
  INFO * pInfo;
  pInfo = (INFO *)GUI_ALLOC_H2P(hMem);
  return pInfo->Info.Size;
}

/*********************************************************************
*
*       _Free
*/
static void _Free(GUI_HMEM  hMem) {
  GUI_ALLOC_FREE(hMem);
}

/*********************************************************************
*
*       GUI_ALLOC_AllocNoInit
*/
GUI_HMEM GUI_ALLOC_AllocNoInit(GUI_ALLOC_DATATYPE Size) {
  GUI_HMEM hMem;
  if (Size == 0) {
    return (GUI_HMEM)0;
  }
  hMem= GUI_ALLOC_ALLOC(Size + sizeof(INFO));
  /* Init info structure */
  if (hMem) {
    INFO * pInfo;
    pInfo = (INFO *)GUI_ALLOC_H2P(hMem);
    pInfo->Info.Size = Size;
  }
  return hMem;
}

/*********************************************************************
*
*       GUI_ALLOC_h2p
*/
void* GUI_ALLOC_h2p(GUI_HMEM  hMem) {
  U8* p = (U8*)GUI_ALLOC_H2P(hMem);    /* Pointer to memory block from memory manager */
  p += sizeof(INFO);                   /* Convert to pointer to usable area */
  return p;
}

/*********************************************************************
*
*       GUI_ALLOC_GetMaxSize
*/
GUI_ALLOC_DATATYPE GUI_ALLOC_GetMaxSize(void) {
  return GUI_ALLOC_GETMAXSIZE();
}

/*********************************************************************
*
*       GUI_ALLOC_Init
*/
void GUI_ALLOC_Init(void) {
  #ifdef GUI_ALLOC_INIT
    GUI_ALLOC_INIT();
  #endif
}

#endif

/*********************************************************************
*
*       Public code, common memory management functions
*
**********************************************************************
*/
/*********************************************************************
*
*       GUI_ALLOC_GetSize
*/
GUI_ALLOC_DATATYPE GUI_ALLOC_GetSize(GUI_HMEM  hMem) {
  /* Do the error checking first */
  #if GUI_DEBUG_LEVEL>0
    if (!hMem) {
      GUI_DEBUG_ERROROUT("\n"__FILE__ " GUI_ALLOC_h2p: illegal argument (0 handle)");
      return 0;
    }
  #endif
  return _GetSize(hMem);
}

/*********************************************************************
*
*       GUI_ALLOC_Free
*/
void GUI_ALLOC_Free(GUI_HMEM hMem) {
  if (hMem == GUI_HMEM_NULL) { /* Note: This is not an error, it is permitted */
    return;
  }
  GUI_LOCK();
  GUI_DEBUG_LOG1("\nGUI_ALLOC_Free(%d)", hMem);
  _Free(hMem);
  GUI_UNLOCK();
}


/*********************************************************************
*
*       GUI_ALLOC_FreePtr
*/
void GUI_ALLOC_FreePtr(GUI_HMEM *ph) {
  GUI_LOCK();
  GUI_ALLOC_Free(*ph);
  *ph =0;
  GUI_UNLOCK();
}


/*************************** End of file ****************************/
