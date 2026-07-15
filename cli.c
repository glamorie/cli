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
    Out[0] = (u8)(0xC0 | (Char >> 6));
    Out[1] = (u8)(0x80 | (Char & 0x3F));
    Length = 2;
  } else if (Char <= 0xFFFF)
  {
    Out[0] = (u8)(0xE0 | (Char >> 12));
    Out[1] = (u8)(0x80 | ((Char >> 6) & 0x3F));
    Out[2] = (u8)(0x80 | (Char & 0x3F));
    Length = 3;
  } else
  {
    Out[0] = (u8)(0xF0 | (Char >> 18));
    Out[1] = (u8)(0x80 | ((Char >> 12) & 0x3F));
    Out[2] = (u8)(0x80 | ((Char >> 6) & 0x3F));
    Out[3] = (u8)(0x80 | (Char & 0x3F));
    Length = 4;
  };
  return Length;
};

static usize
CliCharUtf16Encode(u32 Char, u16 Parts[2])
{
  if (Char <= 0xFFFF) 
  {
    Parts[0] = (u16)Char;
    return 1;
  } else if (Char <= 0x10FFFF) 
  {
    Char -= 0x10000;
    Parts[0] = 0xD800 | ((Char >> 10) & 0x3FF); // high surrogate
    Parts[1] = 0xDC00 | (Char & 0x3FF);         // low surrogate
    return 2;
  };
  return 0;
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
  } Value;
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
  u32 Positional;
  u32 IsAlias;
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

static cli_str
CliExpandName(cli_str Name, u8* Alias);

void
CliCommand(cli* Cli, u32* Called, const char* Name, const char* Desc)
{
  if (!Cli || !Called || !Name || !Desc) return;
  cli_cmd* Node = CliArenaZPush(Cli->Arena, sizeof(*Node));
  if (!Node) return;
  
  *Called = 0;
  Node->Name = CliStrC(Name, Cli->Arena);
  Node->Desc = CliStrC(Desc, Cli->Arena);
  Node->Called = Called;
  u8 Short = 0;
  cli_str Long = CliExpandName(Node->Name, &Short);
  usize Width = 4 + Long.Length + 4;
  Cli->Indentation = CliMax(Cli->Indentation, Width);
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
    u8 Short = 0;
    cli_str Long = CliExpandName(Node->Name, &Short);
    usize Width = 4 + Long.Length + 4;    
    Cli->Indentation = CliMax(Cli->Indentation, Width);
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
  *Value = 0;
  
  u8 Short = 0;
  cli_str Long = CliExpandName(Node->Name, &Short);
  Cli->Indentation = CliMax(Cli->Indentation, Long.Length + 9);

  if (Cli->CTail || Cli->Default)
  {
    cli_cmd* Cmd = Cli->CTail ? Cli->CTail : Cli->Default;
    CliDLLPush(Cmd, Node, OHead, OTail);
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
    Cli->Indentation = CliMax(Cli->Indentation, Node->Name.Length + 4); // [2][name][2]
    CliDLLPush(Parent, Node, AHead, ATail);
  } else 
  {
    u8 Short = 0;
    cli_str Long = CliExpandName(Node->Name, &Short);
    Cli->Indentation = CliMax(Cli->Indentation, Long.Length + 9);
    CliDLLPush(Parent, Node, KHead, KTail);
  };
};

void
CliInt(cli* Cli, i64* Value, const char* Name, const char* Desc)
{
  cli_value In = {0};
  In.Value.Number = Value;
  CliPushArg(Cli, Name, Desc, In, CliValueInt, 1, 1);
};

void
CliFloat(cli* Cli, double* Value, const char* Name, const char* Desc)
{
  cli_value In = {0};
  In.Value.Float = Value;
  CliPushArg(Cli, Name, Desc, In, CliValueFloat, 1, 1);
};

void
CliStr(cli* Cli, const char** Value, const char* Name, const char* Desc)
{
  cli_value In = {0};
  In.Value.String = Value;
  CliPushArg(Cli, Name, Desc, In, CliValueString, 1, 1);
};

void
CliIntOr(cli* Cli, i64* Value, i64 Default, const char* Name, const char* Desc)
{
  if (!Value) return;
  cli_value In = {0};
  In.Value.Number = Value;
  *Value = Default;
  CliPushArg(Cli, Name, Desc, In, CliValueInt, 1, 0);
};

void
CliFloatOr(cli* Cli, double* Value, double Default, const char* Name, const char* Desc)
{
  if (!Value) return;
  cli_value In = {0};
  In.Value.Float = Value;
  *Value = Default;
  CliPushArg(Cli, Name, Desc, In, CliValueFloat, 1, 0);
};

void
CliStrOr(cli* Cli, const char** Value, const char* Default, const char* Name, const char* Desc)
{
  if (!Value) return;
  cli_value In = {0};
  In.Value.String = Value;
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
  In.Value.LNumber = Value;
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
  In.Value.LFloat = Value;
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
  In.Value.LString = Value;
  if (!Required) *Value = 0, *Length = 0;
  CliPushArg(Cli, Name, Desc, In, CliValueString, Count, Required);
};

// Parsing integers and floats
// Ported from https://github.com/odin-lang/Odin/blob/master/core/strconv/strconv.odin

typedef struct cli_flit cli_flit;
struct cli_flit
{
  union 
  {
    u64 Bits;
    double Value;
  } x;
};

#define CLI_FLT_LITERAL(v) (((cli_flit){.x.Bits = (v)}).x.Value)
#define CLI_INFINITY CLI_FLT_LITERAL(0x7ff0000000000000ULL)
#define CLI_NAN CLI_FLT_LITERAL(0x7ff8000000000000ULL)

typedef struct cli_double_parse cli_double_parse;
struct cli_double_parse
{
  double Value;
  int End;
  u32 Ok;
};

typedef struct cli_double_parse_component cli_double_parse_component;
struct cli_double_parse_component
{
  u64 Mantissa;
  int Exponent;
  int Truncate;
  int Hex;
  int i;
  u32 Negative;
  u32 Ok;
};

static cli_double_parse 
CliCheckDoubleLiteral(cli_str s)
{
  cli_double_parse Result = {0};
  
  if (s.Length == 0) return Result;
  
  int sign = 1;
  int nsign = 0;
  
  char c0 = s.Value[0];
  
  if (c0 == '+' || c0 == '-') 
  {
    if (c0 == '-') sign = -1;
    
    nsign = 1;
    s.Value += 1;
    s.Length -= 1;
  };
  
  if (s.Length == 0) return Result;
  
  switch (s.Value[0]) 
  {
    case 'i':
    case 'I':
    {
      usize m = CliStringCompareCaseInsensitive(s, CliStrLit("infinity"));
      
      if (m >= 3 && m < 9) 
      {
        Result.Value = sign * CLI_INFINITY;
        
        if (m == 8) Result.End = (int)(nsign + m);
        else Result.End = nsign + 3;
        
        Result.Ok = 1;
        return Result;
      }
    } break;
    
    case 'n':
    case 'N':
    {
      if (CliStringCompareCaseInsensitive(s, CliStrLit("nan")) == 3) 
      {
        Result.Value = CLI_NAN;
        Result.End = nsign + 3;
        Result.Ok = 1;
        return Result;
      };
    } break;
  };
  
  return Result;
};

static cli_double_parse_component 
CliDoubleParseComponentCliStrLit(cli_str s)
{
  cli_double_parse_component Result = {0};
  
  if (s.Length == 0) return Result;
  
  int i = 0;
  
  if (s.Value[i] == '+') 
  {
    i++;
  } else if (s.Value[i] == '-') 
  {
    Result.Negative = 1;
    i++;
  }
  
  u64 Base = 10;
  int MAX_MANT_DIGITS = 19;
  char ExpChar = 'e';
  
  if (i + 2 < (int)s.Length && s.Value[i] == '0' && CliCharToLower(s.Value[i+1]) == 'x')
  {
    Base = 16;
    MAX_MANT_DIGITS = 16;
    i += 2;
    ExpChar = 'p';
    Result.Hex = 1;
  };
  
  int SawDot = 0;
  int SawDigits = 0;
  
  int Nd = 0;
  int NdMant = 0;
  int DecimalPoint = 0;
  
  for (; i < (int)s.Length; i++) 
  {
    char c = s.Value[i];
    
    if (c == '_') continue;
    if (c == '.') 
    {
      if (SawDot) break;
      
      SawDot = 1;
      DecimalPoint = Nd;
      continue;
    };
    
    if ('0' <= c && c <= '9') 
    {
      SawDigits = 1;
      Nd++;
      
      if (NdMant < MAX_MANT_DIGITS) 
      {
        Result.Mantissa *= Base;
        Result.Mantissa += (u64)(c - '0');
        NdMant++;
      } else if (c != '0') 
      {
        Result.Truncate = 1;
      };      
      continue;
    };
    
    if (Base == 16) 
    {
      char Lch = CliCharToLower(c);
      
      if ('a' <= Lch && Lch <= 'f') 
      {
        SawDigits = 1;
        Nd++;
        
        if (NdMant < MAX_MANT_DIGITS) 
        {
          Result.Mantissa *= 16;
          Result.Mantissa += (u64)(Lch - 'a' + 10);
          NdMant++;
        } else
        {
          Result.Truncate = 1;
        };        
        continue;
      };
    };
    
    break;
  };
  
  if (!SawDigits) return Result;
  
  if (!SawDot) DecimalPoint = Nd;
  
  if (Base == 16) 
  {
    DecimalPoint *= 4;
    NdMant *= 4;
  };
  
  if (i < (int)s.Length && CliCharToLower(s.Value[i]) == ExpChar) 
  {
    
    i++;
    
    int ExpSign = 1;
    
    if (s.Value[i] == '+') 
    {
      i++;
    } else if (s.Value[i] == '-') 
    {
      ExpSign = -1;
      i++;
    };
    
    int e = 0;
    
    for (; i < (int)s.Length; i++) 
    {
      
      char c = s.Value[i];
      
      if (c == '_') continue;
      
      if (c < '0' || c > '9') break;
      
      if (e < 100000) e = e*10 + (c - '0');
    };
    
    DecimalPoint += e * ExpSign;
  } else if (Base == 16) 
  {
    return Result;
  };
  
  if (Result.Mantissa != 0) Result.Exponent = DecimalPoint - NdMant;
  
  Result.i = i;
  Result.Ok = 1;
  
  return Result;
};

static double
CliPow10(int e)
{
  static const double Powers[] = 
  {
    1e1, 1e2, 1e4, 1e8, 1e16, 1e32, 1e64, 1e128, 1e256
  };
  double Result = 1.0;
  
  if (e < 0)
  {
    e = -e;
    for (int i = 0; e; i++, e >>= 1)
    {
      if (e & 1) Result *= Powers[i];
    };
    return 1.0 / Result;
  };
  for (int i = 0; e; i++, e >>= 1)
  {
    if (e & 1) Result *= Powers[i];
  };  
  return Result;
};

static cli_double_parse 
CliDoubleParse(cli_str str)
{
  cli_double_parse Result = {0};
  
  cli_double_parse ParseLiteral = CliCheckDoubleLiteral(str);
  if (ParseLiteral.Ok) return ParseLiteral;
  cli_double_parse_component ParseComponent = CliDoubleParseComponentCliStrLit(str);
  
  if (!ParseComponent.Ok) return Result;
  
  Result.End = ParseComponent.i;
  
  double f = (double)ParseComponent.Mantissa;
  
  if (ParseComponent.Negative) f = -f;
  
  if (ParseComponent.Exponent != 0) f *= CliPow10(ParseComponent.Exponent);
  
  Result.Value = f;
  Result.Ok = 1;
  
  return Result;
};

double
CLiStrToD(cli_str Str, usize* End)
{
  cli_double_parse Result = CliDoubleParse(Str);
  if (End) *End = Result.End;
  return Result.Value;
};

// Parse integers

typedef struct cli_int_parse cli_int_parse;
struct cli_int_parse
{
  i64 Value;
  int End;
  u32 Ok;
};

static cli_int_parse
CliIntParse(cli_str Value)
{
  cli_int_parse Result = {0};
  int i = 0;
  int Negative = 0;
  
  
  switch (i < Value.Length ? Value.Value[i] : 0)
  {
    case '+': i++; break;
    case '-': i++; Negative = 1; break;
  };
  
  
  int Base = 10;
  u32 SawDigits = 0;
  
  if (i < Value.Length && CliCharToLower(Value.Value[i]) == '0')
  {
    
    if (i + 1 < Value.Length)
    {
      u8 Ch = CliCharToLower(Value.Value[i + 1]);
      
      if (Ch == 'x')
      {
        Base = 16;
        i += 2;
      } else if (Ch == 'b')
      {
        Base = 2;
        i += 2;
      } else if (Ch == 'o')
      {
        Base = 8;
        i += 2;
      };
    } else 
    {
      Base = 8; // 012...
    };
  };
  
  
  for (; i < Value.Length; i++)
  {
    u8 Ch = Value.Value[i];
    
    if (Ch == '_') continue;
    
    int D = CliDigitValue(Ch);
    if (D < 0 || D >= Base) break;
    
    SawDigits  = 1;
    
    Result.Value = Result.Value * Base + D;
  };
  
  Result.Ok = i == Value.Length && SawDigits;
  Result.End = i;
  Result.Value = Negative ? -Result.Value : Result.Value;
  return Result;
};

static i64
CliStringToInt(cli_str Value, u8** End)
{
  cli_int_parse Parse = CliIntParse(Value);
  
  if (End) *End = Parse.Ok ? 0 : Value.Value + Parse.End;
  return Parse.Value;
};

// Searching nodes
static cli_str
CliExpandName(cli_str Name, u8* Alias) // Extract the alias and the flag of the name
{
  u8 A = 0;
  cli_str Out = {0};

  if (Name.Value[0] == '!')
  {
    Name.Value = Name.Value + 1;
    Name.Length = Name.Length - 1;
  };
  
  if (Name.Value[0] == ',')
  {
    Out.Value = Name.Value + 1;
    Out.Length = Name.Length - 1;
    A = Name.Value[1];
  } else if (Name.Value[1] == ',')
  {
    A = Name.Value[0];
    Out.Value = Name.Value + 2;
    Out.Length = Name.Length - 2;
  } else 
  {
    Out.Value = Name.Value;
    Out.Length = Name.Length;
  };
  if (Alias) *Alias = A;
  return Out;
};

#define CliSearchNodes(Node, Flag) \
{ \
  u8 Alias = Flag.Length == 1 ? Flag.Value[0] : 0; \
  for (; (Node); (Node) = (Node)->Next) \
  { \
    u8 NAlias = 0; \
    cli_str NFlag = CliExpandName((Node)->Name, &NAlias); \
    if (Flag.Length == 1 ? NAlias == Alias : CliStrEqual(NFlag, Flag)) return (Node); \
  }; \
}

static cli_cmd*
CliCmdSearch(cli_cmd* Head, cli_str Flag) 
{
  CliSearchNodes(Head, Flag);
  return 0;
};

static cli_opt* 
CliOptSearch(cli_opt* Head1, cli_opt* Head2, cli_str Flag)
{
  CliSearchNodes(Head1, Flag);
  CliSearchNodes(Head2, Flag);
  return 0;
};

static cli_arg*
CliArgSearch(cli_arg* Head, cli_str Flag)
{
  CliSearchNodes(Head, Flag);
  return 0;
};

enum
{
  CliErrorNone,
  CliErrorParsing,
  CliErrorMissingValue,
  CliErrorNotEnoughValues,
  CliErrorUnknownOption,
  CliErrorExpectedCommandName,
  CliErrorUnexpectedValue,
  CliErrorArgumentDoesNotExpectValue,
  CliErrorUnknownCommand,
  CliErrorRequiredArgument,
  
};

#if 0
const char* _CliErrorName[] =
{

[CliErrorNone] = "CliErrorNone",
[CliErrorParsing] = "CliErrorParsing",
[CliErrorMissingValue] = "CliErrorMissingValue",
[CliErrorNotEnoughValues] = "CliErrorNotEnoughValues",
[CliErrorUnknownOption] = "CliErrorUnknownOption",
[CliErrorExpectedCommandName] = "CliErrorExpectedCommandName",
[CliErrorUnexpectedValue] = "CliErrorUnexpectedValue",
[CliErrorArgumentDoesNotExpectValue] = "CliErrorArgumentDoesNotExpectValue",
[CliErrorUnknownCommand] = "CliErrorUnknownCommand",
[CliErrorRequiredArgument] = "CliErrorRequiredArgument",
};
#endif

static u32 // Parses and set the value to the pointer
CliParseType(cli_str Source, u16 Type, void* Out, cli_error_cursor* ErrorP)
{
  u32 Ok = 0;
  if (Type == CliValueFloat)
  {
    cli_double_parse Result = CliDoubleParse(Source);
    if (Result.Ok) *((double*)Out) = Result.Value;
    Ok = Result.Ok;
  } else if (Type == CliValueInt)
  {
    cli_int_parse Result = CliIntParse(Source);
    if (Result.Ok) *((i64*)Out) = Result.Value;
    Ok = Result.Ok;
  } else if (Type == CliValueString)
  {
    *((u8**)Out) = Source.Value;
    Ok = 1;
  };
  u32 Error = Ok ? 0 : CliErrorParsing;
  ErrorP->Kind = Error;
  ErrorP->Parsing = Source;
  return Error;
};

static u32 // Parse and store in the value structure
CliParseValue(cli_str Source, u16 Type, cli_value* Out, cli_error_cursor* ErrorP)
{
  u32 Ok = 0;
  if (Type == CliValueFloat)
  {
    cli_double_parse Result = CliDoubleParse(Source);
    if (Result.Ok) *Out->Value.Float = Result.Value;
    Ok = Result.Ok;
  } else if (Type == CliValueInt)
  {
    cli_int_parse Result = CliIntParse(Source);
    if (Result.Ok) *Out->Value.Number = Result.Value;
    Ok = Result.Ok;
  } else if (Type == CliValueString)
  {
    *Out->Value.String = (const char*)Source.Value;
    Ok = 1;
  };
  u32 Error = Ok ? 0 : CliErrorParsing;
  ErrorP->Kind = Error;
  ErrorP->Parsing = Source;
  return Error;
};

// Tokenizing the arguments
enum
{
  CliTokenEof,
  CliTokenFlag, // --<flag> 
  CliTokenAlias, // -<alias>
  CliTokenFlagValue, // --<flag>=<value> or --<flag>:<value>
  CliTokenAliasValue, // -<alias><value>
  CliTokenValue, // Anything not prefixed with a hyphen
  CliTokenEscape, // '--' Escapes anything with a hyphen so it's treated like a value.
};

typedef struct cli_lexer cli_lexer;
struct cli_lexer
{
  const char** Values;
  usize Count;
  usize Index;
  cli_str Partial;
};

static void // Extracts the value and the flag from a --flag:value or --flag=value.
CliLexerUnpackFlag(const char* Value, cli_str* Flag, cli_str* Partial) 
{
  Value += 2;
  usize i = 0;
  usize k = 0;
  u32 FoundMid = 0;
  
  while (Value[i])
  {
    u8 Ch = Value[i];
    if (Ch == '=' || Ch == ':')
    {
      FoundMid = 1;
      k = i;
    };
    i++;
  };
  
  cli_str Long = {0}, Part = {0};
  
  if (FoundMid && k < i)
  {
    Long.Value = (u8*)(Value);
    Long.Length = k;
    Part.Value = Long.Value + (k + 1);
    Part.Length = i - k - 1;
  } else 
  {
    Long.Value = (u8*)(Value);
    Long.Length = i;
  };
  *Flag = Long;
  *Partial = Part;
};

static u32
CliLexerNext(cli_lexer* Args, u32* Token, cli_str* Out, u32 Escape)
{
  *Token = CliTokenValue;
  
  if (Args->Partial.Value)
  {
    *Out = Args->Partial;
    Args->Partial.Value = 0;
    Args->Partial.Length = 0;
    return 1;
  };
  
  if (Args->Count <= Args->Index)
  {
    *Token = CliTokenEof;
    return 0;
  };
  const char* Value = Args->Values[Args->Index++];
  
  if (Escape || Value[0] != '-') // Any value literal
  {
    *Out = CliStrK(Value);
  } else if (Value[1] != '-') // -abcdefg : alias
  {  
    usize Length = CliStrLen(Value + 1);
    *Token = Length == 0 ? CliTokenValue : Length == 1 ? CliTokenAlias : CliTokenAliasValue;
    cli_str F = {(u8*)(Value + 1), Length};
    *Out = F;
  } else if (Value[2] == 0) // '--' Escaping 
  {
    *Token = CliTokenEscape;
  } else // '--flag' / '--flag:value'
  {
    CliLexerUnpackFlag(Value, Out, &Args->Partial);
    *Token = Args->Partial.Length ? CliTokenFlagValue : CliTokenFlag;
  };
  return 1;
};

static u32 // Skip the escape token
CliLexerNextEscaped(cli_lexer* Args, u32* Token, cli_str* Out)
{
  u32 T = 0;
  u32 Escape = 0;
  cli_str Temp = {0};
  Start:
  u32 Ok = CliLexerNext(Args, &T, &Temp, Escape);
  if (!Escape && T == CliTokenEscape)
  {
    Escape = 1;
    goto Start;
  };
  
  *Out = Temp;
  *Token = T;
  return Ok;
};

static u32 // Treat everything from the current position onwards as values.
CliLexerNextAll(cli_lexer* Args, u32* Token, cli_str* Out)
{
  (void)Token;
  u32 T = 0;
  return CliLexerNext(Args, &T, Out, 1);
};

static void
CliLexerRollback(cli_lexer* Args)
{
  Args->Partial.Value = 0;
  Args->Partial.Length = 0;
  
  if (1 < Args->Index) Args->Index--;
};

static void
CliLexerRollbackTo(cli_lexer* Args, usize Index)
{
  Args->Partial.Value = 0;
  Args->Partial.Length = 0;
  if (1 < Index && Index < Args->Count) Args->Index = Index;
};

// If an argument takes in more than one argument and the escape token 
// occurs first, everything else is considered a value
// e.g. --expression -- 12 - -3 as opposed to --expression 12 -- - -- -3
static u32 
CliLexerShouldSkipAll(cli_lexer* Args)
{
  cli_str Temp;
  u32 Token = 0;
  if (CliLexerNext(Args, &Token, &Temp, 0))
  {
    if (Token == CliTokenEscape) return 1;
    CliLexerRollback(Args);
  };
  return 0;
};

typedef u32
cli_lexer_next(cli_lexer* Args, u32* Token, cli_str* Out);

static cli_lexer_next*
CliLexerNextFunction(cli_lexer* Args)
{
  return CliLexerShouldSkipAll(Args) ? CliLexerNextAll : CliLexerNextEscaped;
};

static usize
CliSizeof(u16 Kind)
{
  if (Kind == CliValueInt) return sizeof(i64);
  else if (Kind == CliValueFloat) return sizeof(double);
  else return sizeof(const char*);
};

static usize // Count the values upto the next flag
CliLexerPeekLength(cli_lexer* Args)
{
  usize Count = 0;
  u32 Token = 0;
  cli_str Value = {0};
  
  usize Index = Args->Index;
  
  cli_lexer_next* NextF = CliLexerNextFunction(Args);
  
  while (NextF(Args, &Token, &Value) && Token == CliTokenValue)
  {
    Count++;
  };
  Args->Index = Index;
  return Count;
};

static u32 // Read a single value
CliLexerRead1(cli_lexer* Args, u16 Kind, cli_value* Out, cli_error_cursor* ErrorP)
{
  cli_str Source = {0};
  u32 Token = 0;
  u32 Error = 0;
  
  CliLexerNextEscaped(Args, &Token, &Source);
  
  if (Token == CliTokenValue) Error = CliParseValue(Source, Kind, Out, ErrorP);
  else Error = CliErrorMissingValue;
  
  ErrorP->Kind = Error;
  ErrorP->Parsing = Source;
  return Error;  
};

static u32  // Read an array of values
CliLexerRead(cli_lexer* Args, u16 Kind, void* Array, usize Count, cli_error_cursor* ErrorP)
{
  cli_str Value = {0};
  u32 Token = 0;  
  usize i = 0;
  
  cli_lexer_next* NextF = CliLexerNextFunction(Args);
  u32 Error = 0;
  
  while (i < Count && !Error)
  {
    if (!NextF(Args, &Token, &Value) || Token != CliTokenValue)
    {
      Error = CliErrorNotEnoughValues;
    } else if (Kind == CliValueFloat)
    {
      double* A = Array;
      Error = CliParseType(Value, Kind, A + (i++), ErrorP);
      
    } else if (Kind == CliValueInt)
    {
      i64* A = Array;
      Error = CliParseType(Value, Kind, A + (i++), ErrorP);
    } else 
    {
      const char** A = Array;
      A[i++] = (const char*)Value.Value;
    };
  };
  
  ErrorP->Kind = Error;
  ErrorP->ExpectedCount = Count;
  ErrorP->GotCount = i;
  return Error;
};

static u32 // Read a fixed number of values
CliLexerReadN(cli_lexer* Args, u16 Kind, usize Count, cli_value* Value, cli_error_cursor* ErrorP)
{
  CliFree(Value->Value.LFloat);
  
  void* Array = CliMalloc(CliSizeof(Kind) * Count);
  assert(Array);
  
  u32 Error = CliLexerRead(Args, Kind, Array, Count, ErrorP);
  
  if (Error)
  {
    CliFree(Array);
    Array = 0;
    Count = 0;
  };
  
  *Value->Value.LFloat = Array;
  *Value->Length = Count;
  return Error;
};

static u32 // Read all until a flag is encountered.
CliLexerReadX(cli_lexer* Args, u16 Kind, cli_value* Value, cli_error_cursor* ErrorP)
{
  CliFree(*Value->Value.LFloat);
  usize Count = CliLexerPeekLength(Args);
  
  if (!Count)
  {
    return CliErrorNotEnoughValues;
  };
  
  void* Array = CliMalloc(Count * CliSizeof(Kind));
  assert(Array);
  
  u32 Error = CliLexerRead(Args, Kind, Array, Count, ErrorP);
  
  if (Error) 
  {
    CliFree(Array);
    Array = 0;
    Count = 0;
  };
  
  *Value->Value.LFloat = Array;
  *Value->Length = Count;
  
  return Error;
};

// Aliases can be batched together in one argument. They can also store 
// a cli_value. To resolve this, we check all the letters in the source and if
// it matches an alias for an argument, everything else that follows is assumed to 
// be the cli_value for that argument. If an argument is not found, we try and find
// an option with that alias and set it. 
// e.g. '-VsnJames' is the same as '-V' '-s' '-n' 'James' i.e. '--verbose' '--styled' '--name' 'James'
static u32 
CliResolveBatchedAlias(cli_str Source, cli_opt* Options1, cli_opt* Options2, cli_arg* Args, cli_error_cursor* ErrorP, u32* Stop)
{
  u32 Error = 0;
  
  for (usize i = 0; i < Source.Length; i++)
  {
    cli_str Alias = {Source.Value + i, 1};
    cli_opt* Option = 0;
    cli_arg* Arg = 0;
    
    if ((Arg = CliArgSearch(Args, Alias)))
    {
      if (Arg->Count == 1)
      {
        cli_str Slice = {Source.Value + i, Source.Length - i};
        Error = CliParseValue(Slice, Arg->Kind, &Arg->Value, ErrorP); 
        if (!Error) Arg->Set = 1;       
      } else 
      {
        ErrorP->NotEnoughValues = Arg;
        Error = CliErrorNotEnoughValues;
      };
      break;
    } else if ((Option = CliOptSearch(Options1, Options2, Alias)))
    {
      *Option->Value = 1;
      if (Option->Name.Value[0] == '!')
      {
        *Stop = 1;
        break;
      };
    } else 
    {
      Error = CliErrorUnknownOption;
      ErrorP->UknownOption = Source;
      ErrorP->IsAlias = 1;
      break;
    };
  };
  return Error;
};

static cli_error_cursor
CliLexerParse(cli* Cli, const char** Argv, usize Argc)
{
  cli_lexer Args = 
  {
    .Values = Argv,
    .Count = Argc,
    .Index = 1,
  };
  
  cli_error_cursor Error = {0};
  u32 Token = 0;
  cli_str Value = {0};
  cli_cmd* Command = Cli->Default; // Fall back
  u32 Stop = 0;
  
  
  while (!Error.Kind && CliLexerNext(&Args, &Token, &Value, 0))
  {
    // Escaping could be used to specifiy that some name that is also a sub-command's
    // name should be treated as just a cli_value so we fallback to the default command.
    
    if (Token == CliTokenEscape || Token == CliTokenFlagValue) break; 
    
    if (Token == CliTokenValue)
    {
      cli_cmd* Found = CliCmdSearch(Cli->CHead, Value);
      Error.UknownCommand = Value;
      if (Found) Command = Found;
      else if (Command) CliLexerRollback(&Args);// Assume the name is a positional argument and fallback to the default
      else Error.Kind = CliErrorUnknownCommand;
      break; 
    } else if (Token == CliTokenAliasValue)
    {
      Error.Kind = CliResolveBatchedAlias(Value, Cli->OHead, 0, 0, &Error, &Stop);
      
      if (Command && Error.Kind ==CliErrorUnknownOption)
      {
        CliLexerRollback(&Args);
        Error.Kind = 0;
        break;
      };
      if (Stop) break;

    } else if (Token == CliTokenAlias || Token == CliTokenFlag)
    {
      cli_opt* Opt = CliOptSearch(Cli->OHead, 0, Value);
      if (Opt)
      {
        *Opt->Value = 1;
        if (Opt->Name.Value[0] == '!')
        {
          Stop = 1;
          break;
        };
      };
      
      if (Command && !Opt)
      {
        CliLexerRollback(&Args);
        Error.Kind = 0;
        break;
      } else if (!Command && !Opt)
      {
        Error.Kind = CliErrorUnknownOption;
        Error.UknownOption = Value;
        Error.IsAlias = Token == CliTokenAlias;
      };
    };
  };
  
  if (!Stop && !Error.Kind && Command)
  {
    cli_arg* Positional = Command->AHead;
    while (!Error.Kind && CliLexerNext(&Args, &Token, &Value, 0))
    {
      Error.UknownOption = Value;
      
      if (Token == CliTokenValue || Token == CliTokenEscape)
      {
        CliLexerRollback(&Args);
        
        if (Positional)
        {
          if (Positional->Count == 1) Error.Kind = CliLexerRead1(&Args, Positional->Kind, &Positional->Value, &Error);
          else if (Positional->Count) Error.Kind = CliLexerReadN(&Args, Positional->Kind, Positional->Count, &Positional->Value, &Error);
          else Error.Kind = CliLexerReadX(&Args, Positional->Kind, &Positional->Value, &Error);
          if (!Error.Kind) Positional->Set = 1;
          Positional = Positional->Next;
        } else 
        {
          Error.UnexpectedValue = Value;
          Error.Kind = CliErrorUnexpectedValue;
        };
        if (Error.Kind) Error.Positional = 1;
      } else if (Token == CliTokenFlag || Token == CliTokenAlias)
      {
        cli_opt* Opt = 0;
        cli_arg* Arg = 0;
        if ((Arg = CliArgSearch(Command->KHead, Value)))
        {
          Error.NotEnoughValues = Arg;
          
          if (Arg->Count == 1) Error.Kind = CliLexerRead1(&Args, Arg->Kind, &Arg->Value, &Error);
          else if (Arg->Count) Error.Kind = CliLexerReadN(&Args, Arg->Kind, Arg->Count, &Arg->Value, &Error);
          else Error.Kind = CliLexerReadX(&Args, Arg->Kind, &Arg->Value, &Error);
          if (!Error.Kind) Arg->Set = 1;
        } else if ((Opt = CliOptSearch(Command->OHead, Cli->OHead, Value)))
        {
          *Opt->Value = 1;
          if (Opt->Name.Value[0] == '!')
          {
            Stop = 1;
            break;
          };
        } else 
        {
          Error.Kind = CliErrorUnknownOption;
          Error.UknownOption = Value;
          Error.IsAlias = Token == CliTokenAlias;
        };
      } else if (Token == CliTokenFlagValue)
      {
        cli_arg* Arg = CliArgSearch(Command->KHead, Value);
        Error.NotEnoughValues = Arg;
        
        if (Arg)
        {
          if (Arg->Count == 1) Error.Kind = CliLexerRead1(&Args, Arg->Kind, &Arg->Value, &Error);
          else Error.Kind = CliErrorNotEnoughValues;
          if (!Error.Kind) Arg->Set = 1;
        } else 
        {
          Error.Kind = CliErrorUnknownOption;
          Error.UknownOption = Value;
          Error.IsAlias = 0;
        };

      } else if (Token == CliTokenAliasValue)
      {
        u32 Stop2 = 0;
        Error.Kind = CliResolveBatchedAlias(Value, Command->OHead, Cli->OHead, Command->KHead, &Error, &Stop2);
        if (Stop2) break;
      };
    };
    
    // Check whether all were set
    
    if (!Stop)
    {
      for (cli_arg* Node = Error.Kind? 0 : Command->AHead; Node; Node = Node->Next)
      {
        if (Node->Required && !Node->Set)
        {
          Error.RequiredArg = Node;
          Error.Kind = CliErrorRequiredArgument;
          Error.Positional = 1;
          break;
        };
      };
      
      for (cli_arg* Node = Error.Kind? 0 : Command->KHead; Node; Node = Node->Next)
      {
        if (Node->Required && !Node->Set)
        {
          Error.RequiredArg = Node;
          Error.Kind = CliErrorRequiredArgument;
          break;
        };
      };
    };

    if (!Error.Kind) *Command->Called = 1;
    
  } else if (!Stop && !Command && !Error.Kind)
  {
    Error.Kind = CliErrorExpectedCommandName;
  };

  Cli->Current = Command;
  return Error;
};

u32
CliParse(cli* Cli, const char** Argv, u32 Length)
{
  if (!Cli) return 0;

  Cli->Error = CliLexerParse(Cli, Argv, Length);
  Cli->Argv = Argv;
  Cli->Count = Length;
  return !Cli->Error.Kind;
};

// Writer interface

typedef void cli_writer(void* This, u32 Char);

typedef struct cli_writeable cli_writeable;
struct cli_writeable
{
  cli_writer* Callback;
  void* This;
};

static void
CliPutChar(cli_writeable Out, u32 Char)
{
  if (Out.Callback) Out.Callback(Out.This, Char);
};


static void
CliPutLine(cli_writeable Out)
{
  if (Out.Callback)
  {
    // TODO: Use the os specific line breaking
    Out.Callback(Out.This, '\n');
  };
};

static void
CliPutCharN(cli_writeable Out, u32 Char, usize Count)
{
  if (Out.Callback)
  {
    for (usize i = 0; i < Count; i++)
    {
      Out.Callback(Out.This, Char);
    };
  };
};

static void
CliPuts(cli_writeable Out, const u8* Value, usize Length)
{
  if (!Value) Length = 0;

  if (Out.Callback)
  {
    usize i = 0;
    while (i < Length)
    {
      usize Advance = CliCharUtf8Advance(Value[i]);
      u32 Char = 0;
      if (!Advance || Length < i + Advance)
      {
        Advance = 1;
        Char = Value[i];
      } else 
      {
        Char = CliCharUtf8Decode(Value + i);
      };
      Out.Callback(Out.This, Char);
      i += Advance;
    };
  };
};

static void
CliPutcs(cli_writeable Out, const char* Value)
{
  usize Length = CliStrLen(Value);

  CliPuts(Out, (const u8*)Value, Length);
};

static void
CliWriteIndentedText(cli_writeable Out, usize Indentation, usize Client, cli_str Desc)
{
  usize Space = Client - Indentation;
  usize i = 0;
  usize r = 0;
  while (1)
  {
    while (i < Desc.Length && CliCharIsSpace(Desc.Value[i])) i++;
    usize x = i;
    while (i < Desc.Length && !CliCharIsSpace(Desc.Value[i])) i++;
    usize L = i - x;
    if (!L) break;
    
    if (Space < r + L + 1)
    {
      r = 0;
      CliPutLine(Out);
      CliPutCharN(Out, ' ', Indentation);
    };
    CliPuts(Out, Desc.Value + x, L);
    CliPutChar(Out, ' ');
    r += L + 1;
  };
  CliPutLine(Out);
};

// Help message printing
static void
CliWriteFlagName(cli_writeable Out, cli_str Name, usize Indentation, usize Client)
{
  (void)Client;
  u8 Short = 0;
  cli_str Long = CliExpandName(Name, &Short);
  
  if (Short)
  {
    CliPutCharN(Out, ' ', 2);
    CliPutChar(Out, '-');
    CliPutChar(Out, Short);
    CliPutChar(Out, ',');
  } else
  {
    CliPutCharN(Out, ' ', 5);
  };
  
  CliPutCharN(Out, '-', 2);
  CliPuts(Out, Long.Value, Long.Length);
  CliPutCharN(Out, ' ', Indentation - Long.Length - 9 + 2);
  
};

static void
CliWriteArgName(cli_writeable Out, cli_str Name, usize Indentation, usize Client)
{
  (void)Client;
  CliPutCharN(Out, ' ', 2);
  CliPuts(Out, Name.Value, Name.Length);
  CliPutCharN(Out, ' ', Indentation - Name.Length - 4 + 2);
  
};

static void
CliWriteCmdName(cli_writeable Out, cli_str Name, usize Indentation, usize Client)
{
  (void)Client;
  u8 Short = 0;
  cli_str Long = CliExpandName(Name, &Short);
  
  if (Short)
  {
    CliPutCharN(Out, ' ', 2);
    CliPutChar(Out, Short);
    CliPutChar(Out, '/');
  } else
  {
    CliPutCharN(Out, ' ', 4);
  };
  CliPuts(Out, Long.Value, Long.Length);
  CliPutCharN(Out, ' ', Indentation - Long.Length - 6 + 2);
};

static void
CliWriteOptions(cli_writeable Out, cli_opt* Head, const char* Label, usize Indentation, usize Client)
{
  if (Head)
  {
    CliPutcs(Out, Label);
    CliPutLine(Out);
    for (cli_opt* Node = Head; Node; Node = Node->Next)
    {
      CliWriteFlagName(Out, Node->Name, Indentation, Client);
      CliWriteIndentedText(Out, Indentation, Client, Node->Desc);
    };
  };
  
};

static void
CliWriteKwargs(cli_writeable Out, cli_arg* Head, const char* Label, usize Indentation, usize Client)
{
  if (Head)
  {
    CliPutcs(Out, Label);
    CliPutLine(Out);
    for (cli_arg* Node = Head; Node; Node = Node->Next)
    {
      CliWriteFlagName(Out, Node->Name, Indentation, Client);
      CliWriteIndentedText(Out, Indentation, Client, Node->Desc);
    };
  };
  
};

static void
CliWriteArgs(cli_writeable Out, cli_arg* Head, const char* Label, usize Indentation, usize Client)
{
  if (Head)
  {
    CliPutcs(Out, Label);
    CliPutLine(Out);
    for (cli_arg* Node = Head; Node; Node = Node->Next)
    {
      CliWriteArgName(Out, Node->Name, Indentation, Client);
      CliWriteIndentedText(Out, Indentation, Client, Node->Desc);
    };
    CliPutLine(Out);
  };
};

static void
CliWriteCommands(cli_writeable Out, cli_cmd* Head, const char* Label, usize Indentation, usize Client)
{
  if (Head)
  {
    CliPutcs(Out, Label);
    CliPutLine(Out);
    for (cli_cmd* Node = Head; Node; Node = Node->Next)
    {
      CliWriteCmdName(Out, Node->Name, Indentation, Client);
      CliWriteIndentedText(Out, Indentation, Client, Node->Desc);
    };
  };
};

static void
CliWriteHelp(cli_writeable Out, cli* Cli, usize Client)
{
  if (!Cli) return;
  
  Client = CliMax(Client, Cli->Indentation + 10);
  
  if (Cli->Current && Cli->Current != Cli->Default)
  {
    cli_cmd* Node = Cli->Current;
    CliPuts(Out, Node->Desc.Value, Node->Desc.Length);
    CliPutLine(Out);
    CliPutLine(Out);
    CliWriteArgs(Out, Node->AHead, "Positional: ", Cli->Indentation, Client);
    CliWriteKwargs(Out, Node->KHead, "Arguments: ", Cli->Indentation, Client);
    CliWriteOptions(Out, Node->OHead, "Options: ", Cli->Indentation, Client);
    CliWriteOptions(Out, Cli->OHead, "Global Options: ", Cli->Indentation, Client);
  } else 
  {
    CliPuts(Out, Cli->Name.Value, Cli->Name.Length);
    CliPutLine(Out);
    CliPutLine(Out);
    CliPuts(Out, Cli->Desc.Value, Cli->Desc.Length);
    CliPutLine(Out);
    CliPutLine(Out);
    
    if (Cli->Default)
    {
      cli_cmd* Node = Cli->Default;
      CliWriteArgs(Out, Node->AHead, "Positional: ", Cli->Indentation, Client);
      CliWriteKwargs(Out, Node->KHead, "Arguments: ", Cli->Indentation, Client);
      CliWriteOptions(Out, Node->OHead, "Options: ", Cli->Indentation, Client);
    };
    
    CliWriteCommands(Out, Cli->CHead, "Commands: ", Cli->Indentation, Client);
    CliWriteOptions(Out, Cli->OHead, "Global Options:", Cli->Indentation, Client);
  };
};


// Byte buffer
typedef struct cli_sb_node cli_sb_node;
struct cli_sb_node
{
  cli_sb_node* Next;
  u8* Value;
};

typedef struct cli_sb cli_sb;
struct cli_sb
{
  cli_arena* Arena;
  cli_sb_node* Head;
  cli_sb_node* Tail;
  usize Length;
  usize Capacity;
  usize ChunkSize;
  u32 TryGrow;
};

static u32
CliSbReserve(cli_sb* Buffer, usize Size)
{
  if (!Buffer || !Buffer->TryGrow) return 0;
  
  if (Buffer->Length + Size <= Buffer->Capacity) return 1;

  cli_sb_node* Node = CliArenaZPush(Buffer->Arena, sizeof(*Node));
  if (Node)
  {
    Node->Value = CliArenaPush(Buffer->Arena, Buffer->ChunkSize);
    if (Node->Value)
    {
      if (Buffer->Tail) Buffer->Tail->Next = Node;
      else Buffer->Head = Node;
      Buffer->Tail = Node;
      Buffer->Capacity += Buffer->ChunkSize;
      return 1;
    };
  };
  Buffer->TryGrow = 0;
  return 0;
};

static u32
CliSbPush(cli_sb* Buffer, u8* Bytes, usize Length)
{
  usize i = 0;

  while (i < Length)
  {
    usize Copy = CliMin(Length - i, Buffer->ChunkSize - Buffer->Length % Buffer->ChunkSize);
    if (!CliSbReserve(Buffer, Copy)) break;
    if (Copy)
    {
      CliMemoryCopy(Buffer->Tail->Value + Buffer->Length % Buffer->ChunkSize, Bytes + i, Copy);
      i += Copy;
      Buffer->Length += Copy;
    };
  };
  return i == Length;
};

static void
CliSbCopy(cli_sb* Buffer, u8* Out, usize Length)
{
  if (!Buffer || !Out || Length == 0) return;
  
  usize i = 0;
  for (cli_sb_node* Node = Buffer->Head; Node && i < Length; Node = Node->Next)
  {
    // determine how much to copy from this node
    usize node_len = (Node == Buffer->Tail) ? (Buffer->Length % Buffer->ChunkSize) : Buffer->ChunkSize;
    if (node_len == 0) node_len = Buffer->ChunkSize; // full tail chunk
    
    usize copy = CliMin(Length - i, node_len);
    
    CliMemoryCopy(Out + i, Node->Value, copy);
    i += copy;
  };
};

u8*
CliSbRead8(cli_sb* Buffer, usize* Length)
{
  u8* Out = 0;
  usize L = 0;
  if (Buffer)
  {
    L = Buffer->Length;
    Out = CliMalloc(L + 1);
    CliSbCopy(Buffer, Out, L);
    if (Out) Out[L] = 0;
  };

  if (Length) *Length = L;
  return Out;
};

static u16*
CliSbRead16(cli_sb* Buffer, usize* Length)
{
  u16* Out = 0;
  usize L = 0;
  if (Buffer)
  {
    L = Buffer->Length / sizeof(u16);
    Out = CliMalloc((L + 1) * sizeof(u16));
    CliSbCopy(Buffer, (u8*)Out, L * sizeof(u16));
    if (Out) Out[L] = 0;
  };
  if (Length) *Length = L;
  return Out;
};

static void
CliSb_Write(void* This, u32 Char)
{
  cli_sb* Buffer = This;
  u8 Parts[4];
  usize Length = CliCharUtf8Encode(Char, Parts);
  CliSbPush(Buffer, Parts, Length);
};

static void
CliSb_Write16(void* This, u32 Char)
{
  cli_sb* Buffer = This;
  u16 Parts[2];
  usize Length = CliCharUtf16Encode(Char, Parts);
  CliSbPush(Buffer, (u8*)Parts, Length * sizeof(u16));
};

static void
CliFile_Write(void* This, u32 Char)
{
  cli_file_t File = This;
  u8 Parts[4];
  usize Length = CliCharUtf8Encode(Char, Parts);
  
  CliFileWrite(File, Parts, 1, Length);
};

void
CliHelpWrite(cli* Cli, cli_file_t File, usize ConsoleWidth)
{
  if (!Cli) return;

  cli_writeable Out;
  Out.Callback = CliFile_Write;
  Out.This = (void*)File;
  CliWriteHelp(Out, Cli, ConsoleWidth);
  CliFileFlush(File);
};

const char*
CliHelpAsString(cli* Cli, size_t* Length, usize ConsoleWidth)
{
  const char* String = 0;
  usize L = 0;

  if (Cli)
  {
    usize Position = CliArenaPosition(Cli->Arena);
    cli_sb Buffer = {0};
    Buffer.Arena = Cli->Arena;
    Buffer.ChunkSize = 2<<10;
    cli_writeable Out;
    Out.Callback = CliSb_Write;
    Out.This = &Buffer;
    CliWriteHelp(Out, Cli, ConsoleWidth);
    String = (const char*)CliSbRead8(&Buffer, &L);
    CliArenaPopTo(Cli->Arena, Position);
  };
  if (Length) *Length = L;
  return String;
};

u16*
CliHelpAsString16(cli* Cli, size_t* Length, usize ConsoleWidth)
{
  u16* String = 0;
  usize L = 0;

  if (Cli)
  {
    usize Position = CliArenaPosition(Cli->Arena);
    cli_sb Buffer = {0};
    Buffer.Arena = Cli->Arena;
    Buffer.ChunkSize = 4<<10;
    cli_writeable Out;
    Out.Callback = CliSb_Write16;
    Out.This = &Buffer;
    CliWriteHelp(Out, Cli, ConsoleWidth);
    String = CliSbRead16(&Buffer, &L);
    CliArenaPopTo(Cli->Arena, Position);
  };

  if (Length) *Length = L;
  return String;
};

// Error logging

static void
CliPutUsize(cli_writeable Out, usize Value)
{
  if (Out.Callback)
  {
    u8 Buffer[0x30];
    usize i = 0;

    if (Value == 0) Buffer[i++] = '0';

    while (Value > 0)
    {
      Buffer[i++] = (Value % 10) + '0';
      Value /= 10;
    };

    for (usize k = 0; k < i; k++)
    {
      Out.Callback(Out.This, Buffer[i - k - 1]);
    };
  };
};

static void
CliWriteError(cli* Cli, cli_writeable Out)
{
  if (!Cli) return;

  switch (Cli->Error.Kind)
  {
    default: return;


    // Excruciatingly painful string writing. Would use printf but I want libc to be opt-in
    // and it would not work with the writer interface without requiring every single 
    // implimentation to write it's own vsprintf function.

    case CliErrorParsing:
    {
      CliPutcs(Out, "Error: ");
      CliPutcs(Out, "Could not parse `");
      CliPuts(Out, Cli->Error.Parsing.Value, Cli->Error.Parsing.Length);
      CliPutcs(Out, " `.");
    } break;
    case CliErrorMissingValue:
    {
      CliPutcs(Out, "Error: ");
      cli_str Name = Cli->Error.MissingValue->Name;
      CliPutcs(Out, "Missing value for argument `--");
      CliPuts(Out, Name.Value, Name.Length);
      CliPutcs(Out, " `.");
    } break;
    case CliErrorNotEnoughValues:
    {
      CliPutcs(Out, "Error: ");
      u8 Short = 0;
      cli_str Name = CliExpandName(Cli->Error.NotEnoughValues->Name, &Short);
      usize Expected = Cli->Error.ExpectedCount;
      usize Got = Cli->Error.GotCount;
      u32 Positional = Cli->Error.Positional;
      CliPutcs(Out, "Argument `");
      if (!Positional) CliPutcs(Out, "--");
      CliPuts(Out, Name.Value, Name.Length);
      CliPutcs(Out, "` Expected ");
      CliPutUsize(Out, Expected);
      CliPutcs(Out, " value(s) but recieved ");
      CliPutUsize(Out, Got);
      CliPutcs(Out, " value(s) but recieved.");
    } break;
    case CliErrorUnknownOption:
    {
      CliPutcs(Out, "Error: ");
      cli_str Name = Cli->Error.UknownOption;
      CliPutcs(Out, "Uknown option `-");
      if (!Cli->Error.IsAlias) CliPutChar(Out, '-');
      CliPuts(Out, Name.Value, Name.Length);
      CliPutcs(Out, "`.");
    } break;
    case CliErrorExpectedCommandName:
    {
      CliPutcs(Out, "Error: ");
      CliPutcs(Out, "Expected command name.");
    } break;
    case CliErrorUnexpectedValue:
    {
      CliPutcs(Out, "Error: ");
      cli_str Name = Cli->Error.UnexpectedValue;
      CliPutcs(Out, "`");
      CliPuts(Out, Name.Value, Name.Length);
      CliPutcs(Out, "` was unexpected.");
    } break;
    case CliErrorArgumentDoesNotExpectValue:
    {
      CliPutcs(Out, "Error: ");
      u8 Short = 0;
      cli_str Name = CliExpandName(Cli->Error.ArgumentDoesNotExpectValue->Name, &Short);
      CliPutcs(Out, "Argument `--");
      CliPuts(Out, Name.Value, Name.Length);
      CliPutcs(Out, "` does not require any value.");
    } break;
    case CliErrorUnknownCommand:
    {
      CliPutcs(Out, "Error: ");
      u8 Short = 0;
      cli_str Name = CliExpandName(Cli->Error.UknownCommand, &Short);
      CliPutcs(Out, "Uknown command `");
      CliPuts(Out, Name.Value, Name.Length);
      CliPutcs(Out, "`.");
    } break;
    case CliErrorRequiredArgument:
    {
      CliPutcs(Out, "Error: ");
      u8 Short = 0;
      cli_str Name = CliExpandName(Cli->Error.RequiredArg->Name, &Short);
      u32 Positional = Cli->Error.Positional;
      CliPutcs(Out, "Required argument `");
      if (!Positional) CliPutcs(Out, "--");
      CliPuts(Out, Name.Value, Name.Length);
      CliPutcs(Out, "` did not recieve any value.");
    } break;
  };
  CliPutLine(Out);
  CliPutLine(Out);
  CliPutcs(Out, "Try  : ");
  usize i = 0;
a0x:
  CliPutcs(Out, Cli->Argv[i++]);
  CliPutChar(Out, ' ');
  if (i == 1 && Cli->Current && Cli->Count > 1) goto a0x;

  CliPutcs(Out, "--help");
  CliPutLine(Out);
};

const char*
CliErrorAsString(cli* Cli, size_t* Length)
{
  const char* String = 0;
  usize L = 0;

  if (Cli)
  {
    usize Position = CliArenaPosition(Cli->Arena);
    cli_sb Buffer = {0};
    Buffer.Arena = Cli->Arena;
    Buffer.ChunkSize = 2<<10;
    cli_writeable Out;
    Out.Callback = CliSb_Write;
    Out.This = &Buffer;
    CliWriteError(Cli, Out);
    String = (const char*)CliSbRead8(&Buffer, &L);
    CliArenaPopTo(Cli->Arena, Position);
  };
  if (Length) *Length = L;
  return String;
};

u16*
CliErrorAsString16(cli* Cli, size_t* Length)
{
  u16* String = 0;
  usize L = 0;

  if (Cli)
  {
    usize Position = CliArenaPosition(Cli->Arena);
    cli_sb Buffer = {0};
    Buffer.Arena = Cli->Arena;
    Buffer.ChunkSize = 4<<10;
    cli_writeable Out;
    Out.Callback = CliSb_Write16;
    Out.This = &Buffer;
    CliWriteError(Cli, Out);
    String = CliSbRead16(&Buffer, &L);
    CliArenaPopTo(Cli->Arena, Position);
  };

  if (Length) *Length = L;
  return String;
};

void
CliErrorWrite(cli* Cli, cli_file_t File)
{
  if (!Cli) return;

  cli_writeable Out;
  Out.Callback = CliFile_Write;
  Out.This = (void*)File;
  CliWriteError(Cli, Out);
  CliFileFlush(File);
};
