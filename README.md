# sonar

## Layout

-   `app/` – entry point that wires the CLI, argparse, and replxx
-   `include/sonar/` – public headers for the lexer, parser, AST, and pretty printer
-   `src/` – library implementation
-   `thirdparty/replxx` – vendored terminal line-editing dependency
-   `thirdparty/argparse` – upstream header-only argparse library used for command-line parsing

## Building

```bash
./build.sh
```

## Running

Parse the contents of a file:

```bash
./build/bin/sonar path/to/input.sonar
```

Launch the REPL

```bash
./build/bin/sonar
```

Type `quit` or `exit` to leave the REPL.
