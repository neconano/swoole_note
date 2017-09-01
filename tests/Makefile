
SRC_DIR=./src
INC_DIR=../\
		../include

BIN_DIR=./bin
OBJ_DIR=$(BIN_DIR)/obj

SRC_DIR_2=../src
OBJ_DIR_2=$(BIN_DIR)/inc/obj

MKDIR := mkdir -p  
CC := gcc
TARGETS = opxl

inc_dirs := $(foreach dir,$(INC_DIR), -I$(dir) )
# 引入声明文件目录
CFLAGS := -g $(inc_dirs)
# 动态连接非默认库
LDFLAGS = -pthread -lpthread -ldl

src := $(wildcard $(SRC_DIR)/*.c  )
dir := $(notdir $(src))
obj := $(patsubst %.c,%.o,$(dir) )
ALL_O := $(addprefix $(OBJ_DIR)/, $(obj))
ALL_C := $(SRC_DIR) $(ALL_O)

#core
src_2 := $(wildcard $(SRC_DIR_2)/core/*.c  )
dir_2 := $(notdir $(src_2))
obj_2 := $(patsubst %.c,%.o,$(dir_2) )
CORE_O := $(addprefix $(OBJ_DIR_2)/core/, $(obj_2))
CORE_C := $(SRC_DIR_2) $(CORE_O)

#network
src_2 := $(wildcard $(SRC_DIR_2)/network/*.c  )
dir_2 := $(notdir $(src_2))
obj_2 := $(patsubst %.c,%.o,$(dir_2) )
NETWORK_O := $(addprefix $(OBJ_DIR_2)/network/, $(obj_2))
NETWORK_C := $(SRC_DIR_2) $(NETWORK_O)

#memory
src_2 := $(wildcard $(SRC_DIR_2)/memory/*.c  )
dir_2 := $(notdir $(src_2))
obj_2 := $(patsubst %.c,%.o,$(dir_2) )
MEM_O := $(addprefix $(OBJ_DIR_2)/memory/, $(obj_2))
MEM_C := $(SRC_DIR_2) $(MEM_O)

#factory
src_2 := $(wildcard $(SRC_DIR_2)/factory/*.c  )
dir_2 := $(notdir $(src_2))
obj_2 := $(patsubst %.c,%.o,$(dir_2) )
FAC_O := $(addprefix $(OBJ_DIR_2)/factory/, $(obj_2))
FAC_C := $(SRC_DIR_2) $(FAC_O)

#lock
src_2 := $(wildcard $(SRC_DIR_2)/lock/*.c  )
dir_2 := $(notdir $(src_2))
obj_2 := $(patsubst %.c,%.o,$(dir_2) )
LOCK_O := $(addprefix $(OBJ_DIR_2)/lock/, $(obj_2))
LOCK_C := $(SRC_DIR_2) $(LOCK_O)

#os
src_2 := $(wildcard $(SRC_DIR_2)/os/*.c  )
dir_2 := $(notdir $(src_2))
obj_2 := $(patsubst %.c,%.o,$(dir_2) )
OS_O := $(addprefix $(OBJ_DIR_2)/os/, $(obj_2))
OS_C := $(SRC_DIR_2) $(OS_O)

#pipe
src_2 := $(wildcard $(SRC_DIR_2)/pipe/*.c  )
dir_2 := $(notdir $(src_2))
obj_2 := $(patsubst %.c,%.o,$(dir_2) )
PIPE_O := $(addprefix $(OBJ_DIR_2)/pipe/, $(obj_2))
PIPE_C := $(SRC_DIR_2) $(PIPE_O)

#protocol
src_2 := $(wildcard $(SRC_DIR_2)/protocol/*.c  )
dir_2 := $(notdir $(src_2))
obj_2 := $(patsubst %.c,%.o,$(dir_2) )
PROT_O := $(addprefix $(OBJ_DIR_2)/protocol/, $(obj_2))
PROT_C := $(SRC_DIR_2) $(PROT_O)

#reactor
src_2 := $(wildcard $(SRC_DIR_2)/reactor/*.c  )
dir_2 := $(notdir $(src_2))
obj_2 := $(patsubst %.c,%.o,$(dir_2) )
REAC_O := $(addprefix $(OBJ_DIR_2)/reactor/, $(obj_2))
REAC_C := $(SRC_DIR_2) $(REAC_O)



ALL_SRC = $(ALL_C) $(CORE_C) $(NETWORK_C) $(MEM_C) $(FAC_C) $(LOCK_C) $(OS_C) $(PIPE_C) $(PROT_C) $(REAC_C)
ALL_OBJ = $(ALL_O) $(CORE_O) $(NETWORK_O) $(MEM_O) $(FAC_O) $(LOCK_O) $(OS_O) $(PIPE_O) $(PROT_O) $(REAC_O)

#-D HAVE_EPOLL 等价于在头文件中定义：#define HAVE_EPOLL 1
all:$(ALL_SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(ALL_OBJ) -o $(BIN_DIR)/$(TARGETS)  -D HAVE_EPOLL

#编译依赖
$(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	$(MKDIR) $(OBJ_DIR)
	$(CC) -c $(CFLAGS) $(LDFLAGS) $< -o $@
	
$(OBJ_DIR_2)/core/%.o : $(SRC_DIR_2)/core/%.c
	$(MKDIR) $(OBJ_DIR_2)/core
	$(CC) -c $(CFLAGS) $(LDFLAGS) $< -o $@

$(OBJ_DIR_2)/network/%.o : $(SRC_DIR_2)/network/%.c
	$(MKDIR) $(OBJ_DIR_2)/network
	$(CC) -c $(CFLAGS) $(LDFLAGS) $< -o $@

$(OBJ_DIR_2)/memory/%.o : $(SRC_DIR_2)/memory/%.c
	$(MKDIR) $(OBJ_DIR_2)/memory
	$(CC) -c $(CFLAGS) $(LDFLAGS) $< -o $@

$(OBJ_DIR_2)/factory/%.o : $(SRC_DIR_2)/factory/%.c
	$(MKDIR) $(OBJ_DIR_2)/factory
	$(CC) -c $(CFLAGS) $(LDFLAGS) $< -o $@

$(OBJ_DIR_2)/lock/%.o : $(SRC_DIR_2)/lock/%.c
	$(MKDIR) $(OBJ_DIR_2)/lock
	$(CC) -c $(CFLAGS) $(LDFLAGS) $< -o $@

$(OBJ_DIR_2)/os/%.o : $(SRC_DIR_2)/os/%.c
	$(MKDIR) $(OBJ_DIR_2)/os
	$(CC) -c $(CFLAGS) $(LDFLAGS) $< -o $@

$(OBJ_DIR_2)/pipe/%.o : $(SRC_DIR_2)/pipe/%.c
	$(MKDIR) $(OBJ_DIR_2)/pipe
	$(CC) -c $(CFLAGS) $(LDFLAGS) $< -o $@

$(OBJ_DIR_2)/protocol/%.o : $(SRC_DIR_2)/protocol/%.c
	$(MKDIR) $(OBJ_DIR_2)/protocol
	$(CC) -c $(CFLAGS) $(LDFLAGS) $< -o $@

$(OBJ_DIR_2)/reactor/%.o : $(SRC_DIR_2)/reactor/%.c
	$(MKDIR) $(OBJ_DIR_2)/reactor
	$(CC) -c $(CFLAGS) $(LDFLAGS) $< -o $@


clean:
	rm -fr $(OBJ_DIR)
	rm -fr $(OBJ_DIR_2)
	rm -fr $(TARGETS)