/*
*********************************************************************************************************
*                                                uC/GUI
*                        Universal graphic software for embedded applications
*
*                       (c) Copyright 2002, Micrium Inc., Weston, FL
*                       (c) Copyright 2002, SEGGER Microcontroller Systeme GmbH
*
*              礐/GUI is protected by international copyright laws. Knowledge of the
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
  #define GUI_ALLOC_AUTDEFRAG 1		/* 当这个值为1 的时候，会自动整理碎片 */
#endif

#ifndef GUI_BLOCK_ALIGN        /* 2 means 4 bytes, 1 means 2 bytes      */	
  #define GUI_BLOCK_ALIGN 2    /* 1 can be used on 16-bit CPUs and CPUs */
#endif                         /* which do not require aligned 32-bit   */
                               /* values (such as x86)                  */ 

#ifndef GUI_MAXBLOCKS
  #define GUI_MAXBLOCKS (2 + GUI_ALLOC_SIZE / 32)		/* 这个地方有点玄妙,开始一直没搞明白这个 32 magic  number 是干嘛的，后来仔细debug，发现，
														 * 这个32就一个经验值，太大了，块小，利用率超级低 
														 * 太小了，块太大，很可能申请数目一下就满了，综合一下，诶，32，就这么来的
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
#define HMEM2PTR(hMem) (void*)&GUI_Heap.abHeap[aBlock[hMem].Off]	// 取得 hmem 这个索引指向的内存块，这个地址可以一直往后写，这个是不安全的

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

// 这里是一个 联合，要么是 U8 ，要么是 INT ，对齐
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
static GUI_ALLOC_DATATYPE _GetSize(GUI_HMEM  hMem) {	// 在 ucgui 中，hmem hwin 都是一个索引，一个内存块的索引
  return aBlock[hMem].Size;	// 获得 当前句柄block 的大小
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
  if (aBlock[hMem].Size == 0) { // size 可以当做是否为空闲块的标志
      GUI_DEBUG_ERROROUT("GUI_ALLOC_Free(): Invalid hMem");
      return;
    }
  #endif
  Size = aBlock[hMem].Size;
  #ifdef WIN32
		// 取得 hmem 所在块的地址
		GUI_MEMSET(&GUI_Heap.abHeap[aBlock[hMem].Off], 0xcc, Size);// win32 下，默认的未初始化内存为 0xCC 
  #endif
  GUI_ALLOC.NumFreeBytes += Size;
  GUI_ALLOC.NumUsedBytes -= Size;	// 刷新统计空闲和使用的字节大小
  aBlock[hMem].Size = 0;
  {
    int Next = aBlock[hMem].Next;	// 这里很熟悉了，是一个静态链表 
    int Prev = aBlock[hMem].Prev;	// 删除一个节点
    aBlock[Prev].Next = Next;
    if (Next) {
      aBlock[Next].Prev = Prev;
    }
  }  
  GUI_ALLOC.NumFreeBlocks++;
  GUI_ALLOC.NumUsedBlocks--;	// 刷新统计空闲和使用的块大小
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
  for (i=1; i< GUI_MAXBLOCKS; i++) {	// 索引0 是默认不用的，返回0的时候是 无效句柄 
    if (aBlock[i].Size ==0)		// 通过判断使用的 size  来决定是否是空闲块
	  return i;
  }
  GUI_DEBUG_ERROROUT1("Insufficient memory handles configured (GUI_MAXBLOCKS == %d (See GUIConf.h))", GUI_MAXBLOCKS);
  // 内存不足 
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
  
  for (i=0; (iNext = aBlock[i].Next) != 0; i = iNext) {// 遍历静态链表，找到一个合适的位置挂载
    int NumFreeBytes = aBlock[iNext].Off- (aBlock[i].Off+aBlock[i].Size);	//这里是找到前一个 block 和后一个 block 之间的 间隙大小
    if (NumFreeBytes>=Size) {// 如果间隙的大小满足大小 Size,那么就会在当前的 block  (i)  之后插入一个节点 
							// 如果这个条件满足，说明是前面的申请后释放导致的 hole ，
							// 而且释放的空间至少能大于等于申请的空间，现在可以被拿回来
      return i;
    }
  }
  /* Check last block */
  if (GUI_ALLOC_SIZE - (aBlock[i].Off+aBlock[i].Size) >= Size) {// 如果是这里满足，说明是在尾部追加申请一块新区域
    return i;
  }
  return -1;	/* 运气差，没得地儿了  */
}

/*********************************************************************
*
*       _CreateHole
*
* Return value:
*   Offset to the memory hole (if available)
*   -1 if not available
*/

/* 这个是整理碎片的一个函数 */

static GUI_HMEM _CreateHole(GUI_ALLOC_DATATYPE Size) {		
  int i, iNext;
  int r = -1;
  for (i=0; (iNext =aBlock[i].Next) !=0; i= iNext) {	// 从前端依次往后遍历
    GUI_ALLOC_DATATYPE NumFreeBytes = aBlock[iNext].Off- (aBlock[i].Off+aBlock[i].Size); // 得到这个节点指向的空间大小
    if (NumFreeBytes < Size) {	// 如果这个空间大小
      GUI_ALLOC_DATATYPE NumBytesBeforeBlock = aBlock[iNext].Off - (aBlock[i].Off+aBlock[i].Size);
      if (NumBytesBeforeBlock) {	// 如果前一个block和后一个 block存在空隙，那么就讲后一个block往前挪动
        U8* pData = &GUI_Heap.abHeap[aBlock[iNext].Off];
        memmove(pData-NumBytesBeforeBlock, pData, aBlock[iNext].Size);	// 内存移动
        aBlock[iNext].Off -=NumBytesBeforeBlock;//	后一个的  block 偏移，往前挪动
      }
    }
  }
  /* Check last block */
  if (GUI_ALLOC_SIZE - (aBlock[i].Off+aBlock[i].Size) >= Size) {		// 如果没有空隙，那么就和 find_hole 一样了
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
  _CheckInit();		//先检查是否已经初始化了分配块
  size = _Size2LegalSize(size);	// 将大小强制4字节对齐
  /* Check if memory is available at all ...*/
  if (size > GUI_ALLOC.NumFreeBytes) {//是否申请的过大
    GUI_DEBUG_WARN1("GUI_ALLOC_Alloc: Insufficient memory configured (Trying to alloc % bytes)", size);
    return 0;
  }
  /* Locate free handle */
  if ((hMemNew = _FindFreeHandle()) == 0)	//  申请一个空闲的 block slot 
    return 0;	// 如果申请失败 
  /* Locate or Create hole of sufficient size */
  hMemIns = _FindHole(size);// 申请成功后，找到一个合适的挂载点（挂载在使用的节点链表内
  #if GUI_ALLOC_AUTDEFRAG	// 如果允许自动消除碎片 
    if (hMemIns == -1) {
      hMemIns = _CreateHole(size);// 紧缩内存，意思就是当小块加起来有这么多，但连续大的块没有，那就紧缩内存，重新整理
    }
  #endif
  /* Occupy hole */
  if (hMemIns==-1) {	// 如果紧缩了，还是没有，那么就错误了
    GUI_DEBUG_ERROROUT1("GUI_ALLOC_Alloc: Could not allocate %d bytes",size);
    return 0;
	}
  {  // 这里是挂载使用节点到使用链表的过程
    GUI_ALLOC_DATATYPE Off = aBlock[hMemIns].Off + aBlock[hMemIns].Size;// 获得下一个可用的block offset 起始地址
    int Next = aBlock[hMemIns].Next;
    aBlock[hMemNew].Size  = size; // 赋值 size 成员
    aBlock[hMemNew].Off   = Off; // 赋值 offset 
    if ((aBlock[hMemNew].Next  = Next) >0) { // 插入 new block slot的索引(从插入点插入) 
      aBlock[Next].Prev = hMemNew;  // 双联表
    }
    aBlock[hMemNew].Prev  = hMemIns;
    aBlock[hMemIns].Next  = hMemNew;
  }// 好了，挂载OK了
  /* Keep track of number of blocks and av. memory */	// 更新统计的数据，块，字节，还有最大块，最小块 
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
  GUI_ALLOC.NumFreeBlocksMin = GUI_ALLOC.NumFreeBlocks = GUI_MAXBLOCKS-1;// 能分配的最大块数
  GUI_ALLOC.NumFreeBytesMin  = GUI_ALLOC.NumFreeBytes  = GUI_ALLOC_SIZE;	//  能分配的最大字节数
  GUI_ALLOC.NumUsedBlocks = 0;	//已使用的块数目
  GUI_ALLOC.NumUsedBytes = 0;	// 已使用的字节数
  aBlock[0].Size = (1<<GUI_BLOCK_ALIGN);  /* occupy minimum for a block */   //  第0 块默认不用，占用4字节(是ablock 结构体大小么？)
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
  if (Size == 0) {//相当于不申请
    return (GUI_HMEM)0;
  }
  GUI_LOCK();//再次lock ，这里只是将 k_entrance 增加而已
  GUI_DEBUG_LOG2("\nGUI_ALLOC_AllocNoInit... requesting %d, %d avail", Size, GUI_ALLOC.NumFreeBytes);
  hMem = _Alloc(Size); // 申请size字节
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
    if (NumFreeBytes > r) { // 这里满足就是前面释放得到的空隙，满足申请的需要
      r = NumFreeBytes;
    }
  }
  /* Check last block */
  NumFreeBytes = (GUI_ALLOC_SIZE - (aBlock[i].Off+aBlock[i].Size));
  if (NumFreeBytes > r) {	// 这里满足的话就是最后一个最大的块了
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
