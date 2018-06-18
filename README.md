# Shell
A basic shell with piping, history and output redirection written in C.

## Usage:

### Piping
Prepend command with PP and use -> to pipe commands:

__example(bash):__ `wc -l > out.txt`

__example(shell):__ `PP wc -l -> out.txt`

### Output redirection
Prepend command with OR and use -> to pipe commands:

__example(bash):__ `ls -l | wc -l`

__example(shell):__ `OR ls -l -> wc -l`

### Output redirection and piping
Prepend command with ORPP and use -> to pipe commands:

__example(bash):__ `ls -l | wc -l > out.xt`

__example(shell):__ `ORPP ls -l -> wc -l -> out.txt`

### History
1. Type history too see previous commands
2. use the bang(!) to execute the command id.

__example__:  
```
history
 0  ls
 1  cat out.txt
 2  rm out.txt
 3  ls
 4  history
!2
rm out.txt
 ```






