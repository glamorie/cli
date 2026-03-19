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

int
CliParse(cli* Cli, const char** Argv, int Length);

const char*
CliErrorAsString(cli* Cli, size_t* Length);

void
CliErrorWrite(cli* Cli, cli_file_t File);

const char*
CliHelpAsString(cli* Cli, size_t* Length);

const char*
CliHelpWrite(cli* Cli, cli_file_t File);

void
CliCommand(cli* Cli, int* Called, const char* Name, const char* Desc);

void
CliMain(cli* Cli, int* Called, const char* Name, const char* Desc);

void
CliOption(cli* Cli, int* Value, const char* Name, const char* Desc);

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
CliIntN(cli* Cli, i64** Value, int* Length, int Count, const char* Name, const char* Desc);

void
CliFloatN(cli* Cli, double** Value, int* Length, int Count, const char* Name, const char* Desc);

void
CliStrN(cli* Cli, const char** Value, int* Length, int Count, const char* Name, const char* Desc);

#endif /* CMD_H*/