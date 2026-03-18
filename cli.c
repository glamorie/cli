#include "cli.h"
#include <math.h>
#include <assert.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef size_t usize;

// Utility functions

#define CliDLLPush(Parent, Node, Head, Tail) \
do { \
  if ((Parent)->Tail) (Parent)->Tail->Next = (Node); \
  else (Parent)->Head = (Node); \
  (Node)->Prev = (Parent)->Tail; \
  (Node)->Next = NULL; \
  (Parent)->Tail = (Node); \
} while (0)

static usize
CliDistance(usize A, usize B)
{
  return A < B ? B - A : A - B;
};

static inline usize
CliAlignUp(usize Point, usize Alignment)
{
  return Point + (Alignment - Point % Alignment) % Alignment;
};

static inline usize
CliAlignPadding(usize Point, usize Alignment)
{
  return (Alignment - Point % Alignment) % Alignment;
};

static inline usize
CliMax(usize A, usize B)
{
  return A > B ? A : B;
};

static inline usize
CliMin(usize A, usize B)
{
  return A < B ? A : B;
};

static inline usize
CliClamp(usize X, usize A, usize B)
{
  return X < A ? A : X < B ? X : B;
};

usize
CliStrLen(const char* Value)
{
  usize Length = 0;
  if (Value)
  {
    while (Value[Length]) Length++;
  };
  return Length;
};

void*
CliMemoryZero(void* Memory, usize Size)
{
  if (!Memory) Size = 0;
  
  for (usize i = 0; i < Size; i++)
  {
    ((u8*)Memory)[i] = 0;
  };
  
  return Memory;
};

void*
CliMemoryCopy(void* Dest, const void* Src, usize Length)
{
  if (!Dest || !Src || Src == Dest) Length = 0;
  
  if (CliDistance((usize)Src, (usize)Dest) < Length)
  {
    for (usize i = Length; i; i--)
    {
      ((u8*)Dest)[i - 1] = ((u8*)Src)[i - 1];
    };
  } else 
  {
    for (usize i = 0; i < Length; i++)
    {
      ((u8*)Dest)[i] = ((u8*)Src)[i];
    };
  };
  return Dest;
};

static inline char 
CliCharToLower(char c)
{
  if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
  else return c;
};

static int 
CliDigitValue(char c)
{
  if ('0' <= c && c <= '9') return c - '0';
  if ('a' <= c && c <= 'f') return c - 'a' + 10;
  if ('A' <= c && c <= 'F') return c - 'A' + 10;
  return -1;
};

static inline u32
CliCharIsSpace(u32 Char)
{
  return (
    Char == ' '  ||
    Char == '\t' ||
    Char == '\n' ||
    Char == '\r' ||
    Char == '\v' ||
    Char == '\f'
  );
};


static usize
CliCharUtf8Advance(u8 Start)
{
  if ((Start & 0x80) == 0x00) return 1;
  if ((Start & 0xE0) == 0xC0) return 2;
  if ((Start & 0xF0) == 0xE0) return 3;
  if ((Start & 0xF8) == 0xF0) return 4;
  return 0;
};

static u32
CliCharUtf8Decode(const u8* Parts)
{
  u8 Head = Parts[0];
  u32 Out = 0;
  if (Head < 0x80)
  {
    Out = Head;
  } else if ((Head >> 5) == 0x6)
  {
    Out = ((u32)(Head & 0x1F) << 6) | ((u32)(Parts[1] & 0x3F));
  } else if ((Head >> 4) == 0xE)
  {
    Out = (
      ((u32)(Head & 0x0F) << 12) |
      ((u32)(Parts[1] & 0x3F) << 6) |
      ((u32)(Parts[2] & 0x3F))
    );
  }
  else
  {
    Out = (
      ((u32)(Head & 0x07) << 18) |
      ((u32)(Parts[1] & 0x3F) << 12) |
      ((u32)(Parts[2] & 0x3F) << 6) |
      ((u32)(Parts[3] & 0x3F))
    );
  };
  return Out;
};

static usize
CliCharUtf8Encode(u32 Char, u8* Out)
{
  usize Length  = 0;
  
  if (Char <= 0x7F)
  {
    Out[0] = (u8)Char;
    Length = 1;
  } else if (Char <= 0x7FF)
  {
    Out[0] = 0xC0 | (Char >> 6);
    Out[1] = 0x80 | (Char & 0x3F);
    Length = 2;
  } else if (Char <= 0xFFFF)
  {
    Out[0] = 0xE0 | (Char >> 12);
    Out[1] = 0x80 | ((Char >> 6) & 0x3F);
    Out[2] = 0x80 | (Char & 0x3F);
    Length = 3;
  } else
  {
    Out[0] = 0xF0 | (Char >> 18);
    Out[1] = 0x80 | ((Char >> 12) & 0x3F);
    Out[2] = 0x80 | ((Char >> 6) & 0x3F);
    Out[3] = 0x80 | (Char & 0x3F);
    Length = 4;
  };
  return Length;
};

static usize
CliPeekNewLine(const u8* Value, usize Length)
{
  usize Span = 0;
  
  switch (Value[0])
  {
    case '\r':
    {
      if (1 < Length && Value[1] == '\n') Span = 2;
      else Span = 1;
    } break;
    case '\n':
    case '\v': 
    case '\f': Span = 1; break;
  };
  return Span;
};
