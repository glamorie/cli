#ifndef CMD_H
#define CMD_H
#include <stdint.h>
#include <stddef.h>

#if !defined(CliMalloc) || !defined(CliFree)
  #include <stdlib.h>
  #undef CliMalloc
  #undef CliFree
  #define CliMalloc(Size) malloc(Size)
  #define CliFree(Ptr) free(Ptr)
#endif

#if !defined(cli_file_t) || !defined(CliFileWrite)
  #include <stdio.h>
  #undef cli_file_t
  #undef CliFileWrite
  #define cli_file_t FILE*
  #define CliFileWrite(File, Ptr, Size, Count) fwrite((Ptr), (Size), (Count), (File))
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef size_t usize;

typedef struct cli cli;

cli*
CliMake(const char* Name, const char* Desc);

void
CliTake(cli* Cli);

u32
CliParse(cli* Cli, const char** Argv, u32 Length);

const char*
CliErrorAsString(cli* Cli, size_t* Length, usize ConsoleWidth);

u16*
CliErrorAsString16(cli* Cli, size_t* Length, usize ConsoleWidth);

void
CliErrorWrite(cli* Cli, cli_file_t File, usize ConsoleWidth);

const char*
CliHelpAsString(cli* Cli, size_t* Length, usize ConsoleWidth);

u16*
CliHelpAsString17(cli* Cli, size_t* Length, usize ConsoleWidth);

void
CliHelpWrite(cli* Cli, cli_file_t File, usize ConsoleWidth);

void
CliCommand(cli* Cli, u32* Called, const char* Name, const char* Desc);

void
CliMain(cli* Cli, u32* Called, const char* Name, const char* Desc);

void
CliOption(cli* Cli, u32* Value, const char* Name, const char* Desc);

void
CliInt(cli* Cli, i64* Value, const char* Name, const char* Desc);

void
CliFloat(cli* Cli, double* Value, const char* Name, const char* Desc);

void
CliStr(cli* Cli, const char** Value, const char* Name, const char* Desc);

void
CliIntOr(cli* Cli, i64* Value, i64 Default, const char* Name, const char* Desc);

void
CliFloatOr(cli* Cli, double* Value, double Default, const char* Name, const char* Desc);

void
CliStrOr(cli* Cli, const char** Value, const char* Default, const char* Name, const char* Desc);

void
CliIntN(cli* Cli, i64** Value, usize* Length, usize Count, const char* Name, const char* Desc);

void
CliFloatN(cli* Cli, double** Value, usize* Length, usize Count, const char* Name, const char* Desc);

void
CliStrN(cli* Cli, const char*** Value, usize* Length, usize Count, const char* Name, const char* Desc);

#endif /* CMD_H*/