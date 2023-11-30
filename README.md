# Systems Fundamental Projects

Welcome :wave:! Here you will find all the assignments I completed as part of the systems fundamentals course at my university (Spring 2021).

## Projects Overview

### 1. Argo (```hw1```)
A command-line utility (called `argo`) which can validate JSON input and transform JSON input into JSON output in a "canonical" form.
The usage scenarios for this program are described by the following message, which is printed by the program when it is invoked without any arguments:
<pre>
USAGE: bin/argo [-h] [-c|-v] [-p|-p INDENT]
   -h       Help: displays this help menu.
   -v       Validate: the program reads from standard input and checks whether
            it is syntactically correct JSON.  If there is any error, then a message
            describing the error is printed to standard error before termination.
            No other output is produced.
   -c       Canonicalize: once the input has been read and validated, it is
            re-emitted to standard output in 'canonical form'.  Unless -p has been
            specified, the canonicalized output contains no whitespace (except within
            strings that contain whitespace characters).
   -p       Pretty-print:  This option is only permissible if -c has also been specified.
            In that case, newlines and spaces are used to format the canonical output
            in a more human-friendly way.  For the precise requirements on where this
            whitespace must appear, see the assignment handout.
            The INDENT is an optional nonnegative integer argument that specifies the
            number of additional spaces to be output at the beginning of a line for each
            for each increase in indentation level.  If no value is specified, then a
            default value of 4 is used.
</pre>

### 2. Par (```hw2```)
An ***extended*** and ***debugged*** version of ```par```, a simple paragraph reformatter which was written by Adam M. Costello and posted to Usenet way back in 1993. There are several parameters that can be set which affect the result:  the width of the output text, the length of a "prefix" and a "suffix" to be prepended and appended to each output line, a parameter "hang", which affects the default value of "prefix", and a boolean parameter "last", which affects the way the last line of a paragraph is treated.

Here is the option syntax of the improved ```par```:
- `--version` (long form only):
    Print the version number of the program.

- `-w WIDTH` (short form) or `--width WIDTH` (long form):
    Set the output paragraph width to `WIDTH`.

- `-p PREFIX` (short form) or `--prefix PREFIX` (long form):
    Set the value of the "prefix" parameter to `PREFIX`.

- `-s SUFFIX` (short form) or `--suffix SUFFIX` (long form):
    Set the value of the "suffix" parameter to `SUFFIX`.

- `-h HANG` (short form) or `--hang HANG` (long form):
    Set the value of the "hang" parameter to `HANG`.

- `-l LAST` (short form) or either `--last` or
    `--no-last` (long form):
    Set the value of the boolean "last" parameter.
   For the short form, the values allowed for `LAST` should be either
   `0` or `1`.

- `-m MIN` (short form) or either `--min` or `--no-min` (long form).
   Set the value of the boolean "min" parameter.
   For the short form, the values allowed for `MIN` should be either
   `0` or `1`.

### 3. x86-64 Dynamic Memory Allocator (```hw3```)
A C dynamic memory allocator package for the x86-64 architecture with the following features:

- Free lists segregated by size class, using first-fit policy within each size class,
  augmented with a set of "quick lists" holding small blocks segregated by size
- Immediate coalescing of large blocks on free with adjacent free blocks;
  delayed coalescing on free of small blocks
- Boundary tags to support efficient coalescing, with footer optimization that allows
    footers to be omitted from allocated blocks
- Block splitting without creating splinters
- Allocated blocks aligned to "double memory row" (16-byte) boundaries
- Free lists maintained using **last in first out (LIFO)** discipline
- Obfuscation of block headers and footers to detect heap corruption and attempts to
  free blocks not previously obtained via allocation

The various header and footer formats are specified below:

```c
                                 Format of an allocated memory block
    +-----------------------------------------------------------------------------------------+
    |                                    64-bit-wide row                                      |
    +-----------------------------------------------------------------------------------------+

    +----------------------------+----------------------+--------+--------+---------+---------+ <- header
    |      payload size          |     block_size       | unused | alloc  |prv alloc|in qklst |
    |          (0/1)             |(4 LSB's implicitly 0)|  (0)   |  (1)   |  (0/1)  |   (0)   |
    |        (32 bits)           |      (28 bits)       | 1 bit  | 1 bit  |  1 bit  |  1 bit  |
    +---------------------------------------------------+--------+--------+---------+---------+ <- (aligned)
    |                                                                                         |
    |                                   Payload and Padding                                   |
    |                                        (N rows)                                         |
    |                                                                                         |
    |                                                                                         |
    +-----------------------------------------------------------------------------------------+

    NOTE: For an allocated block, there is no footer (it is used for payload).
    NOTE: The actual stored header is obfuscated by bitwise XOR'ing with MAGIC.
          The above diagram shows the un-obfuscated contents.
```

```c
                             Format of a memory block in a quick list
    +-----------------------------------------------------------------------------------------+
    |                                    64-bit-wide row                                      |
    +-----------------------------------------------------------------------------------------+

    +----------------------------+----------------------+--------+--------+---------+---------+ <- header
    |         unused             |     block_size       | unused | alloc  |prv alloc|in qklst |
    |          (0)               |(4 LSB's implicitly 0)|  (0)   |  (1)   |  (0/1)  |   (1)   |
    |        (32 bits)           |      (28 bits)       | 1 bit  | 1 bit  |  1 bit  |  1 bit  |
    +---------------------------------------------------+--------+--------+---------+---------+ <- (aligned)
    |                                                                                         |
    |                                   Payload and Padding                                   |
    |                                        (N rows)                                         |
    |                                                                                         |
    |                                                                                         |
    +-----------------------------------------------------------------------------------------+

    NOTE: For a block in a quick list, there is no footer.
```

```c
                                     Format of a free memory block


    +----------------------------+----------------------+--------+--------+---------+---------+ <- header
    |         unused             |     block_size       | unused | alloc  |prv alloc|in qklst |
    |          (0)               |(4 LSB's implicitly 0)|  (0)   |  (0)   |  (0/1)  |   (0)   |
    |        (32 bits)           |      (28 bits)       | 1 bit  | 1 bit  |  1 bit  |  1 bit  |
    +------------------------------------------------------------+--------+---------+---------+ <- (aligned)
    |                                                                                         |
    |                                Pointer to next free block                               |
    |                                        (1 row)                                          |
    +-----------------------------------------------------------------------------------------+
    |                                                                                         |
    |                               Pointer to previous free block                            |
    |                                        (1 row)                                          |
    +-----------------------------------------------------------------------------------------+
    |                                                                                         | 
    |                                         Unused                                          | 
    |                                        (N rows)                                         |
    |                                                                                         |
    |                                                                                         |
    +------------------------------------------------------------+--------+---------+---------+ <- footer
    |         unused             |     block_size       | unused | alloc  |prv alloc|in qklst |
    |          (0)               |(4 LSB's implicitly 0)|  (0)   |  (0)   |  (0/1)  |   (0)   |
    |        (32 bits)           |      (28 bits)       | 1 bit  | 1 bit  |  1 bit  |  1 bit  |
    +------------------------------------------------------------+--------+---------+---------+

    NOTE: For a free block, footer contents must always be identical to header contents.
    NOTE: The actual stored footer is obfuscated by bitwise XOR'ing with MAGIC.
          The above diagram shows the un-obfuscated contents.
```

The overall organization of the heap is as shown below:

```c
                                         Format of the heap

    +-----------------------------------------------------------------------------------------+
    |                                    64-bit-wide row                                      |
    +-----------------------------------------------------------------------------------------+

    +-----------------------------------------------------------------------------------------+ <- heap start
    |                                                                                         |    (aligned)
    |                                        Unused                                           |
    |                                        (1 row)                                          |
    +----------------------------+----------------------+--------+--------+---------+---------+ <- header
    |      payload size          |minimum block_size(32)| unused | alloc  |prv alloc|in qklst |
    |          (0)               |(4 LSB's implicitly 0)|  (0)   |  (1)   |  (0/1)  |   (0)   | prologue block
    |        (32 bits)           |      (28 bits)       | 1 bit  | 1 bit  |  1 bit  |  1 bit  |
    +------------------------------------------------------------+--------+---------+---------+ <- (aligned)
    |                                                                                         |
    |                                   Unused Payload Area                                   |
    |                                        (3 rows)                                         |
    +------------------------------------------------------------+--------+---------+---------+ <- header
    |      payload size          |     block_size       | unused | alloc  |prv alloc|in qklst |
    |          (0/1)             |(4 LSB's implicitly 0)|  (0)   | (0/1)  |  (0/1)  |  (0/1)  | first block
    |        (32 bits)           |      (28 bits)       | 1 bit  | 1 bit  |  1 bit  |  1 bit  |
    +------------------------------------------------------------+--------+---------+---------+ <- (aligned)
    |                                                                                         |
    |                                   Payload and Padding                                   |
    |                                        (N rows)                                         |
    |                                                                                         |
    |                                                                                         |
    +--------------------------------------------+------------------------+---------+---------+
    |                                                                                         |
    |                                                                                         |
    |                                                                                         |
    |                                                                                         |
    |                             Additional allocated and free blocks                        |
    |                                                                                         |
    |                                                                                         |
    |                                                                                         |
    +-----------------------------------------------------------------------------------------+
    |      payload size          |     block_size       | unused | alloc  |prv alloc|in qklst |
    |          (0)               |        (0)           |  (0)   |  (1)   |  (0/1)  |   (0)   | epilogue
    |        (32 bits)           |      (28 bits)       | 1 bit  | 1 bit  |  1 bit  |  1 bit  |
    +------------------------------------------------------------+--------+---------+---------+ <- heap_end
                                                                                                   (aligned)

    NOTE: The actual stored epilogue is obfuscated by bitwise XOR'ing with MAGIC.
          The above diagram shows the un-obfuscated contents.
```

### 4. Mush (```hw4```)

A command-line interpreter (called ```mush```) for a simple scripting language
that is capable of managing multiple concurrently executing "jobs".

The `mush` language is a simple programming language which was roughly
inspired by the classic programming language BASIC.
A `mush` program consists of a set of *statements*, with one statement
per line of program text.
The syntax of statements is given by the following context-free grammar:

```
<statement>       ::= list
                    | delete <lineno> , <lineno>
                    | run
                    | cont
                    | <lineno> stop
                    | <optional_lineno> set <name> = <expr>
                    | <optional_lineno> unset <name>
                    | <optional_lineno> goto <lineno>
                    | <optional_lineno> if <expr> goto <lineno>
                    | <optional_lineno> source <file>
                    | <optional_lineno> <pipeline>
                    | <optional_lineno> <pipeline> &
                    | <optional_lineno> wait <expr>
                    | <optional_lineno> poll <expr>
                    | <optional_lineno> cancel <expr>
                    | <optional_lineno> pause
```
### 5. PBX Server (```hw5```)

A multi-threaded network server that simulates the behavior of a Private Branch Exchange (PBX) telephone system.

A PBX is a private telephone exchange that is used within a business or other organization to allow calls to be placed between telephone units (TUs) attached to the system, without having to route those calls over the public telephone network.

The PBX system provides the following basic capabilities:

  * *Register* a TU as an extension in the system
  * *Unregister* a previously registered extension

Once a TU has been registered, the following operations are available to perform on it:

  * *Pick up* the handset of a registered TU (If the TU was ringing,
then a connection is established with a calling TU.  If the TU was not
ringing, then the user hears a dial tone over the receiver.)
  * *Hang up* the handset of a registered TU (Any call in progress is
disconnected.)
  * *Dial* an extension on a registered TU (If the dialed extension is
currently "on hook" [*i.e.,* the telephone handset is on the switchhook],
then the dialed extension starts to ring, indicating the presence of an
incoming call, and a "ring back" notification is played over the receiver
of the calling extension.  Otherwise, if the dialed extension is "off hook",
then a "busy signal" notification is played over the receiver of the
calling extension.)
  * *Chat* over the connection made when one TU has dialed an extension
and the called extension has picked up

When the server is started, a **master server** thread sets up a socket on
which to listen for connections from clients (*i.e.,* the TUs).  When a network
connection is accepted, a **client service thread** is started to handle requests
sent by the client over that connection.  The client service thread registers
the client with the PBX system and is assigned an extension number.
The client service thread then executes a service loop in which it repeatedly
receives a **message** sent by the client, performs some operation determined
by the message, and sends one or more messages in response.
The server will also send messages to a client asynchronously as a result of actions
performed on behalf of other clients.
For example, if one client sends a "dial" message to dial another extension,
then if that extension is currently on-hook it will receive an asynchronous
"ring" message, indicating that the ringer is to be activated.

The PBX system maintains the set of registered clients in the form of a mapping
from assigned extension numbers to clients.
It also maintains, for each registered client, information about the current
state of the TU for that client.  The following are the possible states of a TU:

  * **On hook**: The TU handset is on the switchhook and the TU is idle
  * **Ringing**: The TU handset is on the switchhook and the TU ringer is
active, indicating the presence of an incoming call
  * **Dial tone**:  The TU handset is off the switchhook and a dial tone is
being played over the TU receiver
  * **Ring back**:  The TU handset is off the switchhook and a "ring back"
signal is being played over the TU receiver
  * **Busy signal**:  The TU handset is off the switchhook and a "busy"
signal is being played over the TU receiver
  * **Connected**:  The TU handset is off the switchhook and a connection has been
established between this TU and the TU at another extension (In this state
it is possible for users at the two TUs to "chat" with each other over the
connection.)
  * **Error**:  The TU handset is off the switchhook and an "error" signal
is being played over the TU receiver

