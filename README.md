Command line parsing library for c.

## Concepts
### Arguments
These are placeholders for values that the program expects the user to pass through the command line. There are two categories of arguments i.e.
1. Named arguments - These are arguments that use a unique name to specify their values. They can also specify an alias which is used as an alternative to the longer version of their names. The values appear after their name / alias in the command line arguments.
2. Positional arguments - Their values are specified based on their position in the command line arguments. The values for all positional arguments do not necessarily have to be contagious in the arguments, but the values for a single argument have to follow each other in order.
Each argument can take in a fixed number of values or a dynamic number of values. The parser reads the values until a named argument is encountered.

### Options
This is a subset of named arguments that are used to set a Boolean to true. The distinction is used to provide an ergonomic way to pass in Boolean values from the command line. They do not require a value to be passed in alongside them. Some options can cause the parsing to stop when they are encountered. This is how options like `--help` work. They trigger an action that does not require the program's arguments to be parsed. In the case of `--help`, it shows the help message and exits. Continuing to parse the command line would be useless.

### Commands
They represent a single responsibility in the program. Just like a function, they store a collection of arguments. Each command contains a unique name that is specified by the user before the values are specified. The parser falls back to the default command which the program can specify. The command can optionally contain an alias which is a single letter.
## Syntax
- Names of named arguments and options are prefixed with double hyphens. --`name` 
- Aliases of named arguments and options are prefixed with a single hyphen. e.g. -`n`
- --`name`:*value* or --`name`=*value* syntax is used for named arguments that expect only one value.
- -`n`*value* is used for named arguments that only expect one value.
- -`a`*b*`c`*d*`e` can be used to specify a bunch of options all in one go. The equivalent is -a -b -c -d -e. An alias of a named argument can also be specified here too but everything else afterwards is assumed to be the value. e.g. -`a`*b*`c`*d*`e`*n*`value` which expands to -a -b -c -d -e -n value
- `--` Is used to escape values that begin with hyphens. It should appear before the value that you need to escape. e.g. --operator -- -. If it appears at the start of the values, then all values are escaped automatically. e.g. --operator -- + - /. For arguments with a dynamic number of arguments, if it appears at the beginning of the values, everything else from that point is assumed to be a value.
## API
Names of positional arguments should be prefixed with `*`
e.g.
```c
i64 Number; 
CliInt(Cli, &Number, "*number", "Some number");
```

When providing the name of a command / argument, *,`name`* can be used to indicate that the first character should be used as the alias. An alias can also be specified directly i.e. *n,`name`*
e.g.
```c
u32 Quiet; 
CliOption(Cli, &Quiet, ",quiet", "Disable terminal output");
```

When providing the name of a global option, prefixing it with an exclamation mark can be used to indicate that the parser should stop when it encounters it.
e.g.
```c
u32 ShowHelp; 
CliOption(Cli, &ShowHelp, "!,help", "Show context sensitive help message and exit");
```

Arguments that expect a dynamic number of values should set their Count to 0.
e.g.
```c
const char** Plugins; usize NPlugins; 
CliStrN(Cli, &Plugins, &NPlugins, 0, "*plugins", "The plugins to install");
```

The library provides functions for generating help messages. i.e.
1. `CliHelpAsString` null terminated utf8 string.
2. `CliHelpAsString16` null terminated utf16 string.
3. `CliHelpWrite` write to a file.

The library provides the following functions for generating error messages
1. `CliErrorAsString` null terminated utf8 error message.
2. `CliErrorAsString16` null terminated utf16 error message.
3. `CliErrorWrite` write to a file.
