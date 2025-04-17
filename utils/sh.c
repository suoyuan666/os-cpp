#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXARGS 10

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

const char whitespace[] = " \t\r\n\v";
const char symbols[] = "<|>&;()";

[[noreturn]] void runcmd(struct cmd *cmd);
struct cmd *parsecmd(char *s);

void panic(char *s) {
  printf("%s\n", s);
  exit(1);
}

int getcmd(char *buf, int nbuf) {
  write(2, "$ ", 2);
  memset(buf, 0, nbuf);
  gets_s(buf, nbuf);

  if (buf[0] == 0) {
    return -1;
  }
  return 0;
}

int main(void) {
  int fd = 0;
  static char buf[100] = {0};

  while ((fd = open("console", O_RDWR)) >= 0) {
    if (fd >= 3) {
      close(fd);
      break;
    }
  }

  while (getcmd(buf, sizeof(buf)) >= 0) {
    if (fork() == 0) {
      runcmd(parsecmd(buf));
    }
    wait(nullptr);
  }
}

[[noreturn]] void runcmd(struct cmd *cmd) {
  if (cmd == nullptr) {
    exit(1);
  }

  struct execcmd *ecmd = (struct execcmd *)cmd;
  if (ecmd->argv[0] == nullptr) {
    exit(1);
  }
  execve(ecmd->argv[0], ecmd->argv, nullptr);
  printf("exec %s failed\n", ecmd->argv[0]);
  exit(0);
}

struct cmd *nulterminate(struct cmd *cmd) {
  struct execcmd *ecmd = (struct execcmd *)cmd;
  for (int i = 0; ecmd->argv[i]; ++i) {
    *ecmd->eargv[i] = 0;
  }
  return cmd;
}

int peek(char **ps, char *es, char *toks) {
  char *s = nullptr;

  s = *ps;
  while (s < es && strchr(whitespace, *s)) {
    s++;
  }
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *execcmd(void) {
  struct execcmd *cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  return (struct cmd *)cmd;
}

int gettoken(char **ps, char *es, char **q, char **eq) {
  char *s = nullptr;
  int ret = 0;

  s = *ps;
  while (s < es && strchr(whitespace, *s)) {
    s++;
  }
  if (q) {
    *q = s;
  }
  ret = *s;
  switch (*s) {
    case 0:
      break;
    case '|':
    case '(':
    case ')':
    case ';':
    case '&':
    case '<':
      s++;
      break;
    case '>':
      s++;
      if (*s == '>') {
        ret = '+';
        s++;
      }
      break;
    default:
      ret = 'a';
      while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s)) {
        s++;
      }
      break;
  }
  if (eq) {
    *eq = s;
  }

  while (s < es && strchr(whitespace, *s)) {
    s++;
  }
  *ps = s;
  return ret;
}

struct cmd *parseexec(char **ps, char *es) {
  char *q = nullptr, *eq = nullptr;
  int tok = 0, argc = 0;
  struct cmd *ret = execcmd();
  auto cmd = (struct execcmd *)ret;

  argc = 0;
  while (!peek(ps, es, "|)&;")) {
    tok = gettoken(ps, es, &q, &eq);
    if (tok == 0) {
      break;
    }
    if (tok != 'a') {
      panic("syntax");
    }
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if (argc >= MAXARGS) {
      panic("too many args");
    }
  }
  cmd->argv[argc] = nullptr;
  cmd->eargv[argc] = nullptr;
  return ret;
}

struct cmd *parsecmd(char *s) {
  char *es = nullptr;
  struct cmd *cmd = nullptr;

  es = s + strlen(s);
  cmd = parseexec(&s, es);
  peek(&s, es, "");
  if (s != es) {
    printf("leftovers: %s\n", s);
  }
  nulterminate(cmd);
  return cmd;
}
