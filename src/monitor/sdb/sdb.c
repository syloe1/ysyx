/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include <memory/host.h>
#include <memory/paddr.h>
#include <device/mmio.h>
#include "watchpoint.h"
static int is_batch_mode = false;

void init_regex();
void init_wp_pool();
void sdb_watchpoint_display(){
    bool flag = true;
    for(int i = 0 ; i < NR_WP ; i ++){
	if(wp_pool[i].flag){
	    printf("Watchpoint.No: %d, expr = \"%s\", old_value = %d, new_value = %d\n", 
		    wp_pool[i].NO, wp_pool[i].expr,wp_pool[i].old_value, wp_pool[i].new_value);
		flag = false;
	}
    }
    if(flag) printf("No watchpoint now.\n");
}
void delete_watchpoint(int no){
    for(int i = 0 ; i < NR_WP ; i ++)
	if(wp_pool[i].NO == no){
	    free_wp(&wp_pool[i]);
	    return ;
	}
}
void create_watchpoint(char* args){
    WP* p =  new_wp();
    strcpy(p -> expr, args);
    bool success = false;
    int tmp = expr(p -> expr,&success);
   if(success) p -> old_value = tmp;
   else printf("创建watchpoint的时候expr求值出现问题\n");
    printf("Create watchpoint No.%d success.\n", p -> NO);
}

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  // nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_si(char *args) {
    int N = 1;  // 默认值为1
    char *arg = NULL;

    // 处理输入字符串
    if (args != NULL) {
        arg = strtok(args, " ");
        // 检查命令是否是 "si"
        if (arg != NULL && strcmp(arg, "si") == 0) {
            arg = strtok(NULL, " ");
            // 如果有数字参数，则解析它
            /*
            第一次调用：将字符串 args 按空格分割，返回第一个标记（token）的指针。

            例如，args 为 "si 10"，调用 strtok(args, " ") 后，arg 将指向 "si"。

        后续调用：通过传递 NULL 作为第一个参数继续拆分同一个字符串，返回下一个标记的指针。

            再次调用 strtok(NULL, " ")，将返回下一个标记 "10"。
            */
            if (arg != NULL && sscanf(arg, "%d", &N) != 1) {
                N = 1;  // 如果解析失败，默认值为1
            }
        }
    }

    cpu_exec(N);
    printf("Step execute N=%d\n", N);
    return 0;
}
static int cmd_info(char *args){
    if(args == NULL)
        printf("No args.\n");
    else if(strcmp(args, "r") == 0)
        isa_reg_display();
    else if(strcmp(args, "w") == 0)
        sdb_watchpoint_display();
    return 0;
}
static int cmd_help(char *args);
static int cmd_x(char *args) {
  char *n = strtok(args, " ");
  char *baseaddr = strtok(NULL, " ");
  int len = 0;
  paddr_t addr = 0;
  sscanf(n, "%d",&len);
  sscanf(baseaddr, "%x", &addr);
  for (int i = 0; i < len; i++) {
    printf("%x\n", 
       paddr_read(addr, 4));
    addr = addr + 4;
  }
  return 0;
}
static int cmd_p(char* args){
    if(args == NULL){
        printf("No args\n");
        return 0;
    }
    //  printf("args = %s\n", args);
    bool flag = false;
    expr(args, &flag);
    return 0;
}

 int cmd_d (char *args){
    if(args == NULL)
        printf("No args.\n");
    else{
        delete_watchpoint(atoi(args));
    }
    return 0;
}
 int cmd_w(char* args){
    create_watchpoint(args);
    return 0;
}
static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },

  /* TODO: Add more commands */
  {"si"," Let the programexecute N instuctions and then suspend, not define N , default N is 1", cmd_si},
  {"info", "printf relevant information, choose r or w, representative reg or w_point", cmd_info},
  {"x", "scan memory", cmd_x},
  {"p", "Expression evaluation", cmd_p},
   {"d", "delete watchpoint by NO", cmd_d},
    {"w", "create watchpoint with expr", cmd_w},
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
