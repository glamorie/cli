#include "cli.h"
#include <math.h>
#include <assert.h>

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

// Internal allocator

typedef struct cli_arena cli_arena;
struct cli_arena
{
  cli_arena* Prev;
  cli_arena* Current;
  usize Offset;
  usize Size;
  usize Position;
  u8* Base;
};

#define _CliArenaHeader (0x80)

cli_arena*
CliArenaMake(usize Reserve)
{
  Reserve = CliAlignUp(Reserve, 1<<10);
  
  cli_arena* Arena = CliMalloc(_CliArenaHeader + Reserve);
  
  if (Arena)
  {
    Arena->Prev = 0;
    Arena->Current = Arena;
    Arena->Offset = 0;
    Arena->Size = Reserve;
    Arena->Position = _CliArenaHeader;
    Arena->Base = (void*)(Arena);
  };
  return Arena;
};

void
CliArenaTake(cli_arena* Arena)
{
  Arena = Arena? Arena->Current : 0;
  
  while (Arena)
  {
    cli_arena* Prev = Arena->Prev;
    CliFree(Arena);
    Arena = Prev;
  };
};

void*
CliArenaPush(cli_arena* Arena, usize Size)
{
  const usize Align = sizeof(void*);
  
  if (!Arena || !Size) return 0;
  Start:
  cli_arena* Current = Arena->Current;
  usize Padding = CliAlignPadding((usize)(Current->Base + Current->Position), Align);
  
  if (Current->Size < Current->Position + Padding + Size)
  {
    cli_arena* Node = CliArenaMake(CliMax(_CliArenaHeader + Align + Size, Current->Size));
    
    if (!Node) return 0;
    
    Node->Prev = Current;
    Node->Offset = Current->Offset + Current->Size;
    Arena->Current = Node;
    goto Start;
  };
  
  Current->Position += Padding;
  void* Allocation = Current->Base + Current->Position;
  Current->Position += Size;
  return Allocation;
};

void*
CliArenaPushN(cli_arena* Arena, usize Size, usize Count)
{
  return CliArenaPush(Arena, Size * Count);
};

void*
CliArenaZPush(cli_arena* Arena, usize Size)
{
  return CliMemoryZero(CliArenaPush(Arena, Size), Size);
};

void*
CliArenaZPushN(cli_arena* Arena, usize Size, usize Count)
{
  return CliArenaZPush(Arena, Size * Count);
};

usize
CliArenaPosition(cli_arena* Arena)
{
  return Arena? Arena->Offset + Arena->Position : 0;
};

void
CliArenaPopTo(cli_arena* Arena, usize Position)
{
  if (!Arena) return;
  
  Position = CliMax(Position, _CliArenaHeader);
  cli_arena* Current = Arena->Current;
  
  while (Position < Current->Offset)
  {
    cli_arena* Prev = Current->Prev;
    CliFree(Current);
    Current = Prev;
  };
  
  Current->Position = Position;
  Arena->Current = Current;
};

void
CliArenaPop(cli_arena* Arena, usize Size)
{
  usize CurrentPosition = CliArenaPosition(Arena);
  
  if (Size < CurrentPosition)
  {
    CliArenaPopTo(Arena, CurrentPosition - Size);
  };
};

// Internal sting

typedef struct cli_str cli_str;
struct cli_str
{
  u8* Value;
  usize Length;
};

#define CliStrLit(s) ((cli_str){(u8*)(s), sizeof(s) - 1})

cli_str
CliStrC(const char* Value, cli_arena* Arena)
{
  cli_str Out = {0};
  usize Length = CliStrLen(Value);
  Out.Value = CliArenaPush(Arena, Length + 1);
  if (Out.Value)
  {
    Out.Length = Length;
    CliMemoryCopy(Out.Value, Value, Length + 1);
    Out.Value[Length] = 0;
  };
  return Out;
};

cli_str
CliStrK(const char* Value)
{
  cli_str Out = {0};
  Out.Value = (u8*)Value;
  Out.Length = CliStrLen(Value);
  return Out;
};

u32
CliStrEqual(cli_str A, cli_str B)
{
  if (A.Length != B.Length) return 0;
  
  for (usize i = 0; i < A.Length; i++)
  {
    if (A.Value[i] != B.Value[i]) return 0;
  };
  return 1;
};

static usize
CliStringCompareCaseInsensitive(cli_str s, cli_str Prefix)
{
  usize Count = CliMin(Prefix.Length, s.Length);
  
  for (int i = 0; i < Count; i++) 
  {
    char c = CliCharToLower(s.Value[i]);    
    if (c != Prefix.Value[i]) return i;
  };
  return Count;
};

// Types

enum
{ // Basic types
  CliValueString,
  CliValueInt,
  CliValueFloat,
};

typedef struct cli_value cli_value;
struct cli_value
{
  usize* Length;
  union
  {
    i64* Number;
    double* Float;
    const char** String;
    i64** LNumber;
    double** LFloat;
    const char*** LString;
  };
};

typedef struct cli_arg cli_arg;
struct cli_arg
{
  cli_arg* Prev;
  cli_arg* Next;
  cli_str Name;
  cli_str Desc;
  cli_value Value;
  usize Count;
  u16 Kind;
  u8 Set;
  u8 Required;
};

typedef struct cli_opt cli_opt;
struct cli_opt
{
  cli_opt* Prev;
  cli_opt* Next;
  cli_str Name;
  cli_str Desc;
  u32* Value;
};

typedef struct cli_cmd cli_cmd;
struct cli_cmd
{
  cli_cmd* Prev;
  cli_cmd* Next;
  cli_str Name;
  cli_str Desc;
  cli_arg* AHead;
  cli_arg* ATail;
  
  cli_arg* KHead;
  cli_arg* KTail;
  
  cli_opt* OHead;
  cli_opt* OTail;
  u32* Called;
};

typedef struct cli_error_cursor cli_error_cursor;
struct cli_error_cursor
{
  cli_str Parsing;
  cli_arg* RequiredArg;
  cli_arg* MissingValue;
  cli_arg* NotEnoughValues;
  cli_str UknownOption;
  cli_str UknownCommand;
  cli_arg* ArgumentDoesNotExpectValue;
  cli_str UnexpectedValue;
  usize ExpectedCount, GotCount;
  u32 Kind;
};

typedef struct cli cli;
struct cli
{
  cli_arena* Arena;
  cli_error_cursor Error;
  
  cli_str Name;
  cli_str Desc;
  
  cli_opt* OHead;
  cli_opt* OTail;
  
  cli_cmd* CHead;
  cli_cmd* CTail;
  
  cli_cmd* Default;
  cli_cmd* Current;
  cli_arg* Positional;
  
  usize Indentation;
  u32 Frames;
  const char** Argv;
  usize Count;
};

// Building the cli nodes
cli*
CliMake(const char* Name, const char* Desc)
{
  cli_arena* Arena = CliArenaMake(20<<10);
  cli* Cli = CliArenaZPush(Arena, sizeof(*Cli));

  if (Cli)
  {
    Cli->Arena = Arena;
    Cli->Name = CliStrC(Name, Arena);
    Cli->Desc = CliStrC(Desc, Arena);
  };
  return Cli;
};

void
CliTake(cli* Cli)
{
  if (Cli)
  {
    CliArenaTake(Cli->Arena);
  };
};

void
CliCommand(cli* Cli, u32* Called, const char* Name, const char* Desc)
{
  if (!Cli || !Called || !Name || Desc) return;
  cli_cmd* Node = CliArenaZPush(Cli->Arena, sizeof(*Node));
  if (!Node) return;

  Node->Name = CliStrC(Name, Cli->Arena);
  Node->Desc = CliStrC(Desc, Cli->Arena);
  Node->Called = Called;
  CliDLLPush(Cli, Node, CHead, CTail);
};

void
CliMain(cli* Cli, u32* Called, const char* Name, const char* Desc)
{
  if (!Cli || !Called) return;
  cli_cmd* Node = CliArenaZPush(Cli->Arena, sizeof(*Node));
  if (!Node) return;
  Node->Name = CliStrC(Name, Cli->Arena);
  Node->Desc = CliStrC(Desc, Cli->Arena);
  Node->Called = Called;

  if (Name && *Name)
  {
    CliDLLPush(Cli, Node, CHead, CTail);
  };

  Cli->Default = Node;
};

void
CliOption(cli* Cli, u32* Value, const char* Name, const char* Desc)
{
  if (!Cli || !Value || !Name || !Desc) return;

  cli_opt* Node = CliArenaZPush(Cli->Arena, sizeof(*Node));
  if (!Node) return;
  Node->Name = CliStrC(Name, Cli->Arena);
  Node->Desc = CliStrC(Desc, Cli->Arena);
  Node->Value = Value;

  if (Cli->CTail)
  {
    CliDLLPush(Cli->CTail, Node, OHead, OTail);
  } else 
  {
    CliDLLPush(Cli, Node, OHead, OTail);
  };
};

static void
CliPushArg(cli* Cli, const char* Name, const char* Desc, cli_value Value, u16 Kind, usize Count, u8 Required)
{
  if (!Cli || !Cli->CTail && !Cli->Default || !Name || !Desc) return;
  cli_cmd* Parent = Cli->CTail ? Cli->CTail : Cli->Default;

  cli_arg* Node = CliArenaZPush(Cli->Arena, sizeof(*Node));
  if (!Node) return;

  int IsPositional = Name[0] == '*';
  Node->Name = CliStrC(IsPositional ? Name + 1 : Name, Cli->Arena);
  Node->Desc = CliStrC(Desc, Cli->Arena);
  Node->Value = Value;
  Node->Required = Required;
  Node->Value = Value;
  Node->Count = Count;
  Node->Kind = Kind;

  if (IsPositional)
  {
    CliDLLPush(Parent, Node, AHead, ATail);
  } else 
  {
    CliDLLPush(Parent, Node, KHead, KTail);
  };
};

void
CliInt(cli* Cli, i64* Value, const char* Name, const char* Desc)
{
  cli_value In = {0};
  In.Number = Value;
  CliPushArg(Cli, Name, Desc, In, CliValueInt, 1, 1);
};

void
CliFloat(cli* Cli, double* Value, const char* Name, const char* Desc)
{
  cli_value In = {0};
  In.Float = Value;
  CliPushArg(Cli, Name, Desc, In, CliValueFloat, 1, 1);
};

void
CliStr(cli* Cli, const char** Value, const char* Name, const char* Desc)
{
  cli_value In = {0};
  In.String = Value;
  CliPushArg(Cli, Name, Desc, In, CliValueString, 1, 1);
};

void
CliIntOr(cli* Cli, i64* Value, i64 Default, const char* Name, const char* Desc)
{
  if (!Value) return;
  cli_value In = {0};
  In.Number = Value;
  *Value = Default;
  CliPushArg(Cli, Name, Desc, In, CliValueInt, 1, 0);
};

void
CliFloatOr(cli* Cli, double* Value, double Default, const char* Name, const char* Desc)
{
  if (!Value) return;
  cli_value In = {0};
  In.Float = Value;
  *Value = Default;
  CliPushArg(Cli, Name, Desc, In, CliValueFloat, 1, 0);
};

void
CliStrOr(cli* Cli, const char** Value, const char* Default, const char* Name, const char* Desc)
{
  if (!Value) return;
  cli_value In = {0};
  In.String = Value;
  *Value = Default;
  CliPushArg(Cli, Name, Desc, In, CliValueString, 1, 0);
};

void
CliIntN(cli* Cli, i64** Value, usize* Length, usize Count, const char* Name, const char* Desc)
{
  if (!Value || !Length || !Value) return;
  u8 Required = *Name != '?';
  cli_value In = {0};
  In.Length = Length;
  In.LNumber = Value;
  if (!Required) *Value = 0, *Length = 0;
  CliPushArg(Cli, Name, Desc, In, CliValueInt, Count, Required);
};

void
CliFloatN(cli* Cli, double** Value, usize* Length, usize Count, const char* Name, const char* Desc)
{
  if (!Value || !Length || !Value) return;
  u8 Required = *Name != '?';
  cli_value In = {0};
  In.Length = Length;
  In.LFloat = Value;
  if (!Required) *Value = 0, *Length = 0;
  CliPushArg(Cli, Name, Desc, In, CliValueFloat, Count, Required);
};

void
CliStrN(cli* Cli, const char*** Value, usize* Length, usize Count, const char* Name, const char* Desc)
{
  if (!Value || !Length || !Value) return;
  u8 Required = *Name != '?';
  cli_value In = {0};
  In.Length = Length;
  In.LString = Value;
  if (!Required) *Value = 0, *Length = 0;
  CliPushArg(Cli, Name, Desc, In, CliValueString, Count, Required);
};
